local Audio = require("Core.Audio")
local CameraShake = require("Core.CameraShake")
local DamageSystem = require("Core.DamageSystem")
local GameplayPause = require("GameplayPause")
local Vec = require("Core.Vector")
local WeaponDefs = require("WeaponDefs")

local VehicleRushWeapon = {}
VehicleRushWeapon.__index = VehicleRushWeapon

local function Components(v)
    return v.x or v.X or v[1] or 0.0,
           v.y or v.Y or v[2] or 0.0,
           v.z or v.Z or v[3] or 0.0
end

local function Dot2(a, b)
    local ax, ay = Components(a)
    local bx, by = Components(b)
    return ax * bx + ay * by
end

local function IsValidActor(actor)
    if actor == nil or not actor:IsValid() then
        return false
    end

    if actor.IsVisible ~= nil and not actor:IsVisible() then
        return false
    end

    return true
end

local function GetActorKey(actor)
    if actor ~= nil and actor.GetUUID ~= nil then
        return tostring(actor:GetUUID())
    end

    return tostring(actor)
end

local function SetVehicleVisualPose(visual, position, target)
    if visual == nil or not visual:IsValid() then
        return
    end

    visual:SetWorldLocation(position)
    visual:LookAt(target)
end

local function FindDensestEnemyPosition(world, fallbackPosition, playerPosition, data)
    if world == nil or world.GetActorsByTag == nil then
        return fallbackPosition, 0
    end

    local enemies = world:GetActorsByTag("Enemy")
    if enemies == nil then
        return fallbackPosition, 0
    end

    local cellSize = math.max(data.TargetGridSize or data.VehicleWidth or 6.0, 1.0)
    local searchRadius = data.TargetSearchRadius or
        math.max((data.MaxPassOffset or 18.0) + ((data.VehicleWidth or 5.0) * 2.0), 24.0)
    local searchRadiusSquared = searchRadius * searchRadius
    local cells = {}
    local bestCell = nil
    local _, _, fallbackZ = Components(fallbackPosition)

    for _, enemy in ipairs(enemies) do
        if IsValidActor(enemy) then
            local location = enemy:GetLocation()
            if Vec.DistanceSquared(playerPosition, location) <= searchRadiusSquared then
                local x, y = Components(location)
                local cellX = math.floor(x / cellSize)
                local cellY = math.floor(y / cellSize)
                local key = tostring(cellX) .. ":" .. tostring(cellY)
                local cell = cells[key]

                if cell == nil then
                    cell = {
                        Count = 0,
                        SumX = 0.0,
                        SumY = 0.0,
                    }
                    cells[key] = cell
                end

                cell.Count = cell.Count + 1
                cell.SumX = cell.SumX + x
                cell.SumY = cell.SumY + y

                if bestCell == nil or cell.Count > bestCell.Count then
                    bestCell = cell
                end
            end
        end
    end

    if bestCell == nil or bestCell.Count <= 0 then
        return fallbackPosition, 0
    end

    local invCount = 1.0 / bestCell.Count
    return Vec.New(
        bestCell.SumX * invCount,
        bestCell.SumY * invCount,
        fallbackZ
    ), bestCell.Count
end

local function ComputeSafePassDistance(origin, playerPosition, dir, data, laneExtent)
    local minDistance = data.SpawnDistance or 50.0
    local viewRadius = data.ViewSafeRadius or 78.0
    local safetyMargin = data.ViewSafeMargin or 10.0
    local halfLength = (data.VehicleLength or 5.0) * 0.5
    local radius = viewRadius + safetyMargin + halfLength + (laneExtent or 0.0)
    local offset = Vec.Sub(origin, playerPosition)
    local forward = Dot2(offset, dir)
    local offsetLengthSquared = Dot2(offset, offset)
    local sideLengthSquared = math.max(offsetLengthSquared - (forward * forward), 0.0)
    local radiusSquared = radius * radius

    if sideLengthSquared >= radiusSquared then
        return minDistance
    end

    local exitDistanceOnLine = math.sqrt(radiusSquared - sideLengthSquared)
    local forwardExitDistance = -forward + exitDistanceOnLine
    local backwardExitDistance = forward + exitDistanceOnLine

    return math.max(minDistance, forwardExitDistance, backwardExitDistance)
end

