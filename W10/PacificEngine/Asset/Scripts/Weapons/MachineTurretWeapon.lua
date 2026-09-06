local Co = require("LuaCoroutine")
local GameplayPause = require("GameplayPause")
local DamageSystem = require("Core.DamageSystem")
local WeaponDefs = require("WeaponDefs")

local MachineTurretWeapon = {}
MachineTurretWeapon.__index = MachineTurretWeapon

local DEFAULT_AIM_TURN_SPEED = 360.0
local DEFAULT_HOVER_AMPLITUDE = 0.25
local DEFAULT_HOVER_SPEED = 2.0
local DEFAULT_ORBIT_RADIUS = 2.0
local DEFAULT_ORBIT_SPEED = 2.0
local TWO_PI = math.pi * 2.0

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

local function IsValidTarget(target, origin, range)
    if target == nil or not target:IsValid() then
        return false
    end

    if target.IsVisible ~= nil and not target:IsVisible() then
        return false
    end

    if origin ~= nil and range ~= nil then
        local targetPos = target:GetLocation()
        if targetPos == nil then
            return false
        end

        local ox = GetVecAxis(origin, "x", 1)
        local oy = GetVecAxis(origin, "y", 2)
        local oz = GetVecAxis(origin, "z", 3)

        local tx = GetVecAxis(targetPos, "x", 1)
        local ty = GetVecAxis(targetPos, "y", 2)
        local tz = GetVecAxis(targetPos, "z", 3)

        if ox == nil or oy == nil or oz == nil or
           tx == nil or ty == nil or tz == nil then
            return false
        end

        local dx = tx - ox
        local dy = ty - oy
        local dz = tz - oz

        local distSq = dx * dx + dy * dy + dz * dz
        local rangeSq = range * range

        if distSq > rangeSq then
            return false
        end
    end

    return true
end

function MachineTurretWeapon.New(owner)
    local self = setmetatable({}, MachineTurretWeapon)

    self.Owner = owner
    self.Id = "MachineTurret"
    self.Def = WeaponDefs.MachineTurret
    self.Level = 1
    self.Data = self.Def.Levels[1]
    self.IsRunning = false
    self.TurretSlots = {}

    return self
end

function MachineTurretWeapon:Start()
    if self.IsRunning then
        return
    end

    self.IsRunning = true
    self:RebuildTurretSlots()
end

function MachineTurretWeapon:Stop()
    self.IsRunning = false

    for _, slot in pairs(self.TurretSlots) do
        slot.IsRunning = false

        if self.Owner ~= nil and self.Owner.StopCoroutine ~= nil then
            if slot.SearchCoroutine ~= nil then
                self.Owner.StopCoroutine(slot.SearchCoroutine)
                slot.SearchCoroutine = nil
            end

            if slot.AimCoroutine ~= nil then
                self.Owner.StopCoroutine(slot.AimCoroutine)
                slot.AimCoroutine = nil
            end

            if slot.FireCoroutine ~= nil then
                self.Owner.StopCoroutine(slot.FireCoroutine)
                slot.FireCoroutine = nil
            end

            if slot.HoverCoroutine ~= nil then
                self.Owner.StopCoroutine(slot.HoverCoroutine)
                slot.HoverCoroutine = nil
            end
        end

        self:ResetSlotHover(slot)
    end
end

function MachineTurretWeapon:Upgrade()
    if self:IsMaxLevel() then
        Log("MachineTurret is max level")
        return false
    end

    self.Level = self.Level + 1
    self.Data = self.Def.Levels[self.Level]

    Log("[MachineTurret] upgraded to level " .. tostring(self.Level))
    return true
end

function MachineTurretWeapon:IsMaxLevel()
    return self.Level >= self.Def.MaxLevel
end

