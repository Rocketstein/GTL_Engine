local Context = require("FinalGameJamScript/Character/CharacterContext")

local ExecutionCamera = {}

local function camera_log(ctx, message)
    if ctx ~= nil and ctx.config ~= nil and ctx.config.EXECUTION_CAMERA_DEBUG_LOGS then
        print("[ExecutionCamera] " .. message)
    end
end

local function is_valid_actor(actor)
    if actor == nil then
        return false
    end

    if actor.IsValid ~= nil then
        return actor:IsValid()
    end

    return true
end

local function find_camera_actor(ctx)
    if ctx.state.executionCameraActor ~= nil and is_valid_actor(ctx.state.executionCameraActor) then
        return ctx.state.executionCameraActor
    end

    if World ~= nil and World.FindFirstActorByTag ~= nil then
        local actor = World.FindFirstActorByTag(ctx.config.EXECUTION_CAMERA_TAG)
        if actor ~= nil then
            ctx.state.executionCameraActor = actor
            return actor
        end
    end

    if World ~= nil and World.FindActorsByTag ~= nil then
        local actors = World.FindActorsByTag(ctx.config.EXECUTION_CAMERA_TAG)
        if actors ~= nil and #actors > 0 then
            ctx.state.executionCameraActor = actors[1]
            return actors[1]
        end
    end

    return nil
end

local function get_camera_component(actor)
    if actor == nil then
        return nil
    end

    if actor.GetCamera ~= nil then
        return actor:GetCamera()
    end

    return Context.Call(actor, "GetCamera")
end

local function get_camera_manager()
    if World == nil or World.GetFirstPlayerController == nil then
        return nil
    end

    local controller = World.GetFirstPlayerController()
    if controller == nil or controller.GetPlayerCameraManager == nil then
        return nil
    end

    return controller:GetPlayerCameraManager()
end

local function set_camera_location(camera, location)
    if camera == nil or location == nil then
        return
    end

    if camera.SetLocation ~= nil then
        camera:SetLocation(location)
    else
        camera.Location = location
    end
end

local function look_at(camera, target)
    if camera ~= nil and camera.LookAt ~= nil and target ~= nil then
        camera:LookAt(target)
    end
end

local function set_camera_fov(camera, fov)
    if camera ~= nil and camera.SetFOV ~= nil and fov ~= nil then
        camera:SetFOV(fov)
    end
end

local function play_transition_slomo(ctx)
    if not ctx.config.EXECUTION_CAMERA_SLOMO_ENABLED then
        return
    end

    local action = Context.GetActionComponent(ctx)
    if action == nil or action.Slomo == nil then
        camera_log(ctx, "[Slomo] : skipped, ActionComponent or Slomo binding missing")
        return
    end

    action:Slomo(
        ctx.config.EXECUTION_CAMERA_SLOMO_DURATION,
        ctx.config.EXECUTION_CAMERA_SLOMO_TIME_DILATION
    )
    camera_log(ctx, "[Slomo] : transition complete")
end

local function set_letterbox(camera, ctx, enabled)
    if camera == nil or camera.SetLetterbox == nil then
        return
    end

    if enabled then
        camera:SetLetterbox(
            true,
            ctx.config.EXECUTION_CAMERA_LETTERBOX_AMOUNT,
            ctx.config.EXECUTION_CAMERA_LETTERBOX_THICKNESS
        )
    else
        camera:SetLetterbox(false)
    end
end

