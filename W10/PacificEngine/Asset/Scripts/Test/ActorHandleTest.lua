---@class ActorHandleTest : ScriptComponent
local Script = {}

function Script:BeginPlay()
    local actor = self:GetActor()

    if actor:IsValid() then
        Log("Actor: " .. actor:GetName() .. " / " .. actor:GetClassName())
    else
        Log("Actor handle invalid")
    end
end

function Script:Tick(deltaTime)
    local actor = self:GetActor()

    if actor:IsValid() then
        actor:AddWorldOffset({
            x = 10.0 * deltaTime,
            y = 0.0,
            z = 0.0,
        })
    end
end

return Script
