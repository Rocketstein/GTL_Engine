local Context = require("FinalGameJamScript/Character/CharacterContext")

local AttackCamera = {}

local function is_valid_actor(actor)
    if actor == nil then
        return false
    end

    if actor.IsValid ~= nil then
        return actor:IsValid()
    end

    return true
end

local function get_camera_fov(camera)
    if camera == nil or camera.GetFOV == nil then
        return nil
    end

    return camera:GetFOV()
end

local function set_camera_fov(camera, fov)
    if camera ~= nil and camera.SetFOV ~= nil and fov ~= nil then
        camera:SetFOV(fov)
    end
end

local function clamp_fov(fov)
    return math.max(0.1, math.min(3.14, fov))
end

local function calculate_zoom_fov(baseFov, zoomFactor)
    local factor = zoomFactor or 1.0
    if factor <= 0.0 then
        factor = 1.0
    end

    return clamp_fov(2.0 * math.atan(math.tan(baseFov * 0.5) / factor))
end

local function lerp(a, b, t)
    return a + (b - a) * math.max(0.0, math.min(1.0, t))
end

local function reset(ctx)
    ctx.state.attackHitCameraZoomActive = false
    ctx.state.attackHitCameraZoomElapsed = 0.0
    ctx.state.attackHitCamera = nil
    ctx.state.attackHitCameraBaseFov = nil
    ctx.state.attackHitCameraZoomFov = nil
end

function AttackCamera.Restore(ctx)
    if ctx == nil or not ctx.state.attackHitCameraZoomActive then
        return
    end

    set_camera_fov(ctx.state.attackHitCamera, ctx.state.attackHitCameraBaseFov)
    reset(ctx)
end

local function play_hit_zoom(ctx, target, attacker, damageSpec, zoomFactor)
    if ctx == nil then
        return
    end

    if ctx.state.attackHitCameraZoomActive then
        return
    end

    if ctx.state.executionPlaying or ctx.state.executionCameraActive then
        return
    end

    if attacker ~= nil and attacker ~= ctx.obj then
        return
    end

    if not is_valid_actor(target) then
        return
    end

    if damageSpec ~= nil and damageSpec.Damage ~= nil and damageSpec.Damage <= 0.0 then
        return
    end

    local camera = Context.GetActiveCamera()
    local currentFov = get_camera_fov(camera)
    if currentFov == nil then
        return
    end

    ctx.state.attackHitCameraZoomActive = true
    ctx.state.attackHitCameraZoomElapsed = 0.0
    ctx.state.attackHitCamera = camera
    ctx.state.attackHitCameraBaseFov = ctx.state.attackHitCameraBaseFov or currentFov
    ctx.state.attackHitCameraZoomFov = calculate_zoom_fov(
        ctx.state.attackHitCameraBaseFov,
        zoomFactor
    )
end

function AttackCamera.OnNormalAttackHit(ctx, target, attacker, hitComponent, damageSpec, hitEventName)
    if ctx == nil or not ctx.config.ATTACK_HIT_CAMERA_ZOOM_ENABLED then
        return
    end

    if not ctx.state.attackPlaying then
        return
    end

    if ctx.state.counterPlaying then
        return
    end

    play_hit_zoom(ctx, target, attacker, damageSpec, ctx.config.ATTACK_HIT_CAMERA_ZOOM_FACTOR)
end

function AttackCamera.OnCounterHit(ctx, target, attacker, hitComponent, damageSpec, hitEventName)
    if ctx == nil or not ctx.config.COUNTER_HIT_CAMERA_ZOOM_ENABLED then
        return
    end

    if not ctx.state.counterPlaying then
        return
    end

    play_hit_zoom(ctx, target, attacker, damageSpec, ctx.config.COUNTER_HIT_CAMERA_ZOOM_FACTOR)
end

function AttackCamera.Update(ctx, dt)
    if ctx == nil or not ctx.state.attackHitCameraZoomActive then
        return
    end

    if ctx.state.executionCameraActive then
        AttackCamera.Restore(ctx)
        return
    end

    local camera = ctx.state.attackHitCamera
    local baseFov = ctx.state.attackHitCameraBaseFov
    local zoomFov = ctx.state.attackHitCameraZoomFov
    if camera == nil or baseFov == nil or zoomFov == nil then
        reset(ctx)
        return
    end

    local zoomIn = math.max(0.0, ctx.config.ATTACK_HIT_CAMERA_ZOOM_IN_TIME or 0.0)
    local hold = math.max(0.0, ctx.config.ATTACK_HIT_CAMERA_ZOOM_HOLD_TIME or 0.0)
    local zoomOut = math.max(0.0, ctx.config.ATTACK_HIT_CAMERA_ZOOM_OUT_TIME or 0.0)
    local total = zoomIn + hold + zoomOut
    if total <= 0.0 then
        AttackCamera.Restore(ctx)
        return
    end

    ctx.state.attackHitCameraZoomElapsed = ctx.state.attackHitCameraZoomElapsed + (dt or 0.0)
    local elapsed = ctx.state.attackHitCameraZoomElapsed
    local fov = baseFov

    if zoomIn > 0.0 and elapsed < zoomIn then
        fov = lerp(baseFov, zoomFov, elapsed / zoomIn)
    elseif elapsed < zoomIn + hold then
        fov = zoomFov
    elseif zoomOut > 0.0 and elapsed < total then
        fov = lerp(zoomFov, baseFov, (elapsed - zoomIn - hold) / zoomOut)
    else
        AttackCamera.Restore(ctx)
        return
    end

    set_camera_fov(camera, fov)
end

return AttackCamera
