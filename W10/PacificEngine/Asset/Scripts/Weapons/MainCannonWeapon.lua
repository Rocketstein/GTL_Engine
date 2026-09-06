local Co = require("LuaCoroutine")
local Audio = require("Core.Audio")
local GameplayPause = require("GameplayPause")
local WeaponDefs = require("WeaponDefs")

local MainCannonWeapon = {}
MainCannonWeapon.__index = MainCannonWeapon

local TARGET_TAG = "Enemy"
local DEFAULT_RANGE = 72.0
local DEFAULT_SEARCH_INTERVAL = 0.2
local DEFAULT_AIM_TURN_SPEED = 360.0

local function GetVecAxis(v, axisName, axisIndex)
    if v == nil then
        return nil
    end

    return v[axisName] or v[string.upper(axisName)] or v[axisIndex]
end

local function SetVecAxis(v, axisName, axisIndex, value)
    if v == nil then
        return
    end

    v[axisName] = value
    v[string.upper(axisName)] = value
    v[axisIndex] = value
end

local function NormalizeAngleDegrees(angle)
    angle = (angle or 0.0) + 180.0
    angle = angle % 360.0
    if angle < 0.0 then
        angle = angle + 360.0
    end
    return angle - 180.0
end

local function MoveAngleTowards(current, target, maxDelta)
    local delta = NormalizeAngleDegrees(target - current)

    if delta > maxDelta then
        delta = maxDelta
    elseif delta < -maxDelta then
        delta = -maxDelta
    end

    return current + delta
end

local function Atan2(y, x)
    if math.atan2 ~= nil then
        return math.atan2(y, x)
    end

    if x > 0.0 then
        return math.atan(y / x)
    end

    if x < 0.0 and y >= 0.0 then
        return math.atan(y / x) + math.pi
    end

    if x < 0.0 and y < 0.0 then
        return math.atan(y / x) - math.pi
    end

    if y > 0.0 then
        return math.pi * 0.5
    end

    if y < 0.0 then
        return -math.pi * 0.5
    end

    return 0.0
end

local function GetYawToTarget(fromPos, targetPos)
    local dx = GetVecAxis(targetPos, "x", 1) - GetVecAxis(fromPos, "x", 1)
    local dy = GetVecAxis(targetPos, "y", 2) - GetVecAxis(fromPos, "y", 2)

    if math.abs(dx) <= 0.0001 and math.abs(dy) <= 0.0001 then
        return nil
    end

    return math.deg(Atan2(dy, dx))
end

local function IsTargetInRange(target, origin, range)
    if target == nil or not target:IsValid() then
        return false
    end

    if target.IsVisible ~= nil and not target:IsVisible() then
        return false
    end

    if origin == nil or range == nil then
        return false
    end

    local targetPos = target:GetLocation()
    if targetPos == nil then
        return false
    end

    local ox = GetVecAxis(origin, "x", 1)
    local oy = GetVecAxis(origin, "y", 2)
    local tx = GetVecAxis(targetPos, "x", 1)
    local ty = GetVecAxis(targetPos, "y", 2)

    if ox == nil or oy == nil or tx == nil or ty == nil then
        return false
    end

    local dx = tx - ox
    local dy = ty - oy
    return (dx * dx + dy * dy) <= (range * range)
end

function MainCannonWeapon.New(owner)
    local self = setmetatable({}, MainCannonWeapon)

    self.Owner = owner
    self.Id = "MainCannon"
    self.Level = 1
    self.Data = WeaponDefs.MainCannon.Levels[self.Level]

    self.IsRunning = false

    self.Target = nil

    self.VisualComponent = nil
    self.MuzzleComponent = nil
    self.MuzzleFlashComponent = nil

    self.SearchCoroutine = nil
    self.TargetingCoroutine = nil
    self.FireCoroutine = nil

    return self
end

function MainCannonWeapon:Start()
    if self.IsRunning then
        return
    end

    self.IsRunning = true
    self:RefreshComponents()

    Log("[MainCannon] Start")

    if self.Owner == nil or self.Owner.StartCoroutine == nil then
        Log("[MainCannon] StartCoroutine is nil")
        return
    end

    self.SearchCoroutine = self.Owner.StartCoroutine(function()
        self:SearchLoop()
    end)

    self.TargetingCoroutine = self.Owner.StartCoroutine(function()
        self:TargetingLoop()
    end)

    self.FireCoroutine = self.Owner.StartCoroutine(function()
        self:FireLoop()
    end)
