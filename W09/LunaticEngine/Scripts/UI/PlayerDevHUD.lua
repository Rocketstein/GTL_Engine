local DebugConfig = require("Game.Config.Debug")
local CollectibleConfig = require("Game.Config.Collectible")
local HUDConfig = require("Game.Config.PlayerDevHUD")
local RankConfig = require("Game.Config.Rank")
local GameManager = require("Game.GameManager")
local ScenarioLoader = require("UI.ScenarioLoader")
local DialogueUtils = require("UI.DialogueUtils")
local Log = require("Common.Log")
local Engine = require("Common.Engine")
local Format = require("Common.Format")
local Math = require("Common.Math")
local UI = require("Common.UI")

print("[PlayerDevHUD] module loaded")

local DIALOGUE_PATH = HUDConfig.dialogue_path
local DIALOGUE_SOUND_PATH = HUDConfig.dialogue_sound_path
local EVENT_SOUND_BY_TOKEN = HUDConfig.event_sound_by_token
local DEFAULT_EVENT_SOUND_BY_TRIGGER = HUDConfig.default_event_sound_by_trigger
local TYPEWRITER_INTERVAL_SECONDS = HUDConfig.dialogue_timing.typewriter_interval_seconds
local PORTRAIT_ANIMATION_INTERVAL_SECONDS = HUDConfig.dialogue_timing.portrait_animation_interval_seconds
local DIALOGUE_HOLD_SECONDS = HUDConfig.dialogue_timing.hold_seconds
local DIALOGUE_MAX_LINE_WIDTH_UNITS = HUDConfig.dialogue_timing.max_line_width_units
local DIALOGUE_CONTINUATION_INDENT = HUDConfig.dialogue_timing.continuation_indent
local WINDOW_TEXTURES = HUDConfig.window_textures
local DEFAULT_RANK_TEXTURE = RankConfig.default_texture
local RANK_TEXTURE_BY_CODE = RankConfig.texture_by_code
local title_text = nil
local hud_panel = nil
local core_metrics_text = nil
local run_details_title_text = nil
local run_details_text = nil
local approval_label_text = nil
local approval_value_text = nil
local approval_gauge_track = nil
local approval_gauge_fill = nil
local rank_marker = nil
local dialogue_window = nil
local dialogue_speaker_text = nil
local dialogue_message_text = nil

local last_snapshot_key = nil
local last_rank_texture = nil
local last_approval_ratio = nil
local dialogue_data = nil
local dialogue_entries = {}
local dialogue_entries_by_trigger = {}
local last_dialogue_id = nil
local current_dialogue_entry = nil
local dialogue_audio_handle = nil
local last_event_sound_path = nil
local dialogue_hold_timer = 0.0
local portrait_anim_elapsed = 0.0
local portrait_frame_open = false
local current_window_textures = WINDOW_TEXTURES.BAEK_COMMANDER
local typing_state = DialogueUtils.create_typewriter_state(TYPEWRITER_INTERVAL_SECONDS)

local log = Log.MakeLogger(DebugConfig, "[PlayerDevHUD] ")

------------------------------------------------
-- HUD 텍스트 / 레이아웃 함수들
------------------------------------------------

local function resolve_rank_texture(rank_code)
    local normalized = string.lower(tostring(rank_code or "C"))
    if normalized == "" then
        normalized = "c"
    end

    return RANK_TEXTURE_BY_CODE[normalized] or DEFAULT_RANK_TEXTURE
end

local function build_snapshot_key(data)
    return table.concat({
        tostring(data.reason or ""),
        tostring(data.score or 0),
        tostring(data.distance or 0),
        tostring(data.elapsed_time or 0),
        tostring(data.logs or 0),
        tostring(data.trace or 0),
        tostring(data.dumps or 0),
        tostring(data.hotfix_count or 0),
        tostring(data.critical_analysis_count or 0),
        tostring(data.stability or 0),
        tostring(data.max_stability or 0),
        tostring(data.coach_approval or 0),
        tostring(data.coach_rank or "C"),
        tostring(data.shield_count or 0),
    }, "|")
end

