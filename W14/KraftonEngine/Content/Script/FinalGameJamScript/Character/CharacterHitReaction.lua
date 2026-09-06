local Context = require("FinalGameJamScript/Character/CharacterContext")
local Locomotion = require("FinalGameJamScript/Character/CharacterLocomotion")
local State = require("FinalGameJamScript/Character/CharacterState")
local Counter = require("FinalGameJamScript/Character/CharacterCounter")

local HitReaction = {}

local function load_hit_montages(ctx)
    for key, path in pairs(ctx.config.HIT_MONTAGE_PATHS) do
        if ctx.cache.hitMontages[key] == nil then
            ctx.cache.hitMontages[key] = Context.LoadMontage(path)
        end
    end
end

local function get_damage_source_direction(ctx, damageSpec)
    if damageSpec == nil or ctx.obj == nil or ctx.obj.Location == nil then
        return nil
    end

    --Use attacker position first--
    if damageSpec.DamageCauser ~= nil and damageSpec.DamageCauser.Location ~= nil then
        local sourceDirection = damageSpec.DamageCauser.Location - ctx.obj.Location
        local flat = Context.NormalizeFlatDirection(sourceDirection)
        if flat ~= nil then
            return flat
        end
    end

    --Fallback to instigator position--
    if damageSpec.InstigatorActor ~= nil and damageSpec.InstigatorActor.Location ~= nil then
        local sourceDirection = damageSpec.InstigatorActor.Location - ctx.obj.Location
        return Context.NormalizeFlatDirection(sourceDirection)
    end

    --Fallback to attack travel direction--
    if damageSpec.HitDirection ~= nil then
        local sourceDirection = damageSpec.HitDirection * -1.0
        return Context.NormalizeFlatDirection(sourceDirection)
    end

    return nil
end

local function choose_hit_direction_key(ctx, damageSpec)
    local sourceDirection = get_damage_source_direction(ctx, damageSpec)
    if sourceDirection == nil or ctx.obj == nil or ctx.obj.Forward == nil or ctx.obj.Right == nil then
        return "F"
    end

    local forward = Context.NormalizeFlatDirection(ctx.obj.Forward)
    local right = Context.NormalizeFlatDirection(ctx.obj.Right)
    if forward == nil or right == nil then
        return "F"
    end

    local forwardDot = sourceDirection:Dot(forward)
    local rightDot = sourceDirection:Dot(right)
    if math.abs(forwardDot) >= math.abs(rightDot) then
        if forwardDot >= 0.0 then
            return "F"
        end
        return "B"
    end

    if rightDot >= 0.0 then
        return "L"
    end
    return "R"
end

function HitReaction.StopForCancel(ctx)
    if not ctx.state.hitPlaying or not ctx.state.canCancelHit then
        return false
    end

    Context.StopCurrentMontage(ctx)
    State.ResetHit(ctx)
    Locomotion.Unlock(ctx)
    return true
end

function HitReaction.Play(ctx, damageSpec)
    load_hit_montages(ctx)

    local anim = Context.GetAnimInstance(ctx)
    if anim == nil then
        return
    end

    local key = choose_hit_direction_key(ctx, damageSpec)
    local montage = ctx.cache.hitMontages[key]
    if montage == nil then
        return
    end

    Context.StopCurrentMontage(ctx)
    State.ResetAttack(ctx)
    State.ResetGuard(ctx)
    Counter.Reset(ctx)
    ctx.state.hitPlaying = true
    ctx.state.currentHitMontage = montage
    ctx.state.canCancelHit = false
    local health = Context.GetHealthComponent(ctx)
    if health ~= nil and health.SetInvincible ~= nil then
        --Ignore extra hits while hit montage is playing--
        health:SetInvincible(true)
    end
    Locomotion.Lock(ctx)
    --Clear old EnableAttack flag--
    Context.ConsumeEnableAttack(ctx)
    anim:PlayMontage(montage)
end

function HitReaction.UpdateCancelWindow(ctx)
    if not ctx.state.hitPlaying or ctx.state.canCancelHit then
        return
    end

    if Context.ConsumeEnableAttack(ctx) then
        ctx.state.canCancelHit = true
        Locomotion.Unlock(ctx)
    end
end

function HitReaction.UpdateSequence(ctx)
    if not ctx.state.hitPlaying then
        return
    end

    if ctx.state.canCancelHit and Locomotion.IsMoveKeyDown(ctx) then
        HitReaction.StopForCancel(ctx)
        return
    end

    local anim = Context.GetAnimInstance(ctx)
    if anim == nil or ctx.state.currentHitMontage == nil or not anim:IsMontagePlaying(ctx.state.currentHitMontage) then
        State.ResetHit(ctx)
        Locomotion.Unlock(ctx)
    end
end

function HitReaction.BeginPlay(ctx)
    load_hit_montages(ctx)
end

function HitReaction.Tick(ctx, dt)
    HitReaction.UpdateCancelWindow(ctx)
    HitReaction.UpdateSequence(ctx)
end

return HitReaction
