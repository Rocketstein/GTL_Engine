---@class LevelUpChoice : ScriptComponent
local Script = {
    properties = {
        PanelWidth = { type = "float", default = 680.0, min = 320.0, max = 1200.0, speed = 10.0 },
        PanelHeight = { type = "float", default = 520.0, min = 240.0, max = 900.0, speed = 10.0 },
        OptionWidth = { type = "float", default = 560.0, min = 240.0, max = 1000.0, speed = 10.0 },
        OptionHeight = { type = "float", default = 112.0, min = 44.0, max = 180.0, speed = 2.0 },
    }
}

local GameManager = require("GameManager")
local UIStyle = require("UI.UIStyle")

local function vec2(x, y)
    return { x = x, y = y }
end

local function clamp(value, minValue, maxValue)
    if value < minValue then
        return minValue
    end
    if value > maxValue then
        return maxValue
    end
    return value
end

local function lerp(a, b, t)
    return a + (b - a) * t
end

local function setTint(ui, tint)
    if ui == nil or not ui:IsValid() or tint == nil then
        return
    end

    ui:SetUITint(tint[1], tint[2], tint[3], tint[4])
end

local function configureUI(ui, anchor, position, size, layer, zOrder, tint, hitTest)
    if ui == nil or not ui:IsValid() then
        return false
    end

    ui:SetUIRenderSpace("ScreenSpace")
    ui:SetUIAnchor(anchor)
    ui:SetUIAnchoredPosition(position or vec2(0.0, 0.0))
    ui:SetUISizeDelta(size)
    ui:SetUIPivot(vec2(0.5, 0.5))
    ui:SetUIRotationDegrees(0.0)
    ui:SetUILayer(layer)
    ui:SetUIZOrder(zOrder)
    ui:SetUIHitTestVisible(hitTest == true)

    setTint(ui, tint)
    ui:SetUIVisibility(true)
    return true
end

local function hideActorUI(actor)
    if actor == nil or not actor:IsValid() then
        return
    end

    local components = actor:GetComponents("UUIComponent")
    if components == nil then
        return
    end

    for _, component in ipairs(components) do
        if component ~= nil and component:IsValid() and component.SetUIVisibility ~= nil then
            component:SetUIVisibility(false)
        end
    end
end

local function getOptionBadge(option)
    if option == nil then
        return ""
    end

    if option.Type == "AddWeapon" then
        return "NEW"
    end

    local currentLevel = option.CurrentLevel or 1
    local nextLevel = option.NextLevel or (currentLevel + 1)
    return "Lv." .. tostring(currentLevel) .. " > Lv." .. tostring(nextLevel)
end

function Script:BeginPlay()
    self.pool = GetActorPoolManager()
    self.actors = {}
    self.optionButtons = {}
    self.visible = false
    self.choiceLocked = false
    self.GemRainScript = self.GetComponentByName("UScriptComponent", "GemRainScript")

    self:SetGemRainEnabled(false)
    GameManager.RegisterLevelUpUI(self)
end

function Script:SetGemRainEnabled(enabled)
    if self.GemRainScript ~= nil and self.GemRainScript:IsValid() then
        self.GemRainScript:CallScript("SetEnabled", enabled == true)
    end
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
    table.insert(self.actors, actor)
    return actor
end

function Script:CreateImage(anchor, position, size, layer, zOrder, tint, texturePath)
    local actor = self:AcquireActor("AUIActor")
    if actor == nil then
        return nil
    end

    local ui = actor:GetComponent("UTextureUIComponent")
    if ui == nil or not ui:IsValid() then
        ui = actor:GetComponent("UUIComponent")
    end

    if not configureUI(ui, anchor, position, size, layer, zOrder, tint, false) then
        return nil
    end

    if texturePath ~= nil and ui.SetUITexturePath ~= nil then
        ui:SetUITexturePath(texturePath)
    end

    return ui
end

function Script:CreateText(text, anchor, position, size, layer, zOrder, fontSize, tint, horizontal)
    local actor = self:AcquireActor("ATextUIActor")
    if actor == nil then
        return nil
    end

    local ui = actor:GetComponent("UTextUIComponent")
    if not configureUI(ui, anchor, position, size, layer, zOrder, tint or UIStyle.Colors.Text, false) then
        return nil
    end

    ui:SetUIText(text)
    ui:SetUIFont("Default")
    ui:SetUIFontSize(fontSize or 1.0)
    ui:SetUITextHorizontalAlignment(horizontal or "Center")
    ui:SetUITextVerticalAlignment("Center")
    return ui
