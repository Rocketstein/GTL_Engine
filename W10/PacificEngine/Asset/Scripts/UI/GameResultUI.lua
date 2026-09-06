---@class GameResultUI : ScriptComponent
local Script = {
    properties = {
        PanelWidth = { type = "float", default = 720.0, min = 360.0, max = 1200.0, speed = 10.0 },
        PanelHeight = { type = "float", default = 560.0, min = 260.0, max = 900.0, speed = 10.0 },
        ButtonWidth = { type = "float", default = 260.0, min = 120.0, max = 520.0, speed = 10.0 },
        ButtonHeight = { type = "float", default = 58.0, min = 32.0, max = 120.0, speed = 2.0 },
        Layer = { type = "int", default = 380, min = 0, max = 1000, speed = 1 },
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

local function formatTime(timeRemaining)
    local totalSeconds = math.max(0, math.floor((tonumber(timeRemaining) or 0.0) + 0.5))
    local minutes = math.floor(totalSeconds / 60)
    local seconds = totalSeconds % 60
    return string.format("%02d:%02d", minutes, seconds)
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

local function resultTitle(resultType)
    if resultType == "GameClear" then
        return "GAME CLEAR"
    end

    return "GAME OVER"
end

local function resultShort(resultType)
    if resultType == "GameClear" then
        return "CLEAR"
    end

    return "OVER"
end

local function rankColor(rank)
    if rank == 1 then
        return UIStyle.Colors.Gold
    end
    if rank == 2 then
        return UIStyle.Colors.Silver
    end
    if rank == 3 then
        return UIStyle.Colors.Bronze
    end
    return UIStyle.Colors.Text
end

function Script:BeginPlay()
    self.pool = GetActorPoolManager()
    self.actors = {}
    self.buttons = {}
    self.visible = false
    self.locked = false

    GameManager.RegisterResultUI(self)
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

function Script:CreateImage(anchor, position, size, layer, zOrder, tint)
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

    if ui.SetUITexturePath ~= nil then
        ui:SetUITexturePath("")
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

function Script:CreateButton(label, anchor, position, action)
    local actor = self:AcquireActor("AButtonActor")
    if actor == nil then
        return nil
    end

    local layer = self.Layer or 380
    local width = self.ButtonWidth or 260.0
    local height = self.ButtonHeight or 58.0
    local button = actor:GetComponent("UUIButtonComponent")
    if not configureUI(
        button,
        anchor,
        position,
        vec2(width, height),
        layer + 4,
        0,
        UIStyle.Colors.Button,
        true
    ) then
        return nil
    end

    button:SetUITexturePath("")
    button:SetButtonInteractable(true)

    local labelUI = self:CreateText(
        label,
        anchor,
        position,
        vec2(width - 20.0, height),
        layer + 5,
        0,
        0.96,
        UIStyle.Colors.Text,
        "Center"
    )

    table.insert(self.buttons, {
        component = button,
        label = labelUI,
        lastClickCount = button:GetButtonClickCount() or 0,
        action = action,
        width = width,
        height = height,
        scale = 1.0,
        tint = UIStyle.Colors.Button,
    })

    return button
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

function Script:Hide()
    self.visible = false
    self.locked = false
    self.buttons = {}
    self:ReleaseActors()
end

function Script:GetRecords(source)
    if source ~= nil and source.Records ~= nil then
        return source.Records
    end

    if type(ScoreBoard_LoadRecords) == "function" then
        return ScoreBoard_LoadRecords()
    end

    return {}
end

function Script:CreateFrame(title, titleTint)
    local layer = self.Layer or 380
    local panelWidth = self.PanelWidth or 720.0
    local panelHeight = self.PanelHeight or 560.0
    self:CreateImage(vec2(0.5, 0.5), vec2(0.0, 0.0), vec2(4000.0, 4000.0), layer, 0, UIStyle.Colors.Overlay)
    self:CreateImage(vec2(0.5, 0.5), vec2(0.0, 0.0), vec2(panelWidth, panelHeight), layer + 1, 0, UIStyle.Colors.Panel)
    self:CreateImage(vec2(0.5, 0.252), vec2(0.0, 0.0), vec2(220.0, 3.0), layer + 2, 0, UIStyle.Colors.PanelAccent)
    self:CreateText(title, vec2(0.5, 0.225), vec2(0.0, 0.0), vec2(640.0, 62.0), layer + 3, 0, 1.42, titleTint or UIStyle.Colors.Gold)
end

function Script:CreateStatChip(label, value, xOffset, yAnchor, accentColor)
    local layer = (self.Layer or 380) + 2
    self:CreateImage(vec2(0.5, yAnchor), vec2(xOffset, 0.0), vec2(178.0, 62.0), layer, 0, { 0.070, 0.088, 0.120, 0.94 })
    self:CreateImage(vec2(0.5, yAnchor), vec2(xOffset, -29.0), vec2(144.0, 2.0), layer + 1, 0, accentColor or UIStyle.Colors.Gold)
    self:CreateText(label, vec2(0.5, yAnchor), vec2(xOffset, -13.0), vec2(150.0, 18.0), layer + 2, 0, 0.52, UIStyle.Colors.MutedText)
    self:CreateText(value, vec2(0.5, yAnchor), vec2(xOffset, 12.0), vec2(150.0, 28.0), layer + 2, 1, 0.88, UIStyle.Colors.Text)
end

function Script:CreateScoreHeader(yAnchor, layer)
    local headerTint = { 0.055, 0.070, 0.100, 0.98 }
    self:CreateImage(vec2(0.5, yAnchor), vec2(0.0, 0.0), vec2(620.0, 28.0), layer, 0, headerTint)
    self:CreateText("RANK", vec2(0.5, yAnchor), vec2(-252.0, 0.0), vec2(80.0, 24.0), layer + 1, 0, 0.54, UIStyle.Colors.MutedText)
    self:CreateText("RESULT", vec2(0.5, yAnchor), vec2(-125.0, 0.0), vec2(120.0, 24.0), layer + 1, 0, 0.54, UIStyle.Colors.MutedText)
    self:CreateText("TIME", vec2(0.5, yAnchor), vec2(40.0, 0.0), vec2(120.0, 24.0), layer + 1, 0, 0.54, UIStyle.Colors.MutedText)
    self:CreateText("KILLS", vec2(0.5, yAnchor), vec2(218.0, 0.0), vec2(100.0, 24.0), layer + 1, 0, 0.54, UIStyle.Colors.MutedText)
end

function Script:CreateScoreRow(record, index, yAnchor, layer, highlightRank)
    local rank = tonumber(record.Rank) or index
    local isHighlight = highlightRank ~= nil and rank == highlightRank
    local rowTint = index % 2 == 0 and { 0.050, 0.062, 0.086, 0.86 } or { 0.038, 0.050, 0.074, 0.86 }
    if isHighlight then
        rowTint = UIStyle.Colors.CardHighlight
    end

    self:CreateImage(vec2(0.5, yAnchor), vec2(0.0, 0.0), vec2(620.0, 28.0), layer, index, rowTint)

    local result = resultShort(record.ResultType)
    local resultTint = record.ResultType == "GameClear" and UIStyle.Colors.Clear or UIStyle.Colors.Over
    local rankTint = rankColor(rank)
    if isHighlight then
        rankTint = UIStyle.Colors.Gold
    end

    self:CreateText("#" .. string.format("%02d", rank), vec2(0.5, yAnchor), vec2(-252.0, 0.0), vec2(80.0, 24.0), layer + 1, index, 0.58, rankTint)
    self:CreateText(result, vec2(0.5, yAnchor), vec2(-125.0, 0.0), vec2(120.0, 24.0), layer + 1, index, 0.58, resultTint)
    self:CreateText(formatTime(record.RemainingTime), vec2(0.5, yAnchor), vec2(40.0, 0.0), vec2(120.0, 24.0), layer + 1, index, 0.58, UIStyle.Colors.Text)
    self:CreateText(tostring(record.KillCount or 0), vec2(0.5, yAnchor), vec2(218.0, 0.0), vec2(100.0, 24.0), layer + 1, index, 0.58, UIStyle.Colors.Text)
end

function Script:CreateScoreRows(records, startY, layer, highlightRank, highlightRecord)
    local rowStep = 0.029
    self:CreateScoreHeader(startY, layer)

    local displayRecords = {}
    local sourceRecords = records or {}
    local sourceCount = #sourceRecords
    local maxRows = math.min(10, sourceCount)
    if highlightRecord ~= nil and highlightRank ~= nil and highlightRank > 10 then
        maxRows = math.min(9, sourceCount)
    end

    for index = 1, maxRows do
        table.insert(displayRecords, sourceRecords[index])
    end

    if highlightRecord ~= nil and highlightRank ~= nil and highlightRank > 10 then
        table.insert(displayRecords, highlightRecord)
    end

    maxRows = #displayRecords
    if maxRows <= 0 then
        self:CreateImage(vec2(0.5, startY + 0.080), vec2(0.0, 0.0), vec2(620.0, 74.0), layer, 1, { 0.045, 0.057, 0.080, 0.88 })
        self:CreateText("NO RECORDS YET", vec2(0.5, startY + 0.064), vec2(0.0, 0.0), vec2(620.0, 30.0), layer + 1, 1, 0.78, UIStyle.Colors.Text)
        self:CreateText("Finish a run to place your first score.", vec2(0.5, startY + 0.103), vec2(0.0, 0.0), vec2(620.0, 24.0), layer + 1, 2, 0.54, UIStyle.Colors.MutedText)
        return
    end

    for index = 1, maxRows do
        self:CreateScoreRow(displayRecords[index], index, startY + rowStep * index, layer, highlightRank)
    end
end

function Script:ShowResult(record)
    self:Hide()

    self.visible = true
    self.locked = false
    self.buttons = {}

    local layer = self.Layer or 380
    local resultType = record ~= nil and record.ResultType or "GameOver"
    local titleTint = resultType == "GameClear" and UIStyle.Colors.Clear or UIStyle.Colors.Over
    self:CreateFrame(resultTitle(resultType), titleTint)

    local remainingTime = record ~= nil and record.RemainingTime or 0.0
    local killCount = record ~= nil and record.KillCount or 0
    local rank = record ~= nil and (tonumber(record.Rank) or 0) or 0
    local rankText = rank > 0 and ("#" .. tostring(rank)) or "--"

    self:CreateStatChip("TIME LEFT", formatTime(remainingTime), -205.0, 0.335, UIStyle.Colors.Gold)
    self:CreateStatChip("KILLS", tostring(killCount), 0.0, 0.335, UIStyle.Colors.Clear)
    self:CreateStatChip("RANK", rankText, 205.0, 0.335, UIStyle.Colors.PanelAccent)

    self:CreateText("TOP RECORDS", vec2(0.5, 0.425), vec2(0.0, 0.0), vec2(620.0, 28.0), layer + 3, 0, 0.70, UIStyle.Colors.MutedText)
    self:CreateScoreRows(self:GetRecords(record), 0.462, layer + 2, rank > 0 and rank or nil, record)
    self:CreateButton("MAIN MENU", vec2(0.5, 0.805), vec2(0.0, 0.0), "MainMenu")
end

function Script:ShowScoreBoard()
    self:Hide()

    self.visible = true
    self.locked = false
    self.buttons = {}

    local layer = self.Layer or 380
    self:CreateFrame("SCORE BOARD", UIStyle.Colors.Gold)
    self:CreateText("BEST RUNS", vec2(0.5, 0.315), vec2(0.0, 0.0), vec2(620.0, 30.0), layer + 3, 0, 0.72, UIStyle.Colors.MutedText)
    self:CreateScoreRows(self:GetRecords(nil), 0.360, layer + 2, nil, nil)
    self:CreateButton("CLOSE", vec2(0.5, 0.790), vec2(0.0, 0.0), "Close")
end

function Script:UpdateButton(entry, deltaTime)
    local button = entry.component
    if button == nil or not button:IsValid() then
        return
    end

    local hovered = button.IsButtonHovered ~= nil and button:IsButtonHovered()
    local pressed = button.IsButtonPressed ~= nil and button:IsButtonPressed()
    local targetScale = 1.0
    local targetTint = UIStyle.Colors.Button
    if hovered then
        targetScale = 1.04
        targetTint = UIStyle.Colors.ButtonHover
    end
    if pressed then
        targetScale = 0.96
        targetTint = UIStyle.Colors.ButtonPressed
    end

    local alpha = clamp((deltaTime or 0.0) * 18.0, 0.0, 1.0)
    entry.scale = lerp(entry.scale or 1.0, targetScale, alpha)
    entry.tint = UIStyle.LerpColor(entry.tint or UIStyle.Colors.Button, targetTint, alpha)
    button:SetUISizeDelta(vec2(entry.width * entry.scale, entry.height * entry.scale))
    setTint(button, entry.tint)
    setTint(entry.label, hovered and UIStyle.Colors.Gold or UIStyle.Colors.Text)
end

function Script:Tick(deltaTime)
    if not self.visible then
        return
    end

    for _, entry in ipairs(self.buttons or {}) do
        self:UpdateButton(entry, deltaTime)
        if not self.locked then
            local button = entry.component
            if button ~= nil and button:IsValid() then
                local clickCount = button:GetButtonClickCount() or 0
                if clickCount > (entry.lastClickCount or 0) then
                    entry.lastClickCount = clickCount
                    self.locked = true

                    if entry.action == "MainMenu" then
                        GameManager.ReturnToMainMenu()
                    else
                        self:Hide()
                    end
                    return
                end
            end
        end
    end
end

function Script:EndPlay()
    GameManager.UnregisterResultUI(self)
    self:Hide()
end

return Script