local function save_post_process_state(ctx)
    local manager = get_camera_manager()
    if manager == nil then
        ctx.state.executionPreviousDof = nil
        ctx.state.executionPreviousVignette = nil
        return
    end

    ctx.state.executionPreviousDof = {
        enabled = manager.IsDepthOfFieldEnabled ~= nil and manager:IsDepthOfFieldEnabled() or false,
        focusDistance = manager.GetDoFFocusDistance ~= nil and manager:GetDoFFocusDistance() or 0.0,
        focusRange = manager.GetDoFFocusRange ~= nil and manager:GetDoFFocusRange() or 0.0,
        maxBlur = manager.GetDoFMaxBlurRadius ~= nil and manager:GetDoFMaxBlurRadius() or 0.0,
        bokehRadiusThreshold = manager.GetDoFBokehRadiusThreshold ~= nil and manager:GetDoFBokehRadiusThreshold() or 0.0,
        bokehLumaThreshold = manager.GetDoFBokehLumaThreshold ~= nil and manager:GetDoFBokehLumaThreshold() or 0.0,
        bokehIntensity = manager.GetDoFBokehIntensity ~= nil and manager:GetDoFBokehIntensity() or 0.0,
    }
    ctx.state.executionPreviousVignette = {
        enabled = manager.IsVignetteEnabled ~= nil and manager:IsVignetteEnabled() or false,
        intensity = manager.GetVignetteIntensity ~= nil and manager:GetVignetteIntensity() or 0.0,
        radius = manager.GetVignetteRadius ~= nil and manager:GetVignetteRadius() or 0.0,
        softness = manager.GetVignetteSoftness ~= nil and manager:GetVignetteSoftness() or 0.0,
    }
end

local function set_post_process(ctx, cameraLocation, lookAtLocation)
    local manager = get_camera_manager()
    if manager == nil and CameraManager == nil then
        return
    end

    if ctx.config.EXECUTION_CAMERA_DOF_ENABLED then
        local focusDistance = ctx.config.EXECUTION_CAMERA_DOF_FOCUS_DISTANCE
        if focusDistance == nil or focusDistance <= 0.0 then
            focusDistance = (lookAtLocation - cameraLocation):Length()
        end
        if manager ~= nil and manager.SetDepthOfField ~= nil then
            manager:SetDepthOfField(
                focusDistance,
                ctx.config.EXECUTION_CAMERA_DOF_FOCUS_RANGE,
                ctx.config.EXECUTION_CAMERA_DOF_MAX_BLUR
            )
        elseif CameraManager ~= nil and CameraManager.SetDepthOfField ~= nil then
            CameraManager.SetDepthOfField(
                focusDistance,
                ctx.config.EXECUTION_CAMERA_DOF_FOCUS_RANGE,
                ctx.config.EXECUTION_CAMERA_DOF_MAX_BLUR
            )
        end
    end

    if ctx.config.EXECUTION_CAMERA_BOKEH_ENABLED then
        if manager ~= nil and manager.SetBokeh ~= nil then
            manager:SetBokeh(
                ctx.config.EXECUTION_CAMERA_BOKEH_RADIUS_THRESHOLD,
                ctx.config.EXECUTION_CAMERA_BOKEH_LUMA_THRESHOLD,
                ctx.config.EXECUTION_CAMERA_BOKEH_INTENSITY
            )
        elseif CameraManager ~= nil and CameraManager.SetBokeh ~= nil then
            CameraManager.SetBokeh(
                ctx.config.EXECUTION_CAMERA_BOKEH_RADIUS_THRESHOLD,
                ctx.config.EXECUTION_CAMERA_BOKEH_LUMA_THRESHOLD,
                ctx.config.EXECUTION_CAMERA_BOKEH_INTENSITY
            )
        end
    end

    if ctx.config.EXECUTION_CAMERA_VIGNETTE_ENABLED then
        if manager ~= nil and manager.SetCameraVignette ~= nil then
            manager:SetCameraVignette(
                ctx.config.EXECUTION_CAMERA_VIGNETTE_INTENSITY,
                ctx.config.EXECUTION_CAMERA_VIGNETTE_RADIUS,
                ctx.config.EXECUTION_CAMERA_VIGNETTE_SOFTNESS
            )
        elseif CameraManager ~= nil and CameraManager.SetVignette ~= nil then
            CameraManager.SetVignette(
                ctx.config.EXECUTION_CAMERA_VIGNETTE_INTENSITY,
                ctx.config.EXECUTION_CAMERA_VIGNETTE_RADIUS,
                ctx.config.EXECUTION_CAMERA_VIGNETTE_SOFTNESS
            )
        end
    end
end