end

function MainCannonWeapon:Stop()
    self.IsRunning = false

    if self.Owner ~= nil and self.Owner.StopCoroutine ~= nil then
        if self.SearchCoroutine ~= nil then
            self.Owner.StopCoroutine(self.SearchCoroutine)
        end

        if self.TargetingCoroutine ~= nil then
            self.Owner.StopCoroutine(self.TargetingCoroutine)
        end

        if self.FireCoroutine ~= nil then
            self.Owner.StopCoroutine(self.FireCoroutine)
        end
    end

    self.SearchCoroutine = nil
    self.TargetingCoroutine = nil
    self.FireCoroutine = nil

    self.Target = nil
end

function MainCannonWeapon:RefreshComponents()
    if self.Owner == nil or self.Owner.GetComponentByName == nil then
        return
    end

    self.VisualComponent = self.Owner.GetComponentByName(
        "UStaticMeshComponent",
        "Visual_MainCannon_0"
    )

    self.MuzzleComponent = self.Owner.GetComponentByName(
        "USceneComponent",
        "Muzzle_MainCannon_0"
    )

    self.MuzzleFlashComponent = self.Owner.GetComponentByName(
        "UPointLightComponent",
        "MuzzleFlash_MainCannon_0"
    )

    if self.VisualComponent == nil or not self.VisualComponent:IsValid() then
        Log("[MainCannon] Visual_MainCannon_0 not found")
    end

    if self.MuzzleComponent == nil or not self.MuzzleComponent:IsValid() then
        Log("[MainCannon] Muzzle_MainCannon_0 not found")
    end

    if self.MuzzleFlashComponent ~= nil and self.MuzzleFlashComponent:IsValid() then
        self.MuzzleFlashComponent:SetIntensity(0.0)
    end
end

function MainCannonWeapon:GetSearchOrigin()
    if self.MuzzleComponent ~= nil and self.MuzzleComponent:IsValid() then
        return self.MuzzleComponent:GetWorldLocation()
    end  

    if self.VisualComponent ~= nil and self.VisualComponent:IsValid() then
        return self.VisualComponent:GetWorldLocation()
    end

    local actor = nil
    if self.Owner ~= nil and self.Owner.GetActor ~= nil then
        actor = self.Owner.GetActor()
    end

    if actor ~= nil and actor:IsValid() then
        return actor:GetLocation()
    end

    return nil
end

function MainCannonWeapon:GetOwnerYaw()
    if self.Owner == nil or self.Owner.GetActor == nil then
        return 0.0
    end

    local actor = self.Owner.GetActor()
    if actor == nil or not actor:IsValid() then
        return 0.0
    end

    local rotation = actor:GetRotation()
    return GetVecAxis(rotation, "z", 3) or 0.0
end

function MainCannonWeapon:IsTargetValid()
    return IsTargetInRange(self.Target, self:GetSearchOrigin(), self:GetTargetRange())
end

function MainCannonWeapon:GetTargetRange()
    return self.Data.Range or self.Data.TargetSearchRadius or DEFAULT_RANGE
end

function MainCannonWeapon:SearchTarget()
    if self.Owner == nil or self.Owner.QueryActorByTagClosest == nil then
        return
    end

    local origin = self:GetSearchOrigin()
    if origin == nil then
        self:RefreshComponents()
        origin = self:GetSearchOrigin()
    end

    if origin == nil then
        return
    end

    local range = self:GetTargetRange()

    self.Target = self.Owner.QueryActorByTagClosest(
        TARGET_TAG,
        origin,
        range
    )
end

function MainCannonWeapon:SearchLoop()
    while self.IsRunning do
        if not GameplayPause.IsPaused() then
            self:SearchTarget()
        end

        local interval =
            self.Data.TargetRefreshInterval or
            self.Data.SearchInterval or
            DEFAULT_SEARCH_INTERVAL

        GameplayPause.Wait(interval)
    end
end

