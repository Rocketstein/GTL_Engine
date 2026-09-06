local Config = {}

Config.LMB = "LeftMouseButton"
Config.RMB = "RightMouseButton"
Config.LOCK_ON = "MiddleMouseButton"
Config.EXECUTION_KEY = "E"
Config.GAMEPAD_ATTACK = "GamepadRightShoulder"
Config.GAMEPAD_GUARD = "GamepadLeftShoulder"
Config.GAMEPAD_LOCK_ON = "GamepadRightThumbstick"
Config.GAMEPAD_EXECUTION = "GamepadRT"
Config.GAMEPAD_REVIVE = "GamepadA"
Config.GAMEPAD_MOVE_X_AXIS = "GamepadLeftX"
Config.GAMEPAD_MOVE_Y_AXIS = "GamepadLeftY"
Config.GAMEPAD_LOOK_X_AXIS = "GamepadRightX"
Config.MOVE_FORWARD_KEY = "W"
Config.MOVE_LEFT_KEY = "A"
Config.MOVE_BACK_KEY = "S"
Config.MOVE_RIGHT_KEY = "D"
Config.MOVE_KEYS = {
    Config.MOVE_FORWARD_KEY,
    Config.MOVE_LEFT_KEY,
    Config.MOVE_BACK_KEY,
    Config.MOVE_RIGHT_KEY,
}

Config.WEAPON_MESH_PATH = "Content/Data/Weapon/JapanSword/New_Katana_StaticMesh.uasset"
Config.RWEAPON_SOCKET = "RH_Socket"
Config.LWEAPON_SOCKET = "LH_Socket"

Config.GROUND_SPEED_VAR = "GroundSpeed"

Config.ATTACK_PLAY_RATE = 1.7
Config.ATTACK_MONTAGE_PATHS = {
    "Content/Montages/Twin_Attack1_Montage.uasset",
    "Content/Montages/Twin_Attack2_Montage.uasset",
    "Content/Montages/Twin_Attack3_Montage.uasset",
    "Content/Montages/Twin_Attack4_Montage.uasset",
}
Config.ATTACK_HIT_CAMERA_ZOOM_ENABLED = true
Config.ATTACK_HIT_CAMERA_ZOOM_FACTOR = 1.1
Config.ATTACK_HIT_CAMERA_ZOOM_IN_TIME = 0.03
Config.ATTACK_HIT_CAMERA_ZOOM_HOLD_TIME = 0.04
Config.ATTACK_HIT_CAMERA_ZOOM_OUT_TIME = 0.12
Config.DEFENSE_IDLE_MONTAGE_PATH = "Content/Montages/Twin_Defense_Montage.uasset"
Config.DEATH_MONTAGE_PATH = "Content/Montages/Twin_Death_Montage.uasset"
Config.COUNTER_MONTAGE_PATH = "Content/Montages/Twin_Cross_Montage.uasset"
Config.COUNTER_SECTION = "Cross"
Config.COUNTER_CAMERA_SHAKE_PATH = "Content/Data/FGJ_Character/CameraWork/CrossShake.uasset"
Config.COUNTER_CAMERA_SHAKE_SCALE = 1.0

-- Subtle Sekiro-style camera jolt when the Wolf takes a hit. Procedural Perlin
-- shake via Camera.Shake (no asset). Short + low amplitude = "little" by intent;
-- raise scale or the amplitudes for a heavier hit.
Config.HIT_CAMERA_SHAKE_ENABLED = true
Config.HIT_CAMERA_SHAKE_SCALE = 1.0
Config.HIT_CAMERA_SHAKE_PARAMS = {
    duration     = 0.18,
    blendIn      = 0.01,
    blendOut     = 0.09,
    locAmplitude = { x = 1.2, y = 1.2, z = 0.8 },
    locFrequency = { x = 18.0, y = 20.0, z = 15.0 },
    rotAmplitude = { pitch = 0.25, yaw = 0.35, roll = 0.45 },
    rotFrequency = { pitch = 16.0, yaw = 18.0, roll = 14.0 },
    fovAmplitude = 0.0,
}
Config.COUNTER_HIT_CAMERA_ZOOM_ENABLED = true
Config.COUNTER_HIT_CAMERA_ZOOM_FACTOR = 1.3
Config.COUNTER_PARTICLE_ENABLED = false
Config.COUNTER_PARTICLE_PATH = "Content/Particle System/Parry.uasset"
Config.COUNTER_PARTICLE_DURATION = 1.5
Config.RCOUNTER_PARTICLE_SOCKET = "RH_Particle"
Config.LCOUNTER_PARTICLE_SOCKET = "LH_Particle"
Config.SWORD_TRAIL_PARTICLE_PATH = "Content/Particle System/SwordTrail.uasset"
Config.RSWORD_TRAIL_PARTICLE_SOCKET = "RH_Particle"
Config.LSWORD_TRAIL_PARTICLE_SOCKET = "LH_Particle"
Config.SWORD_TRAIL_GRACE_TIME = 0.35
Config.COUNTER_IMPACT_PARTICLE_SCALE = 0.05
Config.COUNTER_PARRY_SOUND_KEY = "Counter_Parry1"
Config.COUNTER_PARRY_SOUND_PATH = "Parry1.wav"
Config.COUNTER_PARRY_SOUND_VOLUME = 0.6
Config.COUNTER_DEBUG_LOGS = false
Config.COUNTER_REPOSITION_ENABLED = true
Config.COUNTER_REPOSITION_DISTANCE = 2.0
Config.COUNTER_REPOSITION_DURATION = 0.18
Config.COUNTER_PAWN_OVERLAP_RESPONSE = 1
Config.COUNTER_SLOMO_DURATION = 0.35
Config.COUNTER_SLOMO_TIME_DILATION = 0.25
Config.COUNTER_INPUT_BUFFER_SECONDS = 0.2
Config.COUNTER_INPUT_DEFLECT_WINDOW_SECONDS = 0.25

