-- =============================================================================
--  samurai_boss_brain.lua — 사무라이 보스 AI (Utility AI / IAUS)
-- =============================================================================
--  설계: AAA 전투 AI에서 검증된 "Utility AI"를 차용.
--    · Infinite Axis Utility System (Dave Mark, GDC AI Summit; Guild Wars 2 등) —
--      각 행동(Action)을 여러 고려요소(Consideration)의 점수 곱으로 평가하고
--      최고 효용(utility) 행동을 선택한다. if-임계값 하드코딩 대신 response curve로
--      상황을 연속 평가하므로, 거리/체력/자세/상대상태에 따라 매끄럽게 적응한다.
--    · 전투 템포 자원(tempo): 공격하면 소모, 쉬면 회복 → "압박↔간격" 리듬이 창발.
--    · 행동 관성(commitment/inertia): 선택한 행동을 최소시간 유지 + 현재행동 가산점 →
--      매 틱 흔들리는(dithering) 문제 방지(유틸리티 AI의 필수 요소).
--
--  C++ 코어(EnemyCharacter)는 센서/이동/공격실행/몽타주만 제공하고, "무엇을 할지"는
--  전적으로 이 Lua가 결정한다. 방어는 가드/회피만(패링 없음 — 패링은 플레이어 몫).
-- =============================================================================

-- ── 0. 저수준 유틸 ───────────────────────────────────────────────────────────
local function call(o, fn, ...)
    if not o then return nil end
    local direct = o[fn]
    if direct ~= nil then return direct(o, ...) end
    if o.CallFunction ~= nil then return o:CallFunction(fn, ...) end
    if Reflection ~= nil and Reflection.Call ~= nil then return Reflection.Call(o, fn, ...) end
    return nil
end

local function clamp(v, lo, hi)
    if v < lo then return lo elseif v > hi then return hi else return v end
end

local function name_str(v) if v == nil then return "" end return tostring(v) end
local function name_valid(v) local s = name_str(v) return s ~= "" and s ~= "None" end

local PERILOUS = { None = 0, Thrust = 1, Sweep = 2, Grab = 3 }

-- ── 1. Response Curves (입력 0..1 → 효용 0..1) ───────────────────────────────
local Curve = {}
function Curve.linear(x)   x = clamp(x,0,1); return x end
function Curve.inv(x)      x = clamp(x,0,1); return 1 - x end
function Curve.quad(x)     x = clamp(x,0,1); return x * x end
function Curve.invquad(x)  x = clamp(x,0,1); return (1-x) * (1-x) end
function Curve.smooth(x)   x = clamp(x,0,1); return x * x * (3 - 2*x) end          -- smoothstep
-- 중심 center 부근에서 1, width 만큼 멀어지면 0으로 감쇠하는 종 모양.
function Curve.bell(x, center, width)
    if width <= 0 then return 0 end
    local d = math.abs(x - center) / width
    return clamp(1 - d*d, 0, 1)
end
-- t 이상이면 1로 부드럽게 상승.
function Curve.atLeast(x, t, soft)
    soft = soft or 0.25
    return Curve.smooth((x - (t - soft)) / soft)
end
-- t 이하이면 1로 부드럽게 상승(가까울수록↑ 같은 데 사용).
function Curve.atMost(x, t, soft)
    soft = soft or 0.25
    return Curve.smooth(((t + soft) - x) / soft)
end

-- ── 2. 전투 템포 자원 + 행동 상태(모듈 지속) ────────────────────────────────
local S = nil
local AI = {
    tempo = 1.0,        -- 0..1 공세 자원: 공격 소모/시간 회복 → 압박·간격 리듬
    action = nil,       -- 현재 커밋된 행동 이름
    commit = 0.0,       -- 현재 행동 잔여 커밋 시간(관성)
    counterUntil = 0.0, -- 가드 직후 "반격(riposte) 윈도우": 이 동안 가드 억제 + 반격 강제
}

-- ── 3. 사망 처리 ─────────────────────────────────────────────────────────────
local DEATH_DESTROY_DELAY = 2.5
local bDestroyed, bPendingDestroy, deathDelay, bRagdoll = false, false, 0.0, false
local KATANA_PATH = "Content/Data/Weapon/JapanSword/New_Katana_StaticMesh.uasset"
local KATANA_SOCKET = "Katana"
local BOSS_SWORD_TRAIL_PARTICLE_PATH = "Content/Particle System/SwordTrail.uasset"
local BOSS_SWORD_TRAIL_SOURCE_SOCKET = "TrailStart"
local BOSS_SWORD_TRAIL_TARGET_SOCKET = "TrailEnd"
local BOSS_SWORD_TRAIL_GRACE_TIME = 0.35
local katanaComponent = nil
local trailSourceParticle = nil
local trailTargetParticle = nil
local trailActive = false
local trailEmissionActive = false
local trailDeactivateRemaining = 0.0
local trailBindingWarned = false

