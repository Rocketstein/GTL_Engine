local Targeting = require("AI.TargetingAI")
local Query = require("Query")

local Script = {
    properties = Targeting.properties
}

function Script:BeginPlay()
    Log("[TargetAi] BeginPlay")
    local Handle = self.GetComponentByName("UStaticMeshComponent", "RotateTarget");
    self.search = self.StartCoroutine(function()
        Query.SearchActorClosestByTagCoroutine(self, Handle, "TestEnemy")
    end)
    self.Targeting = self.StartCoroutine(function()
        Targeting.TargetingCoroutine(self, Handle, false)
    end)
end

function Script:Tick(deltaTime)
end

function Script:EndPlay()
    self.StopAllCoroutines();
end

return Script
