local Audio = {}

Audio.Bus = {
    Master = "Master",
    BGM = "BGM",
    SFX = "SFX",
    UI = "UI",
    Player = "Player",
    Ambience = "Ambience",
}

local function IsValidKey(key)
    return key ~= nil and key ~= ""
end

local function IsHandleValid(handle)
    if handle == nil then
        return false
    end

    local ok, valid = pcall(function()
        if handle.IsValid == nil then
            return true
        end

        return handle:IsValid()
    end)

    return ok and valid
end

function Audio.Play(key, bus, volume)
    if not IsValidKey(key) or PlaySound == nil then
        return nil
    end

    local handle = PlaySound(key, bus or Audio.Bus.SFX, volume or 1.0)
    if not IsHandleValid(handle) and Log ~= nil then
        Log("[Audio] Play failed. key=" .. tostring(key))
    end

    return handle
end

function Audio.PlayLoop(key, bus, volume)
    if not IsValidKey(key) or PlayLoopSound == nil then
        return nil
    end

    local handle = PlayLoopSound(key, bus or Audio.Bus.SFX, volume or 1.0)
    if not IsHandleValid(handle) and Log ~= nil then
        Log("[Audio] PlayLoop failed. key=" .. tostring(key))
    end

    return handle
end

function Audio.Stop(handle)
    if handle ~= nil and StopSound ~= nil then
        StopSound(handle)
    end
end

function Audio.StopBus(bus)
    if StopSoundBus ~= nil then
        StopSoundBus(bus or Audio.Bus.SFX)
    end
end

function Audio.SetBusVolume(bus, volume)
    if SetSoundBusVolume ~= nil then
        SetSoundBusVolume(bus or Audio.Bus.SFX, volume or 1.0)
    end
end

function Audio.SetMasterVolume(volume)
    if SetMasterSoundVolume ~= nil then
        SetMasterSoundVolume(volume or 1.0)
    end
end

function Audio.SetVolume(handle, volume)
    if handle ~= nil and SetSoundVolume ~= nil then
        SetSoundVolume(handle, volume or 1.0)
    end
end

function Audio.SetPosition(handle, positionMs)
    if handle ~= nil and SetSoundPosition ~= nil then
        SetSoundPosition(handle, positionMs or 0.0)
    end
end

function Audio.GetPosition(handle)
    if handle == nil or GetSoundPosition == nil then
        return nil
    end

    return GetSoundPosition(handle)
end

function Audio.GetLength(key)
    if not IsValidKey(key) or GetSoundLength == nil then
        return nil
    end

    return GetSoundLength(key)
end

function Audio.IsPlaying(handle)
    if handle == nil or IsSoundPlaying == nil then
        return false
    end

    return IsSoundPlaying(handle)
end

return Audio
