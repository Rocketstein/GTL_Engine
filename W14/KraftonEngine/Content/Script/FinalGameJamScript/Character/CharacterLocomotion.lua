local Context = require("FinalGameJamScript/Character/CharacterContext")

local Locomotion = {}
local MOVE_AXIS_THRESHOLD = 0.1

local function call_movement(ctx, functionName, ...)
    local movement = Context.GetCharacterMovement(ctx)
    if movement == nil then
        return nil
    end

    return Context.Call(movement, functionName, ...)
end

function Locomotion.SetMovementInputEnabled(ctx, enabled)
    call_movement(ctx, "SetMovementInputEnabled", enabled)
end

function Locomotion.StopHorizontalMovement(ctx)
    call_movement(ctx, "ClearInputVector")
    call_movement(ctx, "StopHorizontalMovementImmediately")
end

function Locomotion.Lock(ctx)
    ctx.state.movementLocked = true
    Locomotion.SetMovementInputEnabled(ctx, false)
    Locomotion.StopHorizontalMovement(ctx)
end

function Locomotion.Unlock(ctx)
    if not ctx.state.movementLocked then
        return
    end

    ctx.state.movementLocked = false
    Locomotion.SetMovementInputEnabled(ctx, true)
end

function Locomotion.IsMoveKeyDown(ctx)
    for _, key in ipairs(ctx.config.MOVE_KEYS) do
        if Input.GetKey(key) then
            return true
        end
    end

    if Input.GetAxis ~= nil then
        local moveX = tonumber(Input.GetAxis(ctx.config.GAMEPAD_MOVE_X_AXIS)) or 0.0
        local moveY = tonumber(Input.GetAxis(ctx.config.GAMEPAD_MOVE_Y_AXIS)) or 0.0
        if math.abs(moveX) > MOVE_AXIS_THRESHOLD or math.abs(moveY) > MOVE_AXIS_THRESHOLD then
            return true
        end
    end

    return false
end

function Locomotion.Configure(ctx)
    call_movement(ctx, "SetStopImmediatelyWhenNoInput", true)
end

local function get_movement_velocity(movement)
    if movement == nil then
        return nil
    end

    --Use direct binding when movement is bound as CharacterMovementComponent--
    if movement.GetVelocity ~= nil then
        return movement:GetVelocity()
    end

    --Use reflected function when movement is bound as Object/Component--
    if movement.CallFunction ~= nil then
        return movement:CallFunction("GetVelocityValue")
    end

    return nil
end

function Locomotion.UpdateGroundSpeed(ctx)
    local groundSpeed = 0.0

    local movement = Context.GetCharacterMovement(ctx)
    if movement ~= nil then
        local velocity = get_movement_velocity(movement)
        if velocity ~= nil then
            groundSpeed = math.sqrt(velocity.X * velocity.X + velocity.Y * velocity.Y)
        end
    end

    local anim = Context.GetAnimInstance(ctx)
    if anim ~= nil and anim.SetGraphVariableFloat ~= nil then
        anim:SetGraphVariableFloat(ctx.config.GROUND_SPEED_VAR, groundSpeed)
    end
end

function Locomotion.UpdateMovementLock(ctx)
    if ctx.state.attackPlaying
        or ctx.state.defensePlaying
        or ctx.state.executionPlaying
        or (ctx.state.counterPlaying and not ctx.state.canCancelCounter)
        or (ctx.state.hitPlaying and not ctx.state.canCancelHit) then
        Locomotion.Lock(ctx)
        return
    end

    Locomotion.Unlock(ctx)

    --Stop immediately when all movement keys are released--
    if not Locomotion.IsMoveKeyDown(ctx) then
        Locomotion.StopHorizontalMovement(ctx)
    end
end

function Locomotion.BeginPlay(ctx)
    Locomotion.Configure(ctx)
end

function Locomotion.Tick(ctx, dt)
    Locomotion.UpdateMovementLock(ctx)
    Locomotion.UpdateGroundSpeed(ctx)
end

return Locomotion
