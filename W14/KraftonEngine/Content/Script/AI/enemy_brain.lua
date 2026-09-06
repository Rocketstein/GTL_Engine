-- =============================================================================
--  enemy_brain.lua — 적 AI 정책 레이어
-- =============================================================================
--  C++은 센서/블랙보드/이동/공격 실행만 제공하고, 행동 선택·공격 점수·반응·페이즈
--  정책은 이 Lua 계층에서 결정한다.
-- =============================================================================

local STYLE = {
    Passive    = { attack=0.0, approach=0.4, strafe=0.8, retreat=1.2, deflect=0.15, react=0.35 },
    Balanced   = { attack=1.0, approach=1.0, strafe=0.7, retreat=0.35, deflect=0.75, react=0.55 },
    Aggressive = { attack=1.45, approach=1.15, strafe=0.45, retreat=0.15, deflect=0.45, react=0.45 },
    Defensive  = { attack=0.75, approach=0.75, strafe=0.95, retreat=0.85, deflect=1.45, react=0.75 },
    Boss       = { attack=1.30, approach=1.0, strafe=0.55, retreat=0.20, deflect=1.15, react=0.85 },
}

local TACTIC = {
    Neutral     = "Neutral",
    Opener      = "Opener",
    Pressure    = "Pressure",
    Combo       = "Combo",
    GapCloser   = "GapCloser",
    Punish      = "Punish",
    Retreat     = "Retreat",
    PhaseChange = "PhaseChange",
}

local S = nil
local GROUND_SPEED_VAR = "GroundSpeed"
local DEATH_DESTROY_DELAY = 2.0
local bDestroyedByDeath = false
local bPendingDestroyFromDeath = false
local deathDestroyDelayRemaining = 0.0

local SPEAR_PATH = "Content/Data/Weapon/Spear/SM_Yari_StaticMesh.uasset"
local SPEAR_SOCKET = "Spear"
local spearComponent = nil

local LOCOMOTION = {
    Locked = 0,
    InputAllowed = 1,
    Strafe = 2,
    Retreat = 3,
}

local function call(o, fn, ...)
    if not o then return nil end

    local direct = o[fn]
    if direct ~= nil then
        return direct(o, ...)
    end

    if o.CallFunction ~= nil then
        return o:CallFunction(fn, ...)
    end

    if Reflection ~= nil and Reflection.Call ~= nil then
        return Reflection.Call(o, fn, ...)
    end

    return nil
end

local function clamp(v, lo, hi)
    if v < lo then return lo end
    if v > hi then return hi end
    return v
end

local function name_to_string(v)
    if v == nil then return "" end
    return tostring(v)
end

local function is_name_valid(v)
    local s = name_to_string(v)
    return s ~= "" and s ~= "None"
end

local function should_destroy_from_damage_report(damageReport)
    if damageReport == nil then
        return false
    end

    if damageReport.bKilled == true then
        return true
    end

    return damageReport.NewHealth ~= nil and damageReport.NewHealth <= 0.0
end

local function request_destroy_from_death()
    if bDestroyedByDeath then
        return
    end

    bPendingDestroyFromDeath = true
    deathDestroyDelayRemaining = DEATH_DESTROY_DELAY
    call(obj, "Brain_ReleaseAttackToken")
    call(obj, "StopEnemyMovement")
end

local function destroy_self_from_death()
    if bDestroyedByDeath then
        return
    end

    bPendingDestroyFromDeath = false
    bDestroyedByDeath = true

    if obj ~= nil and obj.Destroy ~= nil then
        obj:Destroy()
    else
        call(obj, "Destroy")
    end
end

local function get_character_movement()
    if obj == nil then
        return nil
    end

    if obj.GetCharacterMovement ~= nil then
        return obj:GetCharacterMovement()
    end

    return call(obj, "GetCharacterMovement")
end

local function get_anim_instance()
    if obj == nil then
        return nil
    end

    local mesh = nil
    if obj.GetMesh ~= nil then
        mesh = obj:GetMesh()
    end

    if mesh == nil then
        mesh = call(obj, "GetMesh")
    end

    if mesh == nil and obj.GetSkeletalMeshComponent ~= nil then
        mesh = obj:GetSkeletalMeshComponent()
    end

    if mesh == nil then
        mesh = call(obj, "GetSkeletalMeshComponent")
    end

    if mesh == nil then
        return nil
    end

    if mesh.GetAnimInstance ~= nil then
        return mesh:GetAnimInstance()
    end

    return call(mesh, "GetAnimInstance")
