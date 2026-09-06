local CameraManager = require("CameraManager")

local Script = {
    properties = {
        AutoInitialize = { type = "bool", default = true },
        OwnerTag = { type = "string", default = "Player" },
        PlayerTag = { type = "string", default = "" },
        SearchRadius = { type = "float", default = 100000.0, min = 0.0, max = 1000000.0, speed = 1000.0 },
    }
}

local function getInitializeTag(script)
    if script.OwnerTag ~= nil and script.OwnerTag ~= "" then
        return script.OwnerTag
    end
    if script.PlayerTag ~= nil and script.PlayerTag ~= "" then
        return script.PlayerTag
    end
    return "Player"
end

function Script:BeginPlay()
    CameraManager.Register(self)

    if self.AutoInitialize == false then
        return
    end

    local actor = self:GetActor()
    if actor == nil or not actor:IsValid() then
        Log("[CameraManager] AutoInitialize failed: owner actor is invalid.")
        return
    end

    local tag = getInitializeTag(self)
    local radius = tonumber(self.SearchRadius) or 100000.0
    local owner = self:QueryActorByTagClosest(tag, actor:GetLocation(), radius)
    if owner == nil or not owner:IsValid() then
        Log("[CameraManager] AutoInitialize failed: actor tag not found: " .. tostring(tag))
        return
    end

    if not CameraManager.InitializeFor(owner) then
        Log("[CameraManager] AutoInitialize failed: InitializeFor returned false.")
    end
end

function Script:EndPlay()
    CameraManager.Unregister(self)
end

return Script
