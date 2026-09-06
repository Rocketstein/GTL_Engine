local Config = require("FinalGameJamScript/Character/CharacterConfig")

local Context = {}

local function atan2_degrees(y, x)
    if math.atan2 ~= nil then
        return math.deg(math.atan2(y, x))
    end

    if x > 0.0 then
        return math.deg(math.atan(y / x))
    end

    if x < 0.0 and y >= 0.0 then
        return math.deg(math.atan(y / x)) + 180.0
    end

    if x < 0.0 and y < 0.0 then
        return math.deg(math.atan(y / x)) - 180.0
    end

    if y > 0.0 then
        return 90.0
    end

    if y < 0.0 then
        return -90.0
    end

    return 0.0
end

local function call_function(target, functionName, ...)
    if target == nil then
        return nil
    end

    local directFunction = target[functionName]
    if directFunction ~= nil then
        return directFunction(target, ...)
    end

    if target.CallFunction ~= nil then
        return target:CallFunction(functionName, ...)
    end

    return nil
end

function Context.Create(owner)
    return {
        obj = owner,
        config = Config,
        state = {
            currentAttackIndex = 0,
            attackPlaying = false,
            canChainAttack = false,
            attackInputQueued = false,
            defensePlaying = false,
            defenseIdlePlaying = false,
            counterPlaying = false,
            canCancelCounter = false,
            hitPlaying = false,
            currentHitMontage = nil,
            canCancelHit = false,
            playerDead = false,
            playerDefeated = false,
            deathMontagePlaying = false,
            counterWindowOpen = false,
            movementLocked = false,
            counterParticleTimeRemaining = 0.0,
            counterMove = nil,
            counterCollision = nil,
            counterInvincibleApplied = false,
            counterPreviousInvincible = false,
            counterInputBufferRemaining = 0.0,
            executionPlaying = false,
            executionTarget = nil,
            executionTargetMontage = nil,
            executionCameraActive = false,
            executionCameraActor = nil,
            executionCameraComponent = nil,
            executionPreviousCamera = nil,
            executionPreviousViewTarget = nil,
            executionPreviousDof = nil,
            executionPreviousVignette = nil,
            executionCameraSlomoRemaining = 0.0,
            executionCameraSlomoPlayed = false,
            attackHitCameraZoomActive = false,
            attackHitCameraZoomElapsed = 0.0,
            attackHitCamera = nil,
            attackHitCameraBaseFov = nil,
            attackHitCameraZoomFov = nil,
        },
        cache = {
            animInstance = nil,
            movement = nil,
            actionComponent = nil,
            healthComponent = nil,
            combatStateComponent = nil,
            attackMontages = {},
            hitMontages = {},
            postureBreakMontage = nil,
            defenseIdleMontage = nil,
            deathMontage = nil,
            counterMontage = nil,
            counterImpactParticle = nil,
            successParryMontage = nil,
            executionPlayerMontage = nil,
            executionBossMontage = nil,
            lockOnComponent = nil,
        },
        equipment = {
            RweaponComponent = nil,
            LweaponComponent = nil,
            RtrailParticle = nil,
            LtrailParticle = nil,
            trailActive = false,
            trailEmissionActive = false,
            trailDeactivateRemaining = 0.0,
            weaponBindingWarned = false,
            trailBindingWarned = false,
        },
    }
end

function Context.Call(target, functionName, ...)
    return call_function(target, functionName, ...)
end

function Context.GetAnimInstance(ctx)
    if ctx.cache.animInstance ~= nil then
        if ctx.cache.animInstance.IsValid == nil or ctx.cache.animInstance:IsValid() then
            return ctx.cache.animInstance
        end
        ctx.cache.animInstance = nil
    end

    if ctx.obj == nil or ctx.obj.GetSkeletalMeshComponent == nil then
        return ctx.cache.animInstance
    end

    local mesh = ctx.obj:GetSkeletalMeshComponent()
    if mesh == nil then
        return nil
    end

    ctx.cache.animInstance = mesh:GetAnimInstance()
    return ctx.cache.animInstance
end

function Context.GetCharacterMovement(ctx)
    if ctx.cache.movement ~= nil then
        return ctx.cache.movement
    end

    local owner = ctx.obj
    if owner == nil then
        return nil
    end

    --obj is bound as Actor, so ACharacter functions can be hidden--
    if owner.GetCharacterMovement ~= nil then
        ctx.cache.movement = owner:GetCharacterMovement()
        return ctx.cache.movement
    end

    --Call reflected ACharacter function when obj's lua type is Actor--
    if owner.IsA ~= nil and owner.CallFunction ~= nil and owner:IsA("ACharacter") then
        ctx.cache.movement = owner:CallFunction("GetCharacterMovement")
        return ctx.cache.movement
    end

    return nil
