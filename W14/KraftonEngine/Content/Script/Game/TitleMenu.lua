-- ============================================================
-- TitleMenu.lua
-- Attach to a single controller actor placed in the Title scene.
-- On BeginPlay it builds the title menu (Title.rml) + the Options
-- overlay (Options.rml), and wires the four buttons:
--
--   start_btn    -> transition to the gameplay scene
--   options_btn  -> show Options overlay
--   controls_btn -> show Controls overlay (control tutorial)
--   credits_btn  -> transition to the credits scene
--   exit_btn     -> quit the application
-- ============================================================

local BGMState = require("Game/BGMState")
local SceneTransition = require("Game/SceneTransition")

local START_SCENE    = "Game/GamePlay.Scene"
local CREDITS_SCENE  = "Game/GameCredits.Scene"
local OPTIONS_ZORDER = 11       -- above the title menu (default ZOrder 0)
local CONTROLS_ZORDER = 11      -- same layer as Options (mutually exclusive overlays)
local VOLUME_STEP    = 0.1      -- master-volume increment per -/+ click
local UI_SOUND_VOLUME = 1.0
local SFX_CURSOR_SELECT = "Title_CursorSelect"
local SFX_CANCEL = "Title_Cancel"
local BGM_MAIN       = "BGM_Main"
local BGM_MAIN_FILE  = "BGM/bounce-bay-records-traditional-japanese (main bgm).mp3"
local BGM_VOLUME     = 0.5      -- BGM sits under SFX; scaled by master volume on top

local widget   = nil            -- title menu
local options  = nil            -- options overlay (created hidden, toggled on click)
local controls = nil            -- controls overlay (created hidden, toggled on click)

local function LoadMenuSounds()
    if Audio == nil or Audio.Load == nil then return end
    Audio.Load(SFX_CURSOR_SELECT, "CursorSelect.wav", false)
    Audio.Load(SFX_CANCEL, "Cancel.wav", false)
    -- BGM is loaded lazily by BGMState.Ensure (only on an actual track switch),
    -- so re-entering the title from Credits doesn't re-Load and cut the music.
end

local function PlayMenuSound(key)
    if Audio == nil or Audio.Play == nil then return end
    Audio.Play(key, UI_SOUND_VOLUME)
end

-- Master volume is stored/applied by the engine (Options.* bindings); the UI
-- only mirrors it. Show it as a rounded percentage.
local function RefreshVolumeLabel()
    if options == nil then return end
    local pct = math.floor(Options.GetMasterVolume() * 100 + 0.5)
    options:SetText("vol_value", tostring(pct) .. "%")
end

local function ChangeVolume(delta)
    PlayMenuSound(SFX_CURSOR_SELECT)
    Options.SetMasterVolume(Options.GetMasterVolume() + delta)  -- clamps + applies live
    RefreshVolumeLabel()
end

local function ShowOptions()
    if options == nil then return end
    if SceneTransition.IsActive() then return end
    PlayMenuSound(SFX_CURSOR_SELECT)
    options:SetWantsMouse(true)
    RefreshVolumeLabel()                 -- sync label to current value before showing
    options:AddToViewportZ(OPTIONS_ZORDER)
end

local function HideOptions()
    if options == nil then return end
    PlayMenuSound(SFX_CANCEL)
    Options.Save()                       -- persist to ProjectSettings.ini on close
    options:RemoveFromParent()
end

local function ShowControls()
    if controls == nil then return end
    if SceneTransition.IsActive() then return end
    PlayMenuSound(SFX_CURSOR_SELECT)
    controls:SetWantsMouse(true)
    controls:AddToViewportZ(CONTROLS_ZORDER)
end

local function HideControls()
    if controls == nil then return end
    PlayMenuSound(SFX_CANCEL)
    controls:RemoveFromParent()
end

function BeginPlay()
    LoadMenuSounds()
    SceneTransition.BeginScene()

    -- Main theme loops over the title. Ensure() is a no-op if it's already
    -- playing (e.g. returning from Credits => seamless), and switches the track
    -- when arriving from gameplay (Quit to Title), which was playing battle BGM.
    BGMState.Ensure(BGM_MAIN, BGM_MAIN_FILE, BGM_VOLUME)

    widget = UI.CreateWidget("Content/Game/UI/Title.rml")
    if widget == nil then
        print("[TitleMenu] failed to create Title.rml widget")
        return
    end

    -- Menu needs the cursor: show system cursor, release raw mouse / mouse-look.
    widget:SetWantsMouse(true)
    widget:AddToViewport()

    -- Build the Options overlay once, but don't show it yet.
    options = UI.CreateWidget("Content/Game/UI/Options.rml")
    if options ~= nil then
        options:bind_click("vol_down", function() ChangeVolume(-VOLUME_STEP) end)
        options:bind_click("vol_up",   function() ChangeVolume( VOLUME_STEP) end)
        options:bind_click("back_btn", HideOptions)
    else
        print("[TitleMenu] failed to create Options.rml widget")
    end

    -- Build the Controls overlay once, but don't show it yet.
    controls = UI.CreateWidget("Content/Game/UI/Controls.rml")
    if controls ~= nil then
        controls:bind_click("back_btn", HideControls)
    else
        print("[TitleMenu] failed to create Controls.rml widget")
    end

    widget:bind_click("start_btn", function()
        if SceneTransition.IsActive() then return end
        PlayMenuSound(SFX_CURSOR_SELECT)
        SceneTransition.FadeOutThen(function()
            Engine.OpenScene(START_SCENE)
        end, { fadeInNext = true })
    end)

    widget:bind_click("options_btn", ShowOptions)

    widget:bind_click("controls_btn", ShowControls)

    widget:bind_click("credits_btn", function()
        if SceneTransition.IsActive() then return end
        PlayMenuSound(SFX_CURSOR_SELECT)
        Engine.OpenScene(CREDITS_SCENE)
    end)

    widget:bind_click("exit_btn", function()
        if SceneTransition.IsActive() then return end
        Engine.Exit()
    end)
end

function EndPlay()
    SceneTransition.EndScene()
    if options ~= nil then
        options:RemoveFromParent()
        options = nil
    end
    if controls ~= nil then
        controls:RemoveFromParent()
        controls = nil
    end
    if widget ~= nil then
        widget:RemoveFromParent()
        widget = nil
    end
end

function Tick(dt)
    SceneTransition.Tick(dt)
end
