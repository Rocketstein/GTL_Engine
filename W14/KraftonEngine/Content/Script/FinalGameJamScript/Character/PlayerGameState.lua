local Context = require("FinalGameJamScript/Character/CharacterContext")
local Locomotion = require("FinalGameJamScript/Character/CharacterLocomotion")
local State = require("FinalGameJamScript/Character/CharacterState")
local Counter = require("FinalGameJamScript/Character/CharacterCounter")
local Equipment = require("FinalGameJamScript/Character/CharacterEquipment")

local PlayerGameState = {}

local function should_end_game_from_damage_report(damageReport)
    if damageReport == nil then
        return false
    end

    if damageReport.bKilled == true then
        return true
    end

    return damageReport.NewHealth ~= nil and damageReport.NewHealth <= 0.0
end

local function get_death_montage(ctx)
    if ctx.cache.deathMontage ~= nil then
        return ctx.cache.deathMontage
    end

    local path = ctx.config.DEATH_MONTAGE_PATH
    if path == nil or path == "" then
        return nil
    end

    ctx.cache.deathMontage = Context.LoadMontage(path)
    return ctx.cache.deathMontage
end

local function play_death_montage(ctx)
    local anim = Context.GetAnimInstance(ctx)
    local montage = get_death_montage(ctx)
    if anim == nil or anim.PlayMontage == nil or montage == nil then
        return
    end

    anim:PlayMontage(montage)
    ctx.state.deathMontagePlaying = true
end

local function stop_death_montage(ctx)
    if not ctx.state.deathMontagePlaying then
        return
    end

    Context.StopCurrentMontage(ctx)
    ctx.state.deathMontagePlaying = false
end

local function request_player_death(ctx)
    if ctx.state.playerDead or ctx.state.playerDefeated then
        return
    end

    ctx.state.playerDead = true
    Context.StopCurrentMontage(ctx)
    Counter.RestoreCollision(ctx)
    State.ResetCombat(ctx)
    Equipment.DeactivateTrailNow(ctx)
    play_death_montage(ctx)
    Locomotion.Lock(ctx)

    if Game ~= nil and Game.PlayerDeath ~= nil then
        Game.PlayerDeath()
    end
end

local function reset_player_vitals(ctx)
    if ctx.obj == nil then
        return
    end

    local health = nil
    if ctx.obj.GetHealthComponent ~= nil then
        health = ctx.obj:GetHealthComponent()
    else
        health = Context.Call(ctx.obj, "GetHealthComponent")
    end

    if health ~= nil then
        if health.ResetHealth ~= nil then
            health:ResetHealth()
        else
            Context.Call(health, "ResetHealth")
        end
    end

    local combat = nil
    if ctx.obj.GetCombatStateComponent ~= nil then
        combat = ctx.obj:GetCombatStateComponent()
    else
        combat = Context.Call(ctx.obj, "GetCombatStateComponent")
    end

    if combat ~= nil then
        if combat.StopStagger ~= nil then
            combat:StopStagger()
        else
            Context.Call(combat, "StopStagger")
        end

        if combat.ResetPoise ~= nil then
            combat:ResetPoise()
        else
            Context.Call(combat, "ResetPoise")
        end
    end
end

local function reset_after_revive(ctx)
    ctx.state.playerDead = false
    stop_death_montage(ctx)
    reset_player_vitals(ctx)
    Counter.RestoreCollision(ctx)
    State.ResetCombat(ctx)
    Locomotion.Unlock(ctx)
end

function PlayerGameState.IsSuspended(ctx)
    return ctx.state.playerDead or ctx.state.playerDefeated
end

function PlayerGameState.Tick(ctx, dt)
    if ctx.state.playerDefeated then
        return true
    end

    if ctx.state.playerDead then
        --Dead phase: suspend combat. Resume only when the game flow revives us--
        if Game ~= nil and Game.GetPhase ~= nil and Game.GetPhase() == "Playing" then
            reset_after_revive(ctx)
            return false
        end

        return true
    end

    return false
end

function PlayerGameState.HandleDamaged(ctx, damageSpec, damageReport)
    if should_end_game_from_damage_report(damageReport) then
        request_player_death(ctx)
        return true
    end

    if damageReport ~= nil and damageReport.AppliedDamage ~= nil and damageReport.AppliedDamage <= 0.0 then
        return true
    end

    return false
end

return PlayerGameState