end

local function get_movement_velocity(movement)
    if movement == nil then
        return nil
    end

    if movement.GetVelocity ~= nil then
        return movement:GetVelocity()
    end

    if movement.GetVelocityValue ~= nil then
        return movement:GetVelocityValue()
    end

    local velocity = call(movement, "GetVelocity")
    if velocity ~= nil then
        return velocity
    end

    return call(movement, "GetVelocityValue")
end

local function update_ground_speed()
    local groundSpeed = 0.0
    local movement = get_character_movement()
    if movement ~= nil then
        local velocity = get_movement_velocity(movement)
        if velocity ~= nil and velocity.X ~= nil and velocity.Y ~= nil then
            groundSpeed = math.sqrt(velocity.X * velocity.X + velocity.Y * velocity.Y)
        else
            groundSpeed = call(movement, "GetSpeed") or 0.0
        end
    end

    local anim = get_anim_instance()
    if anim ~= nil then
        call(anim, "SetGraphVariableFloat", GROUND_SPEED_VAR, groundSpeed)
    end
end

local function current_style()
    if S and S.isBoss then return STYLE.Boss end
    return STYLE.Balanced
end

local function range_curve(distance, minRange, maxRange)
    local span = math.max(1.0, maxRange - minRange)
    local t = clamp((distance - minRange) / span, 0.0, 1.0)
    local off = math.abs(t - 0.5) * 2.0
    return 1.0 - 0.6 * off
end

local function angle_curve(absAngle, maxAbsAngle)
    local limit = math.max(1.0, maxAbsAngle)
    local t = clamp(absAngle / limit, 0.0, 1.0)
    return 1.0 - 0.7 * t
end

local function norm_tag(v)
    local s = tostring(v or "Neutral")
    s = s:gsub("_", ""):gsub("%s+", "")
    return s:lower()
end

local function tactic_scale(tactic, isGapCloser, targetThreatening, targetRecovery, targetPosture, phase, style)
    local tag = norm_tag(tactic)
    local scale = 1.0
    if tag == norm_tag(TACTIC.Opener) then
        scale = scale * (call(obj, "Brain_GetStateTime") <= 1.5 and 1.35 or 0.75)
    elseif tag == norm_tag(TACTIC.Pressure) then
        scale = scale * (1.0 + 0.35 * style.attack)
    elseif tag == norm_tag(TACTIC.Combo) then
        scale = scale * (is_name_valid(call(obj, "Brain_GetLastAttackName")) and 1.55 or 0.35)
    elseif tag == norm_tag(TACTIC.GapCloser) or isGapCloser then
        scale = scale * (1.05 + 0.30 * style.approach)
    elseif tag == norm_tag(TACTIC.Punish) then
        scale = scale * (targetRecovery and 1.65 or 0.85)
    elseif tag == norm_tag(TACTIC.Retreat) then
        scale = scale * (0.7 + style.retreat)
    elseif tag == norm_tag(TACTIC.PhaseChange) then
        scale = scale * (S.isBoss and 1.35 or 0.55)
    end

    if targetThreatening and (tag == norm_tag(TACTIC.Punish) or tag == norm_tag(TACTIC.Retreat)) then
        scale = scale * 1.25
    end
    if targetPosture > 0.0 and targetPosture < 0.4 and (tag == norm_tag(TACTIC.Pressure) or tag == norm_tag(TACTIC.Combo)) then
        scale = scale * 1.35
    end
    if phase >= 2 and (tag == norm_tag(TACTIC.Pressure) or tag == norm_tag(TACTIC.GapCloser) or tag == norm_tag(TACTIC.PhaseChange) or isGapCloser) then
        scale = scale * (1.0 + 0.12 * (phase - 1))
    end
    return scale
end

local function combo_gate(index)
    if not call(obj, "Brain_AttackRequiresPreviousAttack", index) then
        return true
    end
    local required = call(obj, "Brain_GetAttackRequiredPreviousAttack", index)
    return is_name_valid(required) and name_to_string(required) == name_to_string(call(obj, "Brain_GetLastAttackName"))
end