end

function Script:GetCardMetrics()
    return {
        panelWidth = math.max(self.PanelWidth or 680.0, 640.0),
        panelHeight = math.max(self.PanelHeight or 520.0, 500.0),
        optionWidth = math.max(self.OptionWidth or 560.0, 540.0),
        optionHeight = math.max(self.OptionHeight or 112.0, 108.0),
        iconSize = 70.0,
    }
end

function Script:CreateButton(option, index, anchor, metrics)
    local actor = self:AcquireActor("AButtonActor")
    if actor == nil then
        return
    end

    local layer = 320
    local zBase = index * 20
    local button = actor:GetComponent("UUIButtonComponent")
    configureUI(
        button,
        anchor,
        vec2(0.0, 0.0),
        vec2(metrics.optionWidth, metrics.optionHeight),
        layer,
        zBase,
        UIStyle.Colors.Card,
        true
    )

    if button ~= nil and button:IsValid() then
        button:SetButtonInteractable(true)
        button:SetUITexturePath("")
    end

    local weaponId = option ~= nil and option.WeaponId or nil
    local displayName = option ~= nil and (option.DisplayName or option.WeaponId) or "Unknown"
    local currentLevel = option ~= nil and option.CurrentLevel or 0
    local nextLevel = option ~= nil and option.NextLevel or 1
    local iconPath = UIStyle.GetSkillIconPath(weaponId)
    local description = UIStyle.GetWeaponDescription(weaponId)
    local preview = UIStyle.GetUpgradePreview(weaponId, currentLevel, nextLevel)
    local badge = getOptionBadge(option)
    local left = -metrics.optionWidth * 0.5

    local icon = self:CreateImage(
        anchor,
        vec2(left + 66.0, 0.0),
        vec2(metrics.iconSize, metrics.iconSize),
        layer + 1,
        zBase,
        { 1.0, 1.0, 1.0, 1.0 },
        iconPath
    )
    self:CreateImage(
        anchor,
        vec2(left + 66.0, 0.0),
        vec2(metrics.iconSize + 16.0, metrics.iconSize + 16.0),
        layer + 1,
        zBase - 1,
        { 0.0, 0.0, 0.0, 0.25 },
        ""
    )

    self:CreateText(
        displayName,
        anchor,
        vec2(42.0, -29.0),
        vec2(metrics.optionWidth - 170.0, 28.0),
        layer + 2,
        zBase,
        0.92,
        UIStyle.Colors.Text,
        "Left"
    )
    self:CreateText(
        description,
        anchor,
        vec2(42.0, 2.0),
        vec2(metrics.optionWidth - 170.0, 26.0),
        layer + 2,
        zBase + 1,
        0.64,
        UIStyle.Colors.SubText,
        "Left"
    )
    self:CreateText(
        preview,
        anchor,
        vec2(42.0, 30.0),
        vec2(metrics.optionWidth - 170.0, 24.0),
        layer + 2,
        zBase + 2,
        0.58,
        UIStyle.Colors.MutedText,
        "Left"
    )

    local badgeBack = self:CreateImage(
        anchor,
        vec2(metrics.optionWidth * 0.5 - 84.0, -30.0),
        vec2(128.0, 30.0),
        layer + 1,
        zBase + 2,
        { 0.16, 0.12, 0.045, 0.92 },
        ""
    )
    local badgeText = self:CreateText(
        badge,
        anchor,
        vec2(metrics.optionWidth * 0.5 - 84.0, -30.0),
        vec2(118.0, 26.0),
        layer + 2,
        zBase + 3,
        0.58,
        UIStyle.Colors.Gold,
        "Center"
    )

    self.optionButtons[index] = {
        component = button,
        icon = icon,
        badgeBack = badgeBack,
        badgeText = badgeText,
        lastClickCount = button ~= nil and button:IsValid() and button:GetButtonClickCount() or 0,
        width = metrics.optionWidth,
        height = metrics.optionHeight,
        iconSize = metrics.iconSize,
        scale = 1.0,
        tint = UIStyle.Colors.Card,
    }
end

function Script:ReleaseActors()
    if self.pool == nil or not self.pool:IsValid() then
        self.actors = {}
        return
    end

    for _, actor in ipairs(self.actors) do
        if actor ~= nil and actor:IsValid() then
            hideActorUI(actor)
            actor:SetVisible(false)
            self.pool:Release(actor)
        end
    end

    self.actors = {}
