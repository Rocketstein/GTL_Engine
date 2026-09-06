local DebugConfig = require("Game.Config.Debug")
local AudioConfig = require("Game.Config.Audio")
local Log = require("Common.Log")
local Engine = require("Common.Engine")

local AudioManager = {}

AudioManager.owner = nil
AudioManager.bgm_component = nil
AudioManager.sfx_component = nil
AudioManager.enabled = true
AudioManager.bgm_started = false

local log = Log.MakeLogger(DebugConfig)

------------------------------------------------
-- Audio 초기화 함수들
------------------------------------------------

function AudioManager.Initialize(owner)
    AudioManager.owner = owner
    AudioManager.enabled = AudioConfig.enabled ~= false
    AudioManager.bgm_started = false

    log("[AudioManager] Initialize enabled=" .. tostring(AudioManager.enabled))

    if not Engine.IsValidObject(owner) then
        log("[AudioManager] Initialize warning: invalid owner")
        AudioManager.bgm_component = nil
        AudioManager.sfx_component = nil
        return false
    end

    AudioManager.bgm_component = Engine.GetComponentByType(owner, "UBackgroundSoundComponent")
    if Engine.IsValidComponent(AudioManager.bgm_component) then
        log("[AudioManager] BGM component found type=" .. tostring(AudioManager.bgm_component:GetTypeName()))
    else
        log("[AudioManager] BGM component missing: UBackgroundSoundComponent")
    end

    AudioManager.sfx_component = Engine.GetComponentByType(owner, "USFXComponent")
    if Engine.IsValidComponent(AudioManager.sfx_component) then
        log("[AudioManager] SFX component found type=" .. tostring(AudioManager.sfx_component:GetTypeName()))
    else
        log("[AudioManager] SFX component missing: USFXComponent")
    end

    return Engine.IsValidComponent(AudioManager.bgm_component) or Engine.IsValidComponent(AudioManager.sfx_component)
end

------------------------------------------------
-- BGM 재생 제어 함수들
------------------------------------------------

function AudioManager.PlayBGM()
    if not AudioManager.enabled then
        log("[AudioManager] PlayBGM skip: audio disabled")
        return false
    end

    if AudioManager.bgm_started then
        log("[AudioManager] PlayBGM skip: already started")
        return true
    end

    local path_or_name = AudioConfig.play_bgm
    if path_or_name == nil or path_or_name == "" then
        log("[AudioManager] PlayBGM skip: AudioConfig.play_bgm is empty")
        return false
    end

    if not Engine.IsValidComponent(AudioManager.bgm_component) then
        log("[AudioManager] PlayBGM failed: BGM component missing")
        return false
    end

    AudioManager.bgm_component:SetAudioCategory("background")
    AudioManager.bgm_component:SetAudioLooping(AudioConfig.bgm_loop == true)
    AudioManager.bgm_component:SetAudioPath(path_or_name)

    log("[AudioManager] PlayBGM attempt sound=" .. tostring(path_or_name))
    local ok = AudioManager.bgm_component:PlayAudio(path_or_name)
    AudioManager.bgm_started = ok == true
    log("[AudioManager] PlayBGM result=" .. tostring(ok))
    return ok == true
end

function AudioManager.StopBGM()
    if not Engine.IsValidComponent(AudioManager.bgm_component) then
        log("[AudioManager] StopBGM skip: BGM component missing")
        AudioManager.bgm_started = false
        return false
    end

    local ok = AudioManager.bgm_component:StopAudio()
    AudioManager.bgm_started = false
    log("[AudioManager] StopBGM result=" .. tostring(ok))
    return ok == true
end

------------------------------------------------
-- SFX 재생 함수들
------------------------------------------------

function AudioManager.PlaySFX(sound_key)
    if not AudioManager.enabled then
        log("[AudioManager] PlaySFX skip: audio disabled key=" .. tostring(sound_key))
        return false
    end

    local path_or_name = AudioConfig[sound_key]
    if path_or_name == nil or path_or_name == "" then
        log("[AudioManager] PlaySFX skip: AudioConfig." .. tostring(sound_key) .. " is empty")
        return false
    end

    if not Engine.IsValidComponent(AudioManager.sfx_component) then
        log("[AudioManager] PlaySFX failed: SFX component missing key=" .. tostring(sound_key))
        return false
    end

    AudioManager.sfx_component:SetAudioCategory("sfx")
    AudioManager.sfx_component:SetAudioLooping(false)
    AudioManager.sfx_component:SetAudioPath(path_or_name)

    log("[AudioManager] PlaySFX attempt key=" .. tostring(sound_key) .. " sound=" .. tostring(path_or_name))
    local ok = AudioManager.sfx_component:PlayAudio(path_or_name)
    log("[AudioManager] PlaySFX result key=" .. tostring(sound_key) .. " ok=" .. tostring(ok))
    return ok == true
end

function AudioManager.PlayOneShotSFX(sound_key)
    if not AudioManager.enabled then
        log("[AudioManager] PlayOneShotSFX skip: audio disabled key=" .. tostring(sound_key))
        return false
    end

    local path_or_name = AudioConfig[sound_key]
    if path_or_name == nil or path_or_name == "" then
        log("[AudioManager] PlayOneShotSFX skip: AudioConfig." .. tostring(sound_key) .. " is empty")
        return false
    end

    if type(play_sfx) == "function" then
        log("[AudioManager] PlayOneShotSFX attempt key=" .. tostring(sound_key) .. " sound=" .. tostring(path_or_name))
        local handle = play_sfx(path_or_name, false)
        local ok = type(handle) == "string" and handle ~= ""
        log("[AudioManager] PlayOneShotSFX result key=" .. tostring(sound_key) .. " ok=" .. tostring(ok))
        return ok
    end

    return AudioManager.PlaySFX(sound_key)
end

------------------------------------------------
-- 게임 이벤트 SFX Shortcut 함수들
------------------------------------------------

function AudioManager.PlayHit()
    return AudioManager.PlaySFX("hit_sfx")
end

function AudioManager.PlayJump()
    return AudioManager.PlaySFX("jump_sfx")
end

function AudioManager.PlaySlide()
    return AudioManager.PlaySFX("slide_sfx")
end

function AudioManager.PlayBarrelRolling()
    return AudioManager.PlayOneShotSFX("barrel_rolling_sfx")
end

function AudioManager.PlayGameOver()
    return AudioManager.PlaySFX("game_over_sfx")
end

------------------------------------------------
-- Audio 상태 제어 함수들
------------------------------------------------

function AudioManager.SetEnabled(enabled)
    AudioManager.enabled = enabled ~= false
    log("[AudioManager] SetEnabled enabled=" .. tostring(AudioManager.enabled))
end

return AudioManager
