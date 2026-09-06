---@class KillCountUI : ScriptComponent
local Script = {
    properties = {
        AnchorX = { type = "float", default = 0.92, min = 0.0, max = 1.0, speed = 0.01 },
        AnchorY = { type = "float", default = 0.035, min = 0.0, max = 1.0, speed = 0.01 },
        YOffset = { type = "float", default = 36.0, min = -400.0, max = 400.0, speed = 1.0 },
        IconSize = { type = "float", default = 28.0, min = 4.0, max = 128.0, speed = 1.0 },
        TextWidth = { type = "float", default = 96.0, min = 16.0, max = 320.0, speed = 1.0 },
        TextHeight = { type = "float", default = 32.0, min = 8.0, max = 128.0, speed = 1.0 },
        TextGap = { type = "float", default = 6.0, min = 0.0, max = 64.0, speed = 1.0 },
        FontSize = { type = "float", default = 1.8, min = 0.1, max = 4.0, speed = 0.05 },
        Layer = { type = "int", default = 205, min = 0, max = 1000, speed = 1 },
    }
}

local GameManager = require("GameManager")

local ICON_PATH = "Asset/Content/Textures/UI/killcounticon.png"

local function vec2(x, y)
    return { x = x, y = y }
end

local function configureBaseUI(ui, anchor, size, position, pivot, layer, zOrder)
    if ui == nil or not ui:IsValid() then
        return false
    end

    ui:SetUIRenderSpace("ScreenSpace")
    ui:SetUIAnchor(anchor)
    ui:SetUIAnchoredPosition(position)
    ui:SetUISizeDelta(size)
    ui:SetUIPivot(pivot)
    ui:SetUIRotationDegrees(0.0)
    ui:SetUILayer(layer)
    ui:SetUIZOrder(zOrder)
    ui:SetUIHitTestVisible(false)
    ui:SetUIVisibility(true)
    return true
end

function Script:BeginPlay()
    self.pool = GetActorPoolManager()
    self.ownerActor = self:GetActor()
    self.killCount = 0
    self.visible = true

    self:CreateWidgets()
    GameManager.RegisterKillCountUI(self)
    self:SyncOwnerVisibility()
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

function Script:CreateWidgets()
    local anchor = vec2(self.AnchorX or 0.92, self.AnchorY or 0.035)
    local yOffset = self.YOffset or 36.0
    local layer = self.Layer or 205
    local textWidth = self.TextWidth or 96.0
    local textGap = self.TextGap or 6.0

    self.iconActor = self:AcquireActor("AUIActor")
    if self.iconActor ~= nil then
        self.iconUI = self.iconActor:GetComponent("UUIComponent")
        if configureBaseUI(
            self.iconUI,
            anchor,
            vec2(self.IconSize or 28.0, self.IconSize or 28.0),
            vec2(-(textWidth + textGap), yOffset),
            vec2(1.0, 0.5),
            layer,
            0
        ) then
            self.iconUI:SetUITexturePath(ICON_PATH)
            self.iconUI:SetUITint(1.0, 1.0, 1.0, 1.0)
        else
            self:ReleaseActor("iconActor", "iconUI")
        end
    end

    self.textActor = self:AcquireActor("ATextUIActor")
    if self.textActor ~= nil then
        self.textUI = self.textActor:GetComponent("UTextUIComponent")
        if configureBaseUI(
            self.textUI,
            anchor,
            vec2(textWidth, self.TextHeight or 32.0),
            vec2(0.0, yOffset),
            vec2(1.0, 0.5),
            layer,
            1
        ) then
            self.textUI:SetUIText("0")
            self.textUI:SetUIFont("Default")
            self.textUI:SetUIFontSize(self.FontSize or 1.8)
            self.textUI:SetUITextHorizontalAlignment("Right")
            self.textUI:SetUITextVerticalAlignment("Center")
            self.textUI:SetUITint(1.0, 1.0, 1.0, 1.0)
        else
            self:ReleaseActor("textActor", "textUI")
        end
    end
end

function Script:SetKillCount(count)
    self.killCount = count or 0

    if self.textUI ~= nil and self.textUI:IsValid() then
        self.textUI:SetUIText(tostring(self.killCount))
    end
end

function Script:SetWidgetsVisible(visible)
    self.visible = visible == true

    if self.iconUI ~= nil and self.iconUI:IsValid() then
        self.iconUI:SetUIVisibility(self.visible)
    end
    if self.iconActor ~= nil and self.iconActor:IsValid() then
        self.iconActor:SetVisible(self.visible)
    end

    if self.textUI ~= nil and self.textUI:IsValid() then
        self.textUI:SetUIVisibility(self.visible)
    end
    if self.textActor ~= nil and self.textActor:IsValid() then
        self.textActor:SetVisible(self.visible)
    end
end

function Script:SyncOwnerVisibility()
    local ownerVisible = true
    if self.ownerActor ~= nil and self.ownerActor:IsValid() then
        ownerVisible = self.ownerActor:IsVisible()
    end

    if self.visible ~= ownerVisible then
        self:SetWidgetsVisible(ownerVisible)
    end
end

function Script:ReleaseActor(actorField, uiField)
    local actor = self[actorField]
    local ui = self[uiField]

    if ui ~= nil and ui:IsValid() then
        ui:SetUIVisibility(false)
    end

    if actor ~= nil and actor:IsValid() then
        actor:SetVisible(false)
        if self.pool ~= nil and self.pool:IsValid() then
            self.pool:Release(actor)
        end
    end

    self[actorField] = nil
    self[uiField] = nil
end

function Script:Tick(deltaTime)
    self:SyncOwnerVisibility()
end

function Script:EndPlay()
    GameManager.UnregisterKillCountUI(self)
    self:ReleaseActor("iconActor", "iconUI")
    self:ReleaseActor("textActor", "textUI")
end

return Script
