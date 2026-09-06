local DamageSystem = require("Core.DamageSystem")
local DamageNumberSystem = require("UI.DamageNumberSystem")
local GameManager = require("GameManager")

local EnemyState = {
    properties = {
        MaxHP = 100,
        CurrentHP = 100,
        GemClassName = {type = "string", default = "APickupActor"},
        DeathDecalClassName = {type = "string", default = "ADeathDecalActor"}
    }
}

function EnemyState:BeginPlay()
    local wasDead = self.bIsDead == true

    self.actor = self:GetActor()
    if self.actor == nil or not self.actor:IsValid() then
        Log("[EnemyState] Invalid owner actor")
        return
    end

    -- EnemyScript 캐싱 (데미지 연출용)
    self.EnemyScriptComp = nil
    local comps = self.actor:GetComponents("UScriptComponent")
    for _, comp in ipairs(comps) do
        if comp:IsValid() and comp:GetName() ~= self:GetName() then
            self.EnemyScriptComp = comp
            break
        end
    end

    self.CurrentHP = self.MaxHP or 100.0
    self.bIsDead = false

    DamageSystem.Register(self.actor, self)
end

function EnemyState:TakeDamage(amount, attacker)
    if self.bIsDead then
        local actorName = "Unknown"
        if self.actor ~= nil and self.actor:IsValid() then
            actorName = self.actor:GetName()
        end

        return
    end

    amount = amount or 0.0

    self.CurrentHP = (self.CurrentHP or self.MaxHP or 100.0) - amount

    local attackerName = "Unknown"
    if attacker ~= nil and attacker:IsValid() then
        attackerName = attacker:GetName()
    end

    Log(string.format("[Enemy:%s] Received %.1f damage from [%s]. HP: %.1f/%.1f",
        self.actor:GetName(), amount, attackerName, self.CurrentHP, self.MaxHP or 100.0))

    DamageNumberSystem.ShowDamage(self.actor, amount)

    -- 피격 연출 (Squash & Stretch)
    if self.EnemyScriptComp and self.EnemyScriptComp:IsValid() then
        self.EnemyScriptComp:CallScript("PlaySquashEffect")
    end

    if self.CurrentHP <= 0 then
        self:Die()
    end
end

function EnemyState:Die()
    if self.bIsDead then
        return
    end

    self.bIsDead = true
    Log("[Enemy] Died: " .. self.actor:GetName())
    GameManager.OnEnemyKilled(self.actor)
    
    local PoolManager = GetActorPoolManager()
    if PoolManager:IsValid() then
        local deathLocation = self:GetDeathLocation()
        deathLocation.z = deathLocation.z + 0.3
        self:SpawnGem(PoolManager, deathLocation)
        self:SpawnDeathDecal(PoolManager, deathLocation)
        PoolManager:Release(self.actor)
    else
        -- Fallback if PoolManager is not available
        self.actor:SetVisible(false)
    end
end

function EnemyState:EndPlay()
    if self.actor ~= nil and self.actor:IsValid() then
        DamageSystem.Unregister(self.actor)
    end
end

function EnemyState:GetDeathLocation()
    local owner = self:GetActor()
    if owner:IsValid() then
        return owner:GetLocation()
    end

    return {x = 0, y = 0, z = 0}
end

function EnemyState:SpawnGem(PoolManager, SpawnPos)
    local Gem = PoolManager:Acquire(self.GemClassName)
    if Gem:IsValid() then
        Gem:SetLocation(SpawnPos)
    end
end

function EnemyState:SpawnDeathDecal(PoolManager, SpawnPos)
    if self.DeathDecalClassName == nil or self.DeathDecalClassName == "" then
        return
    end

    local Decal = PoolManager:Acquire(self.DeathDecalClassName)
    if Decal:IsValid() then
        Decal:SetLocation(SpawnPos)
    end
end

return EnemyState
