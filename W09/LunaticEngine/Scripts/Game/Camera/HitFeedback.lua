local CameraConfig = require("Game.Config.Camera")

local HitFeedback = {}

local function get_config()
    return CameraConfig.hit_feedback or {}
end

-- Function : Play hit camera feedback through PlayerCameraManager modifier chain
-- input : playerController
-- playerController : Lua actor proxy that owns PlayCameraModifier
function HitFeedback.Play(playerController)
    if not playerController or not playerController.PlayCameraModifier then
        warn("[HitFeedback] PlayCameraModifier missing. Hit camera feedback skipped.")
        return false
    end

    local config = get_config()

    playerController:PlayCameraModifier(config.shake_script or "Game/Camera/HitCameraShake", {
        intensity = config.shake_intensity or 2.0,
        duration = config.shake_duration or 0.35,
        frequency = config.shake_frequency or 25.0,
    })

    playerController:PlayCameraModifier(config.post_process_script or "Game/Camera/DamagePostProcess", {
        intensity = config.post_process_intensity or 2.0,
        duration = config.post_process_duration or 0.7,
    })

    return true
end

return HitFeedback
