---@class MainMenuButtonIntro : ScriptComponent
local Script = {
    properties = {
        AutoPlay = { type = "bool", default = true },
        Delay = { type = "float", default = 0.86, min = 0.0, max = 5.0, speed = 0.01 },
        EnterDuration = { type = "float", default = 0.30, min = 0.05, max = 2.0, speed = 0.01 },
        SettleDuration = { type = "float", default = 0.12, min = 0.0, max = 1.0, speed = 0.01 },
        StartScale = { type = "float", default = 0.86, min = 0.1, max = 2.0, speed = 0.01 },
        OvershootScale = { type = "float", default = 1.045, min = 0.5, max = 2.0, speed = 0.005 },
        StartYOffset = { type = "float", default = 42.0, min = -300.0, max = 300.0, speed = 1.0 },
        SettleYOffset = { type = "float", default = -5.0, min = -100.0, max = 100.0, speed = 1.0 },
        HoverScale = { type = "float", default = 1.045, min = 1.0, max = 1.3, speed = 0.005 },
        PressedScale = { type = "float", default = 0.955, min = 0.5, max = 1.0, speed = 0.005 },
        ClickPulseScale = { type = "float", default = 1.080, min = 1.0, max = 1.4, speed = 0.005 },
        ClickPulseDuration = { type = "float", default = 0.18, min = 0.01, max = 1.0, speed = 0.01 },
        InteractionLerpSpeed = { type = "float", default = 16.0, min = 1.0, max = 60.0, speed = 0.5 },
        HoverTintScale = { type = "float", default = 1.12, min = 1.0, max = 2.0, speed = 0.01 },
        PressedTintScale = { type = "float", default = 0.82, min = 0.1, max = 1.0, speed = 0.01 },
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

local function scaledColor(color, tintScale, alphaScale)
    local scale = tintScale or 1.0
    local alpha = alphaScale or 1.0
    return {
        r = clamp(color.r * scale, 0.0, 1.0),
        g = clamp(color.g * scale, 0.0, 1.0),
        b = clamp(color.b * scale, 0.0, 1.0),
        a = clamp(color.a * alpha, 0.0, 1.0),
    }
end

function Script:CaptureFinalState()
    self.ui = self:GetActor():GetComponent("UUIButtonComponent")
    if self.ui == nil or not self.ui:IsValid() then
        self.ui = self:GetActor():GetComponent("UUIComponent")
    end

    if self.ui == nil or not self.ui:IsValid() then
        Log("[MainMenuButtonIntro] UUIButtonComponent not found")
        return false
    end

    self.finalAnchorMin = copyVec2(self.ui:GetUIAnchorMin(), 0.35, 0.5)
    self.finalAnchorMax = copyVec2(self.ui:GetUIAnchorMax(), 0.65, 0.6)
    self.finalPosition = copyVec2(self.ui:GetUIAnchoredPosition(), 0.0, 0.0)
    self.finalRotation = self.ui:GetUIRotationDegrees() or 0.0
    self.finalTint = copyColor(self.ui:GetUITint())
    self.finalInteractable = true
    if self.ui.IsButtonInteractable ~= nil then
        self.finalInteractable = self.ui:IsButtonInteractable()
    end
    if self.ui.GetButtonClickCount ~= nil then
        self.lastClickCount = self.ui:GetButtonClickCount()
    else
        self.lastClickCount = 0
    end
    return true
end

function Script:ApplyState(scale, yOffset, alpha, interactable, tintScale)
    if self.ui == nil or not self.ui:IsValid() then
        return
    end

    if self.ui.SetButtonInteractable ~= nil then
        self.ui:SetButtonInteractable(interactable == true)
    end

    local rect = scaledAnchorRect(self.finalAnchorMin, self.finalAnchorMax, scale)
    self.ui:SetUIAnchorMin(rect.min)
    self.ui:SetUIAnchorMax(rect.max)
    self.ui:SetUIAnchoredPosition({
        x = self.finalPosition.x,
        y = self.finalPosition.y + yOffset,
    })
    self.ui:SetUIRotationDegrees(self.finalRotation)
    local tint = scaledColor(self.finalTint, tintScale or 1.0, alpha)
    self.ui:SetUITint(
        tint.r,
        tint.g,
        tint.b,
        tint.a
    )
    self.ui:SetUIVisibility(true)
end

function Script:ApplyHiddenState()
    self:ApplyState(
        self.StartScale or 0.86,
        self.StartYOffset or 42.0,
        0.0,
        false
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

    if self.ui.SetButtonInteractable ~= nil then
        self.ui:SetButtonInteractable(self.finalInteractable)
    end

    self.currentScale = 1.0
    self.clickPulseTime = nil
    if self.ui.GetButtonClickCount ~= nil then
        self.lastClickCount = self.ui:GetButtonClickCount()
    end
end

function Script:StartButtonIntro()
    if not self:CaptureFinalState() then
        return
    end

    self.elapsed = -(self.Delay or 0.0)
    self.playing = true
    self:ApplyHiddenState()
end

function Script:SkipButtonIntro()
    self.playing = false
    self:ApplyFinalState()
end

function Script:BeginPlay()
    self.elapsed = 0.0
    self.playing = false
    self.currentScale = 1.0
    self.clickPulseTime = nil

    if not self:CaptureFinalState() then
        return
    end

    if self.AutoPlay then
        self:StartButtonIntro()
    else
        self:ApplyFinalState()
    end
end

function Script:UpdateInteraction(deltaTime)
    if self.ui == nil or not self.ui:IsValid() then
        return
    end

    local hovered = false
    local pressed = false
    if self.ui.IsButtonHovered ~= nil then
        hovered = self.ui:IsButtonHovered()
    end
    if self.ui.IsButtonPressed ~= nil then
        pressed = self.ui:IsButtonPressed()
    end

    if self.ui.GetButtonClickCount ~= nil then
        local clickCount = self.ui:GetButtonClickCount()
        if self.lastClickCount == nil then
            self.lastClickCount = clickCount
        elseif clickCount > self.lastClickCount then
            self.clickPulseTime = 0.0
            self.lastClickCount = clickCount
        end
    end

    local targetScale = 1.0
    local tintScale = 1.0
    if hovered then
        targetScale = self.HoverScale or 1.045
        tintScale = self.HoverTintScale or 1.12
    end
    if pressed then
        targetScale = self.PressedScale or 0.955
        tintScale = self.PressedTintScale or 0.82
    end

    if self.clickPulseTime ~= nil then
        local duration = math.max(self.ClickPulseDuration or 0.18, 0.01)
        local t = clamp(self.clickPulseTime / duration, 0.0, 1.0)
        local pulse = math.sin(t * math.pi)
        targetScale = math.max(targetScale, lerp(1.0, self.ClickPulseScale or 1.08, pulse))
        tintScale = math.max(tintScale, lerp(1.0, self.HoverTintScale or 1.12, pulse))

        self.clickPulseTime = self.clickPulseTime + deltaTime
        if self.clickPulseTime >= duration then
            self.clickPulseTime = nil
        end
    end

    local lerpAlpha = clamp(deltaTime * (self.InteractionLerpSpeed or 16.0), 0.0, 1.0)
    self.currentScale = lerp(self.currentScale or 1.0, targetScale, lerpAlpha)
    self:ApplyState(self.currentScale, 0.0, 1.0, self.finalInteractable, tintScale)
end

function Script:Tick(deltaTime)
    if not self.playing then
        self:UpdateInteraction(deltaTime)
        return
    end

    self.elapsed = self.elapsed + deltaTime
    if self.elapsed < 0.0 then
        self:ApplyHiddenState()
        return
    end

    local enterDuration = math.max(self.EnterDuration or 0.30, 0.01)
    local settleDuration = math.max(self.SettleDuration or 0.12, 0.0)
    local totalDuration = enterDuration + settleDuration

    if self.elapsed <= enterDuration then
        local t = easeOutCubic(self.elapsed / enterDuration)
        self:ApplyState(
            lerp(self.StartScale or 0.86, self.OvershootScale or 1.045, t),
            lerp(self.StartYOffset or 42.0, self.SettleYOffset or -5.0, t),
            t,
            false
        )
        return
    end

    if self.elapsed <= totalDuration and settleDuration > 0.0 then
        local t = easeInOutSine((self.elapsed - enterDuration) / settleDuration)
        self:ApplyState(
            lerp(self.OvershootScale or 1.045, 1.0, t),
            lerp(self.SettleYOffset or -5.0, 0.0, t),
            1.0,
            false
        )
        return
    end

    self.playing = false
    self:ApplyFinalState()
    self:UpdateInteraction(deltaTime)
end

function Script:EndPlay()
    if self.ui ~= nil and self.ui:IsValid() then
        self:ApplyFinalState()
    end
end

return Script
