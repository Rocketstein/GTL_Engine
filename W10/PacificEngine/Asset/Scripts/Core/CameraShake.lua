local CameraShake = {}

CameraShake.Assets = {
    Explosion = "CameraShakes/Explosion.shake",
    VehicleRush = "CameraShakes/VehicleRush.shake",
    TrainRush = "CameraShakes/TrainRush.shake",
}

function CameraShake.Play(assetOrPath, scale)
    if PlayCameraShake == nil then
        return false
    end

    local path = CameraShake.Assets[assetOrPath] or assetOrPath
    if path == nil or path == "" then
        return false
    end

    return PlayCameraShake(path, scale or 1.0)
end

return CameraShake
