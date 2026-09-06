local Co = require("LuaCoroutine")
local GameplayPause = require("GameplayPause")
local Vec = require("Core.Vector")
local Handle = require("Core.Handle")

local TurretHead = {}

function TurretHead.RotateToward(self, HeadName, TargetTag)
    local HeadHandle = self.GetComponentByName("USceneComponent", HeadName)

    while true do
        local _, paused = GameplayPause.WaitNextFrame()

        if not paused then
            if not Handle.IsValid(HeadHandle) then
                HeadHandle = self.GetComponentByName("USceneComponent", HeadName)
            end

            if Handle.IsValid(HeadHandle) then
                local HeadLocation = HeadHandle:GetWorldLocation()
                local SearchRadius = self.TargetSearchRadius or 10000.0
                local TargetHandle = self.QueryActorByTagClosest(TargetTag, HeadLocation, SearchRadius)

                if Handle.IsValid(TargetHandle) then
                    local TargetLocation = TargetHandle:GetLocation()
                    Vec.DirectionTo(HeadLocation, TargetLocation)
                end
            end
        end
    end
end

return TurretHead
