--[[
    GameManagerComponent.lua
    GameManager의 세션 설정을 관리하고 메인 틱을 구동합니다.
]]

local Script = {
    properties = {
        GameTimeLimit = { type = "float", default = 600.0, min = 10.0, max = 3600.0, speed = 10.0 },
        AutoStart = { type = "bool", default = true },
    }
}

function Script:BeginPlay()
    Log("--- GameManagerComponent: BeginPlay ---")
    
    local GameManager = require("GameManager")
    GameManager.ManagerComponent = self
    
    -- 플레이어 등록 여부와 상관없이 세션 설정(시간 제한 등)을 먼저 준비합니다.
    if self.AutoStart == false then
        GameManager.PrepareMainMenu(self.GameTimeLimit)
    else
        GameManager.PrepareSession(self.GameTimeLimit)
    end
end

function Script:Tick(deltaTime)
    local GameManager = require("GameManager")
    -- GameManager가 플레이어 등록까지 완료된 후에만 실제 로직(시간 감소 등)을 수행합니다.
    GameManager.Tick(deltaTime)
end

function Script:EndPlay()
    Log("--- GameManagerComponent: EndPlay ---")
    
    local GameManager = require("GameManager")
    if GameManager.WeaponInventory ~= nil then
        for _, weapon in pairs(GameManager.WeaponInventory.Weapons) do
            if weapon.Stop ~= nil then
                weapon:Stop()
            end
        end
    end

    -- 세션 종료 시 전역 상태 초기화 (PIE 재시작 시 stale reference 방지)
    GameManager._ResetState()
end

return Script