local function build_aligned_line(label, value, pad_width)
    local safe_label = tostring(label or "")
    local safe_value = tostring(value or "")
    local width = pad_width or 9
    local padding = width - string.len(safe_label)
    if padding < 0 then
        padding = 0
    end

    return safe_label .. string.rep(" ", padding) .. " " .. safe_value
end

local function build_core_metrics_text(data)
    local trace_max = CollectibleConfig.trace_max or 100
    local dumps_required = CollectibleConfig.crash_dump_required or 3

    return table.concat({
        build_aligned_line("SCORE", Format.Number(data.score or 0)),
        build_aligned_line("LOGS", Format.Number(data.logs or 0)),
        build_aligned_line("TRACE", Format.Percent(data.trace or 0, trace_max)),
        build_aligned_line("DUMPS", Format.Number(data.dumps or 0) .. "/" .. Format.Number(dumps_required)),
        build_aligned_line("STABILITY", Format.Percent(data.stability or 0, data.max_stability or 0)),
    }, "\n")
end

local function build_run_details_text(data)
    return table.concat({
        build_aligned_line("DISTANCE", Format.Decimal(data.distance or 0) .. "m", 8),
        build_aligned_line("TIME", Format.Decimal(data.elapsed_time or 0) .. "s", 8),
    }, "\n")
end

local APPROVAL_GAUGE = HUDConfig.approval_gauge
local RANK_MARKER = HUDConfig.rank_marker
local HUD_LAYOUT = HUDConfig.hud_layout
local DIALOGUE_LAYOUT = HUDConfig.dialogue_layout
local PANEL_LAYOUT = HUDConfig.panel_layout

local function hide_approval_ui()
    UI.SetVisible(approval_label_text, false)
    UI.SetVisible(approval_value_text, false)
    UI.SetVisible(approval_gauge_track, false)
    UI.SetVisible(approval_gauge_fill, false)
    UI.SetVisible(rank_marker, false)
end

local function apply_hud_layout()
    UI.SetScreenSize(hud_panel, PANEL_LAYOUT.width, PANEL_LAYOUT.height)
    UI.SetScreenPosition(title_text, HUD_LAYOUT.title.x, HUD_LAYOUT.title.y)
    UI.SetScreenPosition(core_metrics_text, HUD_LAYOUT.core_text.x, HUD_LAYOUT.core_text.y)
    UI.SetScreenPosition(run_details_title_text, HUD_LAYOUT.details_title.x, HUD_LAYOUT.details_title.y)
    UI.SetScreenPosition(run_details_text, HUD_LAYOUT.details_text.x, HUD_LAYOUT.details_text.y)
    UI.SetScreenPosition(approval_label_text, HUD_LAYOUT.approval_label.x, HUD_LAYOUT.approval_label.y)
    UI.SetScreenPosition(approval_value_text, HUD_LAYOUT.approval_value.x, HUD_LAYOUT.approval_value.y)
    UI.SetScreenPosition(approval_gauge_track, APPROVAL_GAUGE.x, APPROVAL_GAUGE.y)
    UI.SetScreenPosition(approval_gauge_fill, APPROVAL_GAUGE.x, APPROVAL_GAUGE.y)
    UI.SetScreenPosition(dialogue_window, DIALOGUE_LAYOUT.window.x, DIALOGUE_LAYOUT.window.y)
    UI.SetScreenPosition(dialogue_speaker_text, DIALOGUE_LAYOUT.speaker.x, DIALOGUE_LAYOUT.speaker.y)
    UI.SetScreenPosition(dialogue_message_text, DIALOGUE_LAYOUT.message.x, DIALOGUE_LAYOUT.message.y)
end