function MainCannonWeapon:AimAtCurrentTarget(deltaTime, snap)
    if self.VisualComponent == nil or not self.VisualComponent:IsValid() then
        self:RefreshComponents()
    end

    if self.VisualComponent == nil or not self.VisualComponent:IsValid() or not self:IsTargetValid() then
        return false
    end

    local myPos = self.VisualComponent:GetWorldLocation()
    local targetPos = self.Target:GetLocation()

    if myPos == nil or targetPos == nil then
        return false
    end

    local z = GetVecAxis(myPos, "z", 3)
    if z ~= nil then
        SetVecAxis(targetPos, "z", 3, z)
    end

    if snap then
        return self.VisualComponent:LookAt(targetPos)
    end

    local desiredWorldYaw = GetYawToTarget(myPos, targetPos)
    if desiredWorldYaw == nil then
        return false
    end

    local rotation = self.VisualComponent:GetRelativeRotation()
    if rotation == nil then
        return false
    end

    local currentYaw = GetVecAxis(rotation, "z", 3) or 0.0
    local desiredYaw = NormalizeAngleDegrees(desiredWorldYaw - self:GetOwnerYaw())
    local turnSpeed = self.Data.AimTurnSpeed or DEFAULT_AIM_TURN_SPEED
    local maxDelta = turnSpeed * math.max(deltaTime or 0.0, 0.0)
    local newYaw = NormalizeAngleDegrees(MoveAngleTowards(currentYaw, desiredYaw, maxDelta))

    SetVecAxis(rotation, "z", 3, newYaw)
    return self.VisualComponent:SetRelativeRotation(rotation)
end

function MainCannonWeapon:TargetingLoop()
    while self.IsRunning do
        local deltaTime, paused = GameplayPause.WaitNextFrame()

        if not paused then
            self:AimAtCurrentTarget(deltaTime, false)
        end
    end
end

function MainCannonWeapon:FireLoop()
    while self.IsRunning do
        if not GameplayPause.IsPaused() and self:IsTargetValid() then
            if self.Owner ~= nil and self.Owner.FireLinearProjectile ~= nil then
                self:AimAtCurrentTarget(0.0, true)
                self:PlayFireSound()
                self.Owner.FireLinearProjectile("MainCannon", self.Data, 0)

                -- 포인트 라이트를 이용한 포구 화염 연출 (0.1초)
                if self.MuzzleFlashComponent ~= nil and self.MuzzleFlashComponent:IsValid() then
                    self.Owner.StartCoroutine(function()
                        self.MuzzleFlashComponent:SetIntensity(5.0) -- 화염 강도
                        coroutine.yield("wait_time", 0.1)
                        if self.MuzzleFlashComponent:IsValid() then
                            self.MuzzleFlashComponent:SetIntensity(0.0)
                        end
                    end)
                end
            else
                Log("[MainCannon] FireLinearProjectile is nil")
            end
        end

        GameplayPause.Wait(self.Data.FireInterval)
    end
end

function MainCannonWeapon:PlayFireSound()
    local sound = self.Data.Sound
    if sound == nil then
        return
    end

    if type(sound) == "string" then
        Audio.Play(sound, Audio.Bus.SFX, 1.0)
        return
    end

    Audio.Play(
        sound.Fire or sound.Key,
        sound.Bus or Audio.Bus.SFX,
        sound.Volume or 1.0
    )
end

function MainCannonWeapon:Upgrade()
    if self.Level >= WeaponDefs.MainCannon.MaxNormalLevel then
        Log("MainCannon is max normal level")
        return false
    end

    self.Level = self.Level + 1
    self.Data = WeaponDefs.MainCannon.Levels[self.Level]

    -- WeaponInventory가 visual layout을 갱신한 뒤에도 이름은 같지만,
    -- 컴포넌트 재생성/숨김 가능성 때문에 다음 루프에서 다시 찾게 한다.
    self:RefreshComponents()

    Log(
        "MainCannon upgraded. level = " ..
        tostring(self.Level) ..
        ", interval = " ..
        tostring(self.Data.FireInterval)
    )

    return true
end

function MainCannonWeapon:IsMaxLevel()
    return self.Level >= WeaponDefs.MainCannon.MaxNormalLevel
end

function MainCannonWeapon:CanEvolve()
    return self.Level == WeaponDefs.MainCannon.MaxNormalLevel
end

function MainCannonWeapon:Evolve()
    if not self:CanEvolve() then
        return false
    end

    self.Level = 3
    self.Data = WeaponDefs.MainCannon.Levels[self.Level]

    self:RefreshComponents()

    Log(
        "MainCannon evolved. level = " ..
        tostring(self.Level) ..
        ", interval = " ..
        tostring(self.Data.FireInterval)
    )

    return true
end

return MainCannonWeapon
