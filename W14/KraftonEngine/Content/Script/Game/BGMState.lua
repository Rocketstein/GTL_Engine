-- ============================================================
-- BGMState.lua
-- Owns the single background-music track and lets it be ducked/raised
-- live (for scene-transition fades).
--
-- BGM runs through the engine's *named loop channel* (Audio.PlayLoop /
-- Audio.SetLoopVolume) rather than the one-shot BGM channel, because the
-- loop channel exposes runtime volume control -- which is what a fade
-- needs. The channel lives in the FAudioManager singleton, so the music
-- keeps playing across scene loads on its own.
--
-- This module's table is require-cached in package.loaded, which survives
-- scene transitions, so it can remember which track is current and skip a
-- redundant restart when a scene asks for a track that is already playing
-- (e.g. Credits -> Title stays seamless).
-- ============================================================

local LOOP_NAME = "BGM"   -- single named loop channel shared by all BGM tracks

local M = {
    current = nil,   -- Audio key of the track currently playing (nil = none)
    base    = 0,     -- the current track's nominal volume (ceiling a fade rises to)
}

-- Switch BGM to `key`, unless it is already the active track.
--   key    : Audio cache key (e.g. "BGM_Main")
--   file   : path under Content/Audio, loaded lazily on an actual switch
--   volume : 0..1 nominal volume, scaled by master volume on top
-- Returns true if it switched tracks, false if `key` was already playing.
function M.Ensure(key, file, volume)
    M.base = volume or 1.0
    if M.current == key then return false end          -- already playing: leave it
    if Audio == nil or Audio.PlayLoop == nil then return false end

    if Audio.StopLoop ~= nil then Audio.StopLoop(LOOP_NAME) end   -- drop the old track
    if file ~= nil and Audio.Load ~= nil then
        Audio.Load(key, file, true)                    -- PlayLoop also forces LOOP_NORMAL
    end
    Audio.PlayLoop(key, LOOP_NAME, M.base)
    M.current = key
    return true
end

-- Set the live volume of the current track (0..base). Used by fades; master
-- volume still applies on top via the FMOD master group.
function M.SetVolume(v)
    if Audio ~= nil and Audio.SetLoopVolume ~= nil then
        Audio.SetLoopVolume(LOOP_NAME, v)
    end
end

-- The current track's nominal (full) volume -- the ceiling a fade-in rises to.
function M.GetBase()
    return M.base
end

-- Forget the current track so the next Ensure() starts fresh. Call this if BGM
-- is stopped outside this module (e.g. an explicit Audio.StopLoop/StopBGM).
function M.Clear()
    M.current = nil
end

return M
