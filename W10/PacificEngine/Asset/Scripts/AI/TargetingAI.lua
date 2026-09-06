local Co = require("LuaCoroutine")
local GameplayPause = require("GameplayPause")
local Vec = require("Core.Vector")

local TargetingAI = {
    properties = {
        MoveSpeed = { type = "float", default = 1.0, min = 0.1, max = 10.0, speed = 0.05},
        TargetSearchRadius = { type = "float", default = 10000.0, min = 0.0, max = 100000.0, speed = 100.0},
        SearchInterval = { type = "float", default = 0.2, min = 0.01, max = 10.0, speed = 0.2},
    }
}

function TargetingAI.Targeting(self, ComponentHandle, bZAxisOnly, DeltaTime)
    if ComponentHandle == nil or not ComponentHandle:IsValid() then
        return
    end

    local myPos = ComponentHandle:GetWorldLocation()
    if self.target ~= nil and self.target:IsValid() then
        local targetPos = self.target:GetLocation()
        if bZAxisOnly then
            targetPos.z = myPos.z
        end

        ComponentHandle:LookAt(targetPos)
    end
end

function TargetingAI.TargetingCoroutine(self, ComponentHandle, bZAxisOnly)
    while true do
        local deltaTime, paused = GameplayPause.WaitNextFrame()
        if not paused then
            TargetingAI.Targeting(self, ComponentHandle, bZAxisOnly, deltaTime)
        end
    end
end

return TargetingAI 
