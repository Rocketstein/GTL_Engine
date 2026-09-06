local WeaponDefs = require("WeaponDefs")

local UIStyle = {}

UIStyle.Colors = {
    Overlay = { 0.0, 0.0, 0.0, 0.62 },
    Panel = { 0.028, 0.038, 0.060, 0.96 },
    PanelAccent = { 0.98, 0.82, 0.34, 1.0 },
    Card = { 0.075, 0.095, 0.135, 0.96 },
    CardHover = { 0.115, 0.150, 0.210, 0.98 },
    CardPressed = { 0.050, 0.070, 0.105, 0.98 },
    CardHighlight = { 0.135, 0.115, 0.055, 0.98 },
    Text = { 0.96, 0.97, 1.0, 1.0 },
    MutedText = { 0.64, 0.70, 0.80, 1.0 },
    SubText = { 0.78, 0.83, 0.92, 1.0 },
    Gold = { 1.0, 0.82, 0.32, 1.0 },
    Silver = { 0.78, 0.84, 0.92, 1.0 },
    Bronze = { 0.90, 0.56, 0.30, 1.0 },
    Clear = { 0.56, 1.0, 0.70, 1.0 },
    Over = { 1.0, 0.44, 0.36, 1.0 },
    Button = { 0.105, 0.175, 0.285, 0.95 },
    ButtonHover = { 0.145, 0.235, 0.365, 0.98 },
    ButtonPressed = { 0.070, 0.120, 0.205, 0.98 },
}

local SkillIcons = {
    Aura = "Asset/Content/Textures/UI/skillicon_aura.png",
    MainCannon = "Asset/Content/Textures/UI/skillicon_MainCannon.png",
    MachineTurret = "Asset/Content/Textures/UI/skillicon_MachineTurret.png",
    VehicleRush = "Asset/Content/Textures/UI/skillicon_VehicleRush.png",
}

local WeaponDescriptions = {
    Aura = "Damages nearby enemies in a rotating field.",
    MainCannon = "Fires a heavy shell through enemies ahead.",
    MachineTurret = "Adds rapid auto-targeting turret fire.",
    VehicleRush = "Calls vehicles that sweep across the battlefield.",
}

local PreviewKeys = {
    MainCannon = { "Damage", "FireInterval", "Pierce", "ProjectileScale" },
    MachineTurret = { "TurretCount", "FireInterval", "Damage", "TargetRefreshInterval" },
    VehicleRush = { "VehicleCount", "FireInterval", "VehicleSpeed", "VehicleLength" },
    Aura = { "Radius", "TickInterval", "Damage", "RotateSpeed" },
}

local LabelByKey = {
    Damage = "DMG",
    FireInterval = "Cooldown",
    Pierce = "Pierce",
    ProjectileScale = "Size",
    TurretCount = "Turrets",
    TargetRefreshInterval = "Aim",
    VehicleCount = "Vehicles",
    VehicleSpeed = "Speed",
    VehicleLength = "Length",
    Radius = "Radius",
    TickInterval = "Tick",
    RotateSpeed = "Spin",
}

local function copyColor(color)
    if color == nil then
        return { 1.0, 1.0, 1.0, 1.0 }
    end

    return {
        color[1] or color.r or color.R or 1.0,
        color[2] or color.g or color.G or 1.0,
        color[3] or color.b or color.B or 1.0,
        color[4] or color.a or color.A or 1.0,
    }
end

local function clamp(value, minValue, maxValue)
    if value < minValue then
        return minValue
    end
    if value > maxValue then
        return maxValue
    end
    return value
end

local function formatNumber(value)
    local number = tonumber(value)
    if number == nil then
        return tostring(value)
    end
    if math.abs(number - math.floor(number + 0.5)) < 0.001 then
        return tostring(math.floor(number + 0.5))
    end
    return string.format("%.1f", number)
end

local function formatStatValue(key, value)
    if value == nil then
        return nil
    end

    if key == "FireInterval" or key == "TickInterval" or key == "TargetRefreshInterval" then
        return formatNumber(value) .. "s"
    end

    if key == "RotateSpeed" then
        return formatNumber(value) .. "/s"
    end

    return formatNumber(value)
end

local function statText(key, currentValue, nextValue)
    local label = LabelByKey[key] or key
    local nextText = formatStatValue(key, nextValue)
    if nextText == nil then
        return nil
    end

    local currentText = formatStatValue(key, currentValue)
    if currentText ~= nil and currentText ~= nextText then
        return label .. " " .. currentText .. ">" .. nextText
    end

    return label .. " " .. nextText
end

function UIStyle.CopyColor(color)
    return copyColor(color)
end

function UIStyle.MultiplyColor(color, scale, alphaScale)
    local source = copyColor(color)
    local tintScale = scale or 1.0
    local alpha = alphaScale or 1.0
    return {
        clamp(source[1] * tintScale, 0.0, 1.0),
        clamp(source[2] * tintScale, 0.0, 1.0),
        clamp(source[3] * tintScale, 0.0, 1.0),
        clamp(source[4] * alpha, 0.0, 1.0),
    }
end

function UIStyle.LerpColor(a, b, t)
    local left = copyColor(a)
    local right = copyColor(b)
    local alpha = clamp(t or 0.0, 0.0, 1.0)
    return {
        left[1] + (right[1] - left[1]) * alpha,
        left[2] + (right[2] - left[2]) * alpha,
        left[3] + (right[3] - left[3]) * alpha,
        left[4] + (right[4] - left[4]) * alpha,
    }
end

function UIStyle.GetSkillIconPath(weaponId)
    return SkillIcons[weaponId] or ""
end

function UIStyle.GetWeaponDescription(weaponId)
    return WeaponDescriptions[weaponId] or "Unlock a new combat option."
end

function UIStyle.GetUpgradePreview(weaponId, currentLevel, nextLevel)
    local definition = WeaponDefs[weaponId]
    if definition == nil or definition.Levels == nil then
        return ""
    end

    local nextData = definition.Levels[nextLevel or 1]
    if nextData == nil then
        return "Max level reached"
    end

    local currentData = nil
    if currentLevel ~= nil and currentLevel > 0 then
        currentData = definition.Levels[currentLevel]
    end

    local keys = PreviewKeys[weaponId] or { "Damage", "FireInterval", "Range" }
    local parts = {}
    for _, key in ipairs(keys) do
        local text = statText(key, currentData ~= nil and currentData[key] or nil, nextData[key])
        if text ~= nil then
            table.insert(parts, text)
        end
        if #parts >= 2 then
            break
        end
    end

    if #parts <= 0 then
        return "Ready to deploy"
    end

    return table.concat(parts, "   ")
end

return UIStyle
