---@class SkillHUD : ScriptComponent
local Script = {
    properties = {
        AnchorX = { type = "float", default = 0.035, min = 0.0, max = 1.0, speed = 0.01 },
        AnchorY = { type = "float", default = 0.94, min = 0.0, max = 1.0, speed = 0.01 },
        IconSize = { type = "float", default = 56.0, min = 16.0, max = 160.0, speed = 1.0 },
        SlotGap = { type = "float", default = 10.0, min = 0.0, max = 80.0, speed = 1.0 },
        LevelTextHeight = { type = "float", default = 18.0, min = 8.0, max = 64.0, speed = 1.0 },
        FontSize = { type = "float", default = 0.72, min = 0.1, max = 4.0, speed = 0.05 },
        Layer = { type = "int", default = 205, min = 0, max = 1000, speed = 1 },
    }
}

local GameManager = require("GameManager")

local SkillOrder = {
    "Aura",
    "MainCannon",
    "MachineTurret",
    "VehicleRush",
}

local SkillIcons = {
    Aura = "Asset/Content/Textures/UI/skillicon_aura.png",
    MainCannon = "Asset/Content/Textures/UI/skillicon_MainCannon.png",
    MachineTurret = "Asset/Content/Textures/UI/skillicon_MachineTurret.png",
    VehicleRush = "Asset/Content/Textures/UI/skillicon_VehicleRush.png",
}

local function vec2(x, y)
    return { x = x, y = y }
end

local function getWeaponLevel(weapon)
    return math.max(1, math.floor(tonumber(weapon ~= nil and weapon.Level or 1) or 1))
end

function Script:BeginPlay()
    self.pool = GetActorPoolManager()
    self.ownerActor = self:GetActor()
    self.slots = {}
    self.lastSnapshot = nil
    self.visible = true

    self:RefreshSlots()
    self:SyncOwnerVisibility()
end

function Script:GetLearnedSkills()
    local inventory = GameManager.WeaponInventory
    if inventory == nil or inventory.Weapons == nil then
        return {}, ""
    end

    local learned = {}
    local snapshotParts = {}
    for _, id in ipairs(SkillOrder) do
        local weapon = inventory.Weapons[id]
        if weapon ~= nil then
            local level = getWeaponLevel(weapon)
            table.insert(learned, { id = id, level = level })
            table.insert(snapshotParts, id .. ":" .. tostring(level))
        end
    end

    return learned, table.concat(snapshotParts, "|")
end

function Script:AcquireActor(className)
    if self.pool == nil or not self.pool:IsValid() then
        return nil
    end

    local actor = self.pool:Acquire(className)
    if actor == nil or not actor:IsValid() then
        return nil
    end

    actor:SetVisible(true)
    return actor
end

function Script:ConfigureIcon(ui, skill, index)
    if ui == nil or not ui:IsValid() then
        return false
    end

    local iconSize = self.IconSize or 56.0
    local x = (index - 1) * (iconSize + (self.SlotGap or 10.0))

    ui:SetUIRenderSpace("ScreenSpace")
    ui:SetUIAnchor(vec2(self.AnchorX or 0.035, self.AnchorY or 0.94))
    ui:SetUIAnchoredPosition(vec2(x, 0.0))
    ui:SetUISizeDelta(vec2(iconSize, iconSize))
    ui:SetUIPivot(vec2(0.0, 1.0))
    ui:SetUIRotationDegrees(0.0)
    ui:SetUILayer(self.Layer or 205)
    ui:SetUIZOrder(index * 10)
    ui:SetUIHitTestVisible(false)
    ui:SetUITexturePath(SkillIcons[skill.id] or "")
    ui:SetUITint(1.0, 1.0, 1.0, 1.0)
    ui:SetUIVisibility(true)
    return true
end

