-- 키 설정 --
BUTTON_SPACE = 0x20
-- 키 설정 --

---@class InputTest : ScriptComponent
local Script = {
    
}

function Script:BeginPlay()
    Log("Tank.lua BeginPlay")
end

function Script:Tick(deltaTime)
    if self.elapsed == nil then
        self.elapsed = 0
    end

    if Input.GetKey(BUTTON_SPACE) then
        Log("Spacebar")
    end

    value = Input.GetAxis("Horizontal")
    if value > 0.1 or value < -0.1 then
        Log("Horizontal input detected")
    end

    if Input.GetMouseButtonDown(1) then
        Log("Mouse Button Down")
    end
    if Input.GetMouseButtonUp(1) then
        Log("Mouse Button Up")
    end
    
    if Input.GetMouseButton(1) then
        local mouseX, mouseY = Input.GetMousePosition()
        Log("MosueX: " .. mouseX .. ", MouseY: " .. mouseY)
    end

    self.elapsed = self.elapsed + deltaTime
    if self.elapsed >= 1.0 then
        Log("Tank.lua Tick")
        self.elapsed = 0
    end
end

function Script:EndPlay()
    Log("Tank.lua EndPlay")
end

return Script
