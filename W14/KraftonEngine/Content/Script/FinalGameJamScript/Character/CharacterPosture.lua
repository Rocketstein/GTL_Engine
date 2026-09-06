local Context = require("FinalGameJamScript/Character/CharacterContext")
local Counter = require("FinalGameJamScript/Character/CharacterCounter")
local HitReaction = require("FinalGameJamScript/Character/CharacterHitReaction")
local Locomotion = require("FinalGameJamScript/Character/CharacterLocomotion")
local State = require("FinalGameJamScript/Character/CharacterState")

local Posture = {}

local function play_groggy_sound(ctx)
    if Audio == nil then
        return
    end

    local key = ctx.config.POSTURE_BREAK_SOUND_KEY or "Groggy"
    local path = ctx.config.POSTURE_BREAK_SOUND_PATH or "Groggy.wav"
    local volume = ctx.config.POSTURE_BREAK_SOUND_VOLUME or 1.0

    if not ctx.state.postureBreakSoundLoaded and Audio.Load ~= nil then
        Audio.Load(key, path, false)
        ctx.state.postureBreakSoundLoaded = true
    end

    if Audio.Play ~= nil then
        Audio.Play(key, volume)
    end
end

local function apply_groggy_damage(ctx)
    local health = Context.GetHealthComponent(ctx)
    if health == nil then
        return false
    end

    local damage = ctx.config.POSTURE_BREAK_DAMAGE or 35.0
    if damage <= 0.0 then
        return false
    end

    local wasInvincible = false
    if health.IsInvincible ~= nil then
        wasInvincible = health:IsInvincible() == true
    end
    if wasInvincible and health.SetInvincible ~= nil then
        health:SetInvincible(false)
    end

    local combat = Context.GetCombatStateComponent(ctx)
    local wasCombatInvincible = false
    if combat ~= nil and combat.IsInvincible ~= nil then
        wasCombatInvincible = combat:IsInvincible() == true
    end
    if wasCombatInvincible and combat.SetInvincible ~= nil then
        combat:SetInvincible(false)
    end

    ctx.state.postureBreakDamageInProgress = true
    if health.ApplyDamage ~= nil then
        health:ApplyDamage(damage)
    else
        Context.Call(health, "ApplyDamage", damage)
    end
    ctx.state.postureBreakDamageInProgress = false

    return true
end

local function is_player_dead(ctx)
    local health = Context.GetHealthComponent(ctx)
    if health ~= nil and health.IsDead ~= nil then
        return health:IsDead() == true
    end
    return ctx.state.playerDead == true or ctx.state.playerDefeated == true
end

local function load_posture_break_montage(ctx)
    if ctx.cache.postureBreakMontage ~= nil then
        return ctx.cache.postureBreakMontage
    end

    local path = ctx.config.POSTURE_BREAK_MONTAGE_PATH
    if path == nil or path == "" then
        return nil
    end

    ctx.cache.postureBreakMontage = Context.LoadMontage(path)
    return ctx.cache.postureBreakMontage
end

local function play_posture_break_animation(ctx)
    local anim = Context.GetAnimInstance(ctx)
    local montage = load_posture_break_montage(ctx)
    if anim == nil or montage == nil then
        HitReaction.Play(ctx, nil)
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
        --Ignore extra hits while posture break montage is playing--
        health:SetInvincible(true)
    end

    Locomotion.Lock(ctx)
    Context.ConsumeEnableAttack(ctx)
    anim:PlayMontage(montage)
end

function Posture.TriggerGroggy(ctx)
    if ctx == nil or ctx.state == nil then
        return
    end

    if ctx.state.postureBreakHandling == true then
        return
    end

    ctx.state.postureBreakHandling = true
    ctx.state.suppressDamageReactionRemaining = 0.2
    play_groggy_sound(ctx)
    apply_groggy_damage(ctx)

    if not is_player_dead(ctx) then
        play_posture_break_animation(ctx)
    end
end

function Posture.HandleDamaged(ctx)
    if ctx == nil or ctx.state == nil then
        return false
    end

    if ctx.state.postureBreakDamageInProgress == true then
        return true
    end

    if (ctx.state.suppressDamageReactionRemaining or 0.0) > 0.0 then
        ctx.state.suppressDamageReactionRemaining = 0.0
        return true
    end

    return false
end

function Posture.BeginPlay(ctx)
    local combat = Context.GetCombatStateComponent(ctx)
    if combat == nil or combat.BindOnPostureChanged == nil then
        return
    end

    ctx.state.postureBreakHandling = false
    ctx.state.postureBreakDamageInProgress = false
    ctx.state.postureBreakSoundLoaded = false
    ctx.state.suppressDamageReactionRemaining = 0.0

    ctx.state.postureBreakBindingId = combat:BindOnPostureChanged(
        function(_component, previousPoise, currentPoise, maxPoise)
            maxPoise = tonumber(maxPoise) or 0.0
            previousPoise = tonumber(previousPoise) or maxPoise
            currentPoise = tonumber(currentPoise) or maxPoise

            if maxPoise <= 0.0 then
                return
            end

            if previousPoise > 0.0 and currentPoise <= 0.0 then
                Posture.TriggerGroggy(ctx)
            end
        end
    )
end

function Posture.Tick(ctx, dt)
    if ctx ~= nil and ctx.state ~= nil then
        local remaining = ctx.state.suppressDamageReactionRemaining or 0.0
        if remaining > 0.0 then
            ctx.state.suppressDamageReactionRemaining = math.max(0.0, remaining - (dt or 0.0))
        end
    end

    local combat = Context.GetCombatStateComponent(ctx)
    if combat == nil or combat.IsStaggered == nil then
        return
    end

    if combat:IsStaggered() == false then
        ctx.state.postureBreakHandling = false
    end
end

return Posture
