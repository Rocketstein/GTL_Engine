local Co = require("LuaCoroutine")

local GameplayPause = {}

function GameplayPause.IsPaused()
    local gameManager = package.loaded["GameManager"]
    if gameManager == nil then
        return false
    end

    if type(gameManager.IsGameplayPaused) == "function" then
        return gameManager.IsGameplayPaused() == true
    end

    return gameManager.IsPaused == true
end

function GameplayPause.Wait(seconds)
    return Co.WaitGameplay(seconds, GameplayPause.IsPaused)
end

function GameplayPause.WaitNextFrame()
    local deltaTime = Co.WaitNextFrame() or 0.0
    return deltaTime, GameplayPause.IsPaused()
end

return GameplayPause
