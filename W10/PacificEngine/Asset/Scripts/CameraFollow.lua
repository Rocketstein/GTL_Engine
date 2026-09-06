local Vec = require("Core.Vector")

---@class CameraFollow : ScriptComponent
local Script = {
    properties = {
        Offset = { type = "vec3", default = { x = -10.0, y = 0.0, z = 10.0 } },
        FollowSmooth = { type = "float", default = 5.0, min = 0.0, max = 20.0, speed = 0.1 },
    }
}

function Script:BeginPlay()
    self.target = nil
    Log("CameraFollow: BeginPlay")
end

function Script:Tick(deltaTime)
    local actor = self:GetActor()
    if not actor or not actor:IsValid() then
        return
    end

    -- 플레이어 찾기 (태그 "Player" 기준)
    if not self.target or not self.target:IsValid() then
        -- 현재 위치 기준으로 아주 넓은 범위에서 플레이어 태그를 가진 액터를 찾습니다.
        local currentLoc = actor:GetLocation()
        self.target = self:QueryActorByTagClosest("Player", currentLoc, 100000.0)
    end

    if not self.target or not self.target:IsValid() then
        return
    end

    local targetLoc = self.target:GetLocation()
    local currentLoc = actor:GetLocation()

    -- 1. 위치 추적 (Follow)
    -- 목표 위치 계산 (플레이어 위치 + 오프셋)
    local desiredLoc = Vec.Add(targetLoc, self.Offset)

    -- 위치 보간 (Lerp)
    local t = math.min(deltaTime * self.FollowSmooth, 1.0)
    local nextLoc = Vec.Lerp(currentLoc, desiredLoc, t)
    actor:SetLocation(nextLoc)

    -- 2. 회전 제어 (LookAt)
    -- 방향 벡터 (카메라 -> 플레이어)
    local dir = self.Offset
    
    -- Pitch (Y축 회전): atan(dz, dist2D)
    -- 엔진 컨벤션: 대상이 높으면 Pitch가 음수여야 위를 봄
    local pitch = math.atan(dir.z, -dir.x) * (180.0 / math.pi)

    -- FRotator 매핑: Roll=X, Pitch=Y, Yaw=Z
    actor:SetRotation({ x = 0.0, y = pitch, z = 0.0 })
end

function Script:EndPlay()
    Log("CameraFollow: EndPlay")
end

return Script