local function set_particle_spawn_scale(particle, scale)
    if particle == nil then return end
    if Particle ~= nil and Particle.SetRuntimeSpawnRateScale ~= nil then
        Particle.SetRuntimeSpawnRateScale(particle, scale)
    elseif particle.SetRuntimeSpawnRateScale ~= nil then
        particle:SetRuntimeSpawnRateScale(scale)
    end
end

local function set_trail_particle_active(particle, active)
    if particle == nil then return end
    if active then
        if particle.Activate ~= nil then particle:Activate(false) end
    elseif particle.Deactivate ~= nil then
        particle:Deactivate()
    end
end

local function set_trail_emission(active)
    if trailEmissionActive == active then return end
    trailEmissionActive = active
    set_particle_spawn_scale(trailSourceParticle, active and 1.0 or 0.0)
    set_particle_spawn_scale(trailTargetParticle, 0.0)
end

local function deactivate_trail_now()
    set_trail_emission(false)
    trailActive = false
    trailDeactivateRemaining = 0.0
    set_trail_particle_active(trailSourceParticle, false)
    set_trail_particle_active(trailTargetParticle, false)
end

local function request_trail_deactivate()
    if not trailActive then return end
    set_trail_emission(false)
    trailDeactivateRemaining = math.max(trailDeactivateRemaining or 0.0, BOSS_SWORD_TRAIL_GRACE_TIME)
end

local function activate_trail()
    if trailSourceParticle == nil or trailTargetParticle == nil then return end
    trailActive = true
    trailDeactivateRemaining = 0.0
    set_trail_particle_active(trailSourceParticle, true)
    set_trail_particle_active(trailTargetParticle, true)
    set_trail_emission(true)
end

local function update_trail(dt, attackBusy)
    if not trailActive then return end
    if attackBusy then
        trailDeactivateRemaining = 0.0
        return
    end

    if trailEmissionActive then
        request_trail_deactivate()
    end

    if trailDeactivateRemaining <= 0.0 then return end
    trailDeactivateRemaining = trailDeactivateRemaining - (dt or 0.0)
    if trailDeactivateRemaining <= 0.0 then
        deactivate_trail_now()
    end
end

local function attach_boss_trail()
    if trailSourceParticle ~= nil then return end
    if Particle == nil or Particle.AttachSystemToSocket == nil or Particle.SetRibbonEdgeSourceComponents == nil then
        if not trailBindingWarned then
            print("[SamuraiBoss] paired sword trail particle binding missing")
            trailBindingWarned = true
        end
        return
    end

    trailSourceParticle = Particle.AttachSystemToSocket(
        obj,
        BOSS_SWORD_TRAIL_PARTICLE_PATH,
        BOSS_SWORD_TRAIL_SOURCE_SOCKET
    )
    trailTargetParticle = Particle.AttachSystemToSocket(
        obj,
        BOSS_SWORD_TRAIL_PARTICLE_PATH,
        BOSS_SWORD_TRAIL_TARGET_SOCKET
    )

    if trailSourceParticle == nil or trailTargetParticle == nil then
        print("[SamuraiBoss] paired sword trail attach failed: source="
            .. BOSS_SWORD_TRAIL_SOURCE_SOCKET .. " target=" .. BOSS_SWORD_TRAIL_TARGET_SOCKET)
        return
    end

    set_particle_spawn_scale(trailSourceParticle, 0.0)
    set_particle_spawn_scale(trailTargetParticle, 0.0)
    if trailSourceParticle.Activate ~= nil then trailSourceParticle:Activate(true) end
    if trailTargetParticle.Activate ~= nil then trailTargetParticle:Activate(true) end
    if trailSourceParticle.ResetParticles ~= nil then trailSourceParticle:ResetParticles() end
    if trailTargetParticle.ResetParticles ~= nil then trailTargetParticle:ResetParticles() end

    if Particle.SetRibbonEdgeSourceComponents(trailSourceParticle, trailSourceParticle, trailTargetParticle) then
        print("[SamuraiBoss] paired sword trail configured: source="
            .. BOSS_SWORD_TRAIL_SOURCE_SOCKET .. " target=" .. BOSS_SWORD_TRAIL_TARGET_SOCKET)
    end
end

local function request_death()
    if bDestroyed then return end
    deactivate_trail_now()
    bPendingDestroy = true; deathDelay = DEATH_DESTROY_DELAY
    call(obj, "Brain_ReleaseAttackToken"); call(obj, "StopEnemyMovement")
    if not bRagdoll then bRagdoll = true; call(obj, "EnterRagdoll") end
