-- ============================================================
-- CreditsMenu.lua
-- Attach to a controller actor in the GameCredits scene.
-- Builds the full-screen Credits page and wires Back to return
-- to the title scene.
-- ============================================================

local TITLE_SCENE = "Game/GameTitle"

local widget = nil

function BeginPlay()
    widget = UI.CreateWidget("Content/Game/UI/Credits.rml")
    if widget == nil then
        print("[CreditsMenu] failed to create Credits.rml widget")
        return
    end

    widget:SetWantsMouse(true)
    widget:AddToViewport()

    widget:bind_click("back_btn", function()
        Engine.OpenScene(TITLE_SCENE)
    end)
end

function EndPlay()
    if widget ~= nil then
        widget:RemoveFromParent()
        widget = nil
    end
end

function Tick(dt)
end