Config.EXECUTION_PLAYER_MONTAGE_PATH = "Content/Montages/Twin_Execute_Montage.uasset"
Config.EXECUTION_BOSS_MONTAGE_PATH = "Content/Montages/ExecutedA_Montage.uasset"
Config.EXECUTION_BOSS_CLASSES = {
    "BossEnemyCharacter",
    "ABossEnemyCharacter",
}
Config.EXECUTION_BOSS_TAG = "Boss"
Config.EXECUTION_MAX_DISTANCE = 5.0
Config.EXECUTION_ALIGN_ENABLED = true
Config.EXECUTION_ALIGN_BACK_OFFSET = 1.0
Config.EXECUTION_PLAY_RATE = 1.0
Config.EXECUTION_DEBUG_LOGS = true
Config.EXECUTION_GAMEPAD_RUMBLE_ENABLED = true
Config.EXECUTION_GAMEPAD_RUMBLE_LEFT = 0.65
Config.EXECUTION_GAMEPAD_RUMBLE_RIGHT = 1.0
Config.EXECUTION_GAMEPAD_RUMBLE_DURATION = 0.35
Config.EXECUTION_CAMERA_ENABLED = true
Config.EXECUTION_CAMERA_TAG = "ExecutionCamera"
Config.EXECUTION_CAMERA_DEBUG_LOGS = true
Config.EXECUTION_CAMERA_BLEND_IN = 0.15
Config.EXECUTION_CAMERA_BLEND_OUT = 0.25
Config.EXECUTION_CAMERA_SLOMO_ENABLED = true
Config.EXECUTION_CAMERA_SLOMO_DELAY = 0.2
Config.EXECUTION_CAMERA_SLOMO_DURATION = 0.18
Config.EXECUTION_CAMERA_SLOMO_TIME_DILATION = 0.35
Config.EXECUTION_CAMERA_SIDE_OFFSET = 2.8
Config.EXECUTION_CAMERA_BACK_OFFSET = 10
Config.EXECUTION_CAMERA_HEIGHT = 0.0
Config.EXECUTION_CAMERA_LOOK_HEIGHT = 0.8
Config.EXECUTION_CAMERA_SIDE_SIGN = 1.0
Config.EXECUTION_CAMERA_FOV = 0.55
Config.EXECUTION_CAMERA_LETTERBOX_AMOUNT = 1.0
Config.EXECUTION_CAMERA_LETTERBOX_THICKNESS = 0.12
Config.EXECUTION_CAMERA_DOF_ENABLED = true
Config.EXECUTION_CAMERA_DOF_FOCUS_DISTANCE = 0.0
Config.EXECUTION_CAMERA_DOF_FOCUS_RANGE = 1.8
Config.EXECUTION_CAMERA_DOF_MAX_BLUR = 0.35
Config.EXECUTION_CAMERA_BOKEH_ENABLED = false
Config.EXECUTION_CAMERA_BOKEH_RADIUS_THRESHOLD = 0.2
Config.EXECUTION_CAMERA_BOKEH_LUMA_THRESHOLD = 0.7
Config.EXECUTION_CAMERA_BOKEH_INTENSITY = 0.4
Config.EXECUTION_CAMERA_VIGNETTE_ENABLED = true
Config.EXECUTION_CAMERA_VIGNETTE_INTENSITY = 0.35
Config.EXECUTION_CAMERA_VIGNETTE_RADIUS = 0.65
Config.EXECUTION_CAMERA_VIGNETTE_SOFTNESS = 0.35

Config.HIT_MONTAGE_PATHS = {
    F = "Content/Montages/Twin_Hit_F_Montage.uasset",
    B = "Content/Montages/Twin_Hit_B_Montage.uasset",
    L = "Content/Montages/Twin_Hit_L_Montage.uasset",
    R = "Content/Montages/Twin_Hit_R_Montage.uasset",
}

Config.POSTURE_BREAK_MONTAGE_PATH = "Content/Montages/Twin_TakeDown2_Montage.uasset"
Config.POSTURE_BREAK_SOUND_KEY = "Groggy"
Config.POSTURE_BREAK_SOUND_PATH = "Groggy.wav"
Config.POSTURE_BREAK_SOUND_VOLUME = 1.0
Config.POSTURE_BREAK_DAMAGE = 35.0

return Config