function MachineTurretWeapon:RebuildTurretSlots()
    for _, slot in pairs(self.TurretSlots) do
        slot.IsRunning = false

        if self.Owner ~= nil and self.Owner.StopCoroutine ~= nil then
            if slot.SearchCoroutine ~= nil then
                self.Owner.StopCoroutine(slot.SearchCoroutine)
                slot.SearchCoroutine = nil
            end

            if slot.AimCoroutine ~= nil then
                self.Owner.StopCoroutine(slot.AimCoroutine)
                slot.AimCoroutine = nil
            end

            if slot.FireCoroutine ~= nil then
                self.Owner.StopCoroutine(slot.FireCoroutine)
                slot.FireCoroutine = nil
            end

            if slot.HoverCoroutine ~= nil then
                self.Owner.StopCoroutine(slot.HoverCoroutine)
                slot.HoverCoroutine = nil
            end
        end

        self:ResetSlotHover(slot)
    end

    self.TurretSlots = {}

    local count = self.Data.TurretCount or 1

    for i = 0, count - 1 do
        local slot = self:CreateTurretSlot(i)

        if slot ~= nil then
            self.TurretSlots[i] = slot
            self:StartTurretSlot(slot)
        end
    end

    Log("[MachineTurret] rebuilt slots. count = " .. tostring(count))
end

function MachineTurretWeapon:CreateTurretSlot(index)
    local visualName = "Visual_MachineTurret_" .. tostring(index)
    local muzzleName = "Muzzle_MachineTurret_" .. tostring(index)

    local visual = nil
    local muzzle = nil

    if self.Owner ~= nil and self.Owner.GetComponentByName ~= nil then
        visual = self.Owner.GetComponentByName("UStaticMeshComponent", visualName)
        muzzle = self.Owner.GetComponentByName("USceneComponent", muzzleName)
    end

    if visual == nil or not visual:IsValid() then
        Log("[MachineTurret] Visual not found: " .. visualName)
        return nil
    end

    local baseLocation = visual:GetRelativeLocation()
    local baseX = GetVecAxis(baseLocation, "x", 1) or 0.0
    local baseY = GetVecAxis(baseLocation, "y", 2) or 0.0
    local orbitRadius = math.sqrt(baseX * baseX + baseY * baseY)
    local orbitAngle = Atan2(baseY, baseX)

    if orbitRadius <= 0.0001 then
        orbitRadius = self.Data.OrbitRadius or DEFAULT_ORBIT_RADIUS
        orbitAngle = (index / math.max(self.Data.TurretCount or 1, 1)) * TWO_PI
    end

    return {
        Index = index,

        Visual = visual,
        Muzzle = muzzle,
        BaseRelativeLocation = baseLocation,
        OrbitRadius = orbitRadius,
        OrbitAngle = orbitAngle,

        Target = nil,
        IsRunning = true,

        FireInterval = self.Data.FireInterval or 1.0,
        TargetRefreshInterval = self.Data.TargetRefreshInterval or 0.2,

        Range = self.Data.Range or 10000.0,
        Damage = self.Data.Damage or 1.0,

        SearchCoroutine = nil,
        AimCoroutine = nil,
        FireCoroutine = nil,
        HoverCoroutine = nil,
    }
end

function MachineTurretWeapon:GetSlotOrigin(slot)
    if slot == nil then
        return nil
    end

    if slot.Muzzle ~= nil and slot.Muzzle:IsValid() then
        return slot.Muzzle:GetWorldLocation()
    end

    if slot.Visual ~= nil and slot.Visual:IsValid() then
        return slot.Visual:GetWorldLocation()
    end

    if self.Owner ~= nil and self.Owner.GetActor ~= nil then
        local ownerActor = self.Owner.GetActor()
        if ownerActor ~= nil and ownerActor:IsValid() then
            return ownerActor:GetLocation()
        end
    end

    return nil
end

function MachineTurretWeapon:GetOwnerYaw()
    if self.Owner == nil or self.Owner.GetActor == nil then
        return 0.0
    end

    local ownerActor = self.Owner.GetActor()
    if ownerActor == nil or not ownerActor:IsValid() then
        return 0.0
    end

    local rotation = ownerActor:GetRotation()
    return GetVecAxis(rotation, "z", 3) or 0.0
end

function MachineTurretWeapon:StartTurretSlot(slot)
    if self.Owner == nil or self.Owner.StartCoroutine == nil then
        Log("[MachineTurret] StartCoroutine is nil")
        return
    end

    slot.SearchCoroutine = self.Owner.StartCoroutine(function()
        self:TurretSearchLoop(slot)
    end)

    slot.AimCoroutine = self.Owner.StartCoroutine(function()
        self:TurretAimLoop(slot)
    end)

    slot.FireCoroutine = self.Owner.StartCoroutine(function()
        self:TurretFireLoop(slot)
    end)

    slot.HoverCoroutine = self.Owner.StartCoroutine(function()
        self:TurretHoverLoop(slot)
    end)
