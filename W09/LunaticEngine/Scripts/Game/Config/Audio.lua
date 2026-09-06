---------------------------
-- Audio 설정값 모음 Config
---------------------------

local AudioConfig = {
    -- enabled: 게임플레이 오디오 재생 여부
    enabled = true,

    -- play_bgm: 게임플레이 시작 시 재생할 BGM 경로
    play_bgm = "Asset/Content/Sound/Background/gameplay.mp3",

    -- bgm_loop: 게임플레이 BGM 반복 재생 여부
    bgm_loop = true,

    -- stop_bgm_on_game_over: 게임 종료 시 BGM을 멈출지 여부
    stop_bgm_on_game_over = true,

    -- *_sfx: AudioManager shortcut에서 사용하는 게임 이벤트 SFX 경로
    -- TODO 채워야함
    hit_sfx = "",
    jump_sfx = "",
    slide_sfx = "",
    barrel_rolling_sfx = "Asset/Content/Sound/SFX/Barrel_rolling.mp3",
    game_over_sfx = "",
}

return AudioConfig
