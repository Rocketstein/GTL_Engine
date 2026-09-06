---@class ExpBar : ScriptComponent
local Script = {
    properties = {
        LeftAnchor = { type = "float", default = 0.08, min = 0.0, max = 1.0, speed = 0.01 },
        RightAnchor = { type = "float", default = 0.92, min = 0.0, max = 1.0, speed = 0.01 },
        TopAnchor = { type = "float", default = 0.035, min = 0.0, max = 1.0, speed = 0.01 },
        BackHeight = { type = "float", default = 18.0, min = 1.0, max = 80.0, speed = 1.0 },
        FillHeight = { type = "float", default = 12.0, min = 1.0, max = 80.0, speed = 1.0 },
    }
}

local GameManager = require("GameManager")

local function clamp(value, minValue, maxValue)
    return math.max(minValue, math.min(maxValue, value))
end

local function hsvToRgb(h, s, v)
    h = (h % 1.0) * 6.0
    local c = v * s
    local x = c * (1.0 - math.abs((h % 2.0) - 1.0))
    local m = v - c
    local r, g, b = 0.0, 0.0, 0.0

    if h < 1.0 then
        r, g, b = c, x, 0.0
    elseif h < 2.0 then
        r, g, b = x, c, 0.0
    elseif h < 3.0 then
        r, g, b = 0.0, c, x
    elseif h < 4.0 then
        r, g, b = 0.0, x, c
    elseif h < 5.0 then
        r, g, b = x, 0.0, c
    else
        r, g, b = c, 0.0, x
    end

    return r + m, g + m, b + m
end

function Script:GetAnchors(ratio)
    local left = clamp(self.LeftAnchor or 0.08, 0.0, 1.0)
    local right = clamp(self.RightAnchor or 0.92, 0.0, 1.0)
    local top = clamp(self.TopAnchor or 0.035, 0.0, 1.0)
    if right < left then
        left, right = right, left
    end

    local fillRight = left + (right - left) * clamp(ratio or 1.0, 0.0, 1.0)
    return {
        min = { x = left, y = top },
        fillMax = { x = fillRight, y = top },
        fullMax = { x = right, y = top },
    }
end

function Script:BeginPlay()
    self.Back = self.GetComponentByName("UTextureUIComponent", "PlayerExpBar_Back")
    self.Fill = self.GetComponentByName("UTextureUIComponent", "PlayerExpBar_Fill")
    self.LastRatio = nil
    self.LevelUpMode = false
    self.LevelUpTime = 0.0

    self:Configure()
    GameManager.RegisterExpBarUI(self)
    self:UpdateExpBar()
end

function Script:Configure()
    local anchors = self:GetAnchors(1.0)

    if self.Back ~= nil and self.Back:IsValid() then
        self.Back:SetUIRenderSpace("ScreenSpace")
        self.Back:SetUIAnchorMin(anchors.min)
        self.Back:SetUIAnchorMax(anchors.fullMax)
        self.Back:SetUIAnchoredPosition({ x = 0.0, y = 0.0 })
        self.Back:SetUISizeDelta({ x = 0.0, y = self.BackHeight or 18.0 })
        self.Back:SetUIPivot({ 0.5, 0.5 })
        self.Back:SetUITint(0.035, 0.04, 0.065, 0.84)
        self.Back:SetUIVisibility(true)
    end

    if self.Fill ~= nil and self.Fill:IsValid() then
        self.Fill:SetUIRenderSpace("ScreenSpace")
        self.Fill:SetUIAnchorMin(anchors.min)
        self.Fill:SetUIAnchorMax(anchors.min)
        self.Fill:SetUIAnchoredPosition({ x = 0.0, y = 0.0 })
        self.Fill:SetUISizeDelta({ x = 0.0, y = self.FillHeight or 12.0 })
        self.Fill:SetUIPivot({ 0.0, 0.5 })
        self.Fill:SetUITint(0.36, 0.72, 1.0, 0.95)
        self.Fill:SetUIVisibility(false)
    end
end

function Script:UpdateExpBar()
    if self.Fill == nil or not self.Fill:IsValid() then
        return
    end

    local levelSystem = GameManager.LevelSystem
    local exp = 0.0
    local requiredExp = 1.0
    if levelSystem ~= nil then
        exp = tonumber(levelSystem.Exp) or 0.0
        requiredExp = tonumber(levelSystem.RequiredExp) or 1.0
    end

    local ratio = 0.0
    if requiredExp > 0.0001 then
        ratio = clamp(exp / requiredExp, 0.0, 1.0)
    end

    if self.LevelUpMode then
        ratio = 1.0
    end

    local anchors = self:GetAnchors(ratio)
    self.Fill:SetUIAnchorMin(anchors.min)
    self.Fill:SetUIAnchorMax(anchors.fillMax)
    self.Fill:SetUIVisibility(ratio > 0.0 or self.LevelUpMode)

    if not self.LevelUpMode then
        self.Fill:SetUITint(0.36, 0.72, 1.0, 0.95)
    end

    self.LastRatio = ratio
end

function Script:SetLevelUpMode(enabled)
    self.LevelUpMode = enabled == true
    self.LevelUpTime = 0.0

    if not self.LevelUpMode and self.Fill ~= nil and self.Fill:IsValid() then
        self.Fill:SetUITint(0.36, 0.72, 1.0, 0.95)
    end

    self:UpdateExpBar()
end

function Script:UpdateLevelUpEffect(deltaTime)
    if not self.LevelUpMode or self.Fill == nil or not self.Fill:IsValid() then
        return
    end

    self.LevelUpTime = self.LevelUpTime + (deltaTime or 0.0)

    local hue = self.LevelUpTime * 0.45
    local pulse = 0.82 + math.sin(self.LevelUpTime * 8.0) * 0.12
    local r, g, b = hsvToRgb(hue, 0.88, pulse)
    self.Fill:SetUITint(r, g, b, 0.98)
end

function Script:Tick(deltaTime)
    self:UpdateExpBar()
    self:UpdateLevelUpEffect(deltaTime)
end

return Script