function Script:ConfigureLevelText(ui, skill, index)
    if ui == nil or not ui:IsValid() then
        return false
    end

    local iconSize = self.IconSize or 56.0
    local x = (index - 1) * (iconSize + (self.SlotGap or 10.0)) + iconSize * 0.5
    local y = -(self.LevelTextHeight or 18.0) * 0.65

    ui:SetUIRenderSpace("ScreenSpace")
    ui:SetUIAnchor(vec2(self.AnchorX or 0.035, self.AnchorY or 0.94))
    ui:SetUIAnchoredPosition(vec2(x, y))
    ui:SetUISizeDelta(vec2(iconSize, self.LevelTextHeight or 18.0))
    ui:SetUIPivot(vec2(0.5, 0.5))
    ui:SetUIRotationDegrees(0.0)
    ui:SetUILayer(self.Layer or 205)
    ui:SetUIZOrder(index * 10 + 1)
    ui:SetUIHitTestVisible(false)
    ui:SetUIText("Lv." .. tostring(skill.level))
    ui:SetUIFont("Default")
    ui:SetUIFontSize(self.FontSize or 0.72)
    ui:SetUITextHorizontalAlignment("Center")
    ui:SetUITextVerticalAlignment("Center")
    ui:SetUITint(1.0, 1.0, 1.0, 1.0)
    ui:SetUIVisibility(true)
    return true
end

function Script:CreateSlot(skill, index)
    local iconActor = self:AcquireActor("AUIActor")
    local textActor = self:AcquireActor("ATextUIActor")

    if iconActor == nil or textActor == nil then
        self:ReleaseActor(iconActor)
        self:ReleaseActor(textActor)
        return
    end

    local iconUI = iconActor:GetComponent("UTextureUIComponent")
    local textUI = textActor:GetComponent("UTextUIComponent")

    if not self:ConfigureIcon(iconUI, skill, index) or not self:ConfigureLevelText(textUI, skill, index) then
        self:ReleaseActor(iconActor)
        self:ReleaseActor(textActor)
        return
    end

    table.insert(self.slots, {
        iconActor = iconActor,
        iconUI = iconUI,
        textActor = textActor,
        textUI = textUI,
    })
end

function Script:ReleaseActor(actor)
    if actor == nil or not actor:IsValid() then
        return
    end

    local components = actor:GetComponents("UUIComponent")
    if components ~= nil then
        for _, component in ipairs(components) do
            if component ~= nil and component:IsValid() and component.SetUIVisibility ~= nil then
                component:SetUIVisibility(false)
            end
        end
    end

    actor:SetVisible(false)
    if self.pool ~= nil and self.pool:IsValid() then
        self.pool:Release(actor)
    end
end

function Script:ReleaseSlots()
    for _, slot in ipairs(self.slots) do
        self:ReleaseActor(slot.iconActor)
        self:ReleaseActor(slot.textActor)
    end

    self.slots = {}
end

function Script:RefreshSlots()
    local learned, snapshot = self:GetLearnedSkills()
    if self.lastSnapshot == snapshot then
        return
    end

    self.lastSnapshot = snapshot
    self:ReleaseSlots()

    for index, skill in ipairs(learned) do
        self:CreateSlot(skill, index)
    end

    self:SetSlotsVisible(self.visible)
end

function Script:SetSlotsVisible(visible)
    self.visible = visible == true

    for _, slot in ipairs(self.slots) do
        if slot.iconUI ~= nil and slot.iconUI:IsValid() then
            slot.iconUI:SetUIVisibility(self.visible)
        end
        if slot.iconActor ~= nil and slot.iconActor:IsValid() then
            slot.iconActor:SetVisible(self.visible)
        end

        if slot.textUI ~= nil and slot.textUI:IsValid() then
            slot.textUI:SetUIVisibility(self.visible)
        end
        if slot.textActor ~= nil and slot.textActor:IsValid() then
            slot.textActor:SetVisible(self.visible)
        end
    end
end

function Script:SyncOwnerVisibility()
    local ownerVisible = true
    if self.ownerActor ~= nil and self.ownerActor:IsValid() then
        ownerVisible = self.ownerActor:IsVisible()
    end

    if self.visible ~= ownerVisible then
        self:SetSlotsVisible(ownerVisible)
    end
end

function Script:Tick(deltaTime)
    self:RefreshSlots()
    self:SyncOwnerVisibility()
end

function Script:EndPlay()
    self:ReleaseSlots()
end

return Script
