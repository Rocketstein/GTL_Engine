-- ============================================================
-- SceneTransition.lua
-- Reusable RmlUi black fade for scene-to-scene handoffs.
--
-- The widget is recreated per scene; only the incoming-fade request lives in
-- this require-cached module table across Engine.OpenScene / Game.QuitToTitle.
-- ============================================================

local BGMState = require("Game/BGMState")

local DOCUMENT = "Content/Game/UI/SceneTransition.rml"
local SHEET_ID = "transition-sheet"

local DEFAULT_OUT_DURATION = 0.45
local DEFAULT_IN_DURATION  = 0.45
local DEFAULT_ZORDER       = 1000

local M = {
    widget = nil,
    active = false,
    mode = nil,
    elapsed = 0,
    duration = DEFAULT_OUT_DURATION,
    callback = nil,
    zorder = DEFAULT_ZORDER,

    pendingIncoming = false,
    pendingIncomingDuration = DEFAULT_IN_DURATION,
}

local function clamp01(v)
    if v < 0 then return 0 end
    if v > 1 then return 1 end
    return v
end

local function easeInOut(t)
    t = clamp01(t)
    return t * t * (3 - 2 * t)
end

local function ensureWidget(zorder)
    if M.widget == nil then
        M.widget = UI.CreateWidget(DOCUMENT)
    end
    if M.widget == nil then
        print("[SceneTransition] failed to create SceneTransition.rml widget")
        return nil
    end

    M.zorder = zorder or M.zorder or DEFAULT_ZORDER
    M.widget:SetWantsMouse(true)
    M.widget:AddToViewportZ(M.zorder)
    return M.widget
end

local function setOpacity(alpha)
    if M.widget ~= nil then
        M.widget:SetProperty(SHEET_ID, "opacity", tostring(clamp01(alpha)))
    end
end

local function setBGMFactor(factor)
    BGMState.SetVolume(BGMState.GetBase() * clamp01(factor))
end

local function removeWidget()
    if M.widget ~= nil then
        M.widget:RemoveFromParent()
        M.widget = nil
    end
end

function M.IsActive()
    return M.active
end

function M.RequestIncomingFade(duration)
    M.pendingIncoming = true
    M.pendingIncomingDuration = duration or DEFAULT_IN_DURATION
end

function M.BeginScene(options)
    options = options or {}
    if not M.pendingIncoming then
        removeWidget()
        M.active = false
        M.mode = nil
        M.callback = nil
        return
    end

    M.pendingIncoming = false
    M.active = true
    M.mode = "in"
    M.elapsed = 0
    M.duration = options.duration or M.pendingIncomingDuration or DEFAULT_IN_DURATION
    M.callback = nil

    if ensureWidget(options.zorder or DEFAULT_ZORDER) ~= nil then
        setOpacity(1)
        setBGMFactor(0)
    else
        M.active = false
        M.mode = nil
    end
end

function M.FadeOutThen(callback, options)
    options = options or {}
    if M.active then
        return false
    end

    if ensureWidget(options.zorder or DEFAULT_ZORDER) == nil then
        if callback ~= nil then callback() end
        return false
    end

    M.active = true
    M.mode = "out"
    M.elapsed = 0
    M.duration = options.duration or DEFAULT_OUT_DURATION
    M.callback = callback

    if options.fadeInNext then
        M.RequestIncomingFade(options.incomingDuration or DEFAULT_IN_DURATION)
    end

    setOpacity(options.fromAlpha or 0)
    setBGMFactor(1 - (options.fromAlpha or 0))
    return true
end

function M.Tick(dt)
    if not M.active then return end
    if M.duration <= 0 then
        M.elapsed = M.duration
    else
        M.elapsed = M.elapsed + dt
    end

    local t = 1
    if M.duration > 0 then
        t = M.elapsed / M.duration
    end
    local e = easeInOut(t)

    if M.mode == "out" then
        setOpacity(e)
        setBGMFactor(1 - e)
    elseif M.mode == "in" then
        setOpacity(1 - e)
        setBGMFactor(e)
    end

    if t < 1 then return end

    local doneMode = M.mode
    local doneCallback = M.callback
    M.active = false
    M.mode = nil
    M.callback = nil

    if doneMode == "in" then
        setBGMFactor(1)
        removeWidget()
    elseif doneMode == "out" then
        setOpacity(1)
        setBGMFactor(0)
        if doneCallback ~= nil then
            doneCallback()
        end
    end
end

function M.EndScene()
    removeWidget()
    M.active = false
    M.mode = nil
    M.elapsed = 0
    M.callback = nil
end

return M
