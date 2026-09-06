local CollectibleConfig = require("Game.Config.Collectible")
local ResultScreenConfig = require("Game.Config.ResultScreen")
local RankConfig = require("Game.Config.Rank")
local GameManager = require("Game.GameManager")
local Scoreboard = require("Game.Scoreboard")
local Engine = require("Common.Engine")
local Format = require("Common.Format")
local UI = require("Common.UI")

local DIALOGUE_PATH = ResultScreenConfig.dialogue_path
local PREVIEW_RESULT_DATA = ResultScreenConfig.preview_result
local DEFAULT_RANK_TEXTURE = RankConfig.default_texture
local RANK_TEXTURE_BY_CODE = RankConfig.texture_by_code

local result_data = nil
local title_text = nil
local result_keys_text = nil
local result_values_text = nil
local coach_comments_text = nil
local status_text = nil
local save_button = nil
local rank_badge = nil
local score_saved = false
local title_loading = false
local waiting_title_confirm = false
local preview_mode = false

local function normalize_rank(rank_code)
    local normalized = string.upper(tostring(rank_code or "C"))
    if normalized == "" then
        return "C"
    end

    return normalized
end

local function resolve_rank_texture(rank_code)
    local normalized = string.lower(normalize_rank(rank_code))
    return RANK_TEXTURE_BY_CODE[normalized] or DEFAULT_RANK_TEXTURE
end

local function shallow_copy(source)
    local copied = {}
    if type(source) ~= "table" then
        return copied
    end

    for key, value in pairs(source) do
        copied[key] = value
    end

    return copied
end

------------------------------------------------
-- 결과 데이터 구성 함수들
------------------------------------------------

local function build_fallback_result_data()
    return {
        score = GameManager.GetScore and GameManager.GetScore() or 0,
        logs = GameManager.GetLogs and GameManager.GetLogs() or 0,
        trace = GameManager.GetTrace and GameManager.GetTrace() or 0,
        dumps = GameManager.GetDumps and GameManager.GetDumps() or 0,
        hotfix_count = GameManager.GetHotfixCount and GameManager.GetHotfixCount() or 0,
        critical_analysis_count = GameManager.GetCriticalAnalysisCount and GameManager.GetCriticalAnalysisCount() or 0,
        distance = GameManager.GetDistance and GameManager.GetDistance() or 0,
        stability = GameManager.GetStability and GameManager.GetStability() or 0,
        max_stability = GameManager.GetMaxStability and GameManager.GetMaxStability() or 0,
        coach_rank = GameManager.GetCoachRank and GameManager.GetCoachRank() or "C",
    }
end

local function resolve_result_data()
    if not (GameManager.IsGameOver and GameManager.IsGameOver()) then
        preview_mode = true
        -- preview_result는 설정 table이므로 화면에서 쓰기 전에 얕은 복사본으로 분리합니다.
        return shallow_copy(PREVIEW_RESULT_DATA)
    end

    local data = GameManager.GetResultData and GameManager.GetResultData() or nil
    if type(data) == "table" then
        preview_mode = false
        return data
    end

    preview_mode = false
    return build_fallback_result_data()
end

local function build_metric_rows(data)
    local trace_max = CollectibleConfig.trace_max or 100
    local dumps_required = CollectibleConfig.crash_dump_required or 3

    return {
        { key = "SCORE", value = Format.Number(data.score or 0) },
        { key = "COLLECTED LOGS", value = Format.Number(data.logs or 0) },
        { key = "TRACE DATA", value = Format.Percent(data.trace or 0, trace_max) },
        { key = "CRASH DUMPS", value = Format.Number(data.dumps or 0) .. "/" .. Format.Number(dumps_required) },
        { key = "HOTFIX APPLIED", value = tostring(math.floor(data.hotfix_count or 0)) },
        { key = "CRITICAL ANALYSIS", value = tostring(math.floor(data.critical_analysis_count or 0)) },
        { key = "DISTANCE", value = Format.Number(data.distance or 0) .. "m" },
    }
end

local function load_coach_dialogues(data)
    local result = {
        baek_name = ResultScreenConfig.coach_name_baek or "백 사령관",
        lim_name = ResultScreenConfig.coach_name_lim or "임 오퍼레이터",
        baek_message = ResultScreenConfig.coach_comment_baek or "",
        lim_message = ResultScreenConfig.coach_comment_lim or "",
    }

    local dialogue_root = load_json_file(DIALOGUE_PATH)
    if type(dialogue_root) ~= "table" or type(dialogue_root.dialogues) ~= "table" then
        return result
    end

    local rank = normalize_rank(data.coach_rank)
    local dialogues = dialogue_root.dialogues
    for index = 1, #dialogues do
        local entry = dialogues[index]
        if type(entry) == "table" and normalize_rank(entry.rank) == rank then
            if entry.speaker == "BAEK_COMMANDER" and entry.message and entry.message ~= "" then
                result.baek_message = tostring(entry.message)
            elseif entry.speaker == "LIM_COMMANDER" and entry.message and entry.message ~= "" then
                result.lim_message = tostring(entry.message)
            end
        end
    end

    return result
