-- PlayerController.lua
-- Runner Pawn의 입력, 자동 전진, 레인 이동, 중력, 바닥 판정, Stability/GameOver 연결을 담당한다.
-- C++ Runner는 최소한의 Actor/Component 구성만 만들고, 실제 플레이 수치와 상태 전이는 Lua에서 조정한다.
-- Map과 Player 충돌 문제를 추적하기 쉽도록 Ground Query 관련 값과 상태 변화는 이벤트 단위로 로그를 남긴다.

local DebugConfig = require("Game.Config.Debug")
local PlayerConfig = require("Game.Config.PlayerController")
local GameManager = require("Game.GameManager")
local PlayerStatus = require("Game.PlayerStatus")
local AudioManager = require("Game.AudioManager")
local PlayerSlide = require("Game.PlayerSlide")
local ScenarioLoader = require("UI.ScenarioLoader")
local DialogueUtils = require("UI.DialogueUtils")
local Log = require("Common.Log")
local Engine = require("Common.Engine")
local Math = require("Common.Math")
local UI = require("Common.UI")
local HitEffects = require_env("Game.HitEffect")
local Vector = require("Common.Vector")
local HitFeedback = require("Game.Camera.HitFeedback")
local letterBox = require("Game.Camera.LetterBox")
local CameraFade = require("Game.Camera.CameraFade")
local CameraTransition = require("Game.Camera.CameraTransition")

if HitEffects and HitEffects.SetRuntime then
    HitEffects.SetRuntime({
        set_global_time_dilation = set_global_time_dilation,
        wait_real = wait_real,
        raw_delta_time = raw_delta_time,
    })
end

local is_input_locked = false;

-- 무한 overlap 방지용 weakptr 테이블
local hit_obstacles = setmetatable({}, { __mode = "k" })

-- =========================================================
-- 런타임 상태
-- =========================================================

-- PlayerController 설정은 초기 튜닝값이다.
-- require()로 캐시되는 공유 table이므로 런타임 상태를 저장하지 않는다.
-- 런타임 중 바뀌는 값은 PlayerController.lua의 local state에 둔다.
local forward_speed = PlayerConfig.forward_speed                            -- Runner가 매 프레임 자동으로 앞으로 가는 속도
local dream_billboard_offset_x = PlayerConfig.dream_billboard_offset_x or 1000.0 -- Dream billboard의 Player 앞 X+ 초기 거리
local lane_width = PlayerConfig.lane_width                                  -- 레인 사이 간격
local lane_change_speed = PlayerConfig.lane_change_speed                    -- 목표 레인으로 부드럽게 이동하는 속도
local gravity = PlayerConfig.gravity                                        -- 공중에 있을 때 적용되는 수직 가속도
local jump_power = PlayerConfig.jump_power                                  -- 점프 시작 순간 위로 주는 속도입니다.
local max_move_step = PlayerConfig.max_move_step or 0.25                    -- 빠른 전진 중 overlap 누락을 줄이기 위해 이동을 쪼개는 최대 거리
local knockback_enabled = PlayerConfig.knockback_enabled ~= false            -- 장애물 충돌 시 뒤로 밀어낼지 여부
local knockback_distance = PlayerConfig.knockback_distance or 1.5            -- 충돌 시 전진 반대 방향으로 되돌리는 거리

local current_lane = 0                                                      -- 현재 도착한 레인 번호
local target_lane = 0                                                       -- 입력으로 지정된 목표 레인 번호

local vertical_velocity = 0.0                                               -- 점프/낙하에 쓰는 현재 Z 속도
local is_grounded = false                                                   -- 이번 프레임 기준 바닥에 붙어 있는지를 나타냄
local current_ground_z = nil                                                -- 마지막으로 찾은 바닥 상단 Z
local last_ground_hit = false                                               -- 바닥 발견/상실 로그를 한 번만 찍기 위한 이전 상태