end

function Context.GetActionComponent(ctx)
    if ctx.cache.actionComponent ~= nil then
        return ctx.cache.actionComponent
    end

    local owner = ctx.obj
    if owner == nil or owner.GetActionComponent == nil then
        return nil
    end

    ctx.cache.actionComponent = owner:GetActionComponent()
    return ctx.cache.actionComponent
end

function Context.GetHealthComponent(ctx)
    if ctx.cache.healthComponent ~= nil then
        return ctx.cache.healthComponent
    end

    local owner = ctx.obj
    if owner == nil or owner.GetHealthComponent == nil then
        return nil
    end

    ctx.cache.healthComponent = owner:GetHealthComponent()
    return ctx.cache.healthComponent
end

function Context.GetCombatStateComponent(ctx)
    if ctx.cache.combatStateComponent ~= nil then
        return ctx.cache.combatStateComponent
    end

    ctx.cache.combatStateComponent = Context.Call(ctx.obj, "GetCombatStateComponent")
    return ctx.cache.combatStateComponent
end

function Context.StopCurrentMontage(ctx)
    local anim = Context.GetAnimInstance(ctx)
    if anim ~= nil and anim.StopMontage ~= nil then
        --Stop montage immediately--
        anim:StopMontage(0.0)
    end
end

function Context.ConsumeEnableAttack(ctx)
    local anim = Context.GetAnimInstance(ctx)
    if anim == nil or Animation == nil or Animation.ConsumeEnableAttack == nil then
        return false
    end

    return Animation.ConsumeEnableAttack(anim)
end

function Context.IsParryWindowActive(ctx)
    local anim = Context.GetAnimInstance(ctx)
    if anim == nil or Animation == nil or Animation.IsParryWindowActive == nil then
        return false
    end

    return Animation.IsParryWindowActive(anim)
end

function Context.IsCounterWindowActive(ctx)
    return Context.IsParryWindowActive(ctx)
end

function Context.IsCounterInputWindowActive(ctx)
    local anim = Context.GetAnimInstance(ctx)
    if anim == nil or Animation == nil or Animation.IsCounterInputWindowActive == nil then
        return false
    end

    return Animation.IsCounterInputWindowActive(anim)
end

function Context.OpenCounterInputDeflect(ctx)
    local combat = Context.GetCombatStateComponent(ctx)
    if combat == nil or combat.OpenDeflectWindow == nil then
        return false
    end

    combat:OpenDeflectWindow(ctx.config.COUNTER_INPUT_DEFLECT_WINDOW_SECONDS or 0.12)
    return true
end

function Context.ConsumeSuccessfulParry(ctx)
    local anim = Context.GetAnimInstance(ctx)
    if anim == nil or Animation == nil or Animation.ConsumeSuccessfulParry == nil then
        return false
    end

    return Animation.ConsumeSuccessfulParry(anim)
end

function Context.ConsumeCounterOpportunity(ctx)
    local anim = Context.GetAnimInstance(ctx)
    if anim ~= nil and Animation ~= nil and Animation.ConsumeCounterOpportunity ~= nil then
        local opportunity = Animation.ConsumeCounterOpportunity(anim)
        if opportunity == nil then
            return nil, nil
        end

        local target = opportunity.attacker
        if target == nil then
            target = true
        end
        return target, opportunity.hitLocation
    end

    if anim ~= nil and Animation ~= nil and Animation.ConsumeCounterOpportunityAttacker ~= nil then
        return Animation.ConsumeCounterOpportunityAttacker(anim), nil
    end

    if Context.ConsumeSuccessfulParry(ctx) then
        return true, nil
    end

    return nil, nil
end

function Context.LoadMontage(path)
    if Animation == nil or Animation.LoadMontage == nil then
        return nil
    end

    return Animation.LoadMontage(path)
end

function Context.GetActiveCamera()
    if CameraManager == nil then
        return nil
    end

    if CameraManager.GetPossessedCamera ~= nil then
        local possessedCamera = CameraManager.GetPossessedCamera()
        if possessedCamera ~= nil then
            return possessedCamera
        end
    end

    if CameraManager.GetActiveCamera ~= nil then
        return CameraManager.GetActiveCamera()
    end

    return nil
end

function Context.PlayCameraShakeAsset(path, scale)
    if path == nil or path == "" or CameraManager == nil or CameraManager.StartCameraShakeAsset == nil then
        return false
    end

    CameraManager.StartCameraShakeAsset(path, scale or 1.0)
    return true