function VehicleRushWeapon.New(owner)
    local self = setmetatable({}, VehicleRushWeapon)
    self.Owner = owner
    self.Id = "VehicleRush"
    self.Def = WeaponDefs.VehicleRush
    self.Level = 1
    self.Data = self.Def.Levels[self.Level]
    self.Coroutine = nil
    self.IsRunning = false
    self.Visuals = {}
    self.ResumeSoundPositions = {}
    self.ActiveSpawnSound = nil
    self.ActiveSpawnSoundElapsed = 0.0
    self.IsPassActive = false
    self.PendingVisualApply = nil
    return self
end

function VehicleRushWeapon:Start()
    if self.IsRunning then
        return
    end

    self.IsRunning = true
    self:OnVisualApplied()

    if self.Owner ~= nil and self.Owner.StartCoroutine ~= nil then
        self.Coroutine = self.Owner.StartCoroutine(function()
            self:AttackLoop()
        end)
    else
        Log("[VehicleRush] StartCoroutine is nil")
    end
end

function VehicleRushWeapon:Stop()
    self.IsRunning = false

    if self.Owner ~= nil and self.Owner.StopCoroutine ~= nil and self.Coroutine ~= nil then
        self.Owner.StopCoroutine(self.Coroutine)
    end

    self.Coroutine = nil
    self:FinishSpawnSound(self.ActiveSpawnSound, self.ActiveSpawnSoundElapsed)
    self.ActiveSpawnSound = nil
    self.ActiveSpawnSoundElapsed = 0.0
    self.IsPassActive = false
    self.PendingVisualApply = nil
    self:HideVisuals()
end

function VehicleRushWeapon:AttackLoop()
    while self.IsRunning do
        self:WaitWhilePaused()

        if self.IsRunning then
            self:RunVehiclePass()
        end

        if self.IsRunning then
            GameplayPause.Wait(self.Data.FireInterval or 5.0)
        end
    end
end

function VehicleRushWeapon:WaitWhilePaused()
    while self.IsRunning and GameplayPause.IsPaused() do
        GameplayPause.WaitNextFrame()
    end
end

function VehicleRushWeapon:Upgrade()
    if self:IsMaxLevel() then
        Log("[VehicleRush] is max level")
        return false
    end

    self.Level = self.Level + 1
    self.Data = self.Def.Levels[self.Level]
    Log("[VehicleRush] upgraded. level = " .. tostring(self.Level))
    return true
end

function VehicleRushWeapon:IsMaxLevel()
    return self.Level >= self.Def.MaxLevel
end

function VehicleRushWeapon:RequestVisualApply(applyVisual)
    if self.IsPassActive then
        self.PendingVisualApply = applyVisual
        return false
    end

    if type(applyVisual) == "function" then
        applyVisual()
    end

    return true
end

function VehicleRushWeapon:ApplyPendingVisual()
    local applyVisual = self.PendingVisualApply
    self.PendingVisualApply = nil

    if type(applyVisual) == "function" then
        applyVisual()
    end
end

function VehicleRushWeapon:OnVisualApplied()
    self.Visuals = {}
    self:RefreshVisuals()
    self:HideVisuals()
end

function VehicleRushWeapon:RefreshVisuals()
    if self.Owner == nil or self.Owner.GetComponentByName == nil then
        return
    end

    local maxCount = self.Def.MaxVehicleCount or 1
    for i = 1, maxCount do
        local name = "Visual_VehicleRush_" .. tostring(i - 1)
        local visual = self.Owner.GetComponentByName("UStaticMeshComponent", name)
        if visual ~= nil and visual:IsValid() then
            self.Visuals[i] = visual
        end
    end
end

function VehicleRushWeapon:HideVisuals()
    for _, visual in pairs(self.Visuals) do
        if visual ~= nil and visual:IsValid() then
            visual:SetVisibility(false)
        end
    end
end

function VehicleRushWeapon:PlaySpawnSound(data)
    data = data or self.Data

    local sound = data.Sound
    if sound == nil then
        return nil
    end

    if type(sound) == "string" then
        Audio.Play(sound, Audio.Bus.SFX, 1.0)
        return nil
    end

    local key = sound.Spawn or sound.Key
    local handle = Audio.Play(
        key,
        sound.Bus or Audio.Bus.SFX,
        sound.Volume or 1.0
    )

    if handle == nil or sound.Resume ~= true then
        return nil
    end

    local durationMs = Audio.GetLength(key) or 0.0
    if durationMs <= 0.0 then
        durationMs = sound.DurationMs or 0.0
    end
    local startPositionMs = self.ResumeSoundPositions[key] or 0.0
    if durationMs > 0.0 then
        startPositionMs = startPositionMs % durationMs
    end

    Audio.SetPosition(handle, startPositionMs)

    return {
        Handle = handle,
        Key = key,
        Sound = sound,
        StartPositionMs = startPositionMs,
        DurationMs = durationMs,
    }
