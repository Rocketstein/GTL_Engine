local ScenarioLoader = require("UI.ScenarioLoader")
local DialogueUtils = require("UI.DialogueUtils")
local StoryConfig = require("Game.Config.StoryScene")
local Engine = require("Common.Engine")
local Math = require("Common.Math")
local UI = require("Common.UI")

local SCENARIO_PATH = StoryConfig.scenario_path
local NEXT_SCENE = StoryConfig.next_scene

local ui = {}
local scenario = nil
local pages = {}
local current_index = 1
local input_cooldown = 0.0
local auto_advance_timer = nil
local scene_change_timer = nil
local scene_change_duration = StoryConfig.scene_change_duration
local pending_page_index = nil
local current_story_background_path = nil
local story_finished = false
local skip_hold_elapsed = 0.0
local typing_state = {
    active = false,
    chars = {},
    visible_count = 0,
    elapsed = 0.0,
    interval = StoryConfig.text.typewriter_interval_seconds,
    full_text = "",
}
local dialogue_background_state = {
    current_one_key = nil,
}
local OVERLAY_COMPONENTS = StoryConfig.overlay_components
local dialogue_transition = {
    active = false,
    elapsed = 0.0,
    duration = 0.0,
    mode = nil,
    zero_key = nil,
    zero_path = nil,
    one_key = nil,
    one_path = nil,
}
local pending_dialogue_ui = nil
local prologue_state = {
    mode = nil,
    elapsed = 0.0,
    line_index = 0,
    lines = {},
    intro_fade_seconds = StoryConfig.prologue.intro_fade_seconds,
    intro_hold_seconds = StoryConfig.prologue.intro_hold_seconds,
    outro_fade_seconds = StoryConfig.prologue.outro_fade_seconds,
    control_room_fade_seconds = StoryConfig.prologue.control_room_fade_seconds,
    crt_lead_seconds = StoryConfig.prologue.crt_lead_seconds,
    crt_switch_delay_seconds = StoryConfig.prologue.crt_switch_delay_seconds,
    post_crt_hold_seconds = StoryConfig.prologue.post_crt_hold_seconds,
}

local MAX_LINE_WIDTH_UNITS = StoryConfig.text.max_line_width_units
local SPEAKER_NAME_COLOR = StoryConfig.text.speaker_name_color
local DIALOGUE_TEXT_COLOR = StoryConfig.text.dialogue_text_color
local PROLOGUE_TEXT_COLOR = StoryConfig.text.prologue_text_color
local PROLOGUE_HINT_COLOR = StoryConfig.text.prologue_hint_color
local PROLOGUE_DIM_BACKGROUND = StoryConfig.text.prologue_dim_background
local INPUT_COOLDOWN_SECONDS = StoryConfig.text.input_cooldown_seconds
local DIALOGUE_TRANSITION_DURATION = StoryConfig.text.dialogue_transition_duration
local TYPEWRITER_INTERVAL_SECONDS = StoryConfig.text.typewriter_interval_seconds
local NEXT_PAGE_SOUND_PATH = StoryConfig.sounds.next_page
local TYPEWRITER_SOUND_PATH = StoryConfig.sounds.typewriter
local DEFAULT_BOOT_SOUND_PATH = StoryConfig.sounds.boot
local DEFAULT_CRT_ON_SOUND_PATH = StoryConfig.sounds.crt_on
local DEFAULT_GO_SOUND_PATH = StoryConfig.sounds.go
local TERMINAL_PROMPT = StoryConfig.text.terminal_prompt
local HOLD_SKIP_SECONDS = StoryConfig.text.hold_skip_seconds
local HOLD_SKIP_HINT_TEXT = StoryConfig.text.hold_skip_hint_text
local apply_page = nil
local resolve_neutral_background = nil

------------------------------------------------
-- 컴포넌트 / Asset / 사운드 헬퍼 함수들
------------------------------------------------

local function cache_component(name)
    ui[name] = Engine.GetRequiredComponent(obj, "StoryScene component missing:", name)
end

local function play_story_sfx(sound_path)
    if type(sound_path) == "string" and sound_path ~= "" then
        play_sfx(sound_path, false)
    end
end

local function resolve_story_image(image_key, fallback_path)
    local resolved = ScenarioLoader.resolve_asset(scenario, "images", image_key)
    if resolved and resolved ~= "" then
        return resolved
    end
    return fallback_path
end