end

function MachineTurretWeapon:TurretSearchLoop(slot)
    while self.IsRunning and slot.IsRunning do
        if not GameplayPause.IsPaused() then
            self:UpdateSlotTarget(slot)
        end
        GameplayPause.Wait(slot.TargetRefreshInterval)
    end
end

function MachineTurretWeapon:TurretAimLoop(slot)
    while self.IsRunning and slot.IsRunning do
        local deltaTime, paused = GameplayPause.WaitNextFrame()
        if not paused then
            self:UpdateSlotAim(slot, deltaTime, false)
        end
    end
end

function MachineTurretWeapon:TurretFireLoop(slot)
    while self.IsRunning and slot.IsRunning do
        local rapidCount = self.Data.RapidCount or 1
        local rapidInterval = self.Data.RapidInterval or 0.05

        for i = 1, rapidCount do
            if not self.IsRunning or not slot.IsRunning then
                break
            end
            
            if not GameplayPause.IsPaused() then
                self:FireSlot(slot)
            end

            if i < rapidCount then
                GameplayPause.Wait(rapidInterval)
            end
        end

        GameplayPause.Wait(slot.FireInterval)
    end
end

function MachineTurretWeapon:TurretHoverLoop(slot)
    local elapsed = 0.0
    local hoverPhase = (slot.Index or 0) * 0.35
    local amplitude = self.Data.HoverAmplitude or DEFAULT_HOVER_AMPLITUDE
    local hoverSpeed = self.Data.HoverSpeed or DEFAULT_HOVER_SPEED
    local orbitSpeed = self.Data.OrbitSpeed or DEFAULT_ORBIT_SPEED

    while self.IsRunning and slot.IsRunning do
        local deltaTime, paused = GameplayPause.WaitNextFrame()
        if not paused and slot.Visual ~= nil and slot.Visual:IsValid() then
            elapsed = elapsed + math.max(deltaTime or 0.0, 0.0)

            if slot.BaseRelativeLocation == nil then
                slot.BaseRelativeLocation = slot.Visual:GetRelativeLocation()
            end

            local ownerActor = nil
            if self.Owner ~= nil and self.Owner.GetActor ~= nil then
                ownerActor = self.Owner.GetActor()
            end

            if ownerActor ~= nil and ownerActor:IsValid() then
                local ownerLocation = ownerActor:GetLocation()
                local radius = slot.OrbitRadius or self.Data.OrbitRadius or DEFAULT_ORBIT_RADIUS
                local angle = (slot.OrbitAngle or 0.0) + elapsed * orbitSpeed
                local zOffset = (GetVecAxis(slot.BaseRelativeLocation, "z", 3) or 0.0) +
                    math.sin((elapsed + hoverPhase) * hoverSpeed) * amplitude

                local location = {
                    x = (GetVecAxis(ownerLocation, "x", 1) or 0.0) + math.cos(angle) * radius,
                    y = (GetVecAxis(ownerLocation, "y", 2) or 0.0) + math.sin(angle) * radius,
                    z = (GetVecAxis(ownerLocation, "z", 3) or 0.0) + zOffset,
                }

                slot.Visual:SetWorldLocation(location)
            end
        end
    end

    self:ResetSlotHover(slot)
end

function MachineTurretWeapon:ResetSlotHover(slot)
    if slot == nil or slot.Visual == nil or not slot.Visual:IsValid() or slot.BaseRelativeLocation == nil then
        return
    end

    slot.Visual:SetRelativeLocation(slot.BaseRelativeLocation)
end

function MachineTurretWeapon:UpdateSlotTarget(slot)
    local origin = self:GetSlotOrigin(slot)

    if IsValidTarget(slot.Target, origin, slot.Range) then
        return
    end

    slot.Target = nil

    if origin == nil then
        return
    end

    if self.Owner ~= nil and self.Owner.QueryActorByTagClosest ~= nil then
        slot.Target = self.Owner.QueryActorByTagClosest("Enemy", origin, slot.Range)
    end