local function restore_post_process(ctx)
    local manager = get_camera_manager()
    if manager == nil and CameraManager == nil then
        return
    end

    local dof = ctx.state.executionPreviousDof
    if dof ~= nil then
        if dof.enabled then
            if manager ~= nil and manager.SetDepthOfField ~= nil then
                manager:SetDepthOfField(dof.focusDistance, dof.focusRange, dof.maxBlur)
                if manager.SetBokeh ~= nil then
                    manager:SetBokeh(dof.bokehRadiusThreshold, dof.bokehLumaThreshold, dof.bokehIntensity)
                end
            elseif CameraManager ~= nil and CameraManager.SetDepthOfField ~= nil then
                CameraManager.SetDepthOfField(dof.focusDistance, dof.focusRange, dof.maxBlur)
                if CameraManager.SetBokeh ~= nil then
                    CameraManager.SetBokeh(dof.bokehRadiusThreshold, dof.bokehLumaThreshold, dof.bokehIntensity)
                end
            end
        elseif manager ~= nil and manager.ClearDepthOfField ~= nil then
            manager:ClearDepthOfField()
        elseif CameraManager ~= nil and CameraManager.ClearDepthOfField ~= nil then
            CameraManager.ClearDepthOfField()
        end
    end

    local vignette = ctx.state.executionPreviousVignette
    if vignette ~= nil then
        if vignette.enabled then
            if manager ~= nil and manager.SetCameraVignette ~= nil then
                manager:SetCameraVignette(vignette.intensity, vignette.radius, vignette.softness)
            elseif CameraManager ~= nil and CameraManager.SetVignette ~= nil then
                CameraManager.SetVignette(vignette.intensity, vignette.radius, vignette.softness)
            end
        elseif manager ~= nil and manager.ClearCameraVignette ~= nil then
            manager:ClearCameraVignette()
        elseif CameraManager ~= nil and CameraManager.ClearVignette ~= nil then
            CameraManager.ClearVignette()
        end
    end

    ctx.state.executionPreviousDof = nil
    ctx.state.executionPreviousVignette = nil
end

local function calculate_camera_pose(ctx, boss)
    if ctx.obj == nil or boss == nil or ctx.obj.Location == nil or boss.Location == nil then
        return nil, nil
    end

    local playerLocation = ctx.obj.Location
    local bossLocation = boss.Location
    local center = (playerLocation + bossLocation) * 0.5
    local toBoss = Context.NormalizeFlatDirection(bossLocation - playerLocation)
    if toBoss == nil then
        toBoss = Context.NormalizeFlatDirection(ctx.obj.Forward)
    end
    if toBoss == nil then
        return nil, nil
    end

    local sideSign = ctx.config.EXECUTION_CAMERA_SIDE_SIGN or 1.0
    local side = Vec3(-toBoss.Y, toBoss.X, 0.0) * sideSign
    local cameraLocation = center
        + side * (ctx.config.EXECUTION_CAMERA_SIDE_OFFSET or 2.2)
        - toBoss * (ctx.config.EXECUTION_CAMERA_BACK_OFFSET or 1.0)
        + Vec3(0.0, 0.0, ctx.config.EXECUTION_CAMERA_HEIGHT or 1.1)
    local lookAtLocation = center + Vec3(0.0, 0.0, ctx.config.EXECUTION_CAMERA_LOOK_HEIGHT or 0.8)

    return cameraLocation, lookAtLocation
end

function ExecutionCamera.Update(ctx, boss, dt)
    if not ctx.state.executionCameraActive then
        return
    end

    if not ctx.state.executionCameraSlomoPlayed then
        ctx.state.executionCameraSlomoRemaining = ctx.state.executionCameraSlomoRemaining - (dt or 0.0)
        if ctx.state.executionCameraSlomoRemaining <= 0.0 then
            ctx.state.executionCameraSlomoPlayed = true
            play_transition_slomo(ctx)
        end
    end

    local camera = ctx.state.executionCameraComponent
    if camera == nil then
        return
    end

    local cameraLocation, lookAtLocation = calculate_camera_pose(ctx, boss or ctx.state.executionTarget)
    if cameraLocation == nil or lookAtLocation == nil then
        return
    end

    set_camera_location(camera, cameraLocation)
    look_at(camera, lookAtLocation)
    set_camera_fov(camera, ctx.config.EXECUTION_CAMERA_FOV)
    set_post_process(ctx, cameraLocation, lookAtLocation)
