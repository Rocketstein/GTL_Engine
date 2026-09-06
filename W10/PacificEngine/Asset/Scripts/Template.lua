---@class Template : ScriptComponent
local Script = {
    properties = {
        Message = { type = "string", default = "Template.lua" },
        TickInterval = { type = "float", default = 1.0, min = 0.1, max = 10.0, speed = 0.05 },
        bLogTick = { type = "bool", default = true },
        TestVector3 = { type = "vec3", default = { 0, 0, 0} },
    }
}

function Script:BeginPlay()
    Log(self.Message .. " BeginPlay")
end

function Script:Tick(deltaTime)
    if self.elapsed == nil then
        self.elapsed = 0.0
    end

    self.elapsed = self.elapsed + deltaTime
    if self.elapsed >= self.TickInterval then
        if self.bLogTick then
            Log(self.Message .. " Tick")
        end
        self.elapsed = 0.0
    end
end

function Script:EndPlay()
    Log(self.Message .. " EndPlay")
end

return Script