end

function VehicleRushWeapon:FinishSpawnSound(playback, elapsed)
    if playback == nil then
        return
    end

    local key = playback.Key
    local handle = playback.Handle
    local sound = playback.Sound or {}
    local positionMs = Audio.GetPosition(handle)

    if positionMs == nil or positionMs <= 0.0 then
        positionMs = (playback.StartPositionMs or 0.0) + ((elapsed or 0.0) * 1000.0)
    end

    local durationMs = playback.DurationMs or Audio.GetLength(key) or 0.0
    if durationMs <= 0.0 then
        durationMs = sound.DurationMs or 0.0
    end
    if durationMs > 0.0 then
        positionMs = positionMs % durationMs
    end

    if key ~= nil then
        self.ResumeSoundPositions[key] = positionMs
    end

    Audio.Stop(handle)
end

function VehicleRushWeapon:GetVehicleCameraShakeSettings(data)
    data = data or self.Data

    local isTrain = (self.Level or 1) >= 3 or (data.VehicleLength or 0.0) >= 40.0
    local asset = data.CameraShakeAsset or (isTrain and "TrainRush" or "VehicleRush")
    local scale = data.CameraShakeScale
    local interval = data.CameraShakeInterval

    if scale == nil then
        scale = isTrain and 0.32 or 0.24
        scale = scale + math.max((self.Level or 1) - 1, 0) * 0.03
    end

    if interval == nil then
        interval = isTrain and 0.62 or 0.28
    end

    scale = math.max(scale, 0.0)
    interval = math.max(interval, 0.05)

    return asset, scale, interval
end

function VehicleRushWeapon:PlayVehicleCameraShake(data)
    local asset, scale = self:GetVehicleCameraShakeSettings(data)
    CameraShake.Play(asset, scale)
end

function VehicleRushWeapon:GetOwnerActor()
    if self.Owner == nil or self.Owner.GetActor == nil then
        return nil
    end

    local ownerActor = self.Owner.GetActor()
    if ownerActor ~= nil and ownerActor:IsValid() then
        return ownerActor
    end

    return nil
end

function VehicleRushWeapon:GetWorld(ownerActor)
    if self.Owner ~= nil and self.Owner.GetWorld ~= nil then
        local world = self.Owner.GetWorld()
        if world ~= nil and world:IsValid() then
            return world
        end
    end

    if ownerActor ~= nil and ownerActor.GetWorld ~= nil then
        local world = ownerActor:GetWorld()
        if world ~= nil and world:IsValid() then
            return world
        end
    end

    return nil
end

