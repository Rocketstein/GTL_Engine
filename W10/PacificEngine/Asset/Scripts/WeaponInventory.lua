local WeaponInventory = {}
WeaponInventory.__index = WeaponInventory
local WeaponVisualDefs = require("WeaponVisualDefs")
local WeaponDefs = require("WeaponDefs")

local WeaponConstructors = {
    MainCannon = require("Weapons.MainCannonWeapon"),
    MachineTurret = require("Weapons.MachineTurretWeapon"),
    VehicleRush = require("Weapons.VehicleRushWeapon"),
    Aura = require("Weapons.AuraWeapon"),
}

local WeaponIds = {
    "MainCannon",
    "MachineTurret",
    "VehicleRush",
    "Aura",
}

function WeaponInventory.New(owner)
    local self = setmetatable({}, WeaponInventory)
    self.Owner = owner
    self.Weapons = {}
    Log("WeaponInventory created")
    return self
end

function WeaponInventory:HasWeapon(id)
    return self.Weapons[id] ~= nil
end

function WeaponInventory:ApplyWeaponVisual(id, level)
    if self.Owner == nil or self.Owner.EquipWeaponVisual == nil then
        return
    end

    local weaponVisual = WeaponVisualDefs[id]
    if weaponVisual == nil then
        Log("[WeaponInventory] Visual def not found: " .. tostring(id))
        return
    end

    local layout = weaponVisual[level]
    if layout == nil then
        Log("[WeaponInventory] Visual layout not found: " .. tostring(id) .. " level=" .. tostring(level))
        return
    end

    self.Owner.EquipWeaponVisual(id, level, layout)
end

function WeaponInventory:ApplyWeaponVisualForWeapon(id, weapon)
    local level = 1
    if weapon ~= nil and weapon.Level ~= nil then
        level = weapon.Level
    end

    local function applyVisual()
        self:ApplyWeaponVisual(id, level)

        if weapon ~= nil and weapon.OnVisualApplied ~= nil then
            weapon:OnVisualApplied()
        end
    end

    if weapon ~= nil and weapon.RequestVisualApply ~= nil then
        return weapon:RequestVisualApply(applyVisual)
    end

    applyVisual()
    return true
end

function WeaponInventory:AddWeapon(id)
    if self:HasWeapon(id) then
        return self:UpgradeWeapon(id)
    end

    local constructor = WeaponConstructors[id]
    if constructor == nil or constructor.New == nil then
        Log("Unknown weapon id = " .. tostring(id))
        return false
    end

    local weapon = constructor.New(self.Owner)
    self.Weapons[id] = weapon
    Log(id .. " added")
    self:ApplyWeaponVisualForWeapon(id, weapon)

    if weapon.Start ~= nil then
        weapon:Start()
    end

    return true
end

function WeaponInventory:UpgradeWeapon(id)
    local weapon = self.Weapons[id]
    if weapon == nil then
        return self:AddWeapon(id)
    end

    if weapon.Upgrade == nil then
        return false
    end

    local upgraded = weapon:Upgrade()
    if upgraded then
        self:ApplyWeaponVisualForWeapon(id, weapon)
        if weapon.IsRunning and weapon.RebuildTurretSlots ~= nil then
            weapon:RebuildTurretSlots()
        end
    end

    return upgraded
end

function WeaponInventory:SetWeaponLevel(id, targetLevel)
    if not self:HasWeapon(id) then
        if not self:AddWeapon(id) then
            return false
        end
    end

    local weapon = self.Weapons[id]
    local definition = WeaponDefs[id]
    if weapon == nil or definition == nil or definition.Levels == nil then
        return false
    end

    local maxLevel = definition.MaxLevel or targetLevel or 1
    local level = math.max(1, math.min(targetLevel or maxLevel, maxLevel))
    local data = definition.Levels[level]
    if data == nil then
        return false
    end

    weapon.Level = level
    weapon.Data = data

    self:ApplyWeaponVisualForWeapon(id, weapon)

    if weapon.RefreshComponents ~= nil then
        weapon:RefreshComponents()
    end

    if weapon.IsRunning and weapon.RebuildTurretSlots ~= nil then
        weapon:RebuildTurretSlots()
    end

    return true
end

function WeaponInventory:ActivateAllWeaponsAtLevel(targetLevel)
    local activatedCount = 0

    for _, id in ipairs(WeaponIds) do
        if self:SetWeaponLevel(id, targetLevel) then
            activatedCount = activatedCount + 1
        end
    end

    return activatedCount
end

function WeaponInventory:GetUpgradeableWeapons()
    local upgradeableWeapons = {}

    for _, id in ipairs(WeaponIds) do
        local weapon = self.Weapons[id]
        if weapon ~= nil and weapon.IsMaxLevel ~= nil and not weapon:IsMaxLevel() then
            table.insert(upgradeableWeapons, id)
        end
    end

    return upgradeableWeapons
end

function WeaponInventory:GetMissingWeapons()
    local missingWeapons = {}

    for _, id in ipairs(WeaponIds) do
        if not self:HasWeapon(id) then
            table.insert(missingWeapons, id)
        end
    end

    return missingWeapons
end

function WeaponInventory:GetEvolvableWeapons()
    local evolvableWeapons = {}

    for _, id in ipairs(WeaponIds) do
        local weapon = self.Weapons[id]
        if weapon ~= nil and weapon.CanEvolve ~= nil and weapon:CanEvolve() then
            table.insert(evolvableWeapons, id)
        end
    end

    return evolvableWeapons
end

function WeaponInventory:EvolveWeapon(id)
    local weapon = self.Weapons[id]
    if weapon == nil or weapon.Evolve == nil then
        return false
    end

    local evolved = weapon:Evolve()
    if evolved then
        self:ApplyWeaponVisualForWeapon(id, weapon)
        if weapon.IsRunning and weapon.RebuildTurretSlots ~= nil then
            weapon:RebuildTurretSlots()
        end
    end

    return evolved
end

function WeaponInventory:OpenChest()
    local evolvableWeapons = self:GetEvolvableWeapons()
    if #evolvableWeapons > 0 then
        local id = evolvableWeapons[1]
        Log("OpenChest evolve = " .. tostring(id))
        return self:EvolveWeapon(id)
    end

    local upgradeableWeapons = self:GetUpgradeableWeapons()
    if #upgradeableWeapons > 0 then
        local id = upgradeableWeapons[math.random(#upgradeableWeapons)]
        Log("OpenChest upgrade = " .. tostring(id))
        return self:UpgradeWeapon(id)
    end

    Log("OpenChest failed")
    return false
end

return WeaponInventory
