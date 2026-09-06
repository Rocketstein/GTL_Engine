---@class MainTitleIntro : ScriptComponent
local Script = {
    properties = {
        AutoPlay = { type = "bool", default = true },
        Delay = { type = "float", default = 0.12, min = 0.0, max = 3.0, speed = 0.01 },
        EnterDuration = { type = "float", default = 0.48, min = 0.05, max = 3.0, speed = 0.01 },
        SettleDuration = { type = "float", default = 0.18, min = 0.0, max = 2.0, speed = 0.01 },
        StartScale = { type = "float", default = 0.76, min = 0.1, max = 2.0, speed = 0.01 },
        OvershootScale = { type = "float", default = 1.035, min = 0.5, max = 2.0, speed = 0.005 },
        StartYOffset = { type = "float", default = -92.0, min = -500.0, max = 500.0, speed = 1.0 },
        SettleYOffset = { type = "float", default = -8.0, min = -100.0, max = 100.0, speed = 1.0 },
        StartRotation = { type = "float", default = -2.0, min = -45.0, max = 45.0, speed = 0.1 },
        OvershootRotation = { type = "float", default = 0.7, min = -45.0, max = 45.0, speed = 0.1 },
    }
}

local function clamp(value, minValue, maxValue)
    if value < minValue then return minValue end
    if value > maxValue then return maxValue end
    return value
end

local function lerp(a, b, t)
    return a + (b - a) * t
end

local function easeOutCubic(t)
    t = clamp(t, 0.0, 1.0)
    local inv = 1.0 - t
    return 1.0 - inv * inv * inv
end

local function easeInOutSine(t)
    t = clamp(t, 0.0, 1.0)
    return 0.5 - 0.5 * math.cos(t * math.pi)
end

local function copyVec2(value, fallbackX, fallbackY)
    if value == nil then
        return { x = fallbackX or 0.0, y = fallbackY or 0.0 }
    end

    return {
        x = value.x or value.X or value[1] or fallbackX or 0.0,
        y = value.y or value.Y or value[2] or fallbackY or 0.0,
    }
end

local function copyColor(value)
    if value == nil then
        return { r = 1.0, g = 1.0, b = 1.0, a = 1.0 }
    end

    return {
        r = value.r or value.R or value.x or value.X or value[1] or 1.0,
        g = value.g or value.G or value.y or value.Y or value[2] or 1.0,
        b = value.b or value.B or value.z or value.Z or value[3] or 1.0,
        a = value.a or value.A or value.w or value.W or value[4] or 1.0,
    }
end

local function scaledAnchorRect(anchorMin, anchorMax, scale)
    local centerX = (anchorMin.x + anchorMax.x) * 0.5
    local centerY = (anchorMin.y + anchorMax.y) * 0.5
    local halfX = (anchorMax.x - anchorMin.x) * 0.5 * scale
    local halfY = (anchorMax.y - anchorMin.y) * 0.5 * scale

    return {
        min = { x = centerX - halfX, y = centerY - halfY },
        max = { x = centerX + halfX, y = centerY + halfY },
    }
end

function Script:CaptureFinalState()
    self.ui = self:GetActor():GetComponent("UTextureUIComponent")
    if self.ui == nil or not self.ui:IsValid() then
        self.ui = self:GetActor():GetComponent("UUIComponent")
    end

    if self.ui == nil or not self.ui:IsValid() then
        Log("[MainTitleIntro] UTextureUIComponent not found")
        return false
    end

    self.finalAnchorMin = copyVec2(self.ui:GetUIAnchorMin(), 0.1, 0.03)
    self.finalAnchorMax = copyVec2(self.ui:GetUIAnchorMax(), 0.9, 0.48)
    self.finalPosition = copyVec2(self.ui:GetUIAnchoredPosition(), 0.0, 0.0)
    self.finalRotation = self.ui:GetUIRotationDegrees() or 0.0
    self.finalTint = copyColor(self.ui:GetUITint())
    return true
end

function Script:ApplyState(scale, yOffset, rotationOffset, alpha)
    if self.ui == nil or not self.ui:IsValid() then
        return
    end

    local rect = scaledAnchorRect(self.finalAnchorMin, self.finalAnchorMax, scale)
    self.ui:SetUIAnchorMin(rect.min)
    self.ui:SetUIAnchorMax(rect.max)
    self.ui:SetUIAnchoredPosition({
        x = self.finalPosition.x,
        y = self.finalPosition.y + yOffset,
    })
    self.ui:SetUIRotationDegrees(self.finalRotation + rotationOffset)
    self.ui:SetUITint(
        self.finalTint.r,
        self.finalTint.g,
        self.finalTint.b,
        self.finalTint.a * clamp(alpha, 0.0, 1.0)
    )
    self.ui:SetUIVisibility(true)
end

function Script:ApplyHiddenState()
    self:ApplyState(
        self.StartScale or 0.76,
        self.StartYOffset or -92.0,
        self.StartRotation or -2.0,
        0.0
    )
end

function Script:ApplyFinalState()
    if self.ui == nil or not self.ui:IsValid() then
        return
    end

    self.ui:SetUIAnchorMin(self.finalAnchorMin)
    self.ui:SetUIAnchorMax(self.finalAnchorMax)
    self.ui:SetUIAnchoredPosition(self.finalPosition)
    self.ui:SetUIRotationDegrees(self.finalRotation)
    self.ui:SetUITint(self.finalTint.r, self.finalTint.g, self.finalTint.b, self.finalTint.a)
    self.ui:SetUIVisibility(true)
end

function Script:StartTitleIntro()
    if not self:CaptureFinalState() then
        return
    end

    self.elapsed = -(self.Delay or 0.0)
    self.playing = true
    self:ApplyHiddenState()
end

function Script:SkipTitleIntro()
    self.playing = false
    self:ApplyFinalState()
end

function Script:BeginPlay()
    self.elapsed = 0.0
    self.playing = false

    if not self:CaptureFinalState() then
        return
    end

    if self.AutoPlay then
        self:StartTitleIntro()
    else
        self:ApplyFinalState()
    end
end

function Script:Tick(deltaTime)
    if not self.playing then
        return
    end

    self.elapsed = self.elapsed + deltaTime
    if self.elapsed < 0.0 then
        self:ApplyHiddenState()
        return
    end

    local enterDuration = math.max(self.EnterDuration or 0.48, 0.01)
    local settleDuration = math.max(self.SettleDuration or 0.18, 0.0)
    local totalDuration = enterDuration + settleDuration

    if self.elapsed <= enterDuration then
        local t = easeOutCubic(self.elapsed / enterDuration)
        self:ApplyState(
            lerp(self.StartScale or 0.76, self.OvershootScale or 1.035, t),
            lerp(self.StartYOffset or -92.0, self.SettleYOffset or -8.0, t),
            lerp(self.StartRotation or -2.0, self.OvershootRotation or 0.7, t),
            t
        )
        return
    end

    if self.elapsed <= totalDuration and settleDuration > 0.0 then
        local t = easeInOutSine((self.elapsed - enterDuration) / settleDuration)
        self:ApplyState(
            lerp(self.OvershootScale or 1.035, 1.0, t),
            lerp(self.SettleYOffset or -8.0, 0.0, t),
            lerp(self.OvershootRotation or 0.7, 0.0, t),
            1.0
        )
        return
    end

    self.playing = false
    self:ApplyFinalState()
end

function Script:EndPlay()
    if self.ui ~= nil and self.ui:IsValid() then
        self:ApplyFinalState()
    end
end

return Script