function VehicleRushWeapon:RunVehiclePass()
    local ownerActor = self:GetOwnerActor()
    if ownerActor == nil then
        return
    end

    local world = self:GetWorld(ownerActor)
    if world == nil then
        return
    end

    self:RefreshVisuals()

    local data = self.Data
    local playerPos = ownerActor:GetLocation()
    local angle = math.random() * math.pi * 2.0
    local dir = Vec.New(math.cos(angle), math.sin(angle), 0.0)
    local side = Vec.New(-math.sin(angle), math.cos(angle), 0.0)
    local minOffset = data.MinPassOffset or 8.0
    local maxOffset = math.max(data.MaxPassOffset or 18.0, minOffset)
    local offsetDistance = minOffset + math.random() * (maxOffset - minOffset)

    if math.random(0, 1) == 0 then
        offsetDistance = -offsetDistance
    end

    local randomOrigin = Vec.Add(playerPos, Vec.Mul(side, offsetDistance))
    local origin, densestEnemyCount = FindDensestEnemyPosition(world, randomOrigin, playerPos, data)

    local speed = math.max(data.VehicleSpeed or 35.0, 0.001)
    local count = data.VehicleCount or 1
    local laneSpacing = data.LaneSpacing or 5.0
    local halfCount = (count - 1) * 0.5
    local laneExtent = math.abs(halfCount * laneSpacing)
    local spawnDistance = ComputeSafePassDistance(origin, playerPos, dir, data, laneExtent)
    local duration = (spawnDistance * 2.0) / speed
    local vehicles = {}

    for i = 1, count do
        local laneOffset = (i - 1 - halfCount) * laneSpacing
        local laneBase = Vec.Add(origin, Vec.Mul(side, laneOffset))
        local startPos = Vec.Sub(laneBase, Vec.Mul(dir, spawnDistance))
        local endPos = Vec.Add(laneBase, Vec.Mul(dir, spawnDistance))
        local visual = self.Visuals[i]

        if visual ~= nil and visual:IsValid() then
            SetVehicleVisualPose(visual, startPos, endPos)
            visual:SetVisibility(true)
        end

        table.insert(vehicles, {
            Start = startPos,
            End = endPos,
            Previous = startPos,
            Visual = visual,
        })
    end

    local spawnSound = self:PlaySpawnSound(data)
    self.ActiveSpawnSound = spawnSound
    self.ActiveSpawnSoundElapsed = 0.0
    self.IsPassActive = true
    self:PlayVehicleCameraShake(data)

    Log("[VehicleRush] vehicle pass started. count=" ..
        tostring(count) ..
        " offset=" ..
        tostring(offsetDistance) ..
        " densestEnemies=" ..
        tostring(densestEnemyCount) ..
        " passDistance=" ..
        tostring(spawnDistance))

    local elapsed = 0.0
    local hitActors = {}
    local _, _, cameraShakeInterval = self:GetVehicleCameraShakeSettings(data)
    local nextCameraShakeTime = cameraShakeInterval

    self:UpdateVehicles(world, ownerActor, vehicles, dir, side, 0.0, hitActors, data)

    while self.IsRunning and elapsed < duration do
        local dt, paused = GameplayPause.WaitNextFrame()
        dt = dt or 0.0

        if not paused then
            elapsed = elapsed + dt
            self.ActiveSpawnSoundElapsed = elapsed

            local alpha = elapsed / duration
            if alpha > 1.0 then
                alpha = 1.0
            end

            if elapsed >= nextCameraShakeTime and elapsed < duration then
                self:PlayVehicleCameraShake(data)
                repeat
                    nextCameraShakeTime = nextCameraShakeTime + cameraShakeInterval
                until nextCameraShakeTime > elapsed
            end

            self:UpdateVehicles(world, ownerActor, vehicles, dir, side, alpha, hitActors, data)
        end
    end

    for _, vehicle in ipairs(vehicles) do
        local visual = vehicle.Visual
        if visual ~= nil and visual:IsValid() then
            visual:SetVisibility(false)
        end
    end

    self:FinishSpawnSound(spawnSound, elapsed)
    self.ActiveSpawnSound = nil
    self.ActiveSpawnSoundElapsed = 0.0
    self.IsPassActive = false
    self:ApplyPendingVisual()
end

function VehicleRushWeapon:UpdateVehicles(world, ownerActor, vehicles, dir, side, alpha, hitActors, data)
    for _, vehicle in ipairs(vehicles) do
        local position = Vec.Lerp(vehicle.Start, vehicle.End, alpha)
        local previous = vehicle.Previous or position
        local visual = vehicle.Visual

        if visual ~= nil and visual:IsValid() then
            SetVehicleVisualPose(visual, position, vehicle.End)
        end

        self:DamageEnemiesOnSegment(world, ownerActor, previous, position, dir, side, hitActors, data)
        vehicle.Previous = position
    end
end

function VehicleRushWeapon:DamageEnemiesOnSegment(world, ownerActor, previous, position, dir, side, hitActors, data)
    local enemies = world:GetActorsByTag("Enemy")
    if enemies == nil then
        return
    end

    data = data or self.Data

    local halfLength = (data.VehicleLength or 5.0) * 0.5
    local halfWidth = (data.VehicleWidth or 2.0) * 0.5
    local damage = data.Damage or 20.0
    local segmentLength = math.sqrt(Vec.DistanceSquared(previous, position))

    for _, enemy in ipairs(enemies) do
        if IsValidActor(enemy) then
            local key = GetActorKey(enemy)
            if hitActors[key] == nil then
                local targetLocation = enemy:GetLocation()
                local targetRadius = data.TargetRadius or 0.5
                local offset = Vec.Sub(targetLocation, previous)
                local forwardDistance = Dot2(offset, dir)
                local sideDistance = math.abs(Dot2(offset, side))

                if forwardDistance >= -halfLength - targetRadius and
                    forwardDistance <= segmentLength + halfLength + targetRadius and
                    sideDistance <= halfWidth + targetRadius then
                    local success = DamageSystem.ApplyDamage(enemy, damage, ownerActor)

                    if success then
                        hitActors[key] = true
                        Log("[VehicleRush] hit " .. enemy:GetName() .. " damage=" .. tostring(damage))
                    else
                        Log("[VehicleRush] damage failed. target=" .. enemy:GetName())
                    end
                end
            end
        end
    end
end

return VehicleRushWeapon
