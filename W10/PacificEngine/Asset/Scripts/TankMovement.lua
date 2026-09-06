local Vec = require("Core.Vector")

---@class TankMovement : ScriptComponent
local TankMovement = {
    properties = {
        MoveSpeed = { type = "float", default = 20.0 },
        RotateSpeed = { type = "float", default = 30.0 },
        BaseFriction = { type = "float", default = 0.1 },
        DriftFactor = { type = "float", default = 0.3 },
        DriftSmoothness = { type = "float", default = 0.1 }
    }
}

function TankMovement:BeginPlay()
    self.Velocity = Vec.Zero()
end

function TankMovement:Tick(DeltaTime)
    local actor = self:GetActor()
    if actor == nil or not actor:IsValid() then
        return
    end

    local InputH = Input.GetAxis("Horizontal")
    local InputV = Input.GetAxis("Vertical")
    local bDriftButton = Input.GetKey(0x10) -- VK_SHIFT (보통 Shift 키)

    -- 1. 전진/후진에 따른 회전 방향 처리
    local RotateSpeedSign = 1.0
    if InputV < 0.0 then
        RotateSpeedSign = -1.0
    end

    -- 2. 드리프트 상태에 따른 보간 계수 설정
    local DriftSmoothness = 0.8
    if bDriftButton then
        DriftSmoothness = self.DriftSmoothness or 0.1
    end

    -- 3. 드리프트 중 조향 민감도 보정
    local CurrentRotateSpeed = self.RotateSpeed or 1.0
    if bDriftButton then
        CurrentRotateSpeed = CurrentRotateSpeed * 1.5
    end

    -- 4. 궤도 속도 기반 전진 및 회전 속도 계산 (탱크 조향 방식)
    local TrackWidth = 0.8
    local LeftTrackSpeed = (self.MoveSpeed or 1.0) * InputV + CurrentRotateSpeed * InputH * RotateSpeedSign
    local RightTrackSpeed = (self.MoveSpeed or 1.0) * InputV - CurrentRotateSpeed * InputH * RotateSpeedSign

    local TargetForwardSpeed = (LeftTrackSpeed + RightTrackSpeed) * 0.5
    local TargetAngularSpeed = (LeftTrackSpeed - RightTrackSpeed) / TrackWidth

    -- 5. 관성 및 드리프트 처리를 위한 속도 계산
    local Forward = actor:GetForward()
    local IntendedVelocity = Vec.Mul(Forward, TargetForwardSpeed)

    if self.Velocity == nil then 
        self.Velocity = Vec.Zero() 
    end
    self.Velocity = Vec.Lerp(self.Velocity, IntendedVelocity, DriftSmoothness * DeltaTime * 10.0)

    -- 6. 위치 및 회전 반영
    local Location = actor:GetLocation()
    local NewLocation = Vec.Add(Location, Vec.Mul(self.Velocity, DeltaTime))
    actor:SetLocation(NewLocation)

    local Rotation = actor:GetRotation()
    Rotation.z = Rotation.z + (TargetAngularSpeed * DeltaTime)
    actor:SetRotation(Rotation)
end

return TankMovement
