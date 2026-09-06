---@class DamageNumberUI : ScriptComponent
local Script = {
    properties = {
        WarmupCount = { type = "int", default = 32, min = 0, max = 256 },
        MaxActive = { type = "int", default = 64, min = 1, max = 256 },
        Lifetime = { type = "float", default = 0.75, min = 0.1, max = 5.0, speed = 0.05 },
        RiseDistance = { type = "float", default = 1.1, min = 0.0, max = 5.0, speed = 0.05 },
        SpawnHeight = { type = "float", default = 1.6, min = 0.0, max = 5.0, speed = 0.05 },
        RandomRadius = { type = "float", default = 0.25, min = 0.0, max = 3.0, speed = 0.05 },
        WorldWidth = { type = "float", default = 2.6, min = 0.1, max = 8.0, speed = 0.1 },
        WorldHeight = { type = "float", default = 0.9, min = 0.1, max = 4.0, speed = 0.1 },
        FontSize = { type = "float", default = 1.35, min = 0.1, max = 4.0, speed = 0.05 },
    }
}

local DamageNumberSystem = require("UI.DamageNumberSystem")

local function vec2(x, y)
    return { x = x, y = y }
end

local function vec3(x, y, z)
    return { x = x, y = y, z = z }
end

local function copyVec3(value)
    return vec3(value.x or value[1] or 0.0, value.y or value[2] or 0.0, value.z or value[3] or 0.0)
end

local function lerp(a, b, t)
    return a + (b - a) * t
end

local function formatDamage(amount)
    amount = amount or 0.0
    local rounded = math.floor(amount + 0.5)
    if math.abs(amount - rounded) < 0.01 then
        return tostring(rounded)
    end

    return string.format("%.1f", amount)
end

function Script:BeginPlay()
    self.pool = GetActorPoolManager()
    self.activeNumbers = {}

    if self.pool ~= nil and self.pool:IsValid() then
        self.pool:Warmup("ATextUIActor", self.WarmupCount or 32)
    end

    DamageNumberSystem.RegisterPresenter(self)
end

function Script:ConfigureText(ui, amount)
    if ui == nil or not ui:IsValid() then
        return false
    end

    ui:SetUIRenderSpace("WorldSpace")
    ui:SetUIBillboard(true)
    ui:SetUIHitTestVisible(false)
    ui:SetUILayer(240)
    ui:SetUIZOrder(20)
    ui:SetUIWorldSize(vec2(self.WorldWidth or 2.6, self.WorldHeight or 0.9))
    ui:SetUIPivot(vec2(0.5, 0.5))
    ui:SetUIRotationDegrees(0.0)
    ui:SetUITint(1.0, 0.92, 0.32, 1.0)
    ui:SetUIText(formatDamage(amount))
    ui:SetUIFont("Default")
    ui:SetUIFontSize(self.FontSize or 1.35)
    ui:SetUITextHorizontalAlignment("Center")
    ui:SetUITextVerticalAlignment("Center")
    ui:SetUIVisibility(true)
    return true
end

function Script:ShowDamageAt(worldLocation, amount)
    if self.pool == nil or not self.pool:IsValid() then
        return
    end

    if worldLocation == nil then
        return
    end

    local maxActive = math.max(self.MaxActive or 64, 1)
    while #self.activeNumbers >= maxActive do
        self:ReleaseNumber(1)
    end

    local actor = self.pool:Acquire("ATextUIActor")
    if actor == nil or not actor:IsValid() then
        return
    end

    local ui = actor:GetComponent("UTextUIComponent")
    if not self:ConfigureText(ui, amount) then
        actor:SetVisible(false)
        self.pool:Release(actor)
        return
    end

    local base = copyVec3(worldLocation)
    local angle = math.random() * math.pi * 2.0
    local radius = math.random() * (self.RandomRadius or 0.25)
    local startPos = vec3(
        base.x + math.cos(angle) * radius,
        base.y + math.sin(angle) * radius,
        base.z + (self.SpawnHeight or 1.6)
    )
    local endPos = vec3(startPos.x, startPos.y, startPos.z + (self.RiseDistance or 1.1))

    actor:SetVisible(true)
    actor:SetLocation(startPos)

    table.insert(self.activeNumbers, {
        actor = actor,
        ui = ui,
        elapsed = 0.0,
        startPos = startPos,
        endPos = endPos,
    })
end

function Script:ReleaseNumber(index)
    local entry = self.activeNumbers[index]
    if entry == nil then
        return
    end

    if entry.ui ~= nil and entry.ui:IsValid() then
        entry.ui:SetUIVisibility(false)
    end

    if entry.actor ~= nil and entry.actor:IsValid() then
        entry.actor:SetVisible(false)
        if self.pool ~= nil and self.pool:IsValid() then
            self.pool:Release(entry.actor)
        end
    end

    table.remove(self.activeNumbers, index)
end

function Script:Tick(deltaTime)
    local life = math.max(self.Lifetime or 0.75, 0.01)

    for i = #self.activeNumbers, 1, -1 do
        local entry = self.activeNumbers[i]
        local actor = entry.actor
        local ui = entry.ui

        if actor == nil or not actor:IsValid() or ui == nil or not ui:IsValid() then
            self:ReleaseNumber(i)
        else
            entry.elapsed = entry.elapsed + (deltaTime or 0.0)
            local t = entry.elapsed / life

            if t >= 1.0 then
                self:ReleaseNumber(i)
            else
                local eased = 1.0 - ((1.0 - t) * (1.0 - t))
                actor:SetLocation(vec3(
                    lerp(entry.startPos.x, entry.endPos.x, eased),
                    lerp(entry.startPos.y, entry.endPos.y, eased),
                    lerp(entry.startPos.z, entry.endPos.z, eased)
                ))

                ui:SetUITint(1.0, 0.92, 0.32, 1.0 - t)
            end
        end
    end
end

function Script:EndPlay()
    DamageNumberSystem.UnregisterPresenter(self)

    for i = #self.activeNumbers, 1, -1 do
        self:ReleaseNumber(i)
    end
end

return Script
