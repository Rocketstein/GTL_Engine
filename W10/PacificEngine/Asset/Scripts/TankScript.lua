---@class TankScript : ScriptComponent
local Script = {
    properties = {
        MoveSpeed = { type = "float", default = 11.7 },
        RotateSpeed = { type = "float", default = 60.0 },
        BaseFriction = { type = "float", default = 0.1 },
        DriftFactor = { type = "float", default = 0.3 },
        DriftSmoothness = { type = "float", default = 0.1 },
        PickupExp = { type = "float", default = 1.0, min = 0.0, max = 1000.0, speed = 1.0 },
        PickupAttractSpeed = { type = "float", default = 20.0, min = 0.0, max = 1000.0, speed = 1.0 },
        HealthBarWidth = { type = "float", default = 2.0, min = 0.1, max = 10.0, speed = 0.1 },
        HealthBarHeight = { type = "float", default = 0.16, min = 0.02, max = 2.0, speed = 0.01 },
        HealthBarOffsetZ = { type = "float", default = 1.8, min = -10.0, max = 10.0, speed = 0.1 },
    }
}
local Vec = require("Core.Vector")
local Audio = require("Core.Audio")
local GameManager = require("GameManager")

local KEY_SHIFT = 0x10

function Script:BeginPlay()
    self.Velocity = Vec.Zero()
    self.LastSessionID = 0
    
    -- 1. 플레이어 태그 설정
    local actor = self:GetActor()
    if actor then
        actor:SetTag("Player")
    end

    -- 2. GameManager에 자신(Lua 스크립트 테이블)을 등록
    -- 이를 통해 GameManager는 플레이어의 Lua 메서드(EquipWeaponVisual 등)에 접근할 수 있게 됩니다.
    GameManager.RegisterPlayer(self)

    self.RootCollider = self.GetRootComponent()
    if self.RootCollider == nil or not self.RootCollider:IsValid() then
        Log("Invalid RootCollider")
    end

    self.PickupSensor = self.GetComponentByName("UCircleCollider2DComponent", "PickupSensor")
    if self.PickupSensor == nil or not self.PickupSensor:IsValid() then
        Log("Invalid PickupSensor")
    end

    self.HealthBarBack = self.GetComponentByName("UTextureUIComponent", "PlayerHealthBar_Back")
    self.HealthBarFill = self.GetComponentByName("UTextureUIComponent", "PlayerHealthBar_Fill")
    self.LastHealthBarRatio = nil
    self:ConfigureHealthBar()
    self:UpdateHealthBar()
end

function Script:ResetState()
    self.Velocity = Vec.Zero()
    local actor = self:GetActor()
    if actor ~= nil and actor:IsValid() then
        actor:SetRotation({ x = 0, y = 0, z = 0 })
        actor:SetLocation({ x = 0, y = 0, z = 0 })
    end
    self:UpdateHealthBar()
    Log("TankScript: State reset for new session")
end

local function clamp(value, minValue, maxValue)
    return math.max(minValue, math.min(maxValue, value))
end

local function lerp(a, b, t)
    return a + (b - a) * t
end

function Script:ConfigureHealthBar()
    local width = self.HealthBarWidth or 2.0
    local height = self.HealthBarHeight or 0.16
    local offsetZ = self.HealthBarOffsetZ or 1.8

    if self.HealthBarBack ~= nil and self.HealthBarBack:IsValid() then
        self.HealthBarBack:SetUIRenderSpace("WorldSpace")
        self.HealthBarBack:SetUIBillboard(true)
        self.HealthBarBack:SetUIPivot({ 0.5, 8.5 })
        self.HealthBarBack:SetUIWorldSize({ width + 0.16, height + 0.08 })
        self.HealthBarBack:SetRelativeLocation({ x = 0.0, y = 0.0, z = offsetZ })
        self.HealthBarBack:SetUITint(0.04, 0.04, 0.04, 0.78)
        self.HealthBarBack:SetUIVisibility(true)
    end

    if self.HealthBarFill ~= nil and self.HealthBarFill:IsValid() then
        self.HealthBarFill:SetUIRenderSpace("WorldSpace")
        self.HealthBarFill:SetUIBillboard(true)
        self.HealthBarFill:SetUIPivot({ 0.5, 12.5 })
        self.HealthBarFill:SetRelativeLocation({ x = 0.0, y = 0.0, z = offsetZ })
        self.HealthBarFill:SetUIVisibility(true)
    end
end

function Script:UpdateHealthBar()
    if self.HealthBarFill == nil or not self.HealthBarFill:IsValid() then
        return
    end

    local maxHP = tonumber(GameManager.Stats.MaxHP) or 0.0
    local currentHP = tonumber(GameManager.Stats.CurrentHP) or 0.0
    local ratio = 0.0
    if maxHP > 0.0001 then
        ratio = clamp(currentHP / maxHP, 0.0, 1.0)
    end

    local fullWidth = self.HealthBarWidth or 2.0
    local height = self.HealthBarHeight or 0.16
    local fillWidth = math.max(0.001, fullWidth * ratio)
    self.HealthBarFill:SetUIPivot({ fullWidth * 0.5 / fillWidth, 12.5 })
    self.HealthBarFill:SetUIWorldSize({ fillWidth, height })

    local red = { r = 0.95, g = 0.16, b = 0.12 }
    local yellow = { r = 0.95, g = 0.78, b = 0.12 }
    local green = { r = 0.18, g = 0.95, b = 0.28 }
    local from = red
    local to = yellow
    local t = ratio * 2.0
    if ratio > 0.5 then
        from = yellow
        to = green
        t = (ratio - 0.5) * 2.0
    end

    self.HealthBarFill:SetUITint(
        lerp(from.r, to.r, t),
        lerp(from.g, to.g, t),
        lerp(from.b, to.b, t),
        0.95
    )

    self.LastHealthBarRatio = ratio