local function select_attack()
    local count = call(obj, "Brain_GetAttackCount") or 0
    if count <= 0 then return false end

    local style = current_style()
    local distance = call(obj, "Brain_GetDistance") or 9999.0
    local absAngle = call(obj, "Brain_GetAbsAngle") or 180.0
    local phase = call(obj, "Brain_GetPhase") or 1
    local targetPosture = call(obj, "Brain_GetTargetPostureRatio") or 1.0
    local targetRecovery = call(obj, "Brain_TargetInRecovery") == true
    local targetThreatening = call(obj, "Brain_TargetThreatening") == true
    local perceptualScale = ((call(obj, "Brain_CanSeeTarget") == true and call(obj, "Brain_HasLineOfSight") == true) or call(obj, "Brain_IsInProximity") == true) and 1.0 or 0.0

    call(obj, "Brain_BeginDecisionTrace")

    local bestName = nil
    local bestScore = -1.0

    for i = 0, count - 1 do
        if call(obj, "Brain_CanUseAttackIndex", i) and combo_gate(i) then
            local name = call(obj, "Brain_GetAttackName", i)
            local weight = call(obj, "Brain_GetAttackBaseWeight", i) or 0.0
            local priority = call(obj, "Brain_GetAttackPriority", i) or 1.0
            local minRange = call(obj, "Brain_GetAttackMinRange", i) or 0.0
            local maxRange = call(obj, "Brain_GetAttackMaxRange", i) or 1.0
            local maxAngle = call(obj, "Brain_GetAttackMaxAbsAngle", i) or 70.0
            local repeatScale = call(obj, "Brain_GetAttackRepeatWeightScale", i) or 1.0
            local repeatCount = call(obj, "Brain_GetRecentAttackRepeat", name) or 0
            local tactic = call(obj, "Brain_GetAttackTacticTag", i) or "Neutral"
            local isGapCloser = call(obj, "Brain_IsAttackGapCloser", i) == true

            local score = math.max(0.0001, weight)
                * math.max(0.1, priority)
                * range_curve(distance, minRange, maxRange)
                * angle_curve(absAngle, maxAngle)
                * math.pow(clamp(repeatScale, 0.0, 1.0), repeatCount)
                * tactic_scale(tactic, isGapCloser, targetThreatening, targetRecovery, targetPosture, phase, style)
                * style.attack
                * perceptualScale

            score = score * (0.92 + math.random() * 0.16)
            call(obj, "Brain_AddDecisionCandidate", name, score)

            if score > bestScore then
                bestScore = score
                bestName = name
            end
        end
    end

    if bestName and bestScore > 0.05 and call(obj, "Brain_SetSelectedAttack", bestName) then
        call(obj, "Brain_CommitDecisionTrace", bestName)
        return true
    end

    call(obj, "Brain_CommitDecisionTrace", "None")
    return false
end

local function should_deflect(style, bias)
    bias = bias or 1.0
    if call(obj, "Brain_TargetThreatening") ~= true then return false end
    local distance = call(obj, "Brain_GetDistance") or 9999.0
    local range = call(obj, "Brain_GetAttackRange") or 3.0
    local angle = call(obj, "Brain_GetAbsAngle") or 180.0
    if distance > range * 1.8 or angle > 80.0 then return false end
    if not ((call(obj, "Brain_CanSeeTarget") == true and call(obj, "Brain_HasLineOfSight") == true) or call(obj, "Brain_IsInProximity") == true) then
        return false
    end
    return math.random() < style.react * style.deflect * bias
end

local function decide_movement(style)
    local distance = call(obj, "Brain_GetDistance") or 9999.0
    local range = call(obj, "Brain_GetAttackRange") or 3.0
    local hp = call(obj, "Brain_GetSelfHealthRatio") or 1.0

    if distance > range then
        call(obj, "Brain_Chase")
        return
    end

    local retreatScore = style.retreat * ((hp < 0.35) and 1.7 or 1.0) * ((distance < range * 0.55) and 1.35 or 0.7)
    local strafeScore = style.strafe * (0.85 + math.random() * 0.3)
    if retreatScore > strafeScore then
        call(obj, "Brain_Reposition")
    else
        call(obj, "Brain_Strafe")
    end
end

