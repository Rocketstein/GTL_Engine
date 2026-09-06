---@class GameTimerUI : ScriptComponent
local Script = {
    properties = {
        AnchorX = { type = "float", default = 0.5, min = 0.0, max = 1.0, speed = 0.01 },
        AnchorY = { type = "float", default = 0.035, min = 0.0, max = 1.0, speed = 0.01 },
        YOffset = { type = "float", default = 36.0, min = -400.0, max = 400.0, speed = 1.0 },
        TextWidth = { type = "float", default = 140.0, min = 16.0, max = 320.0, speed = 1.0 },
        TextHeight = { type = "float", default = 32.0, min = 8.0, max = 128.0, speed = 1.0 },
        FontSize = { type = "float", default = 1.4, min = 0.1, max = 4.0, speed = 0.05 },
        Layer = { type = "int", default = 205, min = 0, max = 1000, speed = 1 },
    }
}

local GameManager = require("GameManager")

local function vec2(x, y)
    return { x = x, y = y }
end

local function formatTime(timeRemaining)
    local totalSeconds = math.max(0, math.floor((tonumber(timeRemaining) or 0.0) + 0.5))
    local minutes = math.floor(totalSeconds / 60)
    local seconds = totalSeconds % 60
    return string.format("%02d:%02d", minutes, seconds)
end

function Script:BeginPlay()
    self.pool = GetActorPoolManager()
    self.ownerActor = self:GetActor()
    self.visible = true
    self.lastText = nil

    self:CreateText()
    self:UpdateTimerText()
    self:SyncOwnerVisibility()
end

function Script:CreateText()
    if self.pool == nil or not self.pool:IsValid() then
        return
    end

    self.textActor = self.pool:Acquire("ATextUIActor")
    if self.textActor == nil or not self.textActor:IsValid() then
        self.textActor = nil
        return
    end

    self.textActor:SetVisible(true)
    self.textUI = self.textActor:GetComponent("UTextUIComponent")
    if self.textUI == nil or not self.textUI:IsValid() then
        self:ReleaseText()
        return
    end

    self.textUI:SetUIRenderSpace("ScreenSpace")
    self.textUI:SetUIAnchor(vec2(self.AnchorX or 0.5, self.AnchorY or 0.035))
    self.textUI:SetUIAnchoredPosition(vec2(0.0, self.YOffset or 36.0))
    self.textUI:SetUISizeDelta(vec2(self.TextWidth or 140.0, self.TextHeight or 32.0))
    self.textUI:SetUIPivot(vec2(0.5, 0.5))
    self.textUI:SetUIRotationDegrees(0.0)
    self.textUI:SetUILayer(self.Layer or 205)
    self.textUI:SetUIZOrder(0)
    self.textUI:SetUIHitTestVisible(false)
    self.textUI:SetUIText("00:00")
    self.textUI:SetUIFont("Default")
    self.textUI:SetUIFontSize(self.FontSize or 1.4)
    self.textUI:SetUITextHorizontalAlignment("Center")
    self.textUI:SetUITextVerticalAlignment("Center")
    self.textUI:SetUITint(1.0, 1.0, 1.0, 1.0)
    self.textUI:SetUIVisibility(true)
end

function Script:UpdateTimerText()
    if self.textUI == nil or not self.textUI:IsValid() then
        return
    end

    local text = formatTime(GameManager.TimeRemaining)
    if self.lastText == text then
        return
    end

    self.lastText = text
    self.textUI:SetUIText(text)
end

function Script:SetWidgetVisible(visible)
    self.visible = visible == true

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
        self:SetWidgetVisible(ownerVisible)
    end
end

function Script:ReleaseText()
    if self.textUI ~= nil and self.textUI:IsValid() then
        self.textUI:SetUIVisibility(false)
    end

    if self.textActor ~= nil and self.textActor:IsValid() then
        self.textActor:SetVisible(false)
        if self.pool ~= nil and self.pool:IsValid() then
            self.pool:Release(self.textActor)
        end
    end

    self.textActor = nil
    self.textUI = nil
end

function Script:Tick(deltaTime)
    self:UpdateTimerText()
    self:SyncOwnerVisibility()
end

function Script:EndPlay()
    self:ReleaseText()
end

return Script
