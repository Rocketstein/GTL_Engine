local LevelSystem = {}
LevelSystem.__index = LevelSystem

local WeaponDefs = require("WeaponDefs")

local HiddenLevelUpWeaponIds = {}

local function getGameManager()
    return package.loaded["GameManager"]
end

local function shuffle(items)
    for index = #items, 2, -1 do
        local swapIndex = math.random(index)
        items[index], items[swapIndex] = items[swapIndex], items[index]
    end
end

local function makeOption(optionType, weaponId, currentLevel)
    local definition = WeaponDefs[weaponId] or {}
    local nextLevel = (currentLevel or 0) + 1

    return {
        Type = optionType,
        WeaponId = weaponId,
        DisplayName = definition.DisplayName or weaponId,
        CurrentLevel = currentLevel or 0,
        NextLevel = nextLevel,
        MaxLevel = definition.MaxNormalLevel or definition.MaxLevel or nextLevel,
    }
end

local function isHiddenLevelUpWeapon(weaponId)
    return HiddenLevelUpWeaponIds[weaponId] == true
end

function LevelSystem.New(owner, weaponInventory)
    local self = setmetatable({}, LevelSystem)
    self.Owner = owner
    self.WeaponInventory = weaponInventory
    self.Level = 1
    self.Exp = 0
    self.RequiredExp = 10
    self.PendingLevelUps = 0
    self.SelectionActive = false
    self.CurrentOptions = nil
    Log("LevelSystem created")
    return self
end

function LevelSystem:AddExp(amount)
    if amount == nil or amount <= 0 then
        return
    end

    self.Exp = self.Exp + amount
    Log("LevelSystem AddExp " .. tostring(amount))

    while self.Exp >= self.RequiredExp do
        self.Exp = self.Exp - self.RequiredExp
        self.Level = self.Level + 1
        self.PendingLevelUps = self.PendingLevelUps + 1
        self.RequiredExp = self:CalcRequiredExp(self.Level)
        Log("Level up. level = " .. tostring(self.Level) .. ", pending = " .. tostring(self.PendingLevelUps))
    end

    if self.PendingLevelUps > 0 then
        self:RequestLevelUpSelection()
    end
end

function LevelSystem:CalcRequiredExp(level)
    return 10 + level * 5
end

function LevelSystem:RequestLevelUpSelection()
    if self.SelectionActive or self.PendingLevelUps <= 0 then
        return true
    end

    local options = self:GenerateOptions(3)
    if #options <= 0 then
        Log("No level up options")
        self.PendingLevelUps = 0
        return false
    end

    local gameManager = getGameManager()
    if gameManager == nil or type(gameManager.BeginLevelUpSelection) ~= "function" then
        Log("GameManager.BeginLevelUpSelection is missing")
        return false
    end

    self.SelectionActive = true
    self.CurrentOptions = options
    local opened = gameManager.BeginLevelUpSelection(options)
    if not opened then
        self.SelectionActive = false
        self.CurrentOptions = nil
    end
    return opened
end

function LevelSystem:GenerateOptions(count)
    local pool = {}

    if self.WeaponInventory == nil then
        return pool
    end

    local upgradeableWeapons = self.WeaponInventory:GetUpgradeableWeapons()
    for _, id in ipairs(upgradeableWeapons) do
        if not isHiddenLevelUpWeapon(id) then
            local weapon = self.WeaponInventory.Weapons[id]
            local currentLevel = weapon ~= nil and weapon.Level or 1
            table.insert(pool, makeOption("UpgradeWeapon", id, currentLevel))
        end
    end

    local missingWeapons = self.WeaponInventory:GetMissingWeapons()
    for _, id in ipairs(missingWeapons) do
        if not isHiddenLevelUpWeapon(id) then
            table.insert(pool, makeOption("AddWeapon", id, 0))
        end
    end

    shuffle(pool)

    local options = {}
    local maxCount = math.min(count or 3, #pool)
    for index = 1, maxCount do
        table.insert(options, pool[index])
    end

    Log("GenerateOptions count = " .. tostring(#options))
    return options
end

function LevelSystem:SelectPendingOption(index)
    if self.CurrentOptions == nil then
        return false
    end

    local option = self.CurrentOptions[index]
    if option == nil then
        return false
    end

    local selected = self:SelectUpgrade(option)
    if not selected then
        return false
    end

    self.PendingLevelUps = math.max(0, self.PendingLevelUps - 1)
    self.CurrentOptions = nil
    self.SelectionActive = false
    return true
end

function LevelSystem:SelectUpgrade(option)
    if option == nil or self.WeaponInventory == nil then
        return false
    end

    Log("SelectUpgrade type = " .. tostring(option.Type) .. ", weapon = " .. tostring(option.WeaponId))

    if option.Type == "AddWeapon" then
        return self.WeaponInventory:AddWeapon(option.WeaponId)
    end

    if option.Type == "UpgradeWeapon" then
        return self.WeaponInventory:UpgradeWeapon(option.WeaponId)
    end

    return false
end

return LevelSystem