end

function ExecutionCamera.Begin(ctx, boss)
    if not ctx.config.EXECUTION_CAMERA_ENABLED then
        return false
    end

    if CameraManager == nil then
        camera_log(ctx, "disabled: CameraManager missing")
        return false
    end

    local actor = find_camera_actor(ctx)
    local camera = get_camera_component(actor)
    if actor == nil or camera == nil then
        camera_log(ctx, "disabled: place a camera actor with tag " .. tostring(ctx.config.EXECUTION_CAMERA_TAG))
        return false
    end

    ctx.state.executionPreviousCamera = CameraManager.GetActiveCamera ~= nil and CameraManager.GetActiveCamera() or nil
    local manager = get_camera_manager()
    ctx.state.executionPreviousViewTarget = manager ~= nil and manager.GetViewTarget ~= nil and manager:GetViewTarget() or nil
    if ctx.state.executionPreviousViewTarget == nil and CameraManager.GetActiveCameraOwner ~= nil then
        ctx.state.executionPreviousViewTarget = CameraManager.GetActiveCameraOwner()
    end
    if ctx.state.executionPreviousViewTarget == nil and CameraManager.GetPossessedCameraOwner ~= nil then
        ctx.state.executionPreviousViewTarget = CameraManager.GetPossessedCameraOwner()
    end
    ctx.state.executionCameraActor = actor
    ctx.state.executionCameraComponent = camera
    ctx.state.executionCameraActive = true
    ctx.state.executionCameraSlomoRemaining =
        (ctx.config.EXECUTION_CAMERA_BLEND_IN or 0.0)
        + (ctx.config.EXECUTION_CAMERA_SLOMO_DELAY or 0.0)
    ctx.state.executionCameraSlomoPlayed = not ctx.config.EXECUTION_CAMERA_SLOMO_ENABLED

    save_post_process_state(ctx)
    ExecutionCamera.Update(ctx, boss, 0.0)
    set_letterbox(camera, ctx, true)
    if CameraManager.SetViewTargetWithBlend ~= nil then
        CameraManager.SetViewTargetWithBlend(actor, ctx.config.EXECUTION_CAMERA_BLEND_IN or 0.0)
    end
    if CameraManager.SetActiveCameraWithBlend ~= nil then
        CameraManager.SetActiveCameraWithBlend(camera, ctx.config.EXECUTION_CAMERA_BLEND_IN or 0.0)
    elseif CameraManager.PossessCamera ~= nil then
        CameraManager.PossessCamera(camera)
    end

    camera_log(ctx, "started")
    return true
end

function ExecutionCamera.End(ctx)
    if not ctx.state.executionCameraActive then
        return
    end

    local camera = ctx.state.executionCameraComponent
    set_letterbox(camera, ctx, false)
    restore_post_process(ctx)

    if CameraManager ~= nil then
        local previousTarget = ctx.state.executionPreviousViewTarget
        if previousTarget ~= nil and CameraManager.SetViewTargetWithBlend ~= nil then
            CameraManager.SetViewTargetWithBlend(previousTarget, ctx.config.EXECUTION_CAMERA_BLEND_OUT or 0.0)
        end

        local previous = ctx.state.executionPreviousCamera
        if previous ~= nil and CameraManager.SetActiveCameraWithBlend ~= nil then
            CameraManager.SetActiveCameraWithBlend(previous, ctx.config.EXECUTION_CAMERA_BLEND_OUT or 0.0)
        end
    end

    ctx.state.executionCameraActive = false
    ctx.state.executionCameraComponent = nil
    ctx.state.executionPreviousCamera = nil
    ctx.state.executionPreviousViewTarget = nil
    ctx.state.executionCameraSlomoRemaining = 0.0
    ctx.state.executionCameraSlomoPlayed = false
    camera_log(ctx, "ended")
end

return ExecutionCamera
