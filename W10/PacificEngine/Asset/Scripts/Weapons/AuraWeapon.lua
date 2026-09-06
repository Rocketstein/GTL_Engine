local Audio = require("Core.Audio")
local DamageSystem = require("Core.DamageSystem")
local GameplayPause = require("GameplayPause")
local WeaponDefs = require("WeaponDefs")

local AuraWeapon = {}
AuraWeapon.__index = AuraWeapon

local function Clamp01(value)
    if value < 0.0 then
        return 0.0
    end

    if value > 1.0 then
        return 1.0
    end

    return value
end

local function SmoothStep01(t)
    t = Clamp01(t)
    return t * t * (3.0 - 2.0 * t)
end

function AuraWeapon.New(owner)
    local self = setmetatable({}, AuraWeapon)

    self.Owner = owner
    self.Id = "Aura"
    self.Def = WeaponDefs.Aura
    self.Level = 1
    self.Data = self.Def.Levels[1]

    self.IsRunning = false

    self.DamageCoroutine = nil
    self.VisualCoroutine = nil

    self.Visual = nil
    self.CurrentAngle = 0.0
    self.VisualBurstTimer = 0.0
    self.VisualBurstDuration = 0.0

    return self
end

function AuraWeapon:Start()
    if self.IsRunning then
        return
    end

    self.IsRunning = true
    self:RefreshVisual()

    if self.Owner == nil or self.Owner.StartCoroutine == nil then
        Log("[Aura] StartCoroutine is nil")
        self.IsRunning = false
        return
    end

    self.DamageCoroutine = self.Owner.StartCoroutine(function()
        self:DamageLoop()
    end)

    self.VisualCoroutine = self.Owner.StartCoroutine(function()
        self:VisualLoop()
    end)

    Log("[Aura] started")
end

function AuraWeapon:Stop()
    self.IsRunning = false

    if self.Owner ~= nil and self.Owner.StopCoroutine ~= nil then
        if self.DamageCoroutine ~= nil then
            self.Owner.StopCoroutine(self.DamageCoroutine)
        end

        if self.VisualCoroutine ~= nil then
            self.Owner.StopCoroutine(self.VisualCoroutine)
        end
    end

    self.DamageCoroutine = nil
    self.VisualCoroutine = nil
end

function AuraWeapon:DamageLoop()
    while self.IsRunning do
        if not GameplayPause.IsPaused() then
            self:DamageTick()
            self:PlayTickSound()
            self:TriggerVisualBurst()
        end

        GameplayPause.Wait(self.Data.TickInterval or 1.0)
    end
end

function AuraWeapon:DamageTick()
    local ownerActor = self:GetOwnerActor()
    if ownerActor == nil or not ownerActor:IsValid() then
        return
    end

    if self.Owner == nil or self.Owner.QueryActorsByTagInRadius == nil then
        Log("[Aura] QueryActorsByTagInRadius is nil")
        return
    end

    local center = ownerActor:GetLocation()
    local radius = self.Data.Radius or 4.0
    local damage = self.Data.Damage or 1.0

    local enemies = self.Owner.QueryActorsByTagInRadius("Enemy", center, radius)
    if enemies == nil then
        return
    end

    for _, enemy in pairs(enemies) do
        if enemy ~= nil and enemy:IsValid() then
            local success = DamageSystem.ApplyDamage(enemy, damage, ownerActor)

            if not success then
                Log("[Aura] Damage failed")
            end
        end
    end
end

function AuraWeapon:PlayTickSound()
    local sound = self.Data.Sound
    if sound == nil or sound.Tick == nil then
        return
    end

    Audio.Play(
        sound.Tick,
        sound.Bus or Audio.Bus.SFX,
        sound.Volume or 1.0
    )
end

function AuraWeapon:TriggerVisualBurst()
    self.VisualBurstDuration = self.Data.VisualBurstDuration or 1.0
    self.VisualBurstTimer = self.VisualBurstDuration
end

function AuraWeapon:Upgrade()
    if self:IsMaxLevel() then
        Log("[Aura] max level")
        return false
    end

    self.Level = self.Level + 1
    self.Data = self.Def.Levels[self.Level]

    self:RefreshVisual()

    Log("[Aura] upgraded to level " .. tostring(self.Level))

    return true
end

function AuraWeapon:IsMaxLevel()
    return self.Level >= self.Def.MaxLevel
end

function AuraWeapon:OnVisualApplied()
    self:RefreshVisual()
end

function AuraWeapon:RefreshVisual()
    if self.Owner == nil or self.Owner.GetComponentByName == nil then
        return
    end

    self.Visual = self.Owner.GetComponentByName(
        "UDecalComponent",
        "Visual_Aura_0"
    )

    if self.Visual == nil or not self.Visual:IsValid() then
        Log("[Aura] Visual_Aura_0 not found")
    end
end

function AuraWeapon:GetOwnerActor()
    if self.Owner ~= nil and self.Owner.GetActor ~= nil then
        return self.Owner.GetActor()
    end

    return nil
end

function AuraWeapon:VisualLoop()
    while self.IsRunning do
        local dt, paused = GameplayPause.WaitNextFrame()
        dt = dt or 0.0

        if self.Visual == nil or not self.Visual:IsValid() then
            self:RefreshVisual()
        end

        if self.Visual ~= nil and self.Visual:IsValid() then
            local ownerActor = self:GetOwnerActor()
            if ownerActor ~= nil and ownerActor:IsValid() and self.Visual.SetWorldLocation ~= nil then
                local location = ownerActor:GetLocation()
                location[3] = (location[3] or location.z or 0.0) + 0.1
                location.z = location[3]
                self.Visual:SetWorldLocation(location)
            end

            if not paused then
                local baseSpeed = self.Data.RotateSpeed or 25.0
                local burstSpeed = self.Data.VisualBurstRotateSpeed or 360.0

                local burstAlpha = 0.0

                if self.VisualBurstTimer ~= nil and self.VisualBurstTimer > 0.0 then
                    local duration = self.VisualBurstDuration or 1.0

                    if duration > 0.0 then
                        local t = self.VisualBurstTimer / duration
                        burstAlpha = SmoothStep01(t)
                    end

                    self.VisualBurstTimer = self.VisualBurstTimer - dt
                    if self.VisualBurstTimer < 0.0 then
                        self.VisualBurstTimer = 0.0
                    end
                end

                local speed = baseSpeed + burstSpeed * burstAlpha
                self.CurrentAngle = self.CurrentAngle + speed * dt

                while self.CurrentAngle >= 360.0 do
                    self.CurrentAngle = self.CurrentAngle - 360.0
                end

                while self.CurrentAngle < 0.0 do
                    self.CurrentAngle = self.CurrentAngle + 360.0
                end
            end

            if self.Visual.SetRelativeRotation ~= nil then
                self.Visual:SetRelativeRotation({ 0.0, 90.0, self.CurrentAngle })
            elseif self.Visual.SetRotation ~= nil then
                self.Visual:SetRotation({ 0.0, 90.0, self.CurrentAngle })
            end
        end
    end
end

return AuraWeapon
