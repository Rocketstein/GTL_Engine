local DialogueConfig = require("Game.Config.Dialogue")

---------------------------
-- Play 도중 필요한 Sound 및 각종 계수값 모음
---------------------------

local PlayerDevHUDConfig = {
    -- dialogue_path: 플레이 중 HUD 대화 데이터 경로입니다.
    dialogue_path = DialogueConfig.play_path,

    -- dialogue_sound_path: 대사가 출력될 때 재생할 기본 타자음입니다.
    dialogue_sound_path = "Asset/Content/Sound/SFX/dialogue.mp3",

    -- event_sound_by_token: dialogue JSON의 sound token을 실제 SFX 경로로 변경
    event_sound_by_token = {
        agents_are_go = "Asset/Content/Sound/SFX/agents-are-go.mp3",
        pickup_log = "Asset/Content/Sound/SFX/story_next.mp3",
        pickup_dump = "Asset/Content/Sound/SFX/glitch_noise.wav",
        hit_error = "Asset/Content/Sound/SFX/arwing-hit-obstacle.mp3",
        alarm_warning = "Asset/Content/Sound/SFX/windows-98-error.mp3",
        hotfix_ready = "Asset/Content/Sound/SFX/go.mp3",
        hotfix_apply = "Asset/Content/Sound/SFX/crt-on.mp3",
        critical_analysis = "Asset/Content/Sound/SFX/glitch_noise.wav",
        speed_up = "Asset/Content/Sound/SFX/fired-sound-effect.mp3",
        shield_block = "Asset/Content/Sound/SFX/story_next.mp3",
        game_over = "Asset/Content/Sound/SFX/windows-98-error.mp3",
    },

    -- default_event_sound_by_trigger: sound token이 없을 때 trigger별로 재생할 기본 SFX입니다.
    default_event_sound_by_trigger = {
        onRunStart = "Asset/Content/Sound/SFX/agents-are-go.mp3",
        onCollectLog = "Asset/Content/Sound/SFX/story_next.mp3",
        onCollectCrashDump = "Asset/Content/Sound/SFX/glitch_noise.wav",
        onHitObstacle = "Asset/Content/Sound/SFX/arwing-hit-obstacle.mp3",
        onShieldBlocked = "Asset/Content/Sound/SFX/story_next.mp3",
        onStabilityLow = "Asset/Content/Sound/SFX/windows-98-error.mp3",
        onHotfixReady = "Asset/Content/Sound/SFX/go.mp3",
        onApplyHotfix = "Asset/Content/Sound/SFX/crt-on.mp3",
        onCriticalAnalysis = "Asset/Content/Sound/SFX/glitch_noise.wav",
        onSpeedUp = "Asset/Content/Sound/SFX/fired-sound-effect.mp3",
        onGameOver = "Asset/Content/Sound/SFX/windows-98-error.mp3",
    },

    -- dialogue_timing: typewriter, 초상화 입 모양, 대사 유지 시간 설정입니다.
    dialogue_timing = {
        typewriter_interval_seconds = 0.028,
        portrait_animation_interval_seconds = 0.12,
        hold_seconds = 1.8,
        max_line_width_units = 16,
        continuation_indent = "",
    },

    -- window_textures: 코치 초상화 대화창의 닫힘/열림 프레임입니다.
    window_textures = {
        BAEK_COMMANDER = {
            closed = "Asset/Content/Texture/UI/window_portrait_baek_0.png",
            open = "Asset/Content/Texture/UI/window_portrait_baek_1.png",
        },
        LIM_COMMANDER = {
            closed = "Asset/Content/Texture/UI/window_portrait_lim_0.png",
            open = "Asset/Content/Texture/UI/window_portrait_lim_1.png",
        },
    },

    -- approval_gauge / rank_marker: 코치 평가 게이지와 랭크 마커 배치입니다.
    approval_gauge = {
        x = 1240.0,
        y = 490.0,
        width = 316.0,
        height = 18.0,
    },
    rank_marker = {
        width = 110.0,
        height = 48.0,
        offset_y = -42.0,
    },
    panel_layout = {
        width = 707.0,
        height = 320.0,
    },

    -- hud_layout / dialogue_layout: PlayerDev HUD 텍스트와 대화창 위치입니다.
    hud_layout = {
        title = { x = 1202.0, y = 56.0 },
        core_title = { x = 1240.0, y = 118.0 },
        core_text = { x = 1240.0, y = 149.0 },
        details_title = { x = 1510.0, y = 118.0 },
        details_text = { x = 1510.0, y = 149.0 },
        approval_label = { x = 1510.0, y = 200.0 },
        approval_value = { x = 1768.0, y = 200.0 },
    },
    dialogue_layout = {
        window = { x = 32.0, y = 32.0 },
        speaker = { x = 292.0, y = 74.0 },
        message = { x = 292.0, y = 124.0 },
    },
}

return PlayerDevHUDConfig