end

function Script:Show(options)
    self:Hide()

    self.visible = true
    self.choiceLocked = false
    self.options = options or {}
    self.optionButtons = {}

    self:SetGemRainEnabled(true)

    local metrics = self:GetCardMetrics()
    self:CreateImage(vec2(0.5, 0.5), vec2(0.0, 0.0), vec2(4000.0, 4000.0), 300, 0, UIStyle.Colors.Overlay, "")
    self:CreateImage(
        vec2(0.5, 0.5),
        vec2(0.0, 0.0),
        vec2(metrics.panelWidth, metrics.panelHeight),
        310,
        0,
        UIStyle.Colors.Panel,
        ""
    )
    self:CreateImage(vec2(0.5, 0.275), vec2(0.0, 0.0), vec2(180.0, 3.0), 311, 0, UIStyle.Colors.PanelAccent, "")
    self:CreateText("LEVEL UP", vec2(0.5, 0.305), vec2(0.0, 0.0), vec2(560.0, 50.0), 321, 0, 1.35, UIStyle.Colors.Gold)
    self:CreateText("Choose one upgrade to continue", vec2(0.5, 0.352), vec2(0.0, 0.0), vec2(560.0, 26.0), 321, 1, 0.62, UIStyle.Colors.MutedText)

    local startY = 0.440
    local spacing = 0.122
    for index = 1, math.min(3, #self.options) do
        self:CreateButton(self.options[index], index, vec2(0.5, startY + spacing * (index - 1)), metrics)
    end
end

function Script:Hide()
    self.visible = false
    self.choiceLocked = false
    self.options = nil
    self.optionButtons = {}
    self:SetGemRainEnabled(false)
    self:ReleaseActors()
end

function Script:UpdateCard(entry, deltaTime)
    local button = entry.component
    if button == nil or not button:IsValid() then
        return
    end

    local hovered = button.IsButtonHovered ~= nil and button:IsButtonHovered()
    local pressed = button.IsButtonPressed ~= nil and button:IsButtonPressed()
    local targetScale = 1.0
    local targetTint = UIStyle.Colors.Card
    local badgeTint = { 0.16, 0.12, 0.045, 0.92 }

    if hovered then
        targetScale = 1.025
        targetTint = UIStyle.Colors.CardHover
        badgeTint = { 0.22, 0.16, 0.055, 0.95 }
    end
    if pressed then
        targetScale = 0.985
        targetTint = UIStyle.Colors.CardPressed
        badgeTint = { 0.12, 0.09, 0.040, 0.95 }
    end

    local alpha = clamp((deltaTime or 0.0) * 18.0, 0.0, 1.0)
    entry.scale = lerp(entry.scale or 1.0, targetScale, alpha)
    entry.tint = UIStyle.LerpColor(entry.tint or UIStyle.Colors.Card, targetTint, alpha)

    button:SetUISizeDelta(vec2(entry.width * entry.scale, entry.height * entry.scale))
    setTint(button, entry.tint)

    if entry.icon ~= nil and entry.icon:IsValid() then
        local iconScale = hovered and 1.05 or 1.0
        if pressed then
            iconScale = 0.98
        end
        entry.icon:SetUISizeDelta(vec2(entry.iconSize * iconScale, entry.iconSize * iconScale))
        setTint(entry.icon, hovered and { 1.0, 0.96, 0.78, 1.0 } or { 1.0, 1.0, 1.0, 1.0 })
    end

    setTint(entry.badgeBack, badgeTint)
    setTint(entry.badgeText, hovered and UIStyle.Colors.Text or UIStyle.Colors.Gold)
end

function Script:Tick(deltaTime)
    if not self.visible then
        return
    end

    for index, entry in pairs(self.optionButtons) do
        self:UpdateCard(entry, deltaTime)
        if not self.choiceLocked then
            local button = entry.component
            if button ~= nil and button:IsValid() then
                local clickCount = button:GetButtonClickCount()
                if clickCount ~= entry.lastClickCount then
                    self.choiceLocked = true
                    for _, other in pairs(self.optionButtons) do
                        if other.component ~= nil and other.component:IsValid() then
                            other.component:SetButtonInteractable(false)
                        end
                    end
                    GameManager.ConfirmLevelUpChoice(index)
                    return
                end
            end
        end
    end
end

function Script:EndPlay()
    self:Hide()
end

return Script
