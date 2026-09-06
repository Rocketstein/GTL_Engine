local DamageSystem = {}
local GameplayPause = require("GameplayPause")

-- UUID -> ScriptInstance 매핑 테이블
local registry = {}

Log("[DamageSystem] Module Loaded")

--[[
    스크립트 인스턴스를 데미지 시스템에 등록합니다.
    @param actorHandle: 스크립트가 소유한 액터의 핸들
    @param scriptInstance: TakeDamage(amount, attacker) 함수를 가진 Lua 스크립트 인스턴스 (self)
]]
function DamageSystem.Register(actorHandle, scriptInstance)
    if not actorHandle or not actorHandle:IsValid() then 
        Log("[DamageSystem] Error: Invalid actor handle during registration")
        return 
    end
    
    local uuid = actorHandle:GetUUID()
    registry[uuid] = scriptInstance
    Log(string.format("[DamageSystem] Registered Actor: %s (UUID: %d)", actorHandle:GetName(), uuid))
end

--[[
    등록된 스크립트 인스턴스를 해제합니다.
]]
function DamageSystem.Unregister(actorHandle)
    if not actorHandle then return end
    local uuid = actorHandle:GetUUID()
    registry[uuid] = nil
    Log(string.format("[DamageSystem] Unregistered Actor: %d", uuid))
end

--[[
    대상 액터에게 데미지를 전달합니다.
    @param targetActor: 데미지를 받을 액터 핸들
    @param amount: 데미지 양
    @param attacker: 공격자 액터 핸들 (선택 사항)
    @return: 데미지 전달 성공 여부
]]
function DamageSystem.ApplyDamage(targetActor, amount, attacker)
    if GameplayPause.IsPaused() then
        return false
    end

    if not targetActor or not targetActor:IsValid() then 
        Log("[DamageSystem] Error: Invalid target actor during ApplyDamage")
        return false 
    end
    
    local uuid = targetActor:GetUUID()
    local script = registry[uuid]
    
    if script then
        if script.TakeDamage then
            Log(string.format("[DamageSystem] Applying %d damage to %s (UUID: %d)", amount, targetActor:GetName(), uuid))
            script:TakeDamage(amount, attacker)
            return true
        --[[
        else
            Log(string.format("[DamageSystem] Error: Script for Actor %d does not have TakeDamage function", uuid))
        --]]
        end
    --[[
    else
        Log(string.format("[DamageSystem] Error: Actor %s (UUID: %d) not found in registry", targetActor:GetName(), uuid))
    --]]
    end
    
    return false
end

return DamageSystem