end

-- Procedural Perlin shake (no asset). params is the optional override table the
-- Camera.Shake binding accepts (duration/blendIn/locAmplitude/rotAmplitude/...).
function Context.PlayCameraShake(scale, params)
    if Camera == nil or Camera.Shake == nil then
        return false
    end

    Camera.Shake(scale or 1.0, params)
    return true
end

function Context.GetCameraFlatForward()
    local camera = Context.GetActiveCamera()
    if camera == nil or camera.Forward == nil then
        return nil
    end

    local forward = camera.Forward
    forward.Z = 0.0

    if forward:Length() <= 0.0001 then
        return nil
    end

    return forward:Normalized()
end

function Context.GetCameraFlatRight()
    local camera = Context.GetActiveCamera()
    if camera == nil or camera.Right == nil then
        return nil
    end

    local right = camera.Right
    right.Z = 0.0

    if right:Length() <= 0.0001 then
        return nil
    end

    return right:Normalized()
end

local function get_actor_flat_forward(ctx)
    if ctx.obj == nil or ctx.obj.Forward == nil then
        return nil
    end

    return Context.NormalizeFlatDirection(ctx.obj.Forward)
end

local function get_actor_flat_right(ctx)
    if ctx.obj == nil or ctx.obj.Right == nil then
        return nil
    end

    return Context.NormalizeFlatDirection(ctx.obj.Right)
end

local function get_input_axis(axisName)
    if Input == nil or Input.GetAxis == nil or axisName == nil then
        return 0.0
    end

    return tonumber(Input.GetAxis(axisName)) or 0.0
end

function Context.GetMoveInputDirection(ctx)
    if Input == nil then
        return nil
    end

    local forward = Context.GetCameraFlatForward() or get_actor_flat_forward(ctx)
    local right = Context.GetCameraFlatRight() or get_actor_flat_right(ctx)
    if forward == nil or right == nil then
        return nil
    end

    local forwardScale = 0.0
    local rightScale = 0.0

    if Input.GetKey(ctx.config.MOVE_FORWARD_KEY) then
        forwardScale = forwardScale + 1.0
    end

    if Input.GetKey(ctx.config.MOVE_BACK_KEY) then
        forwardScale = forwardScale - 1.0
    end

    if Input.GetKey(ctx.config.MOVE_RIGHT_KEY) then
        rightScale = rightScale + 1.0
    end

    if Input.GetKey(ctx.config.MOVE_LEFT_KEY) then
        rightScale = rightScale - 1.0
    end

    forwardScale = forwardScale + get_input_axis(ctx.config.GAMEPAD_MOVE_Y_AXIS)
    rightScale = rightScale + get_input_axis(ctx.config.GAMEPAD_MOVE_X_AXIS)

    if forwardScale == 0.0 and rightScale == 0.0 then
        return nil
    end

    --Use camera-relative move input as attack direction--
    return Context.NormalizeFlatDirection(forward * forwardScale + right * rightScale)
end

function Context.FaceDirection(ctx, direction)
    if ctx.obj == nil then
        return
    end

    local faceDirection = Context.NormalizeFlatDirection(direction)
    if faceDirection == nil then
        return
    end

    local yaw = atan2_degrees(faceDirection.Y, faceDirection.X)
    --FVector rotation uses X=Roll, Y=Pitch, Z=Yaw--
    ctx.obj.Rotation = Vec3(0.0, 0.0, yaw)

    --RootMotion combo should start from the selected attack yaw--
    if ctx.obj.GetController ~= nil then
        local controller = ctx.obj:GetController()
        if controller ~= nil and controller.GetControlRotation ~= nil and controller.SetControlRotation ~= nil then
            local controlRotation = controller:GetControlRotation()
            if controlRotation ~= nil then
                controller:SetControlRotation(Vec3(controlRotation.X or 0.0, controlRotation.Y or 0.0, yaw))
            else
                controller:SetControlRotation(Vec3(0.0, 0.0, yaw))
            end
        end
    end
end

function Context.FaceCameraForward(ctx)
    Context.FaceDirection(ctx, Context.GetCameraFlatForward())
end

function Context.FaceAttackDirection(ctx)
    local moveDirection = Context.GetMoveInputDirection(ctx)
    if moveDirection ~= nil then
        Context.FaceDirection(ctx, moveDirection)
        return
    end

    Context.FaceCameraForward(ctx)
end

function Context.NormalizeFlatDirection(direction)
    if direction == nil then
        return nil
    end

    direction.Z = 0.0
    if direction:Length() <= 0.0001 then
        return nil
    end

    return direction:Normalized()
end

return Context