local function refresh_approval_gauge(data)
    local approval = Math.Clamp(data.coach_approval or 0, 0, 100)
    local ratio = approval / 100.0

    if ratio ~= last_approval_ratio then
        local fill_width = APPROVAL_GAUGE.width * ratio
        if fill_width < 1.0 then
            fill_width = 1.0
        end

        UI.SetScreenSize(approval_gauge_fill, fill_width, APPROVAL_GAUGE.height)

        local marker_x = APPROVAL_GAUGE.x + APPROVAL_GAUGE.width * ratio - (RANK_MARKER.width * 0.5)
        local min_marker_x = APPROVAL_GAUGE.x - (RANK_MARKER.width * 0.5)
        local max_marker_x = APPROVAL_GAUGE.x + APPROVAL_GAUGE.width - (RANK_MARKER.width * 0.5)
        marker_x = Math.Clamp(marker_x, min_marker_x, max_marker_x)
        UI.SetScreenPosition(rank_marker, marker_x, APPROVAL_GAUGE.y + RANK_MARKER.offset_y)

        last_approval_ratio = ratio
    end

    UI.SetText(approval_value_text, Format.Number(approval))
end

local function refresh_hud()
    local data = GameManager.GetResultData and GameManager.GetResultData() or nil
    if type(data) ~= "table" then
        return
    end

    apply_hud_layout()

    local snapshot_key = build_snapshot_key(data)
    if snapshot_key ~= last_snapshot_key then
        UI.SetText(title_text, "PLAYER DEV FUD  [" .. tostring(data.coach_rank or "C") .. "]")
        UI.SetText(core_metrics_text, build_core_metrics_text(data))
        UI.SetText(run_details_text, build_run_details_text(data))
        last_snapshot_key = snapshot_key
    end
end

------------------------------------------------
-- 대화 사운드 / 포트레이트 함수들
------------------------------------------------

local function stop_dialogue_audio()
    if dialogue_audio_handle and dialogue_audio_handle ~= "" then
        stop_audio_by_handle(dialogue_audio_handle)
    end
    dialogue_audio_handle = nil
end

local function resolve_event_sound_path(token)
    if type(token) ~= "string" or token == "" then
        return nil
    end

    if string.sub(token, 1, 6) == "Asset/" then
        return token
    end

    return EVENT_SOUND_BY_TOKEN[token]
end

local function play_dialogue_event_sfx(entry)
    local selected_path = nil
    local sounds = type(entry) == "table" and entry.sounds or nil
    if type(sounds) == "table" then
        for index = 1, #sounds do
            local token = sounds[index]
            if type(token) == "string" and string.sub(token, 1, 6) ~= "voice_" then
                local resolved = resolve_event_sound_path(token)
                if resolved and resolved ~= "" then
                    selected_path = resolved
                    break
                end
            end
        end
    end

    if not selected_path then
        selected_path = DEFAULT_EVENT_SOUND_BY_TRIGGER[type(entry) == "table" and entry.trigger or ""]
    end

    last_event_sound_path = selected_path
    if selected_path and selected_path ~= "" then
        play_sfx(selected_path, false)
    end
end

local function is_dialogue_audio_playing()
    return dialogue_audio_handle ~= nil
        and dialogue_audio_handle ~= ""
        and is_audio_playing_by_handle(dialogue_audio_handle)
end

local function resolve_window_textures(speaker_id)
    return WINDOW_TEXTURES[speaker_id] or WINDOW_TEXTURES.BAEK_COMMANDER
end

local function set_window_frame(opened)
    local frame_path = current_window_textures and (opened and current_window_textures.open or current_window_textures.closed) or nil
    UI.SetTexture(dialogue_window, frame_path)
end

local function update_dialogue_text()
    UI.SetText(dialogue_message_text, DialogueUtils.get_visible_text(typing_state, ""))
end

------------------------------------------------
-- 대화 엔트리 선택 / 재생 함수들
------------------------------------------------