end

function MachineTurretWeapon:UpdateSlotAim(slot, deltaTime, snap)
    local origin = self:GetSlotOrigin(slot)

    if not IsValidTarget(slot.Target, origin, slot.Range) then
        return
    end

    if slot.Visual == nil or not slot.Visual:IsValid() then
        return
    end

    local myPos = slot.Visual:GetWorldLocation()
    local targetPos = slot.Target:GetLocation()

    if myPos == nil or targetPos == nil then
        return false
    end

    local z = GetVecAxis(myPos, "z", 3)

    if z ~= nil then
        SetVecAxis(targetPos, "z", 3, z)
    end

    if snap then
        return slot.Visual:LookAt(targetPos)
    end

    local desiredWorldYaw = GetYawToTarget(myPos, targetPos)
    if desiredWorldYaw == nil then
        return false
    end

    local rotation = slot.Visual:GetRelativeRotation()
    if rotation == nil then
        return false
    end

    local currentYaw = GetVecAxis(rotation, "z", 3) or 0.0
    local desiredYaw = NormalizeAngleDegrees(desiredWorldYaw - self:GetOwnerYaw())
    local turnSpeed = slot.AimTurnSpeed or self.Data.AimTurnSpeed or DEFAULT_AIM_TURN_SPEED
    local maxDelta = turnSpeed * math.max(deltaTime or 0.0, 0.0)
    local newYaw = NormalizeAngleDegrees(MoveAngleTowards(currentYaw, desiredYaw, maxDelta))

    SetVecAxis(rotation, "z", 3, newYaw)
    return slot.Visual:SetRelativeRotation(rotation)
end

function MachineTurretWeapon:PlayVfx(preset, location)
    if type(GetActorPoolManager) ~= "function" then return end
    
    local pool = GetActorPoolManager()
    if pool == nil or not pool:IsValid() then return end
    
    local vfxActor = pool:Acquire("ASubUVVfxActor")
    if vfxActor ~= nil and vfxActor:IsValid() then
        vfxActor:SetLocation(location)
        
        -- 머신건 이펙트 설정 (크기 2배, 재생 속도 20)
        vfxActor:SetScale({4.0, 4.0, 4.0})
        local subUV = vfxActor:GetComponentByName("USubUVComponent", "SubUV")
        if subUV ~= nil and subUV:IsValid() then
            subUV:SetFrameRate(20.0)
        end
        
        vfxActor:SetVfxPreset(preset)
        
        -- 일정 시간 후 반납
        if self.Owner ~= nil and self.Owner.StartCoroutine ~= nil then
            self.Owner.StartCoroutine(function()
                Co.Wait(0.5) -- 이펙트 재생 시간 충분히 확보
                if vfxActor:IsValid() and pool:IsValid() then
                    pool:Release(vfxActor)
                end
            end)
        end
    end
end

function MachineTurretWeapon:FireSlot(slot)
    local origin = self:GetSlotOrigin(slot)

    if not IsValidTarget(slot.Target, origin, slot.Range) then
        slot.Target = nil
        return
    end

    self:UpdateSlotAim(slot, 0.0, true)

    local ownerActor = nil
    if self.Owner ~= nil and self.Owner.GetActor ~= nil then
        ownerActor = self.Owner.GetActor()
    end

    local success = DamageSystem.ApplyDamage(slot.Target, slot.Damage, ownerActor)

    if success then
        -- 피격 이펙트
        local targetPos = slot.Target:GetLocation()
        if targetPos ~= nil then
            -- 땅에 묻히지 않도록 오프셋 추가
            local offsetPos = {
                x = targetPos.x,
                y = targetPos.y,
                z = targetPos.z + 0.5
            }
            self:PlayVfx("MachineGunHit", offsetPos)
        end
    else
        Log("[MachineTurret] Damage failed. slot=" ..
            tostring(slot.Index) ..
            " damage=" ..
            tostring(slot.Damage))
    end

    if self.Owner ~= nil and self.Owner.NotifyInstantHit ~= nil then
        self.Owner.NotifyInstantHit("MachineTurret", self.Data, slot.Index, slot.Target)
    end
end

return MachineTurretWeapon
