local Context = require("FinalGameJamScript/Character/CharacterContext")

local LockOn = {}
local SWITCH_AXIS_TRIGGER = 0.75
local SWITCH_AXIS_RELEASE = 0.35
local SWITCH_AXIS_DELTA = 100.0

local function get_lock_on_component(ctx)
    if ctx.cache.lockOnComponent ~= nil then
        return ctx.cache.lockOnComponent
    end

    local owner = ctx.obj
    if owner == nil then
        return nil
    end

    if owner.GetLockOnComponent ~= nil then
        ctx.cache.lockOnComponent = owner:GetLockOnComponent()
        return ctx.cache.lockOnComponent
    end

    ctx.cache.lockOnComponent = Context.Call(owner, "GetLockOnComponent")
    return ctx.cache.lockOnComponent
end

local function is_lock_on_pressed(ctx)
    return Input.GetKeyDown(ctx.config.LOCK_ON)
        or Input.GetKeyDown(ctx.config.GAMEPAD_LOCK_ON)
end

function LockOn.BeginPlay(ctx)
    get_lock_on_component(ctx)
end

function LockOn.ExecuteInput(ctx)
    local lockOn = get_lock_on_component(ctx)
    if lockOn == nil then
        return
    end

    Context.Call(lockOn, "ToggleLockOn")
end

function LockOn.UpdateSwitchInput(ctx)
    local lockOn = get_lock_on_component(ctx)
    if lockOn == nil then
        return
    end

    local mouseDeltaX = Input.GetMouseDeltaX()
    if mouseDeltaX ~= nil and mouseDeltaX ~= 0.0 then
        Context.Call(lockOn, "AddSwitchInput", mouseDeltaX)
    end

    if Input.GetAxis == nil then
        return
    end

    local axisX = tonumber(Input.GetAxis(ctx.config.GAMEPAD_LOOK_X_AXIS)) or 0.0
    if math.abs(axisX) <= SWITCH_AXIS_RELEASE then
        ctx.state.lockOnSwitchAxisEngaged = false
        return
    end

    if ctx.state.lockOnSwitchAxisEngaged then
        return
    end

    if axisX >= SWITCH_AXIS_TRIGGER then
        ctx.state.lockOnSwitchAxisEngaged = true
        Context.Call(lockOn, "AddSwitchInput", SWITCH_AXIS_DELTA)
    elseif axisX <= -SWITCH_AXIS_TRIGGER then
        ctx.state.lockOnSwitchAxisEngaged = true
        Context.Call(lockOn, "AddSwitchInput", -SWITCH_AXIS_DELTA)
    end
end

function LockOn.IsInputTriggered(ctx)
    return is_lock_on_pressed(ctx)
end

return LockOn
