---------------------------
-- Story Scene에서 필요한 resource 모음
---------------------------

local StorySceneConfig = {
    -- scenario_path / next_scene: 스토리 데이터 경로와 완료 후 이동할 씬입니다.
    scenario_path = "Asset/Content/Data/Scenarios/story.json",
    next_scene = "PlayerDev.Scene",

    -- scene_change_duration: 페이지 전환 fade 시간입니다.
    scene_change_duration = 0.18,

    -- overlay_components: 배경 이미지 위에 겹쳐 켜고 끄는 UI 컴포넌트 이름입니다.
    overlay_components = {
        control_room_baek_0 = "StoryBackgroundBaek0",
        control_room_baek_1 = "StoryBackgroundBaek1",
        control_room_lim_0 = "StoryBackgroundLim0",
        control_room_lim_1 = "StoryBackgroundLim1",
        control_room_pod = "StoryBackgroundPod",
        control_room_baek_lim_0 = "StoryBackgroundBaekLim0",
        control_room_baek_lim_1 = "StoryBackgroundBaekLim1",
    },

    -- prologue: 프롤로그 자동 전환과 CRT 연출 타이밍입니다.
    prologue = {
        intro_fade_seconds = 3.0,
        intro_hold_seconds = 1.5,
        outro_fade_seconds = 0.9,
        control_room_fade_seconds = 0.9,
        crt_lead_seconds = 1.0,
        crt_switch_delay_seconds = 0.2,
        post_crt_hold_seconds = 1.2,
    },

    -- text: 스토리 대사 출력, 색상, 입력 안내 설정입니다.
    text = {
        max_line_width_units = 28,
        speaker_name_color = { 134.0 / 255.0, 251.0 / 255.0, 255.0 / 255.0, 233.0 / 255.0 },
        dialogue_text_color = { 235.0 / 255.0, 247.0 / 255.0, 255.0 / 255.0, 209.0 / 255.0 },
        prologue_text_color = { 235.0 / 255.0, 247.0 / 255.0, 255.0 / 255.0, 0.94 },
        prologue_hint_color = { 0.72, 0.84, 1.0, 0.88 },
        prologue_dim_background = 0.32,
        input_cooldown_seconds = 0.18,
        dialogue_transition_duration = 0.3,
        typewriter_interval_seconds = 0.028,
        terminal_prompt = ">_ ",
        hold_skip_seconds = 2.0,
        hold_skip_hint_text = "Hold Space to Skip...",
    },

    -- sounds: 스토리 진행 중 사용할 fallback SFX 경로입니다.
    sounds = {
        next_page = "Asset/Content/Sound/SFX/story_next.mp3",
        typewriter = "Asset/Content/Sound/SFX/crt-type.wav",
        boot = "Asset/Content/Sound/SFX/windows-98-startup.mp3",
        crt_on = "Asset/Content/Sound/SFX/crt-on.mp3",
        go = "Sound.SFX.go",
    },
}

return StorySceneConfig
