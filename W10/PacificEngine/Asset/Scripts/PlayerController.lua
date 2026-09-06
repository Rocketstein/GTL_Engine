---@class PlayerController : ScriptComponent
local Script = {
    properties = {
        MoveSpeed = { type = "float", default = 300.0, min = 0.0, max = 2000.0, speed = 10.0 },
        bLogBeginPlay = { type = "bool", default = true },
    }
}

local BUTTON_SPACE = 0x20

function Script:BeginPlay()
    if self.bLogBeginPlay then
        Log("PlayerController BeginPlay")
    end
    self:GetActor():SetTag("Player")
end

function Script:Tick(deltaTime)
    local horizontal = Input.GetAxis("Horizontal")
    local vertical = Input.GetAxis("Vertical")

    if horizontal ~= 0.0 or vertical ~= 0.0 then
        self:AddActorWorldOffset({
            x = vertical * self.MoveSpeed * deltaTime,
            y = horizontal * self.MoveSpeed * deltaTime,
            z = 0.0,
        })
    end

    if Input.GetKeyDown(BUTTON_SPACE) then
        Log("PlayerController Space")
    end
end

function Script:EndPlay()
    Log("PlayerController EndPlay")
end

return Script
