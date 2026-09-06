local DialogueConfig = require("Game.Config.Dialogue")
---------------------------
-- Player 조작 관련 Config값
---------------------------

local PlayerControllerConfig = {
    -- forward_speed: 플레이어가 매 프레임 자동 전진하는 속도
    forward_speed = 10.0,

    -- dream_billboard_offset_x: Dream.png billboard를 Player 앞 X+ 방향에 두는 초기 거리
    dream_billboard_offset_x = 50.0,

    -- max_move_step: overlap 누락을 줄이기 위해 전진 이동을 쪼개는 최대 거리
    max_move_step = 0.25,

    -- knockback_*: 장애물 충돌 시 Runner를 전진 반대 방향으로 밀어내는 거리
    knockback_enabled = false,
    knockback_distance = 1.5,

    -- lane_*: 레인 개수, 간격, 레인 이동 보간 속도
    lane_width = 4.0,
    lane_count = 3,
    lane_change_speed = 12.0,

    -- barrel_roll_*: 레인 변경 시 포드 롤 연출 설정값
    barrel_roll_enabled = true,
    barrel_roll_degrees = 360.0,
    barrel_roll_duration = 0.34,

    -- gravity / jump_power: 점프와 낙하에 쓰는 기본 물리값
    gravity = -25.0,
    jump_power = 10.0,

    -- fall_dead_z: 이 높이 아래로 떨어지면 낙사 처리
    fall_dead_z = -5.0,

    -- ground_* / skin_width: 바닥 감지와 snap 보정값
    ground_probe_distance = 5.0,
    ground_snap_distance = 0.25,
    skin_width = 0.05,
    fallback_half_height = 1.0,

    -- dialogue_data_path: 런타임 코치 대화 데이터를 읽는 경로
    dialogue_data_path = DialogueConfig.play_path,

    -- PlayerController가 코치 대화창을 직접 갱신할 때 쓰는 초상화 창
    coach_window_textures = {
        BAEK_COMMANDER = "Asset/Content/Texture/UI/window_portrait_baek_0.png",
        LIM_COMMANDER = "Asset/Content/Texture/UI/window_portrait_lim_0.png",
    },
}

return PlayerControllerConfig