local function resolve_story_sound(sound_key, fallback_path)
    local resolved = ScenarioLoader.resolve_asset(scenario, "sounds", sound_key)
    if resolved and resolved ~= "" then
        return resolved
    end
    return fallback_path
end

local function play_story_bgm(sound_path, looping)
    if type(sound_path) == "string" and sound_path ~= "" then
        stop_bgm()
        play_bgm(sound_path, looping == true)
    end
end

local function ease_in_power(value, power)
    local t = Math.Clamp01(value or 0.0)
    local exponent = power or 4.0
    return math.pow(t, exponent)
end

------------------------------------------------
-- 배경 레이어 함수들
------------------------------------------------

local function set_story_background_texture(texture_path)
    if not texture_path or texture_path == "" then
        return
    end

    if current_story_background_path == texture_path then
        return
    end

    UI.SetCachedTexture(ui, "StoryBackground", texture_path)
    current_story_background_path = texture_path
end

------------------------------------------------
-- 대화 UI / 전환 상태 함수들
------------------------------------------------

local function preload_component_texture(name)
    local component = ui[name]
    if not component or not component.GetTexturePath then
        return
    end

    local texture_path = component:GetTexturePath()
    if not texture_path or texture_path == "" or texture_path == "None" then
        return
    end

    component:SetTexture(texture_path)
end

local function preload_story_background_layers()
    preload_component_texture("StoryBackground")

    for _, component_name in pairs(OVERLAY_COMPONENTS) do
        preload_component_texture(component_name)
    end
end

local function set_default_background()
    local _, neutral_path = resolve_neutral_background()
    if neutral_path then
        set_story_background_texture(neutral_path)
    end
    UI.SetCachedTint(ui, "StoryBackground", 1.0, 1.0, 1.0, 1.0)
end

local function set_overlay_alpha(background_key, alpha)
    local component_name = OVERLAY_COMPONENTS[background_key]
    if component_name then
        UI.SetCachedVisible(ui, component_name, true)
        UI.SetCachedTint(ui, component_name, 1.0, 1.0, 1.0, alpha or 0.0)
    end
end

local function hide_dialogue_background_layers()
    for background_key, component_name in pairs(OVERLAY_COMPONENTS) do
        UI.SetCachedVisible(ui, component_name, true)
        UI.SetCachedTint(ui, component_name, 1.0, 1.0, 1.0, 0.0)
    end
end

local function reset_dialogue_transition()
    dialogue_transition.active = false
    dialogue_transition.elapsed = 0.0
    dialogue_transition.duration = 0.0
    dialogue_transition.mode = nil
    dialogue_transition.zero_key = nil
    dialogue_transition.zero_path = nil
    dialogue_transition.one_key = nil
    dialogue_transition.one_path = nil
end

local function clear_pending_dialogue_ui()
    pending_dialogue_ui = nil
end

local function reset_typewriter()
    typing_state.active = false
    typing_state.chars = {}
    typing_state.visible_count = 0
    typing_state.elapsed = 0.0
    typing_state.interval = TYPEWRITER_INTERVAL_SECONDS
    typing_state.full_text = ""
end

local function hide_dialogue_ui()
    reset_typewriter()
    UI.SetCachedText(ui, "SpeakerName", "")
    UI.SetCachedText(ui, "DialogueText", "")
    UI.SetCachedText(ui, "NarrationText", "")
    UI.SetCachedText(ui, "PageHint", HOLD_SKIP_HINT_TEXT)
    UI.SetCachedVisible(ui, "SpeakerName", false)
    UI.SetCachedVisible(ui, "DialogueText", false)
    UI.SetCachedVisible(ui, "NarrationText", false)
    UI.SetCachedVisible(ui, "PageHint", true)
end

local function finish_story(target_scene)
    if story_finished then
        return
    end

    story_finished = true
    stop_bgm()
    load_scene(target_scene or NEXT_SCENE)
end

local function start_scene_change_flash()
    scene_change_duration = 2.0
    scene_change_timer = scene_change_duration
    UI.SetCachedVisible(ui, "WhiteFlash", true)
    UI.SetCachedTint(ui, "WhiteFlash", 1.0, 1.0, 1.0, 0.0)
end

------------------------------------------------
-- 스킵 입력 함수들
------------------------------------------------

local function is_skip_key_held()
    return GetKey("SPACE") or GetKey("Space")
end