end
local function destroy_self()
    if bDestroyed then return end
    bPendingDestroy = false; bDestroyed = true
    if obj ~= nil and obj.Destroy ~= nil then obj:Destroy() else call(obj, "Destroy") end
end

-- ── 4. 블랙보드(인지 스냅샷) ─────────────────────────────────────────────────
local function sense()
    local range = call(obj, "Brain_GetAttackRange") or 3.0
    if range < 0.1 then range = 0.1 end
    local dist = call(obj, "Brain_GetDistance") or 9999.0
    local bb = {
        range        = range,
        dist         = dist,
        ratio        = dist / range,                                   -- 0=밀착 1=사거리 >1=원거리
        absAngle     = call(obj, "Brain_GetAbsAngle") or 180.0,
        phase        = call(obj, "Brain_GetPhase") or 1,
        selfHP       = call(obj, "Brain_GetSelfHealthRatio") or 1.0,
        targetHP     = call(obj, "Brain_GetTargetHealthRatio") or 1.0,
        posture      = call(obj, "Brain_GetTargetPostureRatio") or 1.0,
        recovery     = call(obj, "Brain_TargetInRecovery") == true,    -- 상대 후딜(빈틈)
        acting       = call(obj, "Brain_IsTargetActing") == true,      -- 상대 공격 동작 중
        threatening  = call(obj, "Brain_TargetThreatening") == true,
        perilous     = call(obj, "Brain_GetTargetPerilous") or 0,
        lastHit      = call(obj, "Brain_LastAttackConnected") == true,
        lastDeflect  = call(obj, "Brain_LastAttackWasDeflected") == true,
        cornered     = call(obj, "Brain_IsCornered") == true,
        perceive     = ((call(obj, "Brain_CanSeeTarget") == true and call(obj, "Brain_HasLineOfSight") == true)
                         or call(obj, "Brain_IsInProximity") == true),
        tempo        = AI.tempo,
    }
    bb.frontness = 1.0 - clamp(bb.absAngle / 180.0, 0, 1)
    return bb
end

-- ── 5. 공격 선택(어떤 공격을? — 가중치 기반 하위 유틸리티) ───────────────────
-- 페이즈/거리/각도/반복/전술태그/상대상태로 각 공격을 점수화해 최고를 고른다.
local function norm_tag(v)
    local s = tostring(v or "Neutral"):gsub("_", ""):gsub("%s+", ""):lower()
    return s
end
local function range_curve(d, mn, mx)
    local span = math.max(1.0, mx - mn)
    local t = clamp((d - mn) / span, 0, 1)
    return 1.0 - 0.6 * (math.abs(t - 0.5) * 2.0)
end
local function tactic_scale(tag, isGap, bb)
    tag = norm_tag(tag); local s = 1.0; local ph = bb.phase
    if tag == "opener" then s = s * ((call(obj,"Brain_GetStateTime") or 99) <= 1.5 and 1.35 or 0.7)
    elseif tag == "pressure" then s = s * 1.25
    elseif tag == "combo" then s = s * (bb.lastHit and 1.6 or 0.5)
    elseif tag == "gapcloser" or isGap then s = s * (1.15 + 0.15*ph)
    elseif tag == "punish" then s = s * (bb.recovery and 1.9 or 0.7)
    elseif tag == "phasechange" then s = s * (ph >= 3 and 1.6 or 0.5) end
    if bb.posture > 0.0 and bb.posture < 0.4 and (tag == "pressure" or tag == "combo") then s = s * 1.35 end
    if ph >= 2 and (tag == "gapcloser" or tag == "phasechange" or isGap) then s = s * (1.0 + 0.12*(ph-1)) end
    return s
end
local function combo_gate(i, bb)
    if not call(obj, "Brain_AttackRequiresPreviousAttack", i) then return true end
    local req = call(obj, "Brain_GetAttackRequiredPreviousAttack", i)
    return name_valid(req) and name_str(req) == name_str(call(obj, "Brain_GetLastAttackName"))
end
-- 공격 이름의 페이즈 접두사("...P2...", "...P3...")로 "열리는 페이즈" 추정(전용 바인딩 없음 → 이름 파싱).
local function attack_open_phase(name)
    local s = name_str(name)
    if s:find("P3") then return 3 end
    if s:find("P2") then return 2 end
    return 1
