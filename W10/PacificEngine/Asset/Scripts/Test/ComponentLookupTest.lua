---@class ComponentLookupTest : ScriptComponent
local Script = {}

function Script:BeginPlay()
    local comp = self:GetComponent("UStaticMeshComponent")

    if comp:IsValid() then
        Log("Component: " .. comp:GetName() .. " / " .. comp:GetClassName())
        Log("Is Primitive: " .. tostring(comp:IsA("UPrimitiveComponent")))
    else
        Log("Component not found")
    end
end

return Script