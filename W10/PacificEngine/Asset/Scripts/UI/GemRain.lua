---@class GemRain : ScriptComponent
local Script = {
    properties = {
        MaxGems = { type = "int", default = 28, min = 1, max = 200, speed = 1 },
        AutoStart = { type = "bool", default = true },
        InitialBurst = { type = "int", default = 10, min = 0, max = 100, speed = 1 },
        SpawnInterval = { type = "float", default = 0.12, min = 0.01, max = 2.0, speed = 0.01 },
        TopOffset = { type = "float", default = -72.0, min = -400.0, max = 0.0, speed = 1.0 },
        FallDistance = { type = "float", default = 760.0, min = 100.0, max = 2000.0, speed = 5.0 },
        CornerBand = { type = "float", default = 0.22, min = 0.02, max = 0.50, speed = 0.01 },
        StartSize = { type = "float", default = 72.0, min = 4.0, max = 256.0, speed = 1.0 },
        EndSizeScale = { type = "float", default = 0.35, min = 0.05, max = 1.0, speed = 0.01 },
        MinSpeed = { type = "float", default = 140.0, min = 1.0, max = 1000.0, speed = 5.0 },
        MaxSpeed = { type = "float", default = 320.0, min = 1.0, max = 1000.0, speed = 5.0 },
        MaxHorizontalDrift = { type = "float", default = 0.055, min = 0.0, max = 0.5, speed = 0.005 },
        MaxRotationSpeed = { type = "float", default = 90.0, min = 0.0, max = 720.0, speed = 5.0 },
        Seed = { type = "int", default = 31, min = 1, max = 999999, speed = 1 },
    }
}

local textures = {
    "Asset/Content/Textures/UI/gem_green.png",
    "Asset/Content/Textures/UI/gem_blue.png",
    "Asset/Content/Textures/UI/gem_red.png",
}

local function clamp(value, minValue, maxValue)
    if value < minValue then return minValue end
    if value > maxValue then return maxValue end
    return value
end

local function randf(minValue, maxValue)
    return minValue + (maxValue - minValue) * math.random()
end

local function randomCornerAnchorX(cornerBand)
    local band = clamp(cornerBand, 0.02, 0.5)
    if math.random(1, 2) == 1 then
        return randf(0.0, band)
    end
    return randf(1.0 - band, 1.0)
end

function Script:BeginPlay()
    math.randomseed(self.Seed)

    self.pool = GetActorPoolManager()
    self.gems = {}
    self.spawnTimer = 0.0
    self.enabled = false

    if self.AutoStart then
        self:Start()
    end
end

function Script:Start()
    if self.enabled then
        return
    end

    self.enabled = true
    self.spawnTimer = 0.0

    local burst = math.min(self.InitialBurst, self.MaxGems)
    for _ = 1, burst do
        self:SpawnGem()
    end
end

function Script:Stop()
    self.enabled = false
    self:ClearGems()
end

function Script:SetEnabled(enabled)
    if enabled then
        self:Start()
    else
        self:Stop()
    end
end

function Script:ClearGems()
    if self.gems == nil then
        return
    end

    for i = #self.gems, 1, -1 do
        self:ReleaseGem(i)
    end
end

function Script:SpawnGem()
    if self.pool == nil or not self.pool:IsValid() then
        return
    end

    if #self.gems >= self.MaxGems then
        return
    end

    local actor = self.pool:Acquire("AUIActor")
    if actor == nil or not actor:IsValid() then
        return
    end

    local ui = actor:GetComponent("UUIComponent")
    if ui == nil or not ui:IsValid() or not ui:IsUIComponent() then
        self.pool:Release(actor)
        return
    end

    local speedMin = math.min(self.MinSpeed, self.MaxSpeed)
    local speedMax = math.max(self.MinSpeed, self.MaxSpeed)
    --local anchorX = randomCornerAnchorX(self.CornerBand)
    local anchorX = randf(0.0, 1.0)
    local startSize = self.StartSize * randf(0.85, 1.20)

    local gem = {
        actor = actor,
        ui = ui,
        anchorX = anchorX,
        y = self.TopOffset,
        speed = randf(speedMin, speedMax),
        drift = randf(-self.MaxHorizontalDrift, self.MaxHorizontalDrift),
        rotation = randf(0.0, 360.0),
        rotationSpeed = randf(-self.MaxRotationSpeed, self.MaxRotationSpeed),
        startSize = startSize,
    }

    ui:SetUITexturePath(textures[math.random(1, #textures)])
    ui:SetUITint(1.0, 1.0, 1.0, 1.0)
    ui:SetUIPivot({ 0.5, 0.5 })
    ui:SetUIAnchor({ gem.anchorX, 0.0 })
    ui:SetUIAnchoredPosition({ 0.0, gem.y })
    ui:SetUISizeDelta({ startSize, startSize })
    ui:SetUIRotationDegrees(gem.rotation)
    ui:SetUIVisibility(true)

    table.insert(self.gems, gem)
end

function Script:ReleaseGem(index)
    local gem = self.gems[index]
    if gem == nil then
        return
    end

    if gem.ui ~= nil and gem.ui:IsValid() then
        gem.ui:SetUIRotationDegrees(0.0)
        gem.ui:SetUIVisibility(false)
    end

    if self.pool ~= nil and self.pool:IsValid() and gem.actor ~= nil and gem.actor:IsValid() then
        self.pool:Release(gem.actor)
    end

    table.remove(self.gems, index)
end

function Script:Tick(deltaTime)
    if not self.enabled then
        return
    end

    if self.pool == nil or not self.pool:IsValid() then
        return
    end

    local interval = math.max(self.SpawnInterval, 0.01)
    self.spawnTimer = self.spawnTimer + deltaTime
    while self.spawnTimer >= interval and #self.gems < self.MaxGems do
        self.spawnTimer = self.spawnTimer - interval
        self:SpawnGem()
    end

    local endY = self.FallDistance
    local travel = math.max(endY - self.TopOffset, 1.0)
    local endScale = clamp(self.EndSizeScale, 0.05, 1.0)

    for i = #self.gems, 1, -1 do
        local gem = self.gems[i]
        local ui = gem.ui

        if ui == nil or not ui:IsValid() then
            self:ReleaseGem(i)
        else
            gem.y = gem.y + gem.speed * deltaTime
            gem.anchorX = clamp(gem.anchorX + gem.drift * deltaTime, 0.0, 1.0)
            gem.rotation = gem.rotation + gem.rotationSpeed * deltaTime

            local t = clamp((gem.y - self.TopOffset) / travel, 0.0, 1.0)
            local size = gem.startSize * (1.0 + (endScale - 1.0) * t)

            ui:SetUIAnchor({ gem.anchorX, 0.0 })
            ui:SetUIAnchoredPosition({ 0.0, gem.y })
            ui:SetUISizeDelta({ size, size })
            ui:SetUIRotationDegrees(gem.rotation)

            if gem.y > endY then
                self:ReleaseGem(i)
            end
        end
    end
end

function Script:EndPlay()
    self:Stop()
end

return Script