local function load_dialogue_entries()
    dialogue_data = ScenarioLoader.load(DIALOGUE_PATH, load_json_file)
    if type(dialogue_data) ~= "table" or type(dialogue_data.dialogues) ~= "table" then
        dialogue_entries = {}
        log("Failed to load dialogue data from " .. tostring(DIALOGUE_PATH))
        return
    end

    dialogue_entries = {}
    dialogue_entries_by_trigger = {}
    for index = 1, #dialogue_data.dialogues do
        local entry = dialogue_data.dialogues[index]
        if type(entry) == "table" and type(entry.message) == "string" and entry.message ~= "" then
            dialogue_entries[#dialogue_entries + 1] = entry

            local trigger_name = type(entry.trigger) == "string" and entry.trigger or ""
            if trigger_name ~= "" then
                if type(dialogue_entries_by_trigger[trigger_name]) ~= "table" then
                    dialogue_entries_by_trigger[trigger_name] = {}
                end
                dialogue_entries_by_trigger[trigger_name][#dialogue_entries_by_trigger[trigger_name] + 1] = entry
            end
        end
    end

    log("Loaded dialogue entries count=" .. tostring(#dialogue_entries))
end

local function pick_random_dialogue_entry(entries)
    local pool = type(entries) == "table" and entries or dialogue_entries
    local count = #pool
    if count <= 0 then
        return nil
    end

    if count == 1 then
        return pool[1]
    end

    local chosen = nil
    local guard = 0
    repeat
        chosen = pool[math.random(1, count)]
        guard = guard + 1
    until not chosen or chosen.id ~= last_dialogue_id or guard >= 8

    return chosen
end

local function pick_trigger_dialogue_entry(trigger_name)
    if type(trigger_name) ~= "string" or trigger_name == "" then
        return nil
    end

    local entries = dialogue_entries_by_trigger[trigger_name]
    if type(entries) ~= "table" or #entries <= 0 then
        return nil
    end

    return pick_random_dialogue_entry(entries)
end

local function pick_initial_dialogue_entry()
    local start_entries = dialogue_entries_by_trigger.onRunStart
    if type(start_entries) == "table" and #start_entries > 0 then
        for index = 1, #start_entries do
            local entry = start_entries[index]
            if type(entry) == "table" and entry.speaker == "BAEK_COMMANDER" then
                return entry
            end
        end

        return start_entries[1]
    end

    return pick_random_dialogue_entry()
end

local function start_dialogue_preview(entry)
    if type(entry) ~= "table" then
        return
    end

    log(
        "StartDialogue id=" .. tostring(entry.id) ..
        " trigger=" .. tostring(entry.trigger) ..
        " speaker=" .. tostring(entry.speaker)
    )

    current_dialogue_entry = entry
    last_dialogue_id = entry.id
    current_window_textures = resolve_window_textures(entry.speaker)
    portrait_anim_elapsed = 0.0
    portrait_frame_open = false
    set_window_frame(false)

    UI.SetText(dialogue_speaker_text, ScenarioLoader.resolve_speaker_name(dialogue_data, entry.speaker))

    local message = DialogueUtils.format_terminal_message(tostring(entry.message or ""), {
        max_line_width_units = DIALOGUE_MAX_LINE_WIDTH_UNITS,
        continuation_indent = DIALOGUE_CONTINUATION_INDENT,
    })
    DialogueUtils.start_typewriter(typing_state, message, TYPEWRITER_INTERVAL_SECONDS)
    update_dialogue_text()

    stop_dialogue_audio()
    play_dialogue_event_sfx(entry)
    dialogue_audio_handle = play_sfx(DIALOGUE_SOUND_PATH, false)
    dialogue_hold_timer = DIALOGUE_HOLD_SECONDS
end

local function advance_random_dialogue()
    local next_entry = pick_random_dialogue_entry()
    if not next_entry then
        UI.SetText(dialogue_speaker_text, "DIALOGUE OFFLINE")
        UI.SetText(dialogue_message_text, "play.dialogue.json load failed.")
        return
    end

    start_dialogue_preview(next_entry)
end

local function is_preview_mode()
    local is_running = GameManager.IsRunning and GameManager.IsRunning() or false
    local is_paused = GameManager.IsPaused and GameManager.IsPaused() or false
    local is_game_over = GameManager.IsGameOver and GameManager.IsGameOver() or false
    return not is_running and not is_paused and not is_game_over
end

local function consume_runtime_dialogue_trigger()
    if not (GameManager.ConsumeDialogueTrigger and not is_preview_mode()) then
        return nil
    end

    local trigger_name = GameManager.ConsumeDialogueTrigger()
    if trigger_name then
        log("ConsumeTrigger trigger=" .. tostring(trigger_name))
    end
    return trigger_name
end

-- 대화 preview의 런타임 trigger, typewriter, 초상화 입 모양, 자동 다음 대사를 갱신합니다.
-- 게임 이벤트로 들어온 trigger는 현재 preview보다 우선 적용해 반응성을 유지합니다.
local function update_dialogue_preview(dt)
    if #dialogue_entries <= 0 then
        return
    end

    -- Runtime triggers should preempt the current preview so hit/low-stability
    -- events feel reactive instead of waiting for the previous line to finish.
    local runtime_trigger = consume_runtime_dialogue_trigger()
    if runtime_trigger then
        local runtime_entry = pick_trigger_dialogue_entry(runtime_trigger)
        if runtime_entry then
            start_dialogue_preview(runtime_entry)
            return
        end

        log("No dialogue entry found for trigger=" .. tostring(runtime_trigger))
    end

    local advanced = DialogueUtils.update_typewriter(typing_state, dt, nil)
    if advanced then
        update_dialogue_text()
    end

    if not typing_state.active and typing_state.visible_count >= #typing_state.chars then
        update_dialogue_text()
    end

    if is_dialogue_audio_playing() then
        portrait_anim_elapsed = portrait_anim_elapsed + (dt or 0.0)
        while portrait_anim_elapsed >= PORTRAIT_ANIMATION_INTERVAL_SECONDS do
            portrait_anim_elapsed = portrait_anim_elapsed - PORTRAIT_ANIMATION_INTERVAL_SECONDS
            portrait_frame_open = not portrait_frame_open
            set_window_frame(portrait_frame_open)
        end
    else
        portrait_anim_elapsed = 0.0
        if portrait_frame_open then
            portrait_frame_open = false
            set_window_frame(false)
        end
    end

    if typing_state.active or is_dialogue_audio_playing() then
        return
    end

    dialogue_hold_timer = dialogue_hold_timer - (dt or 0.0)
    if dialogue_hold_timer <= 0.0 and is_preview_mode() then
        advance_random_dialogue()
    end
end

------------------------------------------------
-- HUD 생명주기 함수들
------------------------------------------------

function BeginPlay()
    print("[PlayerDevHUD] BeginPlay entered")
    math.randomseed(math.floor(time() * 1000) % 2147483647)

    hud_panel = Engine.GetComponent(obj, "FUDPanel")
    title_text = Engine.GetComponent(obj, "FUDTitle")
    core_metrics_text = Engine.GetComponent(obj, "FUDCoreMetricsText")
    run_details_title_text = Engine.GetComponent(obj, "FUDRunDetailsTitle")
    run_details_text = Engine.GetComponent(obj, "FUDRunDetailsText")
    approval_label_text = Engine.GetComponent(obj, "FUDApprovalLabel")
    approval_value_text = Engine.GetComponent(obj, "FUDApprovalValue")
    approval_gauge_track = Engine.GetComponent(obj, "FUDApprovalGaugeTrack")
    approval_gauge_fill = Engine.GetComponent(obj, "FUDApprovalGaugeFill")
    rank_marker = Engine.GetComponent(obj, "FUDRankMarker")
    dialogue_window = Engine.GetComponent(obj, "DialogueWindowPortrait")
    dialogue_speaker_text = Engine.GetComponent(obj, "DialogueSpeakerName")
    dialogue_message_text = Engine.GetComponent(obj, "DialogueMessage")

    last_snapshot_key = nil
    last_rank_texture = nil
    last_approval_ratio = nil
    current_window_textures = WINDOW_TEXTURES.BAEK_COMMANDER
    DialogueUtils.reset_typewriter(typing_state, TYPEWRITER_INTERVAL_SECONDS)

    hide_approval_ui()
    apply_hud_layout()
    refresh_hud()
    load_dialogue_entries()
    start_dialogue_preview(pick_initial_dialogue_entry())
end

function Tick(dt)
    refresh_hud()
    update_dialogue_preview(dt)
end

function EndPlay()
    stop_dialogue_audio()
end
