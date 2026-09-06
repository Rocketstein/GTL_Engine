local CameraConfig = {
    -- relative_*: 플레이어 추적 카메라의 기본 상대 위치
    relative_x = -8.0,
    relative_y = 0.0,
    relative_z = 5.0,

    -- look_*: 카메라가 플레이어 앞쪽을 바라보는 보정값
    look_ahead = 5.0,
    look_height = 1.5,

    hit_feedback = {
        shake_script = "Game/Camera/HitCameraShake",
        shake_intensity = 2.0,
        shake_duration = 0.35,
        shake_frequency = 25.0,

        post_process_script = "Game/Camera/DamagePostProcess",
        post_process_intensity = 2.0,
        post_process_duration = 0.7,
        post_process_material = "Game/PostProcess/Damage",
    },
}

return CameraConfig
