local Context = require("FinalGameJamScript/Character/CharacterContext")
local Locomotion = require("FinalGameJamScript/Character/CharacterLocomotion")
local State = require("FinalGameJamScript/Character/CharacterState")
local HitReaction = require("FinalGameJamScript/Character/CharacterHitReaction")

local Guard = {}

local function load_guard_montages(ctx)
    if ctx.cache.defenseIdleMontage == nil then
        ctx.cache.defenseIdleMontage = Context.LoadMontage(ctx.config.DEFENSE_IDLE_MONTAGE_PATH)
    end
end

local function is_guard_pressed(ctx)
    return Input.GetKeyDown(ctx.config.RMB)
        or Input.GetKeyDown(ctx.config.GAMEPAD_GUARD)
end

local function is_guard_down(ctx)
    return Input.GetKey(ctx.config.RMB)
        or Input.GetKey(ctx.config.GAMEPAD_GUARD)
end

local function is_guard_released(ctx)
    local released = Input.GetKeyUp(ctx.config.RMB)
        or Input.GetKeyUp(ctx.config.GAMEPAD_GUARD)
    return released and not is_guard_down(ctx)
end

function Guard.StopDefense(ctx)
    if not ctx.state.defensePlaying then
        return
    end

    Context.StopCurrentMontage(ctx)
    ctx.state.defensePlaying = false
    ctx.state.defenseIdlePlaying = false
    ctx.state.counterWindowOpen = false
    Locomotion.Unlock(ctx)
end

function Guard.PlayDefenseIdle(ctx)
    if ctx.state.defenseIdlePlaying then
        return
    end

    load_guard_montages(ctx)

    local anim = Context.GetAnimInstance(ctx)
    if anim == nil or ctx.cache.defenseIdleMontage == nil then
        State.ResetGuard(ctx)
        return
    end

    Context.StopCurrentMontage(ctx)
    State.ResetAttack(ctx)

    ctx.state.defensePlaying = true
    ctx.state.defenseIdlePlaying = true
    ctx.state.counterWindowOpen = false

    Locomotion.Lock(ctx)
    anim:PlayMontage(ctx.cache.defenseIdleMontage)
end

function Guard.ExecuteInput(ctx)
    if ctx.state.attackPlaying or ctx.state.defensePlaying or ctx.state.counterPlaying then
        return
    end

    if ctx.state.hitPlaying then
        if not ctx.state.canCancelHit then
            return
        end

        --EnableAttack Notify opens guard cancel from hit--
        HitReaction.StopForCancel(ctx)
    end

    if ctx.state.playerDefeated then
        return
    end

    Guard.PlayDefenseIdle(ctx)
end

function Guard.UpdateCounterWindow(ctx)
    if not ctx.state.defensePlaying then
        ctx.state.counterWindowOpen = false
        return
    end

    ctx.state.counterWindowOpen = Context.IsCounterWindowActive(ctx)
end

Guard.UpdateParryWindow = Guard.UpdateCounterWindow

function Guard.UpdateDefenseSequence(ctx)
    if not ctx.state.defensePlaying then
        return
    end

    if is_guard_released(ctx) then
        Guard.StopDefense(ctx)
        return
    end

    local anim = Context.GetAnimInstance(ctx)
    if anim == nil then
        State.ResetGuard(ctx)
        Locomotion.Unlock(ctx)
        return
    end

    if ctx.state.defenseIdlePlaying
        and ctx.cache.defenseIdleMontage ~= nil
        and not anim:IsMontagePlaying(ctx.cache.defenseIdleMontage) then
        if is_guard_down(ctx) then
            ctx.state.defenseIdlePlaying = false
            Guard.PlayDefenseIdle(ctx)
            return
        end

        Guard.StopDefense(ctx)
    end
end

function Guard.BeginPlay(ctx)
    load_guard_montages(ctx)
end

function Guard.Tick(ctx, dt)
    Guard.UpdateCounterWindow(ctx)
    Guard.UpdateDefenseSequence(ctx)
end

function Guard.IsInputTriggered(ctx)
    return is_guard_pressed(ctx)
end

return Guard
