local ChaseAI = require("AI.ChaseAI")
local GameManager = require("GameManager")
local GameplayPause = require("GameplayPause")

local Script = {
    properties = ChaseAI.properties
}
function Script:BeginPlay()
    local actor = self:GetActor()
    self.UseNativeFlyingWaveMovement =
        actor ~= nil and
        actor:IsValid() and
        actor:GetClassName() == "AFlyingWaveEnemyActor"

    -- 원본 스케일 저장 (다양한 크기의 적 대응)
    if actor and actor:IsValid() then
        local s = actor:GetScale()
        self.OriginalScale = { x = s.x, y = s.y, z = s.z }
    end

    -- GameManager가 이번 세션에서 완전히 초기화되었는지 확인 후 플레이어 참조 가져오기
    if GameManager.Initialized and GameManager.PlayerScript and type(GameManager.PlayerScript.GetActor) == "function" then
        self.target = GameManager.PlayerScript:GetActor()
    end

    -- 만약 GameManager를 통해 못 찾았다면 직접 태그로 검색 (세션 초기화 지연 대응)
    if not self.target or not self.target:IsValid() then
        self.target = self:QueryActorByTagClosest("Player", {x=0, y=0, z=0}, 1000000.0)
    end

    self.AttackTimer = 0

    if not self.UseNativeFlyingWaveMovement then
        self.ai = self.StartCoroutine(function()
            ChaseAI.ChaseTargetCoroutine(self, "Player", self.GetRootComponent())
        end)
    end
end

function Script:FaceTarget()
    if self.UseNativeFlyingWaveMovement then
        return
    end

    if GameplayPause.IsPaused() then
        return
    end

    if self.target == nil or not self.target:IsValid() then
        return
    end

    local root = self:GetRootComponent()
    if root == nil or not root:IsValid() then
        return
    end

    local myPos = root:GetWorldLocation()
    local targetPos = self.target:GetLocation()
    targetPos.z = myPos.z

    root:LookAt(targetPos)
end

function Script:Tick(deltaTime)
    self:FaceTarget()

    if self.AttackTimer > 0 then
        self.AttackTimer = self.AttackTimer - deltaTime
    end

    -- 연속 충돌(Overlap Stay) 처리: 이미 겹쳐있는 상태에서도 주기적으로 데미지 적용
    if self.AttackTimer <= 0 then
        if self.target and self.target:IsValid() then
            local root = self:GetRootComponent()
            if root and root:IsValid() and root:IsOverlappingActor(self.target) then
                self:ApplyAttack()
            end
        end
    end
end

function Script:ApplyAttack()
    GameManager.PlayerGetDamage(self.Damage or 10.0)
    self.AttackTimer = self.AttackInterval or 1.0
    Log("[EnemyScript] Player hit! Damage: " .. tostring(self.Damage or 10.0))
end

local function HandleAttack(self, other)
    if self.AttackTimer > 0 then
        return
    end

    if not other or not other:IsValid() then return end

    local otherActor = other:GetOwner()
    if not otherActor or not otherActor:IsValid() then return end

    if otherActor:GetTag() == "Player" then
        Log("Collision with player")
        self:ApplyAttack()
    end
end

function Script:PlaySquashEffect()
    if self.SquashCoroutine then
        self.StopCoroutine(self.SquashCoroutine)
    end

    self.SquashCoroutine = self.StartCoroutine(function()
        local actor = self:GetActor()
        if not actor or not actor:IsValid() or not self.OriginalScale then
            return
        end

        local base = self.OriginalScale
        -- Squash: 가로로 넓어지고 세로로 짧아짐
        actor:SetScale({ x = base.x * 1.3, y = base.y * 1.3, z = base.z * 0.7 })
        coroutine.yield("wait_time", 0.05)

        -- Stretch: 가로로 좁아지고 세로로 길어짐
        actor:SetScale({ x = base.x * 0.8, y = base.y * 0.8, z = base.z * 1.2 })
        coroutine.yield("wait_time", 0.05)

        -- 원복
        actor:SetScale(base)
        self.SquashCoroutine = nil
    end)
end

function Script:OnOverlapBegin(other)
    HandleAttack(self, other)
end

function Script:EndPlay()
    self.StopAllCoroutines()
end

return Script