local function update_skip_hold(dt)
    if scene_change_timer or story_finished then
        skip_hold_elapsed = 0.0
        return false
    end

    if is_skip_key_held() then
        skip_hold_elapsed = skip_hold_elapsed + (dt or 0.0)
        if skip_hold_elapsed >= HOLD_SKIP_SECONDS then
            finish_story()
            return true
        end
    else
        skip_hold_elapsed = 0.0
    end

    return false
end

local function set_dialogue_alpha(alpha)
    local clamped = Math.Clamp01(alpha or 0.0)
    UI.SetCachedTint(ui, "NarrationText", PROLOGUE_TEXT_COLOR[1], PROLOGUE_TEXT_COLOR[2], PROLOGUE_TEXT_COLOR[3], PROLOGUE_TEXT_COLOR[4] * clamped)
    UI.SetCachedTint(ui, "PageHint", PROLOGUE_HINT_COLOR[1], PROLOGUE_HINT_COLOR[2], PROLOGUE_HINT_COLOR[3], PROLOGUE_HINT_COLOR[4] * clamped)
end

------------------------------------------------
-- 프롤로그 출력 함수들
------------------------------------------------

local function build_prologue_text(line_count)
    local lines = {}
    local visible_count = math.min(line_count or 0, #prologue_state.lines)

    for index = 1, visible_count do
        lines[#lines + 1] = tostring(prologue_state.lines[index] or "")
    end

    return table.concat(lines, "\n")
end

local function show_prologue_lines()
    UI.SetCachedVisible(ui, "SpeakerName", false)
    UI.SetCachedVisible(ui, "DialogueText", false)
    UI.SetCachedVisible(ui, "NarrationText", true)
    UI.SetCachedVisible(ui, "PageHint", true)
    UI.SetCachedText(ui, "SpeakerName", "")
    UI.SetCachedText(ui, "NarrationText", build_prologue_text(prologue_state.line_index))
    UI.SetCachedText(ui, "PageHint", HOLD_SKIP_HINT_TEXT)
    UI.SetCachedTint(ui, "StoryBackground", PROLOGUE_DIM_BACKGROUND, PROLOGUE_DIM_BACKGROUND, PROLOGUE_DIM_BACKGROUND, 1.0)
    set_dialogue_alpha(1.0)
end

local function update_typewriter_text()
    if not typing_state.full_text or typing_state.full_text == "" then
        UI.SetCachedText(ui, "DialogueText", TERMINAL_PROMPT)
        return
    end

    if typing_state.visible_count <= 0 then
        UI.SetCachedText(ui, "DialogueText", TERMINAL_PROMPT)
        return
    end

    UI.SetCachedText(ui, "DialogueText", TERMINAL_PROMPT .. table.concat(typing_state.chars, "", 1, typing_state.visible_count))
end

local function complete_typewriter()
    if typing_state.full_text == "" then
        reset_typewriter()
        UI.SetCachedText(ui, "DialogueText", TERMINAL_PROMPT)
        return
    end

    typing_state.active = false
    typing_state.visible_count = #typing_state.chars
    update_typewriter_text()
end

local function start_typewriter(text)
    reset_typewriter()
    typing_state.full_text = type(text) == "string" and text or ""
    typing_state.chars = DialogueUtils.split_utf8_chars(typing_state.full_text)

    if #typing_state.chars == 0 then
        UI.SetCachedText(ui, "DialogueText", TERMINAL_PROMPT)
        return
    end

    typing_state.active = true
    typing_state.interval = TYPEWRITER_INTERVAL_SECONDS
    update_typewriter_text()
end

local function show_dialogue_ui(page)
    local speaker_name = ScenarioLoader.resolve_speaker_name(scenario, page.speaker)
    local message = DialogueUtils.format_terminal_message(tostring(page.message or ""), {
        max_line_width_units = MAX_LINE_WIDTH_UNITS,
        continuation_indent = "   ",
    })

    UI.SetCachedVisible(ui, "SpeakerName", true)
    UI.SetCachedVisible(ui, "DialogueText", true)
    UI.SetCachedVisible(ui, "NarrationText", false)
    UI.SetCachedVisible(ui, "PageHint", true)
    UI.SetCachedText(ui, "SpeakerName", speaker_name)
    UI.SetCachedTint(ui, "SpeakerName", SPEAKER_NAME_COLOR[1], SPEAKER_NAME_COLOR[2], SPEAKER_NAME_COLOR[3], SPEAKER_NAME_COLOR[4])
    UI.SetCachedTint(ui, "DialogueText", DIALOGUE_TEXT_COLOR[1], DIALOGUE_TEXT_COLOR[2], DIALOGUE_TEXT_COLOR[3], DIALOGUE_TEXT_COLOR[4])
    UI.SetCachedText(ui, "PageHint", HOLD_SKIP_HINT_TEXT)
    UI.SetCachedText(ui, "DialogueText", TERMINAL_PROMPT)
    start_typewriter(message)
end

------------------------------------------------
-- 대화 페이지 배경 / 전환 함수들
------------------------------------------------

local function resolve_page_background(page)
    if not scenario or type(page) ~= "table" then
        return nil
    end

    if type(page.background) == "string" then
        local resolved = ScenarioLoader.resolve_asset(scenario, "images", page.background)
        if resolved then
            return resolved
        end
        if page.background:sub(1, 6) == "Asset/" then
            return page.background
        end
    end

    return nil
end

resolve_neutral_background = function()
    if not scenario then
        return nil, nil
    end

    local neutral_key = "control_room_on"
    local neutral_path = ScenarioLoader.resolve_asset(scenario, "images", neutral_key)
    return neutral_key, neutral_path
end

local function resolve_fade_source_background(page)
    if not scenario or type(page) ~= "table" or type(page.background) ~= "string" then
        return nil, nil
    end

    local background_key = page.background
    if background_key:sub(-2) ~= "_1" then
        return nil, nil
    end

    local base_key = background_key:sub(1, -3) .. "_0"
    local base_path = ScenarioLoader.resolve_asset(scenario, "images", base_key)
    if not base_path then
        return nil, nil
    end

    return base_key, base_path
end

local function complete_dialogue_transition()
    local completed_mode = dialogue_transition.mode
    if completed_mode == "enter" then
        set_overlay_alpha(dialogue_transition.zero_key, 1.0)
        set_overlay_alpha(dialogue_transition.one_key, 1.0)
        dialogue_background_state.current_one_key = dialogue_transition.one_key
        show_dialogue_ui(pending_dialogue_ui)
        clear_pending_dialogue_ui()
    elseif completed_mode == "exit" then
        hide_dialogue_background_layers()
        dialogue_background_state.current_one_key = nil
    end

    reset_dialogue_transition()

    if completed_mode == "exit" and pending_page_index then
        current_index = pending_page_index
        pending_page_index = nil
        apply_page(pages[current_index])
    end
end

local function set_background_immediately(background_key, background_path)
    if not background_path then
        return
    end

    reset_dialogue_transition()
    clear_pending_dialogue_ui()
    set_default_background()
    hide_dialogue_background_layers()
    if background_key then
        set_overlay_alpha(background_key, 1.0)
    end

    dialogue_background_state.current_one_key = background_key
end

local function set_non_dialogue_background(background_path)
    reset_dialogue_transition()
    clear_pending_dialogue_ui()
    set_default_background()
    hide_dialogue_background_layers()
    if type(background_path) == "string" then
        for background_key, component_name in pairs(OVERLAY_COMPONENTS) do
            local resolved = ScenarioLoader.resolve_asset(scenario, "images", background_key)
            if resolved == background_path then
                set_overlay_alpha(background_key, 1.0)
                break
            end
        end
    end
    UI.SetCachedVisible(ui, "StoryBackground", true)
    UI.SetCachedTint(ui, "StoryBackground", 1.0, 1.0, 1.0, 1.0)
    dialogue_background_state.current_one_key = nil
end

local function begin_dialogue_transition(mode, zero_key, zero_path, one_key, one_path)
    if not zero_path or not one_path then
        if mode == "enter" then
            set_background_immediately(one_key, one_path)
        else
            set_non_dialogue_background(select(2, resolve_neutral_background()))
        end
        return
    end

    set_default_background()
    hide_dialogue_background_layers()
    if zero_key then
        set_overlay_alpha(zero_key, 1.0)
    end

    if mode == "enter" then
        if one_key then
            set_overlay_alpha(one_key, 0.0)
        end
    else
        if one_key then
            set_overlay_alpha(one_key, 1.0)
        end
    end

    dialogue_transition.active = true
    dialogue_transition.elapsed = 0.0
    dialogue_transition.duration = DIALOGUE_TRANSITION_DURATION
    dialogue_transition.mode = mode
    dialogue_transition.zero_key = zero_key
    dialogue_transition.zero_path = zero_path
    dialogue_transition.one_key = one_key
    dialogue_transition.one_path = one_path
end

local function is_transition_dialogue_page(page)
    return type(page) == "table"
        and page.type == "dialogue"
        and type(page.background) == "string"
        and page.background:sub(-2) == "_1"
end

local function start_dialogue_intro_transition(page, target_path)
    local base_key, base_path = resolve_fade_source_background(page)
    if not base_path or not target_path then
        set_background_immediately(page.background, target_path)
        return
    end

    begin_dialogue_transition("enter", base_key, base_path, page.background, target_path)
end

local function start_dialogue_exit_transition(page)
    local base_key, base_path = resolve_fade_source_background(page)
    local one_path = resolve_page_background(page)
    if not one_path or not base_path then
        set_non_dialogue_background(select(2, resolve_neutral_background()))
        if pending_page_index then
            current_index = pending_page_index
            pending_page_index = nil
            apply_page(pages[current_index])
        end
        return
    end

    begin_dialogue_transition("exit", base_key, base_path, page.background, one_path)
end

local function is_auto_page(page)
    return type(page) == "table"
        and (page.type == "image" or page.type == "splash")
        and type(page.duration) == "number"
        and page.duration > 0.0
end

------------------------------------------------
-- 스토리 진행 함수들
------------------------------------------------

local function start_story_sequence()
    prologue_state.mode = "story"
    prologue_state.elapsed = 0.0
    prologue_state.line_index = 0
    UI.SetCachedTint(ui, "StoryBackground", 1.0, 1.0, 1.0, 1.0)
    UI.SetCachedVisible(ui, "StoryBackground", true)
    hide_dialogue_background_layers()
    play_story_bgm(resolve_story_sound("stage_intro", "Asset/Content/Sound/Background/06. Stage Intro.mp3"), true)
    apply_page(pages[current_index])
end

local function start_story_prologue()
    local prologue = type(scenario) == "table" and scenario.prologue or nil
    local lines = type(prologue) == "table" and prologue.lines or nil
    if type(lines) ~= "table" or #lines == 0 then
        start_story_sequence()
        return
    end

    prologue_state.lines = lines
    prologue_state.line_index = 0
    prologue_state.elapsed = 0.0
    prologue_state.mode = "intro_fade_in"
    prologue_state.intro_fade_seconds = (type(prologue.introFadeSeconds) == "number" and prologue.introFadeSeconds > 0.0) and prologue.introFadeSeconds or 3.0
    prologue_state.intro_hold_seconds = (type(prologue.introHoldSeconds) == "number" and prologue.introHoldSeconds >= 0.0) and prologue.introHoldSeconds or 1.5
    prologue_state.outro_fade_seconds = (type(prologue.outroFadeSeconds) == "number" and prologue.outroFadeSeconds > 0.0) and prologue.outroFadeSeconds or 0.9
    prologue_state.control_room_fade_seconds = (type(prologue.controlRoomFadeSeconds) == "number" and prologue.controlRoomFadeSeconds > 0.0) and prologue.controlRoomFadeSeconds or 0.9
    prologue_state.crt_lead_seconds = (type(prologue.crtLeadSeconds) == "number" and prologue.crtLeadSeconds >= 0.0) and prologue.crtLeadSeconds or 1.0
    prologue_state.crt_switch_delay_seconds = (type(prologue.crtSwitchDelaySeconds) == "number" and prologue.crtSwitchDelaySeconds >= 0.0) and prologue.crtSwitchDelaySeconds or 0.2
    prologue_state.post_crt_hold_seconds = (type(prologue.postCrtHoldSeconds) == "number" and prologue.postCrtHoldSeconds >= 0.0) and prologue.postCrtHoldSeconds or 1.2

    stop_bgm()
    reset_typewriter()
    clear_pending_dialogue_ui()
    hide_dialogue_background_layers()
    UI.SetCachedTexture(ui, "StoryBackground", resolve_story_image("introduce", "Asset/Content/Texture/Story/introduce.png"))
    current_story_background_path = nil
    UI.SetCachedVisible(ui, "StoryBackground", true)
    UI.SetCachedTint(ui, "StoryBackground", 0.0, 0.0, 0.0, 1.0)
    UI.SetCachedVisible(ui, "SpeakerName", false)
    UI.SetCachedVisible(ui, "DialogueText", false)
    UI.SetCachedVisible(ui, "NarrationText", false)
    UI.SetCachedVisible(ui, "PageHint", false)
    UI.SetCachedText(ui, "SpeakerName", "")
    UI.SetCachedText(ui, "DialogueText", "")
    UI.SetCachedText(ui, "NarrationText", "")
    UI.SetCachedText(ui, "PageHint", "")
    play_story_bgm(resolve_story_sound("boot_startup", DEFAULT_BOOT_SOUND_PATH), false)
end

local function try_advance_prologue()
    if input_cooldown > 0.0 then
        return false
    end

    if prologue_state.mode ~= "line_reveal" then
        return false
    end

    input_cooldown = INPUT_COOLDOWN_SECONDS
    play_story_sfx(NEXT_PAGE_SOUND_PATH)

    if prologue_state.line_index < #prologue_state.lines then
        prologue_state.line_index = prologue_state.line_index + 1
        show_prologue_lines()
        if prologue_state.line_index >= #prologue_state.lines then
            prologue_state.mode = "line_complete_hold"
            prologue_state.elapsed = 0.0
        end
        return true
    end

    return false
end

-- 프롤로그의 fade, 줄 단위 진행, CRT 켜짐 연출을 mode 상태값으로 순차 처리합니다.
-- story mode로 넘어가기 전까지 Tick의 일반 페이지 진행을 막는 gate 역할도 합니다.
local function update_prologue(dt)
    local delta = dt or 0.0

    if prologue_state.mode == "intro_fade_in" then
        prologue_state.elapsed = prologue_state.elapsed + delta
        local raw_progress = Math.Clamp01(prologue_state.elapsed / prologue_state.intro_fade_seconds)
        local progress = ease_in_power(raw_progress, 2.8)
        UI.SetCachedTint(ui, "StoryBackground", progress, progress, progress, 1.0)
        if raw_progress >= 1.0 then
            prologue_state.mode = "intro_hold"
            prologue_state.elapsed = 0.0
        end
        return true
    end

    if prologue_state.mode == "intro_hold" then
        prologue_state.elapsed = prologue_state.elapsed + delta
        if prologue_state.elapsed >= prologue_state.intro_hold_seconds then
            prologue_state.mode = "line_reveal"
            prologue_state.elapsed = 0.0
            prologue_state.line_index = 0
            UI.SetCachedTexture(ui, "StoryBackground", resolve_story_image("introduce", "Asset/Content/Texture/Story/introduce.png"))
            show_prologue_lines()
        end
        return true
    end

    if prologue_state.mode == "line_reveal" then
        return true
    end

    if prologue_state.mode == "line_complete_hold" then
        prologue_state.elapsed = prologue_state.elapsed + delta
        if prologue_state.elapsed >= 0.8 then
            prologue_state.mode = "outro_fade_out"
            prologue_state.elapsed = 0.0
        end
        return true
    end

    if prologue_state.mode == "outro_fade_out" then
        prologue_state.elapsed = prologue_state.elapsed + delta
        local progress = Math.Clamp01(prologue_state.elapsed / prologue_state.outro_fade_seconds)
        local alpha = 1.0 - progress
        local brightness = PROLOGUE_DIM_BACKGROUND + ((1.0 - PROLOGUE_DIM_BACKGROUND) * progress)
        UI.SetCachedTint(ui, "StoryBackground", brightness, brightness, brightness, 1.0)
        set_dialogue_alpha(alpha)
        if progress >= 1.0 then
            prologue_state.mode = "control_room_fade_in"
            prologue_state.elapsed = 0.0
            hide_dialogue_ui()
            UI.SetCachedTexture(ui, "StoryBackground", resolve_story_image("control_room_off", "Asset/Content/Texture/Story/control_room_screen_off.png"))
            UI.SetCachedTint(ui, "StoryBackground", 1.0, 1.0, 1.0, 0.0)
        end
        return true
    end

    if prologue_state.mode == "control_room_fade_in" then
        prologue_state.elapsed = prologue_state.elapsed + delta
        local progress = Math.Clamp01(prologue_state.elapsed / prologue_state.control_room_fade_seconds)
        UI.SetCachedTint(ui, "StoryBackground", 1.0, 1.0, 1.0, progress)
        if progress >= 1.0 then
            play_story_sfx(resolve_story_sound("crt_on", DEFAULT_CRT_ON_SOUND_PATH))
            prologue_state.mode = "crt_audio_lead"
            prologue_state.elapsed = 0.0
        end
        return true
    end

    if prologue_state.mode == "crt_audio_lead" then
        prologue_state.elapsed = prologue_state.elapsed + delta
        if prologue_state.elapsed >= prologue_state.crt_lead_seconds then
            prologue_state.mode = "crt_power_on"
            prologue_state.elapsed = 0.0
            UI.SetCachedTexture(ui, "StoryBackground", resolve_story_image("control_room_on", "Asset/Content/Texture/Story/control_room_screen_on.png"))
            UI.SetCachedTint(ui, "StoryBackground", 0.2, 0.2, 0.2, 1.0)
        end
        return true
    end

    if prologue_state.mode == "crt_power_on" then
        prologue_state.elapsed = prologue_state.elapsed + delta
        local crt_progress = 1.0
        if prologue_state.crt_switch_delay_seconds > 0.0 then
            crt_progress = Math.Clamp01(prologue_state.elapsed / prologue_state.crt_switch_delay_seconds)
        end

        local brightness = 1.0
        if crt_progress < 0.12 then
            brightness = 0.0
        elseif crt_progress < 0.24 then
            brightness = 1.35
        elseif crt_progress < 0.38 then
            brightness = 0.18
        elseif crt_progress < 0.62 then
            brightness = 1.08
        elseif crt_progress < 0.82 then
            brightness = 0.82
        else
            brightness = 1.0
        end
        UI.SetCachedTint(ui, "StoryBackground", brightness, brightness, brightness, 1.0)

        if crt_progress >= 1.0 then
            UI.SetCachedTint(ui, "StoryBackground", 1.0, 1.0, 1.0, 1.0)
            prologue_state.mode = "post_crt_hold"
            prologue_state.elapsed = 0.0
        end
        return true
    end

    if prologue_state.mode == "post_crt_hold" then
        prologue_state.elapsed = prologue_state.elapsed + delta
        if prologue_state.elapsed >= prologue_state.post_crt_hold_seconds then
            start_story_sequence()
        end
        return true
    end

    return prologue_state.mode ~= "story"
end

------------------------------------------------
-- 페이지 진행 함수들
------------------------------------------------

apply_page = function(page)
    if type(page) ~= "table" then
        return
    end

    local background = resolve_page_background(page)
    local auto_page = is_auto_page(page)
    if background then
        if page.type == "dialogue" then
            set_background_immediately(page.background, background)
        else
            set_non_dialogue_background(background)
        end
    end

    if auto_page then
        reset_typewriter()
        clear_pending_dialogue_ui()
        auto_advance_timer = page.duration
        UI.SetCachedText(ui, "SpeakerName", "")
        UI.SetCachedText(ui, "DialogueText", "")
        UI.SetCachedText(ui, "NarrationText", "")
        UI.SetCachedText(ui, "PageHint", HOLD_SKIP_HINT_TEXT)
        UI.SetCachedVisible(ui, "SpeakerName", false)
        UI.SetCachedVisible(ui, "DialogueText", false)
        UI.SetCachedVisible(ui, "NarrationText", false)
        UI.SetCachedVisible(ui, "PageHint", true)
        return
    end

    auto_advance_timer = nil

    clear_pending_dialogue_ui()
    show_dialogue_ui(page)
end

local function advance_page()
    if #pages == 0 then
        return
    end

    if current_index >= #pages then
        play_story_sfx(resolve_story_sound("go", DEFAULT_GO_SOUND_PATH))
        hide_dialogue_ui()
        start_scene_change_flash()
        return
    end

    play_story_sfx(NEXT_PAGE_SOUND_PATH)

    local next_index = current_index + 1
    current_index = next_index
    apply_page(pages[current_index])
end

local function try_advance_page()
    if prologue_state.mode and prologue_state.mode ~= "story" then
        return try_advance_prologue()
    end

    if scene_change_timer then
        return false
    end

    if input_cooldown > 0.0 then
        return false
    end

    if typing_state.active then
        input_cooldown = INPUT_COOLDOWN_SECONDS
        complete_typewriter()
        return true
    end

    input_cooldown = INPUT_COOLDOWN_SECONDS
    advance_page()
    return true
end

local function load_story()
    scenario = ScenarioLoader.load(SCENARIO_PATH, load_json_file)
    if not scenario then
        warn("Failed to load story scenario:", SCENARIO_PATH)
        UI.SetCachedText(ui, "SpeakerName", "SYSTEM")
        UI.SetCachedText(ui, "DialogueText", "Scenario load failed.")
        UI.SetCachedText(ui, "PageHint", SCENARIO_PATH)
        return
    end

    pages = scenario.sequence or {}
    current_index = 1

    if #pages == 0 then
        UI.SetCachedText(ui, "SpeakerName", "SYSTEM")
        UI.SetCachedText(ui, "DialogueText", "No pages in scenario.")
        UI.SetCachedText(ui, "PageHint", SCENARIO_PATH)
        return
    end

    start_story_prologue()
end

------------------------------------------------
-- StoryScene 생명주기 함수들
------------------------------------------------

function BeginPlay()
    cache_component("StoryBackground")
    cache_component("StoryBackgroundBaek0")
    cache_component("StoryBackgroundBaek1")
    cache_component("StoryBackgroundLim0")
    cache_component("StoryBackgroundLim1")
    cache_component("StoryBackgroundPod")
    cache_component("StoryBackgroundBaekLim0")
    cache_component("StoryBackgroundBaekLim1")
    cache_component("SpeakerName")
    cache_component("DialogueText")
    cache_component("NarrationText")
    cache_component("PageHint")
    cache_component("WhiteFlash")
    cache_component("NextPageButton")

    preload_story_background_layers()
    UI.SetCachedVisible(ui, "StoryBackground", true)
    hide_dialogue_background_layers()
    hide_dialogue_ui()
    UI.SetCachedText(ui, "PageHint", HOLD_SKIP_HINT_TEXT)
    UI.SetCachedVisible(ui, "PageHint", true)
    UI.SetCachedVisible(ui, "WhiteFlash", false)
    UI.SetCachedTint(ui, "WhiteFlash", 1.0, 1.0, 1.0, 0.0)

    load_story()
end

-- 스토리 화면의 전체 진행 루프입니다.
-- skip hold, 프롤로그, typewriter, 자동 페이지, 씬 전환 flash, 입력 advance를 순서대로 처리합니다.
function Tick(dt)
    if story_finished then
        return
    end

    if update_skip_hold(dt) then
        return
    end

    if update_prologue(dt) then
        if input_cooldown > 0.0 then
            input_cooldown = math.max(0.0, input_cooldown - (dt or 0.0))
        end

        if prologue_state.mode == "line_reveal" then
            if GetKeyDown("SPACE") then
                try_advance_page()
                return
            end

            if ui["NextPageButton"] and ui["NextPageButton"]:WasClicked() then
                try_advance_page()
            end
        end
        return
    end

    if typing_state.active then
        typing_state.elapsed = typing_state.elapsed + (dt or 0.0)

        while typing_state.active
            and typing_state.visible_count < #typing_state.chars
            and typing_state.elapsed >= typing_state.interval do
            typing_state.elapsed = typing_state.elapsed - typing_state.interval
            typing_state.visible_count = typing_state.visible_count + 1

            local revealed_char = typing_state.chars[typing_state.visible_count]
            if (typing_state.visible_count % 3) == 0 and DialogueUtils.is_typing_sound_character(revealed_char) then
                play_story_sfx(TYPEWRITER_SOUND_PATH)
            end
        end

        update_typewriter_text()

        if typing_state.visible_count >= #typing_state.chars then
            typing_state.active = false
        end
    end

    if auto_advance_timer then
        auto_advance_timer = auto_advance_timer - (dt or 0.0)
        if auto_advance_timer <= 0.0 then
            auto_advance_timer = nil
            advance_page()
            return
        end
    end

    if scene_change_timer then
        scene_change_timer = scene_change_timer - (dt or 0.0)
        local progress = 1.0
        if scene_change_duration > 0.0 then
            progress = 1.0 - math.max(scene_change_timer, 0.0) / scene_change_duration
        end
        local alpha = ease_in_power(progress, 1.8)
        UI.SetCachedVisible(ui, "WhiteFlash", true)
        UI.SetCachedTint(ui, "WhiteFlash", 1.0, 1.0, 1.0, alpha)
        if scene_change_timer <= 0.0 then
            scene_change_timer = nil
            finish_story("PlayerDev.Scene")
            return
        end
    end

    if input_cooldown > 0.0 then
        input_cooldown = math.max(0.0, input_cooldown - (dt or 0.0))
    end

    if GetKeyDown("SPACE") then
        try_advance_page()
        return
    end

    if ui["NextPageButton"] and ui["NextPageButton"]:WasClicked() then
        try_advance_page()
    end
end
