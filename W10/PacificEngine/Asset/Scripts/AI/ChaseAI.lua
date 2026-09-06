local Co = require("LuaCoroutine")
local GameplayPause = require("GameplayPause")
local Vec = require("Core.Vector")

local ChaseAI = {
    properties = {
        MoveSpeed = { type = "float", default = 1.0, min = 0.1, max = 10.0, speed = 0.05},
        Damage = { type = "float", default = 10.0, min = 0.0, max = 100.0, speed = 1.0 },
        AttackInterval = { type = "float", default = 1.0, min = 0.1, max = 10.0, speed = 0.1 },
        CollisionRadius = { type = "float", default = 1.0, min = 0.1, max = 10.0, speed = 0.1 },
    }
}

function ChaseAI.ChaseTarget(self, TargetTag, ComponentHandle, DeltaTime)
        if ComponentHandle == nil or not ComponentHandle:IsValid() then
            return
        end

        -- self.target이 이미 설정되어 있거나, 없으면 태그로 찾음 (이미 EnemyScript에서 설정함)
        if self.target == nil or not self.target:IsValid() then
            -- Fallback: 태그로 찾기 (GameManager가 아직 준비 안 된 경우 등)
            local world = self:GetWorld()
            if world then
                local players = world:GetActorsByTag(TargetTag)
                if players and #players > 0 then
                    self.target = players[1]
                end
            end
        end

        local myPos = ComponentHandle:GetWorldLocation()
        if self.target ~= nil and self.target:IsValid() then
            local targetPos = self.target:GetLocation()
            local dir = Vec.DirectionTo(myPos, targetPos)
            local moveDelta = Vec.Mul(dir, (self.MoveSpeed or 1.0) * DeltaTime)
            ComponentHandle:SetWorldLocation(Vec.Add(myPos, moveDelta))
        end
end

function ChaseAI.ChaseTargetCoroutine(self, TargetTag, ComponentHandle)

    while true do
        local deltaTime, paused = GameplayPause.WaitNextFrame()
        if not paused then
            ChaseAI.ChaseTarget(self, TargetTag, ComponentHandle, deltaTime)
        end
    end
end

return ChaseAI