end

function Script:UpdateMovement(deltaTime)
    local actor = self.GetActor()
    if actor == nil or not actor:IsValid() then
        return
    end

    local inputH = Input.GetAxis("Horizontal")
    local inputV = Input.GetAxis("Vertical")
    local driftButton = Input.GetKey(KEY_SHIFT)

    local rotateSpeedSign = 1.0
    if inputV < 0.0 then
        rotateSpeedSign = -1.0
    end

    local driftSmoothness = 0.8
    if driftButton then
        driftSmoothness = self.DriftSmoothness or 0.1
    end

    local currentRotateSpeed = self.RotateSpeed or 1.0
    if driftButton then
        currentRotateSpeed = currentRotateSpeed * 1.5
    end

    local trackWidth = 0.8
    
    -- GameManager의 이동 속도 배율 적용
    local finalMoveSpeed = (self.MoveSpeed or 1.0) * (GameManager.Stats.MoveSpeedMult or 1.0)
    
    local leftTrackSpeed = finalMoveSpeed * inputV + currentRotateSpeed * inputH * rotateSpeedSign
    local rightTrackSpeed = finalMoveSpeed * inputV - currentRotateSpeed * inputH * rotateSpeedSign

    local targetForwardSpeed = (leftTrackSpeed + rightTrackSpeed) * 0.5
    local targetAngularSpeed = (leftTrackSpeed - rightTrackSpeed) / trackWidth

    local forward = actor:GetForward()
    local intendedVelocity = Vec.Mul(forward, targetForwardSpeed)

    if self.Velocity == nil then
        self.Velocity = Vec.Zero()
    end

    local lerpAlpha = math.min(1.0, driftSmoothness * deltaTime * 10.0)
    self.Velocity = Vec.Lerp(self.Velocity, intendedVelocity, lerpAlpha)

    local location = actor:GetLocation()
    local newLocation = Vec.Add(location, Vec.Mul(self.Velocity, deltaTime))
    actor:SetLocation(newLocation)

    local rotation = actor:GetRotation()
    rotation.z = rotation.z + targetAngularSpeed * deltaTime
    actor:SetRotation(rotation)
end

function Script:AddExp(amount)
    GameManager.OnPickupExp(amount)
end

function Script:OpenChest()
    return GameManager.OnPickupChest()
end

function Script:AttractPickup(pickup, deltaTime)
    if pickup == nil or not pickup:IsValid() then
        return
    end

    local owner = self.GetActor()
    if owner == nil or not owner:IsValid() then
        return
    end

    local pickupPos = pickup:GetLocation()
    local targetPos = owner:GetLocation()
    local toTarget = Vec.Sub(targetPos, pickupPos)
    local distance = Vec.Length(toTarget)
    if distance <= 0.0001 then
        return
    end

    -- 자석 속도와 범위 배율 적용 가능
    local attractSpeed = (self.PickupAttractSpeed or 20.0)
    local step = math.min(distance, attractSpeed * deltaTime)
    pickup:SetLocation(Vec.Add(pickupPos, Vec.Mul(toTarget, step / distance)))
end

function Script:CollectPickup(pickup, poolManager)
    -- 보석의 경우 경험치 지급 (추후 pickup 종류에 따라 분기 가능)
    self:AddExp(self.PickupExp or 1)
    Audio.Play("pickup", Audio.Bus.SFX, 1.0)

    if poolManager ~= nil and poolManager:IsValid() then
        poolManager:Release(pickup)
    end
    pickup:SetVisible(false)
end

function Script:UpdatePickups(deltaTime)
    if self.PickupSensor == nil or not self.PickupSensor:IsValid() then
        return
    end
    if self.RootCollider == nil or not self.RootCollider:IsValid() then
        return
    end

    local world = self.GetWorld()
    if world == nil or not world:IsValid() then
        return
    end

    local pickups = world:GetActorsByTag("Pickup")
    if pickups == nil then
        return
    end

    local poolManager = GetActorPoolManager()
    for _, pickup in ipairs(pickups) do
        if pickup:IsValid() and pickup:IsVisible() then
            if self.PickupSensor:IsOverlappingActor(pickup) then
                self:AttractPickup(pickup, deltaTime)
            end

            if self.RootCollider:IsOverlappingActor(pickup) then
                self:CollectPickup(pickup, poolManager)
            end
        end
    end
end

function Script:Tick(deltaTime)
    -- 세션 변경 확인 및 리셋
    if self.LastSessionID ~= GameManager.SessionID then
        self:ResetState()
        self.LastSessionID = GameManager.SessionID
    end

    if not GameManager.IsInputAllowed() then
        self:UpdateHealthBar()
        return
    end

    self:UpdateMovement(deltaTime)
    self:UpdatePickups(deltaTime)
    self:UpdateHealthBar()
end

function Script:EndPlay()
    self.StopAllCoroutines()
end

return Script