end
-- 막 열린 페이즈의 신규 공격을 살짝만 강조한다. 과거 1.6 은 P2 진입 시 신규 P2 공격이 기존 P1
-- 공격을 압도해 "2페이즈에서 이전 공격을 거의 안 쓰는" 문제를 만들었다(요청 #2). 1.2 로 낮춰
-- 신규 패턴은 여전히 우선되되 P1 검격(드로우/와이드/백핸드)도 로테이션에 꾸준히 섞이게 한다.
local PHASE_OPEN_BONUS = 1.2

-- rangedOnly=true 면 갭클로저/원거리(MinRange≥3) 공격만 후보 → gapstrike 가 헛스윙 없이 거리를 좁히며 친다.
local function select_attack(bb, rangedOnly)
    local n = call(obj, "Brain_GetAttackCount") or 0
    if n <= 0 then return false end
    call(obj, "Brain_BeginDecisionTrace")
    local best, bestScore = nil, -1.0
    for i = 0, n - 1 do
        if call(obj, "Brain_CanUseAttackIndex", i) and combo_gate(i, bb) then
            local name   = call(obj, "Brain_GetAttackName", i)
            local w      = call(obj, "Brain_GetAttackBaseWeight", i) or 0.0
            local prio   = call(obj, "Brain_GetAttackPriority", i) or 1.0
            local mn     = call(obj, "Brain_GetAttackMinRange", i) or 0.0
            local mx     = call(obj, "Brain_GetAttackMaxRange", i) or 1.0
            local maxAng = call(obj, "Brain_GetAttackMaxAbsAngle", i) or 70.0
            local rScale = call(obj, "Brain_GetAttackRepeatWeightScale", i) or 1.0
            local rCount = call(obj, "Brain_GetRecentAttackRepeat", name) or 0
            local tag    = call(obj, "Brain_GetAttackTacticTag", i) or "Neutral"
            local isGap  = call(obj, "Brain_IsAttackGapCloser", i) == true
            local peril  = call(obj, "Brain_GetAttackPerilousType", i) or 0
            local ranged = isGap or mn >= 3.0

            if (not rangedOnly) or ranged then
                local score = math.max(0.0001, w) * math.max(0.1, prio)
                    * range_curve(bb.dist, mn, mx)
                    * (1.0 - 0.7 * clamp(bb.absAngle / math.max(1.0, maxAng), 0, 1))
                    * math.pow(clamp(rScale, 0, 1), math.max(0, rCount))
                    * tactic_scale(tag, isGap, bb)
                if peril > 0 then score = score * (bb.phase >= 3 and 1.2 or (bb.phase >= 2 and 1.05 or 0.45)) end
                -- 현재 페이즈에서 막 열린 공격을 강조 → 페이즈 전환 시 신규 패턴이 부각됨.
                if attack_open_phase(name) == bb.phase then score = score * PHASE_OPEN_BONUS end
                score = score * (0.9 + math.random() * 0.2)
                call(obj, "Brain_AddDecisionCandidate", name, score)
                if score > bestScore then bestScore = score; best = name end
            end
        end
    end
    if best and bestScore > 0.05 and call(obj, "Brain_SetSelectedAttack", best) then
        call(obj, "Brain_CommitDecisionTrace", best)
        return true
    end
    call(obj, "Brain_CommitDecisionTrace", "None")
    return false
end

local function try_attack(bb)
    if call(obj, "Brain_AcquireAttackToken") ~= true then return false end
    if select_attack(bb) and call(obj, "Brain_PlaySelectedAttack") == true then
        activate_trail()
        return true
    end
    call(obj, "Brain_ReleaseAttackToken")
    return false
end

-- 갭스트라이크 전용: 원거리/갭클로저 공격만 골라 거리를 좁히며 친다(공간 있을 때 사용).
local function try_gap_attack(bb)
    if call(obj, "Brain_AcquireAttackToken") ~= true then return false end
    if select_attack(bb, true) and call(obj, "Brain_PlaySelectedAttack") == true then
        activate_trail()
        return true
    end
    call(obj, "Brain_ReleaseAttackToken")
    return false
end

-- ── 6. 행동(Action) 정의 — 데이터 주도 ───────────────────────────────────────
--  score(bb): 0이면 비후보. run(bb): 실행, true 반환 시 그 틱 종결(추가 이동/로코 생략).
--  commit: 선택 후 최소 유지 시간(관성). attack 은 Brain_IsBusy 로 커밋되어 0.
local Actions = {
    -- ▷ 회피: 막을 수 없는 위험공격(하단/잡기) 최우선, 그 외엔 상대 동작 중 가끔 날렵하게.
    {
        name = "dodge", commit = 0.0,
        score = function(bb)
            if not bb.perceive or bb.ratio > 1.8 then return 0 end
            if bb.perilous == PERILOUS.Sweep or bb.perilous == PERILOUS.Grab then return 0.97 end
            if bb.acting then return (0.35 + 0.05*bb.phase) * Curve.atMost(bb.ratio, 1.2) end
            return 0
        end,
        run = function(bb) call(obj, "Brain_Dodge"); return true end,
    },
    -- ▷ 백점프(하드 디스인게이지 = "빠지고"의 주역): 연타로 자원 바닥나면 크게 도약해 거리를 확 벌린다.
    --   걷는 후퇴는 속도 상한이 있어 소극적이라, 거대 보스의 "치고 빠지고"는 이 큰 점프가 담당한다.
    --   (LeapHorizontalForce 가 거리를 결정 — scene/패널에서 상향.) 플레이어 공격만으로는 안 빠짐(도망 방지).
    {
        name = "leap", commit = 1.7,
        score = function(bb)
            if not bb.perceive or bb.ratio > 1.4 then return 0 end     -- 근접에서 분리할 때만
            local s = 0.0
            -- 자원 완전 바닥(연타 끝)에서 백점프로 한 박자 분리해 "쉴 틈"을 만든다(요청). 살짝만 올려
            -- (0.30→0.38) 콤보 끝에서 공격(저템포 ~0.30)보다 약간 더 자주 이기게 — 매번은 아니고 자주.
            if bb.tempo < 0.12 then s = s + 0.38 end                   -- 깊은 바닥 → 백점프로 분리(쉴 틈)
            if bb.perilous == PERILOUS.Sweep or bb.perilous == PERILOUS.Grab then s = s + 0.30 end
            if bb.selfHP < 0.25 and (bb.acting or bb.threatening) then s = s + 0.15 end
            -- 바짝 붙어(자원·거리 무관) 공격 중이어도 아주 낮은 확률(~0.1%)로 기습 백점프해 거리를 벌린다
            -- (요청). 1.0 을 더해 그 틱엔 leap 이 공격을 이기게 → 예측 불가한 이탈. 확률만 올리면 더 자주.
            if math.random() < 0.001 then s = s + 1.0 end
            return s
        end,
        run = function(bb) call(obj, "Brain_LeapBack"); return true end,
    },
    -- ▷ 가드: 상대가 공격 동작 중 + 정면 근접이면 막는다(반사 없음).
    {
        name = "guard", commit = 0.0,
        score = function(bb)
            -- 가드 직후 반격 윈도우 동안은 다시 가드하지 않는다 → 가드↔반격 교대(상시 가드 락 방지).
            if AI.counterUntil > 0.0 then return 0 end
            if not bb.perceive or bb.ratio > 1.6 or bb.absAngle > 110 then return 0 end
            if bb.perilous == PERILOUS.Sweep or bb.perilous == PERILOUS.Grab then return 0 end
            if not (bb.acting or bb.threatening) then return 0 end
            local s = 0.72 * Curve.atMost(bb.ratio, 1.3) * bb.frontness
            if bb.selfHP < 0.35 then s = s + 0.1 end
            if bb.phase >= 3 then s = s * 0.85 end           -- 후반엔 막기보다 받아쳐 넣음
            return clamp(s, 0, 1.1)
        end,
        run = function(bb)
            call(obj, "Brain_OpenGuard")   -- 반격 윈도우는 "선택"이 아니라 "실제로 막은" 순간에만 연다(Tick 참조)
            return true
        end,
    },
    -- ▷ 공격: 사거리 적합 × 템포(자원) × 빈틈. 기본부터 공세적이되, 자원이 바닥나면 잠깐 쉰다.
    {
        name = "attack", commit = 0.0,
        score = function(bb)
            if not bb.perceive or bb.ratio > 2.8 then return 0 end
            -- 가드 직후 반격(riposte) 윈도우: 자원/상대상태와 무관하게 강하게 반격해 상시 가드 락을
            -- 깨고 공세로 전환한다. 단, 막을 수 없는 위험공격(하단/잡기)이면 회피가 처리하도록 양보.
            if AI.counterUntil > 0.0 and bb.ratio <= 1.6
               and bb.perilous ~= PERILOUS.Sweep and bb.perilous ~= PERILOUS.Grab then
                return 1.2
            end
            local fit  = (bb.ratio <= 1.15) and 1.0 or Curve.atMost(bb.ratio, 1.6, 0.9)  -- 사거리 안 최고, 밖이면 갭클로저용 잔존
            -- 저템포 바닥을 ~0.42→~0.58 로 올린다. 자원이 바닥나도 근접에선 후퇴/점프보다 공격이 이기게
            -- 해 "콤보 끝마다 빠지는" 대신 계속 압박하게 한다(요청: 빠지는 빈도 축소).
            local fuel = Curve.smooth(0.55 + 0.45 * bb.tempo)
            local opening = 1.0
            if bb.recovery then opening = 1.75
            elseif bb.posture > 0 and bb.posture < 0.4 then opening = 1.35
            elseif bb.acting then opening = 0.5 end              -- 상대 스윙에 무리 진입 자제(가드 우선)
            if bb.targetHP > 0 and bb.targetHP < 0.3 then opening = opening * 1.2 end
            -- 막 탄기(반격)당했어도 공세를 크게 줄이지 않는다 → 반격 리코일 직후 곧장 공격(요청 A). 0.5→0.9.
            local parried = bb.lastDeflect and 0.9 or 1.0
            -- 페이즈 성향: P1 절제된 검술(공세↓) → P2 → P3 광폭화(공세↑). 각 페이즈를 확연히 다르게.
            local phaseAggr = (bb.phase >= 3) and 1.20 or ((bb.phase >= 2) and 0.95 or 0.70)
            return 0.75 * fit * fuel * opening * parried * phaseAggr
        end,
        run = function(bb)
            if try_attack(bb) then
                -- 연타 자원 소모: tempo 는 0..1 이라 과거의 -3.5 는 단 한 번의 공격으로 자원을 0으로
                -- 만들어 "한 대 치고 곧장 빠지는"(요청 #3) 원인이었다. 한 타당 소모를 작게(페이즈가
                -- 높을수록 더 작게) 잡아, 바닥나기 전 P1≈3타·P2≈4타·P3≈5타를 연속으로 이어가게 한다.
                local drain = (bb.phase >= 3) and 0.18 or ((bb.phase >= 2) and 0.24 or 0.30)
                AI.tempo = math.max(0.0, AI.tempo - drain)
                return true                                      -- 이후 Brain_IsBusy 로 커밋
            end
            return false                                         -- 실패 시 다른 행동에 양보
        end,
    },
    -- ▷ 갭스트라이크: 공간이 생기면(후퇴/도약 후·플레이어가 물러설 때) 원거리/갭클로저 공격으로
    --   거리를 좁히며 친다 → P2/P3 원거리 애니(FlashStep/AerialDrop/Meteor) 활용 + 근접 단조로움 해소.
    --   ranged 전용 선택이라 헛스윙 없음. 가용 원거리 공격 없으면(쿨다운) 접근에 양보.
    {
        name = "gapstrike", commit = 0.0,
        score = function(bb)
            if not bb.perceive or bb.phase < 2 then return 0 end
            if bb.ratio < 1.1 or bb.ratio > 3.5 then return 0 end   -- 공간이 있을 때만(근접은 attack 이 처리)
            local fuel = Curve.smooth(0.45 + 0.55 * bb.tempo)
            return 1.0 * (0.6 + 0.4 * fuel) * ((bb.phase >= 3) and 1.2 or 1.0)
        end,
        run = function(bb)
            if try_gap_attack(bb) then
                -- 갭스트라이크도 동일하게 작은 소모만(과거 -3.5 는 한 번에 자원을 비웠다). 거리를 좁히며
                -- 친 직후에도 자원이 남아 곧바로 근접 연타로 이어가게 한다.
                local drain = (bb.phase >= 3) and 0.20 or 0.26
                AI.tempo = math.max(0.0, AI.tempo - drain)
                return true
            end
            return false
        end,
    },
    -- ▷ 접근(갭클로즈): 사거리 밖이면 적극적으로 파고든다.
    {
        name = "approach", commit = 0.25, loco = true,
        score = function(bb)
            if bb.ratio <= 0.9 then return 0 end          -- 타격권 안쪽까지 들어오면 멈춤(공격에 양보)
            -- 타격권 밖이면 강하게 1로 올라 파고든다 — 서성임 대신 압박.
            local far = Curve.atLeast(bb.ratio, 0.95, 0.35)
            return 0.85 * far * Curve.smooth(0.55 + 0.45 * bb.tempo)
        end,
        run = function(bb) call(obj, "Brain_Chase"); return true end,
    },
    -- ▷ 스트레이프(서클/관찰): 아주 낮은 기본값 — 타격권 안에선 접근/공격에 양보, 중거리에서만 가끔 돈다.
    {
        name = "strafe", commit = 0.30, loco = true,
        score = function(bb)
            if not bb.perceive then return 0 end
            -- 사실상 비활성 — 아주 먼 관찰 거리에서만 미세하게. 근접/교전 서성임을 완전히 제거.
            return 0.06 * Curve.atLeast(bb.ratio, 1.6, 0.4)
        end,
        run = function(bb) call(obj, "Brain_Strafe"); return true end,
    },
    -- ▷ 후퇴(걷는 간격 조절): 속도 상한이 있어 큰 분리는 leap 이 담당. 여긴 보조로 더 오래 걸어 거리 확보.
    {
        name = "retreat", commit = 0.65, loco = true,
        score = function(bb)
            if not bb.perceive then return 0 end
            -- 이미 충분히 벌어졌으면 더는 안 물러선다(끝없는 후퇴 방지). 단, 컷오프를 1.15→1.30 으로
            -- 약간 키워 한 번 물러날 때 조금 더 멀리 간다(요청: 너무 짧게 가지 않게).
            if bb.ratio >= 1.30 then return 0 end
            -- 자원이 바닥났을 때만, 그것도 '약간의 공간이 있을 때만' 물러선다. 플레이어가 코앞이면
            -- (ratio 낮음) atLeast 가 0에 수렴 → 후퇴 대신 근접 공격(attack)이 이긴다. 커밋도 0.40→0.65
            -- 로 늘려 한 번 물러날 때 더 오래(=더 멀리) 걷는다(요청: 너무 짧게 가지 않게).
            local rest = 0.36 * Curve.invquad(bb.tempo) * Curve.atLeast(bb.ratio, 0.85, 0.30)
            local s = rest
            if bb.cornered then s = s * 0.3 end                  -- 코너면 후퇴 대신 스트레이프/공격이 이김
            return s
        end,
        run = function(bb) call(obj, "Brain_Reposition"); return true end,
    },
}

-- ── 7. 유틸리티 선택기 (점수화 → 관성/노이즈 → 최고 선택) ────────────────────
local function find_action(name)
    for _, a in ipairs(Actions) do if a.name == name then return a end end
    return nil
end

-- 이동(loco) 행동일 때만 걷기/달리기 몽타주를 구동한다(공격/가드/회피는 자기 몽타주가 재생).
local function run_action(a, bb)
    if not a.run(bb) then return false end
    if a.loco then call(obj, "Brain_DriveLocomotion") end
    return true
end

local function choose_and_run(bb, dt)
    -- 도약 중에는 끝까지 진행한다(공중에선 방향 못 바꿈). 공중 점프 단계 애니(Up/Down)는 계속 구동.
    if AI.action == "leap" and AI.commit > 0.0 then
        call(obj, "Brain_DriveLocomotion")
        return
    end

    -- 관성: 현재 커밋된 이동/관찰 행동이 아직 유효하면 유지(디더링 방지).
    if AI.action and AI.commit > 0.0 then
        local cur = find_action(AI.action)
        if cur and cur.score(bb) > 0.0 and run_action(cur, bb) then
            return
        end
    end

    -- 모든 행동 점수화(노이즈 + 현재행동 관성 가산점) 후 내림차순.
    local scored = {}
    for _, a in ipairs(Actions) do
        local u = a.score(bb)
        if u > 0.0 then
            u = u * (0.92 + math.random() * 0.16)
            if a.name == AI.action then u = u * 1.12 end
            scored[#scored + 1] = { a = a, u = u }
        end
    end
    table.sort(scored, function(p, q) return p.u > q.u end)

    -- 최고부터 실행 시도. 실패(예: 공격 불가)면 다음 후보로 — 정적/멈춤 방지.
    for _, e in ipairs(scored) do
        AI.action = e.a.name
        AI.commit = e.a.commit
        if run_action(e.a, bb) then return end
    end

    AI.action = "approach"; AI.commit = 0.25
    call(obj, "Brain_Chase"); call(obj, "Brain_DriveLocomotion")   -- 폴백: 서성이지 말고 파고든다(공격 쿨다운 중에도 압박 유지)
end

-- ── 8. 런타임 상태 정의 ──────────────────────────────────────────────────────
local LOCO = { Locked = 0, InputAllowed = 1, Strafe = 2, Retreat = 3 }
local function define_states()
    local sm = call(obj, "GetStateMachine")
    if not sm then return end
    call(sm, "ClearStateDefinitions")
    call(sm, "DefineState", "Idle", LOCO.InputAllowed, 0, true, false)
    call(sm, "DefineState", "Chase", LOCO.InputAllowed, 1, true, true)
    call(sm, "DefineState", "Strafe", LOCO.Strafe, 2, true, true)
    call(sm, "DefineState", "Reposition", LOCO.Retreat, 3, true, true)
    call(sm, "DefineState", "Attack", LOCO.Locked, 4, false, false)
    call(sm, "DefineState", "Deflect", LOCO.Locked, 5, true, true)
    call(sm, "DefineState", "Guard", LOCO.Locked, 5, true, true)
    call(sm, "DefineState", "Dodge", LOCO.Locked, 4, true, true)
    call(sm, "DefineState", "Recover", LOCO.InputAllowed, 6, true, false)
    call(sm, "DefineState", "Hit", LOCO.Locked, 7, true, false)
    call(sm, "DefineState", "Staggered", LOCO.Locked, 8, true, false)
    call(sm, "DefineState", "Dead", LOCO.Locked, 9, false, false)
    call(sm, "DefineState", "Intro", LOCO.Locked, 10, true, true)
    call(sm, "DefineState", "PhaseChanged", LOCO.Locked, 11, true, true)
    call(sm, "DefineState", "EncounterCompleted", LOCO.InputAllowed, 12, true, false)
end

-- ── 9. 라이프사이클 ──────────────────────────────────────────────────────────
function BeginPlay()
    define_states()
    S = { isBoss = call(obj, "Brain_IsBoss") == true }
    pcall(function() math.randomseed((tonumber(obj.UUID) or os.time() or 1) + 23) end)

    if Equipment ~= nil and Equipment.AttachStaticMeshToSocket ~= nil then
        katanaComponent = Equipment.AttachStaticMeshToSocket(
            obj,
            KATANA_PATH,
            KATANA_SOCKET,
            Vec3(1.0, 1.0, 1.0)
        )
        attach_boss_trail()
    else
        print("[SamuraiBoss] Equipment.AttachStaticMeshToSocket binding missing")
    end
end

function EndPlay()
    deactivate_trail_now()
    call(obj, "Brain_ReleaseAttackToken")
end

function OnDamaged(damageSpec, damageReport)
    if damageReport and (damageReport.bKilled == true
        or (damageReport.NewHealth ~= nil and damageReport.NewHealth <= 0.0)) then
        request_death()
    end
end

function Tick(dt)
    if bDestroyed or not S then return end
    if bPendingDestroy then
        deathDelay = deathDelay - dt
        if deathDelay <= 0.0 then destroy_self() end
        return
    end

    if S.isBoss and call(obj, "IsBossEncounterActive") ~= true then
        deactivate_trail_now()
        call(obj, "Brain_ReleaseAttackToken")
        call(obj, "StopEnemyMovement")
        return
    end

    call(obj, "Brain_Sense")
    -- 반격 윈도우는 공격(busy) 중에도 흘러야 "가드→반격 1회→(여전히 압박 시)가드→반격" 교대가 된다.
    AI.counterUntil = math.max(0.0, AI.counterUntil - dt)
    -- 실제로 가드로 막은 직후에만 반격 윈도우를 연다(가드 "선택"만으로 반격하지 않음 — 정확한 리포스트).
    if call(obj, "Brain_GuardBlockedRecently") == true then
        AI.counterUntil = 0.55
    end
    local attackBusy = call(obj, "Brain_IsBusy") == true
    update_trail(dt, attackBusy)
    if attackBusy then return end
    if call(obj, "Brain_IsBusy") == true then return end          -- 공격/경직 중 — 두뇌 정지
    if call(obj, "Brain_ConsumeCombatStep") ~= true then return end

    -- 템포 자원 회복 + 행동 커밋 타이머 감소(프레임당 1회). 회복 속도를 페이즈로 달리해 리듬을
    -- 차별화: P1 느림(긴 호흡, 절제) → P3 빠름(짧은 호흡, 연속 압박).
    local _ph = call(obj, "Brain_GetPhase") or 1
    -- 회복을 다소 느리게 → 연속 공격 후 다시 파고들기까지 "쉬는 시간"이 길어진다(요청: 휴식 ↑).
    -- 단, 너무 느리면 소극적이 되니 적당히(P1 0.30→0.20, P2 0.35→0.26, P3 0.50→0.40).
    local _regen = (_ph >= 3) and 0.40 or ((_ph >= 2) and 0.26 or 0.20)
    AI.tempo  = clamp(AI.tempo + _regen * dt, 0.0, 1.0)
    AI.commit = math.max(0.0, AI.commit - dt)

    if call(obj, "Brain_AcquireTarget") ~= true then
        AI.action, AI.commit, AI.tempo = nil, 0.0, 1.0
        call(obj, "Brain_Idle"); call(obj, "Brain_DriveLocomotion")
        return
    end

    -- 전투 확정 전(은신 게이팅)에는 조사/대기만.
    if call(obj, "Brain_IsAlerted") == false then
        local aw = call(obj, "Brain_GetAwarenessState") or 3
        if aw == 2 or aw == 4 then call(obj, "Brain_Investigate") else call(obj, "Brain_Idle") end
        call(obj, "Brain_DriveLocomotion")
        return
    end

    call(obj, "Brain_FaceTarget")

    local bb = sense()
    -- 행동 선택 + 실행. 이동 행동일 때만 내부에서 로코모션 몽타주를 구동한다(공격/가드/회피는
    -- 자기 몽타주 재생, 피격 리액션엔 C++ DriveLocomotion 이 양보).
    choose_and_run(bb, dt)
end
