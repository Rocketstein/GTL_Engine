local DebugConfig = require("Game.Config.Debug")
local AudioConfig = require("Game.Config.Audio")
local PlayerStatusConfig = require("Game.Config.PlayerStatus")
local CoachConfig = require("Game.Config.Coach")
local CollectibleConfig = require("Game.Config.Collectible")
local ResultScreenConfig = require("Game.Config.ResultScreen")
local AudioManager = require("Game.AudioManager")
local Log = require("Common.Log")
local Math = require("Common.Math")

local ScoreSettings = {
    -- distance_weight: 주행 거리 1m당 점수 가중치입니다.
    distance_weight = 10.0,

    -- survival_time_weight: 생존 시간 1초당 점수 가중치입니다.
    survival_time_weight = 5.0,
}

local GameManager = {
    State = {
        Ready = "Ready",
        Running = "Running",
        Paused = "Paused",
        GameOver = "GameOver",
    },

    state = "Ready",
    score = 0,
    bonus_score = 0,
    distance = 0.0,
    elapsed_time = 0.0,
    score_log_timer = 0.0,
    logs = 0,
    trace = 0,
    dumps = 0,
    hotfix_count = 0,
    critical_analysis_count = 0,
    stability = PlayerStatusConfig.max_hp,
    max_stability = PlayerStatusConfig.max_hp,
    coach_approval = CoachConfig.initial_approval,
    coach_rank = "C",
    shield_count = 0,
    result_data = nil,
    dialogue_trigger_queue = {},
    stability_low_triggered = false,
}

local log = Log.MakeLogger(DebugConfig)

------------------------------------------------
-- 내부 유틸리티 함수들
------------------------------------------------

-- score_log_interval은 설정이 비어 있어도 안전한 기본 로그 간격을 돌려줍니다.
local function score_log_interval()
    return DebugConfig.score_log_interval or 1.0
end

local function calculate_coach_rank(approval)
    for index = 1, #CoachConfig.rank_thresholds do
        local threshold = CoachConfig.rank_thresholds[index]
        if approval >= threshold.approval then
            return threshold.rank
        end
    end

    return CoachConfig.fallback_rank or "F"
end

-- refresh_coach_rank는 coach_approval이 바뀐 뒤 랭크를 최신 상태로 맞춥니다.
local function refresh_coach_rank()
    GameManager.coach_rank = calculate_coach_rank(GameManager.coach_approval)
end

-- build_result_data는 GameOver 순간 결과창에서 읽을 데이터를 한 번에 모읍니다.
-- 실제 UI/씬 전환은 아직 붙이지 않고, 다른 작업자가 이 테이블을 읽어 결과창을 만들면 됩니다.
local function build_result_data(reason)
    return {
        reason = reason or "Unknown",
        score = GameManager.score,
        distance = GameManager.distance,
        elapsed_time = GameManager.elapsed_time,
        logs = GameManager.logs,
        trace = GameManager.trace,
        dumps = GameManager.dumps,
        hotfix_count = GameManager.hotfix_count,
        critical_analysis_count = GameManager.critical_analysis_count,
        stability = GameManager.stability,
        max_stability = GameManager.max_stability,
        coach_approval = GameManager.coach_approval,
        coach_rank = GameManager.coach_rank,
        shield_count = GameManager.shield_count,
    }
end

local function copy_table(source)
    local copied = {}
    if type(source) ~= "table" then
        return copied
    end

    for key, value in pairs(source) do
        copied[key] = value
    end

    return copied
end

local function build_sample_result_data()
    return copy_table(ResultScreenConfig.preview_result)
end

local function refresh_result_data(reason)
    local resolved_reason = reason
    if resolved_reason == nil then
        if GameManager.state == GameManager.State.GameOver then
            resolved_reason = "GameOver"
        elseif GameManager.state == GameManager.State.Running then
            resolved_reason = "Running"
        else
            resolved_reason = "Ready"
        end
    end

    GameManager.result_data = build_result_data(resolved_reason)
    return GameManager.result_data