end

------------------------------------------------
-- 결과 화면 텍스트 구성 함수들
------------------------------------------------

local function build_result_keys_text(data)
    local rows = build_metric_rows(data)
    local lines = {}
    for index = 1, #rows do
        lines[index] = rows[index].key
    end

    return table.concat(lines, "\n")
end

local function build_result_values_text(data)
    local rows = build_metric_rows(data)
    local lines = {}
    for index = 1, #rows do
        lines[index] = rows[index].value
    end

    return table.concat(lines, "\n")
end

local function build_coach_comments_text(data)
    local coach_dialogues = load_coach_dialogues(data)

    return table.concat({
        tostring(coach_dialogues.baek_name),
        "\"" .. tostring(coach_dialogues.baek_message or "") .. "\"",
        "",
        tostring(coach_dialogues.lim_name),
        "\"" .. tostring(coach_dialogues.lim_message or "") .. "\"",
    }, "\n")
end

------------------------------------------------
-- 결과 화면 생명주기 함수들
------------------------------------------------

-- 결과 화면 컴포넌트를 찾아 최종 점수, 수집량, 코치 코멘트, 랭크 이미지를 채웁니다.
-- 런타임 결과 데이터가 없으면 에디터 preview용 fallback 데이터를 표시합니다.
function BeginPlay()
    play_sfx("Sound.SFX.canceled.sound.effect", false)

    title_text = Engine.GetComponent(obj, "ResultTitle", "UUIScreenTextComponent_0")
    result_keys_text = Engine.GetComponent(obj, "ResultKeys", "UUIScreenTextComponent_1")
    result_values_text = Engine.GetComponent(obj, "ResultValues", "UUIScreenTextComponent_3")
    coach_comments_text = Engine.GetComponent(obj, "CoachComments", "UUIScreenTextComponent_4")
    rank_badge = Engine.GetComponent(obj, "RankBadge", "UUIImageComponent_1")
    status_text = Engine.GetComponent(obj, "SaveStatusText", "UUIScreenTextComponent_2")
    save_button = Engine.GetComponent(obj, "SaveScoreButton", "UIButtonComponent_0")

    if not title_text then
        warn("GameResultScene missing ResultTitle component")
    end
    if not result_keys_text then
        warn("GameResultScene missing ResultKeys component")
    end
    if not result_values_text then
        warn("GameResultScene missing ResultValues component")
    end
    if not coach_comments_text then
        warn("GameResultScene missing CoachComments component")
    end
    if not rank_badge then
        warn("GameResultScene missing RankBadge component")
    end

    result_data = resolve_result_data()
    UI.SetText(title_text, ResultScreenConfig.title or "DEBUG SESSION RESULT")
    UI.SetText(result_keys_text, build_result_keys_text(result_data))
    UI.SetText(result_values_text, build_result_values_text(result_data))
    UI.SetText(coach_comments_text, build_coach_comments_text(result_data))
    UI.SetTexture(rank_badge, resolve_rank_texture(result_data and result_data.coach_rank))
    UI.SetText(status_text, preview_mode and "PREVIEW SAMPLE DATA" or "")
    UI.SetLabel(save_button, "SAVE SCORE")
    score_saved = false
    title_loading = false
    waiting_title_confirm = false
end

-- 점수 저장 popup 결과와 title 복귀 확인 popup을 처리합니다.
-- 저장이 끝난 뒤에는 버튼 상태와 안내 문구를 갱신하고 확인 입력을 기다립니다.
function Tick(dt)
    local nickname = consume_score_save_popup_result()
    if nickname and not score_saved then
        local saved, safe_nickname, save_path = Scoreboard.SaveResult(nickname, result_data)
        if saved then
            score_saved = true
            waiting_title_confirm = true
            UI.SetText(status_text, "SAVED: " .. tostring(safe_nickname))
            UI.SetLabel(save_button, "SAVED")
            print("[Scoreboard] Saved to " .. tostring(save_path) .. " nickname=" .. tostring(safe_nickname) .. " score=" .. tostring(math.floor((result_data and result_data.score) or 0)))
            open_message_popup("Returning to title.")
        else
            UI.SetText(status_text, "SAVE FAILED")
        end
    end

    if waiting_title_confirm and consume_message_popup_ok() then
        waiting_title_confirm = false
        GoToTitle()
    end
end

------------------------------------------------
-- 결과 화면 액션 함수들
------------------------------------------------

function SaveScore()
    play_sfx("Sound.SFX.arwing.hit.obstacle", false)

    if score_saved or waiting_title_confirm then
        return false
    end

    local score = math.floor((result_data and result_data.score) or 0)
    UI.SetText(status_text, "")
    return open_score_save_popup(score)
end

function DelayedGoToTitle()
    wait(1.0)
    return load_scene("game/title.scene")
end

function GoToTitle()
    if title_loading then
        return false
    end

    title_loading = true
    play_sfx("Sound.SFX.arwing.hit.obstacle", false)
    return StartCoroutine("DelayedGoToTitle")
end