local function define_runtime_states()
    local sm = call(obj, "GetStateMachine")
    if not sm then return end
    call(sm, "ClearStateDefinitions")
    call(sm, "DefineState", "Idle", LOCOMOTION.InputAllowed, 0, true, false)
    call(sm, "DefineState", "Chase", LOCOMOTION.InputAllowed, 1, true, true)
    call(sm, "DefineState", "Strafe", LOCOMOTION.Strafe, 2, true, true)
    call(sm, "DefineState", "Reposition", LOCOMOTION.Retreat, 3, true, true)
    call(sm, "DefineState", "Attack", LOCOMOTION.Locked, 4, false, false)
    call(sm, "DefineState", "Deflect", LOCOMOTION.Locked, 5, true, true)
    call(sm, "DefineState", "Guard", LOCOMOTION.Locked, 5, true, true)
    call(sm, "DefineState", "Recover", LOCOMOTION.InputAllowed, 6, true, false)
    call(sm, "DefineState", "Hit", LOCOMOTION.Locked, 7, true, false)
    call(sm, "DefineState", "Staggered", LOCOMOTION.Locked, 8, true, false)
    call(sm, "DefineState", "Dead", LOCOMOTION.Locked, 9, false, false)
    call(sm, "DefineState", "Intro", LOCOMOTION.Locked, 10, true, true)
    call(sm, "DefineState", "PhaseChanged", LOCOMOTION.Locked, 11, true, true)
    call(sm, "DefineState", "EncounterCompleted", LOCOMOTION.InputAllowed, 12, true, false)
end

function BeginPlay()
    define_runtime_states()
    S = {
        isBoss = call(obj, "Brain_IsBoss") == true,
    }
    pcall(function() math.randomseed((tonumber(obj.UUID) or os.time() or 1) + 13) end)

    if not S.isBoss and Equipment and Equipment.AttachStaticMeshToSocket then
        spearComponent = Equipment.AttachStaticMeshToSocket(
            obj,
            SPEAR_PATH,
            SPEAR_SOCKET,
            Vec3(1.0, 1.0, 1.0)
        )
    end

end

function EndPlay()
    call(obj, "Brain_ReleaseAttackToken")
end

function OnDamaged(damageSpec, damageReport)
    if should_destroy_from_damage_report(damageReport) then
        request_destroy_from_death()
    end
end

function Tick(dt)
    if bDestroyedByDeath or not S then return end

    if bPendingDestroyFromDeath then
        deathDestroyDelayRemaining = deathDestroyDelayRemaining - dt
        if deathDestroyDelayRemaining <= 0.0 then
            destroy_self_from_death()
        end
        return
    end

    if S.isBoss and call(obj, "IsBossEncounterActive") ~= true then
        call(obj, "Brain_ReleaseAttackToken")
        call(obj, "StopEnemyMovement")
        return
    end

    update_ground_speed()

    if call(obj, "Brain_ConsumeCombatStep") ~= true then return end
    call(obj, "Brain_Sense")
    if call(obj, "Brain_IsBusy") == true then return end

    if call(obj, "Brain_AcquireTarget") ~= true then
        call(obj, "Brain_Idle")
        return
    end

    -- 은신 인지 게이팅(보고서 1군 #2): 전투 확정(Alert) 전에는 추격/공격하지 않는다.
    -- bUseAwarenessGating 이 꺼져 있으면 Brain_IsAlerted 가 항상 true → 기존 동작 유지.
    if call(obj, "Brain_IsAlerted") == false then
        local aw = call(obj, "Brain_GetAwarenessState") or 3
        if aw == 2 or aw == 4 then      -- Investigating / Searching
            call(obj, "Brain_Investigate")
        else
            call(obj, "Brain_Idle")
        end
        return
    end

    call(obj, "Brain_FaceTarget")

    local style = current_style()
    -- 공격 문법 분기(보고서 1군 #3): 직전 공격이 탄기당했으면 무리한 재돌입 대신 받아치기 성향↑.
    local deflectBias = (call(obj, "Brain_LastAttackWasDeflected") == true) and 2.2 or 1.0
    if should_deflect(style, deflectBias) then
        call(obj, "Brain_OpenDeflect")
        return
    end

    local distance = call(obj, "Brain_GetDistance") or 9999.0
    local range = call(obj, "Brain_GetAttackRange") or 3.0
    -- 동시 공격자 제한은 공격 토큰(Brain_AcquireAttackToken)이 전담한다. 역할(Brain_GetSquadRole)은
    -- 디버그/포지셔닝 참고용으로만 노출하고, 공격 개시를 막지 않는다(후열이 영영 공격 못 해
    -- strafe 만 반복하는 것을 방지).
    if distance <= range * 2.5 and style.attack > 0.0 then
        if call(obj, "Brain_AcquireAttackToken") == true then
            if select_attack() and call(obj, "Brain_PlaySelectedAttack") == true then
                return
            end
            call(obj, "Brain_ReleaseAttackToken")
        end
    end

    decide_movement(style)
end