end

local function queue_dialogue_trigger(trigger_name)
    if type(trigger_name) ~= "string" or trigger_name == "" then
        return false
    end

    GameManager.dialogue_trigger_queue[#GameManager.dialogue_trigger_queue + 1] = trigger_name
    log("[GameManager] QueueDialogue trigger=" .. tostring(trigger_name))
    return true
end

------------------------------------------------
-- 게임 생명주기 함수들
------------------------------------------------

function GameManager.StartGame()
    -- StartGame은 한 런의 모든 HUD용 수치를 초기화하는 지점입니다.
    -- 비주얼/HUD 담당자가 "게임 시작 시 초기값"을 확인해야 하면 여기 값을 보면 됩니다.
    -- 게임 시작 진입점은 StartGame 하나로 고정해 호출 흐름을 추적하기 쉽게 합니다.
    GameManager.state = GameManager.State.Running
    GameManager.score = 0
    GameManager.bonus_score = 0
    GameManager.distance = 0.0
    GameManager.elapsed_time = 0.0
    GameManager.score_log_timer = 0.0

    GameManager.logs = 0
    GameManager.trace = 0
    GameManager.dumps = 0
    GameManager.hotfix_count = 0
    GameManager.critical_analysis_count = 0
    GameManager.stability = PlayerStatusConfig.max_hp
    GameManager.max_stability = PlayerStatusConfig.max_hp
    GameManager.coach_approval = CoachConfig.initial_approval or 50
    refresh_coach_rank()
    GameManager.shield_count = 0
    GameManager.dialogue_trigger_queue = {}
    GameManager.stability_low_triggered = false
    refresh_result_data("Running")

    log("[GameManager] StartGame state=Running score=0 distance=0 elapsed_time=0")
    AudioManager.PlayBGM()
    queue_dialogue_trigger("onRunStart")
end

function GameManager.Tick(dt, moved_distance)
    -- Tick은 거리/시간 기반 점수를 갱신합니다.
    -- 아이템 보너스는 bonus_score로 유지해서 다음 프레임 재계산 때도 사라지지 않습니다.
    if GameManager.state ~= GameManager.State.Running then
        return
    end

    local safe_dt = dt or 0.0
    local safe_moved_distance = moved_distance or 0.0

    GameManager.distance = GameManager.distance + safe_moved_distance
    GameManager.elapsed_time = GameManager.elapsed_time + safe_dt
    GameManager.score = math.floor(
        GameManager.distance * ScoreSettings.distance_weight
        + GameManager.elapsed_time * ScoreSettings.survival_time_weight
    ) + GameManager.bonus_score

    GameManager.score_log_timer = GameManager.score_log_timer + safe_dt
    if GameManager.score_log_timer >= score_log_interval() then
        GameManager.score_log_timer = 0.0
        log(
            "[GameManager] ScoreLog score=" .. tostring(GameManager.score) ..
            " distance=" .. tostring(GameManager.distance) ..
            " elapsed_time=" .. tostring(GameManager.elapsed_time) ..
            " logs=" .. tostring(GameManager.logs) ..
            " trace=" .. tostring(GameManager.trace) ..
            " dumps=" .. tostring(GameManager.dumps) ..
            " stability=" .. tostring(GameManager.stability) ..
            "/" .. tostring(GameManager.max_stability) ..
            " approval=" .. tostring(GameManager.coach_approval) ..
            " rank=" .. tostring(GameManager.coach_rank)
        )
    end

    refresh_result_data("Running")
end

------------------------------------------------
-- 점수 / 안정도 / 인정도 갱신 함수들
------------------------------------------------

function GameManager.AddScore(amount, reason)
    -- 아이템/보너스 점수 공통 진입점입니다.
    -- score와 bonus_score를 같이 갱신해서 즉시 HUD/log에 보이고, 다음 Tick 재계산 후에도 유지되게 합니다.
    local safe_amount = amount or 0
    GameManager.bonus_score = GameManager.bonus_score + safe_amount
    GameManager.score = GameManager.score + safe_amount
    log(
        "[GameManager] AddScore amount=" .. tostring(safe_amount) ..
        " score=" .. tostring(GameManager.score) ..
        " reason=" .. tostring(reason or "Unknown")
    )
end

function GameManager.SetStabilitySnapshot(stability, max_stability)
    -- PlayerStatus 내부 저장값을 포드 안정도 HUD 스냅샷으로 반영합니다.
    -- PlayerStatus가 실제 값을 바꾼 직후 이 함수로 GameManager/HUD 수치를 동기화합니다.
    GameManager.max_stability = max_stability or GameManager.max_stability or PlayerStatusConfig.max_hp
    GameManager.stability = Math.Clamp(stability or GameManager.stability or GameManager.max_stability, 0, GameManager.max_stability)

    local low_threshold = math.max(1, math.ceil((GameManager.max_stability or 1) * 0.34))
    local is_low_stability = GameManager.stability > 0 and GameManager.stability <= low_threshold
    if is_low_stability and not GameManager.stability_low_triggered then
        GameManager.stability_low_triggered = true
        queue_dialogue_trigger("onStabilityLow")
    elseif not is_low_stability then
        GameManager.stability_low_triggered = false
    end
end

function GameManager.AddCoachApproval(amount, reason)
    -- 코치 인정도 공통 변경 함수입니다.
    -- HUD 작업자는 이 값이 바뀔 때 coach_approval/coach_rank만 다시 읽으면 됩니다.
    local old_approval = GameManager.coach_approval
    GameManager.coach_approval = Math.Clamp(GameManager.coach_approval + (amount or 0), 0, 100)
    refresh_coach_rank()
    log(
        "[GameManager] CoachApproval " .. tostring(old_approval) ..
        "->" .. tostring(GameManager.coach_approval) ..
        " rank=" .. tostring(GameManager.coach_rank) ..
        " reason=" .. tostring(reason or "Unknown")
    )
end

------------------------------------------------
-- 게임 이벤트 Hook 함수들
------------------------------------------------

function GameManager.OnLogCollected()
    -- Log Fragment를 먹었을 때 코치 인정도가 소폭 올라가는 이벤트입니다.
    GameManager.AddCoachApproval(CoachConfig.log_collected_delta or 1, "LogCollected")
    queue_dialogue_trigger("onCollectLog")
end

function GameManager.OnSpeedUp()
    queue_dialogue_trigger("onSpeedUp")
end

function GameManager.OnHotfixApplied()
    -- Hotfix 발동 성공 이벤트입니다. 여기서 화면에 "HOTFIX APPLIED" 띄우면 됨.
    GameManager.AddCoachApproval(CoachConfig.hotfix_delta or 8, "HotfixApplied")
end

------------------------------------------------
-- CrashDumpAnalyzed listener
------------------------------------------------

GameManager.crash_dump_analyzed_listeners =
    GameManager.crash_dump_analyzed_listeners or {}

function GameManager.AddCrashDumpAnalyzedListener(key, callback)
    if not key or type(callback) ~= "function" then
        return false
    end

    GameManager.crash_dump_analyzed_listeners[key] = callback
    return true
end

function GameManager.RemoveCrashDumpAnalyzedListener(key)
    if not key then
        return false
    end

    GameManager.crash_dump_analyzed_listeners[key] = nil
    return true
end

local function notify_crash_dump_analyzed_listeners()
    local listeners = GameManager.crash_dump_analyzed_listeners
    if not listeners then
        return
    end

    for key, callback in pairs(listeners) do
        if type(callback) == "function" then
            local alive = callback()

            -- callback이 false를 반환하면 죽은 listener로 보고 제거
            if alive == false then
                listeners[key] = nil
            end
        else
            listeners[key] = nil
        end
    end
end

function GameManager.OnCrashDumpAnalyzed()
    -- Critical Analysis 발동 성공 이벤트입니다.
    GameManager.AddCoachApproval(CoachConfig.critical_analysis_delta or 10, "CrashDumpAnalyzed")

    notify_crash_dump_analyzed_listeners()
end

function GameManager.OnCrashDumpCollected()
    queue_dialogue_trigger("onCollectCrashDump")
    -- Crash Dump를 한 개 주울 때마다 호출되는 훅입니다.
    -- FakeCrashEvent 등 외부 스크립트가 여기 hook을 덮어써서 매 픽업 연출을 붙입니다.
end

function GameManager.OnPlayerHit()
    -- 장애물 충돌 이벤트입니다. 여기서 화면 흔들림/위험 HUD 연출을 나중에 붙이면 됨.
    GameManager.AddCoachApproval(CoachConfig.player_hit_delta or -5, "PlayerHit")
    queue_dialogue_trigger("onHitObstacle")
end

------------------------------------------------
-- 수집 보상 처리 함수들
------------------------------------------------

function GameManager.ApplyHotfix()
    -- trace 100%가 된 순간 들어오는 Hotfix 발동 지점입니다.
    -- 여기서 화면에 HOTFIX APPLIED 띄우면 됨. 실제 visual effect는 지금 만들지 않습니다.
    GameManager.trace = 0
    GameManager.hotfix_count = GameManager.hotfix_count + 1
    GameManager.AddScore(CollectibleConfig.hotfix_score or 1000, "Hotfix")
    GameManager.OnHotfixApplied()
    queue_dialogue_trigger("onApplyHotfix")
    log("[GameManager] Hotfix applied count=" .. tostring(GameManager.hotfix_count))

    return {
        hotfix_applied = true,
        recover_stability = CollectibleConfig.hotfix_stability_recover or 15,
    }
end

function GameManager.ApplyCriticalAnalysis()
    -- Crash Dump 3개를 모은 순간 들어오는 Critical Analysis 발동 지점입니다.
    -- 여기서 화면에 CRITICAL ANALYSIS 같은 연출 붙이면 됨. 실제 visual effect는 지금 만들지 않습니다.
    GameManager.dumps = 0
    GameManager.critical_analysis_count = GameManager.critical_analysis_count + 1
    GameManager.shield_count = GameManager.shield_count + (CollectibleConfig.critical_analysis_shield_reward or 1)
    GameManager.AddScore(CollectibleConfig.critical_analysis_score or 3000, "CriticalAnalysis")
    GameManager.OnCrashDumpAnalyzed()
    queue_dialogue_trigger("onCriticalAnalysis")
    log(
        "[GameManager] CriticalAnalysis count=" .. tostring(GameManager.critical_analysis_count) ..
        " shield_count=" .. tostring(GameManager.shield_count)
    )

    return {
        critical_analysis_applied = true,
        shield_added = CollectibleConfig.critical_analysis_shield_reward or 1,
    }
end

function GameManager.CollectLogFragment()
    -- Log Fragment 획득 규칙입니다.
    -- HUD는 logs/score/trace를 읽으면 되고, 이 함수가 Hotfix까지 자동으로 이어줍니다.
    GameManager.logs = GameManager.logs + 1
    GameManager.trace = GameManager.trace + (CollectibleConfig.log_fragment_trace or 2)
    GameManager.AddScore(CollectibleConfig.log_fragment_score or 10, "LogFragment")
    GameManager.OnLogCollected()

    log(
        "[GameManager] LogFragment logs=" .. tostring(GameManager.logs) ..
        " trace=" .. tostring(GameManager.trace)
    )

    if GameManager.trace >= (CollectibleConfig.trace_max or 100) then
        queue_dialogue_trigger("onHotfixReady")
        return GameManager.ApplyHotfix()
    end

    return {
        hotfix_applied = false,
        recover_stability = 0,
    }
end

function GameManager.CollectCrashDump()
    -- Crash Dump 획득 규칙입니다.
    -- dumps가 3개가 되는 순간 Critical Analysis가 발동되고 shield_count가 올라갑니다.
    GameManager.dumps = GameManager.dumps + 1
    log("[GameManager] CrashDump dumps=" .. tostring(GameManager.dumps))

    -- 매 픽업마다 외부 hook을 호출한다. FakeCrashEvent는 여기에 연결되어
    -- 줍는 즉시 가짜 크래시 UI를 띄운다. ApplyCriticalAnalysis 분기는
    -- coroutine과 별개이므로 3개째에 함께 발동돼도 문제 없다.
    GameManager.OnCrashDumpCollected()

    if GameManager.dumps >= (CollectibleConfig.crash_dump_required or 3) then
        return GameManager.ApplyCriticalAnalysis()
    end

    return {
        critical_analysis_applied = false,
        shield_added = 0,
    }
end

function GameManager.ConsumeShield()
    -- shield_count가 있으면 충돌 1회를 안정도 피해 없이 막습니다.
    -- HUD 작업자는 GetShieldCount()/HasShield()만 읽으면 남은 방어막 표시가 가능합니다.
    if GameManager.shield_count <= 0 then
        return false
    end

    GameManager.shield_count = GameManager.shield_count - 1
    queue_dialogue_trigger("onShieldBlocked")
    log("[GameManager] Shield consumed remain=" .. tostring(GameManager.shield_count))
    return true
end

------------------------------------------------
-- 게임 상태 제어 함수들
------------------------------------------------

function GameManager.GameOver(reason)
    if GameManager.state == GameManager.State.GameOver then
        log("[GameManager] GameOver ignored: already GameOver")
        return
    end

    GameManager.state = GameManager.State.GameOver
    refresh_result_data(reason)
    queue_dialogue_trigger("onGameOver")

    log("[GameManager] GameOver reason=" .. tostring(reason or "Unknown"))
    AudioManager.PlayGameOver()

    local should_stop_bgm = AudioConfig.stop_bgm_on_game_over
    if should_stop_bgm == nil then
        should_stop_bgm = true
    end
    if should_stop_bgm then
        AudioManager.StopBGM()
    end

    log(
        "[GameManager] FinalScore score=" .. tostring(GameManager.score) ..
        " distance=" .. tostring(GameManager.distance) ..
        " elapsed_time=" .. tostring(GameManager.elapsed_time) ..
        " logs=" .. tostring(GameManager.logs) ..
        " trace=" .. tostring(GameManager.trace) ..
        " dumps=" .. tostring(GameManager.dumps) ..
        " stability=" .. tostring(GameManager.stability) ..
        "/" .. tostring(GameManager.max_stability) ..
        " approval=" .. tostring(GameManager.coach_approval) ..
        " rank=" .. tostring(GameManager.coach_rank)
    )

    local result_scene = ResultScreenConfig.scene_path or "gameresult.scene"
    log("[GameManager] LoadResultScene scene=" .. tostring(result_scene))
    load_scene(result_scene)
end
function GameManager.IsRunning()
    return GameManager.state == GameManager.State.Running
end

function GameManager.Pause()
    if GameManager.state == GameManager.State.Running then
        GameManager.state = GameManager.State.Paused
        log("[GameManager] Paused")
    end
end

function GameManager.Resume()
    if GameManager.state == GameManager.State.Paused then
        GameManager.state = GameManager.State.Running
        log("[GameManager] Resumed")
    end
end

function GameManager.IsPaused()
    return GameManager.state == GameManager.State.Paused
end


function GameManager.IsRunning()
    return GameManager.state == GameManager.State.Running
end

function GameManager.IsGameOver()
    return GameManager.state == GameManager.State.GameOver
end

------------------------------------------------
-- HUD / 결과 조회 함수들
------------------------------------------------

function GameManager.GetScore()
    -- HUD 작업자가 점수 표시할 때 이 함수만 읽으면 됨.
    return GameManager.score
end

function GameManager.GetDistance()
    -- HUD 작업자가 주행 거리 표시할 때 이 함수만 읽으면 됨.
    return GameManager.distance
end

function GameManager.GetElapsedTime()
    -- HUD 작업자가 경과 시간 표시할 때 이 함수만 읽으면 됨.
    return GameManager.elapsed_time
end

function GameManager.GetLogs()
    -- HUD 작업자가 Log Fragment 개수 표시할 때 이 함수만 읽으면 됨.
    return GameManager.logs
end

function GameManager.GetTrace()
    -- HUD 작업자가 Hotfix 게이지 표시할 때 이 함수만 읽으면 됨.
    return GameManager.trace
end

function GameManager.GetDumps()
    -- HUD 작업자가 Crash Dump 개수 표시할 때 이 함수만 읽으면 됨.
    return GameManager.dumps
end

function GameManager.GetHotfixCount()
    -- HUD/결과창 작업자가 Hotfix 발동 횟수 표시할 때 이 함수만 읽으면 됨.
    return GameManager.hotfix_count
end

function GameManager.GetCriticalAnalysisCount()
    -- HUD/결과창 작업자가 Critical Analysis 발동 횟수 표시할 때 이 함수만 읽으면 됨.
    return GameManager.critical_analysis_count
end

function GameManager.GetStability()
    -- HUD 작업자가 포드 안정도 현재값 표시할 때 이 함수만 읽으면 됨.
    return GameManager.stability
end

function GameManager.GetMaxStability()
    -- HUD 작업자가 포드 안정도 최대값 표시할 때 이 함수만 읽으면 됨.
    return GameManager.max_stability
end

function GameManager.GetStabilityPercent()
    -- HUD 작업자가 안정도 게이지 퍼센트 표시할 때 이 함수만 읽으면 됨.
    if GameManager.max_stability <= 0 then
        return 0
    end
    return GameManager.stability / GameManager.max_stability
end

function GameManager.GetCoachApproval()
    -- HUD 작업자가 코치 인정도 숫자 표시할 때 이 함수만 읽으면 됨.
    return GameManager.coach_approval
end

function GameManager.GetCoachRank()
    -- HUD 작업자가 S/A/B/C/D/F 랭크 표시할 때 이 함수만 읽으면 됨.
    return GameManager.coach_rank
end

function GameManager.GetShieldCount()
    -- HUD 작업자가 방어막 개수 표시할 때 이 함수만 읽으면 됨.
    return GameManager.shield_count
end

function GameManager.HasShield()
    -- HUD 작업자가 방어막 아이콘 on/off만 필요하면 이 함수만 읽으면 됨.
    return GameManager.shield_count > 0
end

function GameManager.GetResultData()
    -- 결과창/다른 씬이 GameOver 이후 최종 데이터를 읽는 함수입니다.
    -- UI는 만들지 않고, 데이터가 사라지지 않도록 여기만 준비해 둡니다.
    if GameManager.result_data then
        return GameManager.result_data
    end

    if GameManager.state == GameManager.State.Ready then
        return build_sample_result_data()
    end

    return refresh_result_data()
end

function GameManager.GetSampleResultData()
    return build_sample_result_data()
end

------------------------------------------------
-- 씬 전환 / 대화 큐 함수들
------------------------------------------------

function GameManager.ChangeLevel(level_name)
    -- TODO: 실제 결과창/씬 전환 흐름이 정해지면 여기서 안전하게 연결하면 됨.
    log("[GameManager] TODO ChangeLevel: " .. tostring(level_name))
end

function GameManager.QueueDialogueTrigger(trigger_name)
    return queue_dialogue_trigger(trigger_name)
end

function GameManager.ConsumeDialogueTrigger()
    if #GameManager.dialogue_trigger_queue <= 0 then
        return nil
    end

    local trigger_name = table.remove(GameManager.dialogue_trigger_queue, 1)
    if type(trigger_name) ~= "string" or trigger_name == "" then
        return nil
    end

    return trigger_name
end

return GameManager
