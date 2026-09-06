local CameraManager = {
    ManagerComponent = nil
}

local function getManagerComponent()
    local component = CameraManager.ManagerComponent
    if component == nil then
        return nil
    end

    if type(component.InitializeFor) ~= "function" then
        return nil
    end

    return component
end

function CameraManager.Register(component)
    CameraManager.ManagerComponent = component
end

function CameraManager.Unregister(component)
    if component == nil or CameraManager.ManagerComponent == component then
        CameraManager.ManagerComponent = nil
    end
end

function CameraManager.InitializeFor(ownerActor)
    local component = getManagerComponent()
    if component == nil or ownerActor == nil then
        return false
    end

    return component.InitializeFor(ownerActor) == true
end

function CameraManager.SetViewTarget(targetActor)
    local component = getManagerComponent()
    if component == nil or targetActor == nil then
        return false
    end

    return component.SetViewTarget(targetActor) == true
end

function CameraManager.SetViewTargetBlend(targetActor, blendTime, blendFunction, blendExp, lockOutgoing)
    local component = getManagerComponent()
    if component == nil or targetActor == nil then
        return false
    end

    return component.SetViewTargetBlend(
        targetActor,
        tonumber(blendTime) or 0.0,
        blendFunction or "Cubic",
        tonumber(blendExp) or 2.0,
        lockOutgoing == true
    ) == true
end

function CameraManager.SetGameCameraCutThisFrame()
    local component = getManagerComponent()
    if component == nil or type(component.SetGameCameraCutThisFrame) ~= "function" then
        return
    end

    component.SetGameCameraCutThisFrame()
end

function CameraManager.GetCameraLocation()
    local component = getManagerComponent()
    if component == nil or type(component.GetCameraLocation) ~= "function" then
        return nil
    end

    return component.GetCameraLocation()
end

function CameraManager.GetCameraRotation()
    local component = getManagerComponent()
    if component == nil or type(component.GetCameraRotation) ~= "function" then
        return nil
    end

    return component.GetCameraRotation()
end

function CameraManager.HasValidCameraCache()
    local component = getManagerComponent()
    if component == nil or type(component.HasValidCameraCache) ~= "function" then
        return false
    end

    return component.HasValidCameraCache() == true
end

return CameraManager