local ground_probe_distance = PlayerConfig.ground_probe_distance            -- 발 아래로 바닥을 얼마나 멀리까지 찾을지
local ground_snap_distance = PlayerConfig.ground_snap_distance              -- 거의 닿았을 때만 바닥에 붙이는 허용 거리
local skin_width = PlayerConfig.skin_width                                  -- 바닥/충돌 판정 여유값
local fallback_half_height = PlayerConfig.fallback_half_height              -- collision shape를 못 찾았을 때 쓰는 반높이
local fall_dead_z = PlayerConfig.fall_dead_z                                -- 이 높이 아래로 떨어지면 낙사 처리하는 기준
local DIALOGUE_DATA_PATH = PlayerConfig.dialogue_data_path                  -- Player dialogue 경로

local camera = nil
local dream_billboard = nil
local dream_billboard_location = nil
local slide = nil                                                           --  PlayerSlide 모듈 인스턴스입니다. collision/mesh 변경은 여기서 위임
local pod_mesh = nil
local pod_base_local_rotation = nil

-- Barrel Roll 관련 파라미터
local barrel_roll_active = false
local barrel_roll_elapsed = 0.0
local barrel_roll_direction = 0.0
local barrel_roll_duration = PlayerConfig.barrel_roll_duration or 0.34
local barrel_roll_degrees = PlayerConfig.barrel_roll_degrees or 360.0

local game_over_tick_logged = false                                         -- GameOver 후 Tick 중단 로그를 한 번만 찍기 위한 플래그입니다.
local half_height_fallback_logged = false                                   -- collision half-height fallback 로그를 한 번만 찍기 위한 플래그입니다.
local previous_key_state = {}                                               -- A/D/Space가 꾹 눌린 동안 매 프레임 반복 발동하지 않게 이전 프레임 상태를 저장
local current_key_state = {}                                                -- 이번 프레임에 확인한 키 상태를 임시로 모아두는 테이블입니다.

local coach_dialogue_actor = nil
local coach_dialogue_speaker = nil
local coach_dialogue_message = nil
local coach_dialogue_window = nil
local coach_dialogue_data = nil
local coach_dialogue_entries_by_trigger = {}
local coach_last_dialogue_id = nil

local COACH_WINDOW_TEXTURES = PlayerConfig.coach_window_textures

------------------------------------------------
-- Player 유틸리티 함수
------------------------------------------------

local log = Log.MakeLogger(DebugConfig, "[Player]")

local function cache_dream_billboard()
    dream_billboard = Engine.GetComponent(obj, "DreamBillboard")
    if not Engine.IsValidComponent(dream_billboard) then
        log("[PlayerController] DreamBillboard component not found")
        return false
    end

    local player_loc = obj:GetWorldLocation()
    dream_billboard_location = Vector.Make(
        player_loc.x + dream_billboard_offset_x,
        player_loc.y,
        player_loc.z - 25.0
    )
    dream_billboard:SetWorldLocation(dream_billboard_location)

    log("[PlayerController] DreamBillboard initialized offset_x=" .. tostring(dream_billboard_offset_x))
    return true
end

local function advance_dream_billboard(delta_x)
    if not Engine.IsValidComponent(dream_billboard) then
        return
    end

    if not dream_billboard_location then
        local current_location = dream_billboard:GetWorldLocation()
        if not current_location then
            return
        end
        dream_billboard_location = Vector.Copy(current_location)
    end

    dream_billboard_location.x = dream_billboard_location.x + delta_x
    dream_billboard:SetWorldLocation(dream_billboard_location)
end

-- 가장 왼쪽 Lane 번호
local function lane_min()
    return -math.floor(PlayerConfig.lane_count / 2)
end

-- 가장 오른쪽 레인 번호
local function lane_max()
    return math.floor(PlayerConfig.lane_count / 2)
end

-- 지금 키를 누르고 있는가 확인
local function key_held(name)    
    if GetKey then
        return GetKey(name)
    end
    return false
end

-- InputSystem의 GetKeyDown이 환경에 따라 held처럼 들어와도 안전하게 막기 위해 Lua에서 edge trigger를 직접 생성
-- A/D 레인 이동과 Space 점프는 "방금 누른 순간"에만 처리
local function key_pressed_once(name)
    local is_down = key_held(name)
    local was_down = previous_key_state[name] == true
    current_key_state[name] = is_down
    return is_down and not was_down
