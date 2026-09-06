---@class MainMenuController : ScriptComponent
local Script = {
    properties = {
        MainMenuTag = { type = "string", default = "MainMenu" },
        HudTag = { type = "string", default = "UI" },
    }
}

local GameManager = require("GameManager")

local function isValid(handle)
    return handle ~= nil and handle.IsValid ~= nil and handle:IsValid()
end

local function getActorsByTag(script, tag)
    local world = nil
    if script.GetWorld ~= nil then
        world = script:GetWorld()
    end

    if world == nil or not world:IsValid() or world.GetActorsByTag == nil then
        return {}
    end

    return world:GetActorsByTag(tag)
end

local function getButton(actor)
    if not isValid(actor) or actor.GetComponent == nil then
        return nil
    end

    local button = actor:GetComponent("UUIButtonComponent")
    if isValid(button) then
        return button
    end

    return nil
end

function Script:CollectTaggedActors(tag)
    local result = {}
    local actors = getActorsByTag(self, tag)

    for _, actor in ipairs(actors) do
        if isValid(actor) then
            table.insert(result, actor)
        end
    end

    return result
end

function Script:EnsureActorLists()
    if self.menuActors == nil or #self.menuActors == 0 then
        self.menuActors = self:CollectTaggedActors(self.MainMenuTag or "MainMenu")
    end

    if self.hudActors == nil or #self.hudActors == 0 then
        self.hudActors = self:CollectTaggedActors(self.HudTag or "UI")
    end
end

function Script:SetActorsVisible(actors, visible)
    for _, actor in ipairs(actors or {}) do
        if isValid(actor) and actor.SetVisible ~= nil then
            actor:SetVisible(visible == true)
        end
    end
end

function Script:CollectButtons()
    self.buttons = {}

    for _, actor in ipairs(self.menuActors or {}) do
        local name = actor:GetName()
        if name == "GameStartBT" or name == "ScoreBoardBT" or name == "GameEndBT" then
            local button = getButton(actor)
            if button ~= nil then
                self.buttons[name] = {
                    actor = actor,
                    component = button,
                    lastClickCount = button:GetButtonClickCount() or 0,
                }
            end
        end
    end
end

function Script:SetButtonInteractable(name, interactable)
    local entry = self.buttons ~= nil and self.buttons[name] or nil
    if entry == nil or not isValid(entry.component) then
        return
    end

    if entry.component.SetButtonInteractable ~= nil then
        entry.component:SetButtonInteractable(interactable == true)
    end
end

function Script:SetAllButtonsInteractable(interactable)
    self:SetButtonInteractable("GameStartBT", interactable)
    self:SetButtonInteractable("ScoreBoardBT", interactable)
    self:SetButtonInteractable("GameEndBT", interactable)
end

function Script:BeginPlay()
    self.started = false
    self.menuActors = self:CollectTaggedActors(self.MainMenuTag or "MainMenu")
    self.hudActors = self:CollectTaggedActors(self.HudTag or "UI")

    self:SetActorsVisible(self.menuActors, true)
    self:SetActorsVisible(self.hudActors, false)
    self:CollectButtons()
    self:SetAllButtonsInteractable(true)
    GameManager.RegisterMainMenuController(self)

    if self.buttons["GameStartBT"] == nil then
        Log("[MainMenuController] GameStartBT not found.")
    end
end

function Script:ShowMenu()
    self.started = false
    self:EnsureActorLists()

    self:SetActorsVisible(self.menuActors, true)
    self:SetActorsVisible(self.hudActors, false)
    self:CollectButtons()
    self:SetAllButtonsInteractable(true)
end

function Script:StartGame()
    if self.started then
        return
    end

    self.started = true
    self:SetAllButtonsInteractable(false)

    self:SetActorsVisible(self.menuActors, false)
    self:SetActorsVisible(self.hudActors, true)

    GameManager.RequestStartGame(true)
end

function Script:HandleButtonClick(name)
    if name == "GameStartBT" then
        self:StartGame()
    elseif name == "ScoreBoardBT" then
        GameManager.ShowScoreBoard()
    elseif name == "GameEndBT" then
        Log("[MainMenuController] GameEnd is not implemented yet.")
    end
end

function Script:Tick(deltaTime)
    if self.started or self.buttons == nil then
        return
    end

    for name, entry in pairs(self.buttons) do
        local button = entry.component
        if isValid(button) and button.GetButtonClickCount ~= nil then
            local clickCount = button:GetButtonClickCount() or 0
            if clickCount > (entry.lastClickCount or 0) then
                entry.lastClickCount = clickCount
                self:HandleButtonClick(name)
                if self.started then
                    return
                end
            end
        end
    end
end

return Script
