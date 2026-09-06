local Context = require("FinalGameJamScript/Character/CharacterContext")
local Equipment = require("FinalGameJamScript/Character/CharacterEquipment")
local Locomotion = require("FinalGameJamScript/Character/CharacterLocomotion")
local State = require("FinalGameJamScript/Character/CharacterState")
local HitReaction = require("FinalGameJamScript/Character/CharacterHitReaction")
local Counter = require("FinalGameJamScript/Character/CharacterCounter")

local Combat = {}

local function load_attack_montages(ctx)
    if #ctx.cache.attackMontages > 0 then
        return
    end

    for i, path in ipairs(ctx.config.ATTACK_MONTAGE_PATHS) do
        ctx.cache.attackMontages[i] = Context.LoadMontage(path)
    end
end

local function is_attack_pressed(ctx)
    return Input.GetKeyDown(ctx.config.LMB)
        or Input.GetKeyDown(ctx.config.GAMEPAD_ATTACK)
end

function Combat.PlayAttack(ctx, index)
    local anim = Context.GetAnimInstance(ctx)
    local montage = ctx.cache.attackMontages[index]
    if anim == nil or montage == nil then
        State.ResetAttack(ctx)
        return
    end

    --If Valid Attack--
    ctx.state.currentAttackIndex = index
    ctx.state.attackPlaying = true
    ctx.state.canChainAttack = false
    ctx.state.attackInputQueued = false
    Context.FaceAttackDirection(ctx)
    Locomotion.Lock(ctx)
    --Clear old EnableAttack flag--
    Context.ConsumeEnableAttack(ctx)
    Equipment.ActivateTrail(ctx)
    anim:PlayMontage(montage, nil, ctx.config.ATTACK_PLAY_RATE)
end

function Combat.StopAttackForMovement(ctx)
    if not ctx.state.attackPlaying or not ctx.state.canChainAttack or not Locomotion.IsMoveKeyDown(ctx) then
        return
    end

    local anim = Context.GetAnimInstance(ctx)
    if anim ~= nil and anim.StopMontage ~= nil then
        --Stop montage immediately and return to locomotion pose--
        anim:StopMontage(0.0)
    end

    State.ResetAttack(ctx)
    Locomotion.Unlock(ctx)
    Context.ConsumeEnableAttack(ctx)
end

function Combat.TryPlayNextAttack(ctx)
    if not ctx.state.attackPlaying or not ctx.state.canChainAttack then
        return false
    end

    local nextIndex = ctx.state.currentAttackIndex + 1
    if nextIndex <= #ctx.config.ATTACK_MONTAGE_PATHS then
        Combat.PlayAttack(ctx, nextIndex)
        return true
    end

    return false
end

function Combat.TryPlayQueuedAttack(ctx)
    if not ctx.state.attackInputQueued or not ctx.state.attackPlaying or not ctx.state.canChainAttack then
        return false
    end

    --Use buffered attack before movement cancel--
    ctx.state.attackInputQueued = false
    return Combat.TryPlayNextAttack(ctx)
end

function Combat.StartAttackSequence(ctx)
    --fill in attackMontages--
    load_attack_montages(ctx)
    --start attack--
    Combat.PlayAttack(ctx, 1)
end

function Combat.ExecuteInput(ctx)
    if ctx.state.defensePlaying then
        return
    end

    if ctx.state.counterPlaying then
        if not ctx.state.canCancelCounter then
            return
        end

        Counter.StopForCancel(ctx)
    end

    if ctx.state.hitPlaying then
        if not ctx.state.canCancelHit then
            return
        end

        HitReaction.StopForCancel(ctx)
    end

    if ctx.state.attackPlaying then
        if not ctx.state.canChainAttack then
            --Queue combo input until EnableAttack Notify opens--
            ctx.state.attackInputQueued = true
            return
        end

        Combat.TryPlayNextAttack(ctx)
        return
    end

    Combat.StartAttackSequence(ctx)
end

function Combat.UpdateAttackChainWindow(ctx)
    if not ctx.state.attackPlaying then
        return
    end

    if Context.ConsumeEnableAttack(ctx) then
        ctx.state.canChainAttack = true
    end
end

function Combat.UpdateAttackSequence(ctx)
    if not ctx.state.attackPlaying then
        return
    end

    local anim = Context.GetAnimInstance(ctx)
    local montage = ctx.cache.attackMontages[ctx.state.currentAttackIndex]
    if anim == nil or montage == nil then
        State.ResetAttack(ctx)
        Locomotion.Unlock(ctx)
        return
    end

    if anim:IsMontagePlaying(montage) then
        return
    end

    State.ResetAttack(ctx)
    Locomotion.Unlock(ctx)
    Context.ConsumeEnableAttack(ctx)
end

function Combat.BeginPlay(ctx)
    load_attack_montages(ctx)
end

function Combat.Tick(ctx, dt)
    Combat.UpdateAttackChainWindow(ctx)
    Combat.TryPlayQueuedAttack(ctx)
    Combat.StopAttackForMovement(ctx)
    Combat.UpdateAttackSequence(ctx)
end

function Combat.IsInputTriggered(ctx)
    return is_attack_pressed(ctx)
end

return Combat
