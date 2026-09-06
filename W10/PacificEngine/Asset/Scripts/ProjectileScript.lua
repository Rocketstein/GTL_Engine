---@class ProjectileScript : ScriptComponent
local Script = {
    properties = {
        LifeTimeSeconds = { type = "float", default = 3.0 },
    },
}

local Co = require("LuaCoroutine")
local GameplayPause = require("GameplayPause")
local DamageSystem = require("Core.DamageSystem")

function Script:BeginPlay()
    self.actor = self:GetActor()
    self.HitActors = {}
    self.bAlive = false
    self.LifeId = 0
end

function Script:OnProjectileFired()
    self.actor = self:GetActor()
    self.bAlive = true
    self.HitActors = {}
    self.LifeId = (self.LifeId or 0) + 1

    local lifeId = self.LifeId
    local lifeTime = self.LifeTimeSeconds or 3.0

    self.StartCoroutine(function()
        GameplayPause.Wait(lifeTime)

        if self.LifeId ~= lifeId then
            return
        end

        if not self.bAlive then
            return
        end

        self.bAlive = false

        if self.ReturnToPool ~= nil then
            self.ReturnToPool()
        end
    end)
end

function Script:OnOverlapBegin(otherCollider)
    if GameplayPause.IsPaused() then
        return
    end

    if not self.bAlive then
        return
    end

    if otherCollider == nil or not otherCollider:IsValid() then
        return
    end

    if otherCollider.GetOwner == nil then
        return
    end

    local otherActor = otherCollider:GetOwner()
    if otherActor == nil or not otherActor:IsValid() then
        return
    end

    if self.actor ~= nil and otherActor == self.actor then
        return
    end

    local tag = nil
    if otherActor.GetActorTag ~= nil then
        tag = otherActor:GetActorTag()
    elseif otherActor.GetTag ~= nil then
        tag = otherActor:GetTag()
    end

    if tag ~= "Enemy" then
        return
    end

    local uuid = nil
    if otherActor.GetUUID ~= nil then
        uuid = otherActor:GetUUID()
    else
        uuid = tostring(otherActor)
    end

    if self.HitActors[uuid] then
        return
    end

    self.HitActors[uuid] = true

    local damage = 100.0
    if self.GetProjectileDamage ~= nil then
        damage = self.GetProjectileDamage()
    elseif self.Damage ~= nil then
        damage = self.Damage
    end

    local success = DamageSystem.ApplyDamage(otherActor, damage, self.actor)
    if success then
        Log("[Projectile] Damage applied: " .. tostring(damage))
    else
        Log("[Projectile] Damage failed")
    end

    -- Pierce is extra enemies after the first hit, so the projectile expires
    -- only after the consumed remaining pierce count drops below zero.
    local remainPierce = -1
    if self.ConsumePierce ~= nil then
        remainPierce = self.ConsumePierce()
    end

    if remainPierce < 0 then
        self.bAlive = false

        if self.ReturnToPool ~= nil then
            self.ReturnToPool()
        end
    end
end

function Script:EndPlay()
    self.bAlive = false
    self.LifeId = (self.LifeId or 0) + 1
    self.StopAllCoroutines()
end

return Script