end

local function finish_input_frame()
    -- 이번 프레임에 확인한 키 상태를 다음 프레임 previous로 넘깁니다.
    -- 확인하지 않은 키는 false로 내려서 다음 입력이 다시 edge로 잡히게 합니다.
    for key, _ in pairs(previous_key_state) do
        if current_key_state[key] == nil then
            current_key_state[key] = false
        end
    end

    previous_key_state = current_key_state
    current_key_state = {}
end

local function cache_coach_dialogue_components()
    coach_dialogue_actor = find_actor("AUIRootActor_PlayerDevHUD")
    if not Engine.IsValidActor(coach_dialogue_actor) then
        log("[PlayerController] Coach dialogue actor not found")
        return false
    end

    coach_dialogue_speaker = coach_dialogue_actor:GetComponent("DialogueSpeakerName")
    coach_dialogue_message = coach_dialogue_actor:GetComponent("DialogueMessage")
    coach_dialogue_window = coach_dialogue_actor:GetComponent("DialogueWindowPortrait")
    log("[PlayerController] Coach dialogue components cached")
    return true
end

local function load_coach_dialogue_entries()
    coach_dialogue_entries_by_trigger = {}
    coach_dialogue_data = ScenarioLoader.load(DIALOGUE_DATA_PATH, load_json_file)

    if type(coach_dialogue_data) ~= "table" or type(coach_dialogue_data.dialogues) ~= "table" then
        log("[PlayerController] Coach dialogue data load failed path=" .. tostring(DIALOGUE_DATA_PATH))
        return
    end

    for index = 1, #coach_dialogue_data.dialogues do
        local entry = coach_dialogue_data.dialogues[index]
        if type(entry) == "table"
            and type(entry.trigger) == "string" and entry.trigger ~= ""
            and type(entry.message) == "string" and entry.message ~= "" then
            if type(coach_dialogue_entries_by_trigger[entry.trigger]) ~= "table" then
                coach_dialogue_entries_by_trigger[entry.trigger] = {}
            end
            coach_dialogue_entries_by_trigger[entry.trigger][#coach_dialogue_entries_by_trigger[entry.trigger] + 1] = entry
        end
    end

    log("[PlayerController] Coach dialogue entries loaded")
end

local function pick_coach_dialogue_entry(trigger_name)
    local entries = coach_dialogue_entries_by_trigger[trigger_name]
    if type(entries) ~= "table" or #entries <= 0 then
        return nil
    end

    if #entries == 1 then
        return entries[1]
    end

    local selected = nil
    local guard = 0
    repeat
        selected = entries[math.random(1, #entries)]
        guard = guard + 1
    until not selected or selected.id ~= coach_last_dialogue_id or guard >= 8

    return selected
end

local function apply_coach_dialogue(trigger_name)
    if type(trigger_name) ~= "string" or trigger_name == "" then
        return false
    end

    if not Engine.IsValidActor(coach_dialogue_actor) then
        if not cache_coach_dialogue_components() then
            return false
        end
    end

    local entry = pick_coach_dialogue_entry(trigger_name)
    if not entry then
        log("[PlayerController] No coach dialogue entry for trigger=" .. tostring(trigger_name))
        return false
    end

    coach_last_dialogue_id = entry.id
    UI.SetText(coach_dialogue_speaker, ScenarioLoader.resolve_speaker_name(coach_dialogue_data, entry.speaker) or "")
    UI.SetText(
        coach_dialogue_message,
        DialogueUtils.format_terminal_message(tostring(entry.message or ""), {
            max_line_width_units = 27,
            continuation_indent = "  ",
        }) or ""
    )
    UI.SetTexture(coach_dialogue_window, COACH_WINDOW_TEXTURES[entry.speaker] or COACH_WINDOW_TEXTURES.BAEK_COMMANDER)
    log("[PlayerController] Coach dialogue updated trigger=" .. tostring(trigger_name) .. " id=" .. tostring(entry.id))
    return true
end

local function consume_coach_dialogue_triggers()
    if not GameManager.ConsumeDialogueTrigger then
        return
    end

    while true do
        local trigger_name = GameManager.ConsumeDialogueTrigger()
        if not trigger_name then
            break
        end

        apply_coach_dialogue(trigger_name)
    end
end

------------------------------------------------
-- 바닥 판정 헬퍼 함수들
------------------------------------------------

local function get_player_half_height()
    -- Player의 발 위치는 "Actor 중심 Z - collision half-height"로 계산된다.
    -- FindGround()도 이 half-height를 기준으로 현재 발 아래에 있는 floor AABB를 찾는다.
    -- 따라서 collision shape를 못 찾으면 fallback 값을 써야 하지만,
    -- fallback이 너무 작거나 크면 바닥이 위/아래로 잘못 판정될 수 있으므로 최초 1회 로그를 남긴다.
    local collision_shape = slide and slide:GetCollisionShape() or nil
    if collision_shape and collision_shape.GetShapeHalfHeight then
        local half_height = collision_shape:GetShapeHalfHeight()
        if half_height then
            return half_height
        end
    end

    if not half_height_fallback_logged then
        log("[PlayerController] Box half height fallback=" .. tostring(fallback_half_height))
        half_height_fallback_logged = true
    end
    return fallback_half_height
end

-- 현재 발 아래 floor를 찾고 착지/snap 여부를 갱신합니다.
-- 위치 보정, 수직 속도 초기화, ground lost/found 로그를 한곳에서 처리합니다.
local function update_ground_state(dt, allow_snap)
    -- Map floor는 StaticMeshComponent의 world AABB로 판정된다.
    -- ground_probe_distance는 "발 아래로 얼마나 멀리까지 바닥을 찾을지"이고,
    -- skin_width는 X/Y 수평 겹침 판정에 약간의 여유를 주는 값이다.
    -- C++ 쪽 floor collision이 꺼져 있거나 bounds가 갱신되지 않으면 여기서 ground.hit이 false가 된다.
    local loc = obj:GetWorldLocation()
    local half_height = get_player_half_height()
    local ground = obj:FindGround(ground_probe_distance, skin_width)

    if ground and ground.hit then
        if not last_ground_hit then
            log(
                "[PlayerController] Ground found ground_z=" .. tostring(ground.ground_z) ..
                " distance=" .. tostring(ground.distance)
            )
        end
        last_ground_hit = true
        current_ground_z = ground.ground_z

        local desired_center_z = current_ground_z + half_height
        local distance_to_ground = loc.z - desired_center_z

        -- snap은 떨어지는 중이거나 정지 상태일 때만 수행한다.
        -- 점프 상승 중에 위쪽 floor나 obstacle 상단으로 강제로 붙으면 조작감이 깨지므로
        -- vertical_velocity <= 0 조건을 유지한다.
        -- Ground snap은 바닥보다 아주 살짝 아래로 파고든 정도까지만 허용합니다.
        -- distance_to_ground가 너무 큰 음수면 플레이어가 지면 아래/이상한 위치에 있는 것이므로 잘못 끌어올리지 않습니다.
        if allow_snap and vertical_velocity <= 0.0
            and distance_to_ground >= -skin_width
            and distance_to_ground <= ground_snap_distance then
            loc.z = desired_center_z
            obj:SetWorldLocation(loc)
            vertical_velocity = 0.0
            is_grounded = true
            return
        end
    else
        if last_ground_hit then
            log("[PlayerController] Ground lost")
        end
        last_ground_hit = false
        current_ground_z = nil
    end

    is_grounded = false
end

------------------------------------------------
-- 레인 이동 / 점프 입력 함수들
------------------------------------------------

local function apply_pod_roll_angle(angle)
    if not Engine.IsValidComponent(pod_mesh) or not pod_base_local_rotation then
        return
    end

    pod_mesh:SetLocalRotation(rotator(
        pod_base_local_rotation.pitch or 0.0,
        pod_base_local_rotation.yaw or 0.0,
        (pod_base_local_rotation.roll or 0.0) + angle
    ))
end

local function reset_pod_barrel_roll()
    barrel_roll_active = false
    barrel_roll_elapsed = 0.0
    barrel_roll_direction = 0.0
    apply_pod_roll_angle(0.0)
end

local function start_pod_barrel_roll(lane_delta)
    if PlayerConfig.barrel_roll_enabled == false or barrel_roll_active then
        return
    end
    if not Engine.IsValidComponent(pod_mesh) or not pod_base_local_rotation then
        return
    end

    barrel_roll_direction = lane_delta < 0 and 1.0 or -1.0
    barrel_roll_elapsed = 0.0
    barrel_roll_active = true
end

local function update_pod_barrel_roll(dt)
    if not barrel_roll_active then
        return
    end

    local duration = barrel_roll_duration
    if duration == nil or duration <= 0.0 then
        duration = 0.34
    end

    barrel_roll_elapsed = barrel_roll_elapsed + dt
    local alpha = Math.Clamp(barrel_roll_elapsed / duration, 0.0, 1.0)
    local eased = alpha * alpha * (3.0 - (2.0 * alpha))
    apply_pod_roll_angle(barrel_roll_direction * barrel_roll_degrees * eased)

    if alpha >= 1.0 then
        reset_pod_barrel_roll()
    end
end

local function move_lane(delta)
    if PlayerStatus.IsDead() or GameManager.IsGameOver() then
        return
    end

    local previous_lane = target_lane
    target_lane = Math.Clamp(target_lane + delta, lane_min(), lane_max())

    if previous_lane ~= target_lane then
        log(
            "[PlayerController] Lane change input delta=" .. tostring(delta) ..
            " target_lane=" .. tostring(target_lane) ..
            " target_y=" .. tostring(target_lane * lane_width)
        )
        start_pod_barrel_roll(delta)
    end
end

local function try_jump()
    if PlayerStatus.IsDead() or GameManager.IsGameOver() or not is_grounded then
        return
    end

    vertical_velocity = jump_power
    is_grounded = false
    AudioManager.PlayJump()
    log("[PlayerController] Jump velocity=" .. tostring(jump_power))
end

------------------------------------------------
-- 중력 / 낙하 사망 함수들
------------------------------------------------

local function check_fall_death()
    if PlayerStatus.IsDead() or GameManager.IsGameOver() then
        return true
    end

    local loc = obj:GetWorldLocation()
    if loc.z < fall_dead_z then
        log(
            "[PlayerController] Fall death z=" .. tostring(loc.z) ..
            " threshold=" .. tostring(fall_dead_z)
        )
        PlayerStatus.Kill("Fall")
        return true
    end

    return false
end

local function apply_gravity(dt)
    -- 이동 전 먼저 현재 발 아래 바닥을 검사한다.
    -- 이미 floor 위에 있으면 vertical_velocity를 0으로 만들고, 불필요한 낙하 누적을 막는다.
    update_ground_state(dt, true)

    if not is_grounded then
        vertical_velocity = vertical_velocity + gravity * dt

        local loc = obj:GetWorldLocation()
        loc.z = loc.z + vertical_velocity * dt
        obj:SetWorldLocation(loc)

        -- 중력으로 내려온 뒤 다시 검사한다.
        -- 이 두 번째 검사가 있어야 한 프레임 안에서 floor를 통과하지 않고 바로 착지/snap할 수 있다.
        update_ground_state(dt, true)
    end
end

------------------------------------------------
-- 슬라이드 입력 함수들
------------------------------------------------

local function slide_key_pressed()
    -- 슬라이드는 duration 기반 타이머가 아니라 누르고 있는 동안 유지하는 방식입니다.
    -- S/아래/CTRL 중 하나를 잡고 있으면 slide 유지, 모두 떼면 종료합니다.
    return key_held("S") or key_held("DOWN") or key_held("Down") or key_held("CTRL") or key_held("Control")
end

local function begin_slide()
    if PlayerStatus.IsDead() or GameManager.IsGameOver() then
        return
    end

    if not is_grounded then
        -- 공중에서 slide Begin을 걸면 점프/낙하 상태와 충돌할 수 있으므로 지상에서만 시작합니다.
        return
    end

    if slide and slide:IsSliding() then
        -- 슬라이드 중 Begin이 반복 호출되면 timer/shape가 계속 초기화되던 문제를 막습니다.
        return
    end

    if slide then
        slide:Begin()
        AudioManager.PlaySlide()
        log("[PlayerController] Slide start hold")
    end
end

local function update_slide(dt, wants_slide)
    if not slide or not slide:IsSliding() then
        return
    end

    -- 슬라이드는 고정 타이머가 아니라 현재 입력 상태와 지상 여부로 끝냅니다.
    -- 키를 떼거나 공중으로 뜨면 안전하게 slide를 끝냅니다.
    if not wants_slide or not is_grounded then
        slide:End()
        log("[PlayerController] Slide finished by key release or air")
    end
end

------------------------------------------------
-- 장애물 충돌 콜백 헬퍼 함수들
------------------------------------------------

local function has_obstacle_tag(actor)
    if not Engine.IsValidActor(actor) then
        return false
    end

    if actor:HasTag("Obstacle") then
        return true
    end

    if actor.Name and string.find(tostring(actor.Name), "Obstacle") then
        actor.Tag = "Obstacle"
        log("[PlayerController] Obstacle tag fallback applied actor=" .. tostring(actor.Name))
        return true
    end

    return false
end

local function apply_knockback()
    if not knockback_enabled then
        return false
    end

    local distance = Math.SafeNonNegative(knockback_distance, 0.0)
    if distance <= 0.0 then
        return false
    end

    local loc = obj:GetWorldLocation()
    if not loc then
        return false
    end

    -- Runner는 Tick에서 +X로 자동 전진하므로, knockback은 레인/Y축을 흔들지 않고 -X로만 적용한다.
    loc.x = loc.x - distance
    obj:SetWorldLocation(loc)
    update_ground_state(0.0, true)

    log("[PlayerController] Knockback distance=" .. tostring(distance) .. " x=" .. tostring(loc.x))
    return true
end

local function handle_obstacle_collision(event_name, other_actor)
    -- 플레이어가 사망했거나 게임이 종료된 상태라면 스킵
    if PlayerStatus.IsDead() or GameManager.IsGameOver() then
        return
    end

    -- actor가 유효하지 않다면 바로 스킵
    if not Engine.IsValidActor(other_actor) then
        return
    end

    log("[PlayerController] " .. event_name .. " actor=" .. tostring(other_actor.Name))

    if not has_obstacle_tag(other_actor) then
        return
    end

    local damage = 1 -- 데미지 하드코딩
    -- 장애물별 피해량을 다르게 줄 수 있게 C++ Damage 값을 Lua에서 읽는다.
    if other_actor and other_actor.GetDamage then
        local obstacle_damage = other_actor:GetDamage()
        if obstacle_damage and obstacle_damage > 0 then
            damage = obstacle_damage
            log("Start Coroutine")
            StartCoroutine(function() HitEffects.HitStopAndSlomo(0.1, 0.7, 1.0) end)
            -- StartCoroutine(function() HitEffects.HitStop(0.1) end)
            -- StartCoroutine(function() HitEffects.Slomo(0.1, 1.0) end)
            if HitEffects and HitEffects.PlayHitSquash then
                StartCoroutine(function() HitEffects.PlayHitSquash(obj, 0.8, 0.8, 0.8, 0.1, 1.0) end)
            end
        end
    end
    log("[PlayerController] Obstacle collision damage=" .. tostring(damage))
    if PlayerStatus.DamageStability(damage) then
        apply_knockback()
        HitFeedback.Play(obj)
    end
end

------------------------------------------------
-- PlayerController 생명주기 함수들
------------------------------------------------

function TestCameraFadeIn()
    return CameraFade.FadeIn(obj, 0.8, 0.0, 0.0, 0.0, 1.0)
end

function TestCameraFadeOut()
    return CameraFade.FadeOut(obj, 0.8, 0.0, 0.0, 0.0, 1.0)
end

function TestCameraTransitionTo(target)
    return CameraTransition.To(obj, target, 1.0, "EaseInOut", 2.0, true)
end

-- BeginPlay example:
-- CameraFade.FadeIn(obj, 0.8, 0.0, 0.0, 0.0, 1.0)

-- Runner C++ 생성자에서 시작 위치를 바닥보다 약간 높은 Z로 둔다.
-- MapManager가 Player BeginPlay 이후에 Chunk를 만들 수 있으므로,
-- 첫 ground query가 실패하더라도 다음 Tick에서 생성된 floor를 찾고 중력으로 snap되게 한다.
-- PlayerController가 한 런에 필요한 이동/충돌/슬라이드/오디오 상태를 초기화합니다.
-- Map 생성 순서가 늦을 수 있어 첫 바닥 판정은 실패해도 Tick에서 다시 복구되게 둡니다.
function BeginPlay()
    -- 장애물 초기화
    hit_obstacles = setmetatable({}, { __mode = "k" })
    local loc = obj:GetWorldLocation()

    current_lane = Math.Clamp(Math.Round(loc.y / lane_width), lane_min(), lane_max())
    target_lane = current_lane

    vertical_velocity = 0.0
    is_grounded = false
    current_ground_z = nil
    last_ground_hit = false
    game_over_tick_logged = false
    half_height_fallback_logged = false
    previous_key_state = {}
    current_key_state = {}
    camera = obj:FindComponentByClass("CameraComponent")
    dream_billboard = nil
    dream_billboard_location = nil
    pod_mesh = obj:GetStaticMeshComponent()
    pod_base_local_rotation = nil
    if Engine.IsValidComponent(pod_mesh) then
        pod_base_local_rotation = pod_mesh:GetLocalRotation()
    end
    barrel_roll_active = false
    barrel_roll_elapsed = 0.0
    barrel_roll_direction = 0.0

    obj.Tag = "Player"
    cache_dream_billboard()

    slide = PlayerSlide.new(obj)
    if slide and slide.Configure then
        local slide_ok, slide_error = pcall(function()
            slide:Configure()
        end)
        if not slide_ok then
            warn("[PlayerController] PlayerSlide.Configure failed:", tostring(slide_error))
        end
    end

    log("[PlayerController] BeginPlay")
    log(
        "[PlayerController] Config forward_speed=" .. tostring(forward_speed) ..
        " max_move_step=" .. tostring(max_move_step) ..
        " lane_width=" .. tostring(lane_width) ..
        " lane_count=" .. tostring(PlayerConfig.lane_count) ..
        " gravity=" .. tostring(gravity) ..
        " jump_power=" .. tostring(jump_power) ..
        " knockback_enabled=" .. tostring(knockback_enabled) ..
        " knockback_distance=" .. tostring(knockback_distance) ..
        " slide_mode=hold" ..
        " fall_dead_z=" .. tostring(fall_dead_z)
    )
    log(
        "[PlayerController] Ground config probe=" .. tostring(ground_probe_distance) ..
        " snap=" .. tostring(ground_snap_distance) ..
        " skin=" .. tostring(skin_width) ..
        " fallback_half_height=" .. tostring(fallback_half_height)
    )

    local audio_ok = AudioManager.Initialize(obj)
    log("[PlayerController] AudioManager.Initialize result=" .. tostring(audio_ok))

    math.randomseed(math.floor(time() * 1000) % 2147483647)
    -- PlayerDevHUD owns runtime coach dialogue presentation.
    -- A second consumer here can desync speaker names from portrait frames.

    GameManager.StartGame()
    log("[PlayerController] GameManager.StartGame")

    PlayerStatus.ResetForStart()
    log("[PlayerController] PlayerStatus.ResetForStart")

    update_ground_state(0.0, true)

    letterBox.End(obj)
end

-- 매 프레임 입력, 레인 이동, 점프/슬라이드, 중력, 점수 갱신을 순서대로 처리합니다.
-- 전진 이동은 작은 step으로 쪼개 overlap 누락과 바닥 통과를 줄입니다.
function Tick(dt)
    if GameManager.IsPaused and GameManager.IsPaused() then
        return
    end
    -- GameOver 이후에는 이동, 입력, 중력, score 갱신을 모두 멈춘다.
    -- 로그는 최초 1회만 남겨 Tick마다 콘솔이 도배되지 않게 한다.
    if GameManager.IsGameOver() then
        if not game_over_tick_logged then
            log("[PlayerController] Tick stopped: GameOver")
            game_over_tick_logged = true
            if slide then
                slide:Restore()
            end
            reset_pod_barrel_roll()
        end
        return
    end

    PlayerStatus.Tick(dt)
    if PlayerStatus.IsDead() then
        return
    end

    if check_fall_death() then
        return
    end

    local a_pressed = key_pressed_once("A")
    local d_pressed = key_pressed_once("D")
    local move_left_pressed = a_pressed or key_pressed_once("LEFT") or key_pressed_once("Left")
    local move_right_pressed = key_pressed_once("D") or key_pressed_once("RIGHT") or key_pressed_once("Right")
    local jump_pressed = key_pressed_once("SPACE") or key_pressed_once("Space")
    local barrel_rolling_sound_pressed = a_pressed or d_pressed
    local wants_slide = slide_key_pressed()
    finish_input_frame()

    if barrel_rolling_sound_pressed then
        AudioManager.PlayBarrelRolling()
    end

    if move_left_pressed then
        move_lane(-1)
    elseif move_right_pressed then
        move_lane(1)
    end

    if jump_pressed then
        try_jump()
    end

    if wants_slide then
        begin_slide()
    end
    update_slide(dt, wants_slide)
    update_pod_barrel_roll(dt)

    -- score는 "실제로 전진시킨 거리"를 GameManager에 넘겨서 계산한다.
    -- 고속 이동 시 한 프레임 이동 거리를 잘게 나눠 teleport 방식 overlap 누락을 줄인다.
    local total_distance = forward_speed * dt
    local safe_max_step = max_move_step
    if safe_max_step == nil or safe_max_step <= 0.0 then
        safe_max_step = 0.25
    end

    local steps = math.max(1, math.ceil(math.abs(total_distance) / safe_max_step))
    local step_dt = dt / steps
    local step_distance = total_distance / steps
    local actual_moved_distance = 0.0

    for _ = 1, steps do
        if PlayerStatus.IsDead() or GameManager.IsGameOver() then
            break
        end

        local loc = obj:GetWorldLocation()
        loc.x = loc.x + step_distance
        actual_moved_distance = actual_moved_distance + step_distance

        local target_y = target_lane * lane_width
        local diff_y = target_y - loc.y
        if math.abs(diff_y) > 0.1 then
            local step = lane_change_speed * step_dt
            if math.abs(diff_y) <= step then
                loc.y = target_y
                current_lane = target_lane
                log("[PlayerController] Lane reached lane=" .. tostring(current_lane) .. " y=" .. tostring(loc.y))
            else
                loc.y = loc.y + Math.Sign(diff_y) * step
            end
        else
            loc.y = target_y
            current_lane = target_lane
        end

        obj:SetWorldLocation(loc)
        advance_dream_billboard(step_distance)
        apply_gravity(step_dt)

        if check_fall_death() then
            break
        end
    end

    GameManager.Tick(dt, actual_moved_distance)
end

function EndPlay()

    letterBox.End(obj)

    if HitEffects and HitEffects.StopTimeEffects then
        HitEffects.StopTimeEffects()
    end
    if slide then
        slide:Restore()
    end
    reset_pod_barrel_roll()
    dream_billboard = nil
    dream_billboard_location = nil
    log("[PlayerController] EndPlay")
end

------------------------------------------------
-- PlayerController 충돌 콜백 함수
------------------------------------------------

function OnBeginOverlap(otherActor, otherComp, selfComp)
    if not otherActor then
        return
    end

    if hit_obstacles[otherActor] then
        return
    end

    if PlayerStatus:IsInvincible() then
        return
    end
    hit_obstacles[otherActor] = true

    handle_obstacle_collision("BeginOverlap", otherActor)
end

function OnHit(otherActor, otherComp, selfComp, impactLocation, impactNormal)
    handle_obstacle_collision("Hit", otherActor)
end
