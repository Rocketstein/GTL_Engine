local Equipment = require("FinalGameJamScript/Character/CharacterEquipment")
local Context = require("FinalGameJamScript/Character/CharacterContext")

local State = {}

function State.ResetAttack(ctx)
    Equipment.RequestTrailDeactivate(ctx)
    ctx.state.attackPlaying = false
    ctx.state.currentAttackIndex = 0
    ctx.state.canChainAttack = false
    ctx.state.attackInputQueued = false
end

function State.ResetGuard(ctx)
    ctx.state.defensePlaying = false
    ctx.state.defenseIdlePlaying = false
    ctx.state.counterWindowOpen = false
end

function State.ResetCounter(ctx)
    if ctx.state.counterInvincibleApplied then
        local health = Context.GetHealthComponent(ctx)
        if health ~= nil and health.SetInvincible ~= nil then
            health:SetInvincible(ctx.state.counterPreviousInvincible == true)
        end
    end

    Equipment.RequestTrailDeactivate(ctx)
    ctx.state.counterPlaying = false
    ctx.state.canCancelCounter = false
    ctx.state.counterMove = nil
    ctx.state.counterInvincibleApplied = false
    ctx.state.counterPreviousInvincible = false
end

function State.ResetHit(ctx)
    local health = Context.GetHealthComponent(ctx)
    if health ~= nil and health.SetInvincible ~= nil then
        --Hit invincible ends when hit state ends--
        health:SetInvincible(false)
    end

    ctx.state.hitPlaying = false
    ctx.state.currentHitMontage = nil
    ctx.state.canCancelHit = false
end

function State.ResetCombat(ctx)
    State.ResetAttack(ctx)
    State.ResetGuard(ctx)
    State.ResetCounter(ctx)
    State.ResetHit(ctx)
end

return State
