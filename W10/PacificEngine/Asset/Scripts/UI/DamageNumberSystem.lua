local DamageNumberSystem = {}

local presenter = nil

local function copyVec3(value)
    if value == nil then
        return nil
    end

    return {
        x = value.x or value[1] or 0.0,
        y = value.y or value[2] or 0.0,
        z = value.z or value[3] or 0.0,
    }
end

function DamageNumberSystem.RegisterPresenter(inPresenter)
    presenter = inPresenter
end

function DamageNumberSystem.UnregisterPresenter(inPresenter)
    if presenter == inPresenter then
        presenter = nil
    end
end

function DamageNumberSystem.ShowDamage(targetActor, amount)
    if presenter == nil or presenter.ShowDamageAt == nil then
        return
    end

    if targetActor == nil or not targetActor:IsValid() then
        return
    end

    local location = copyVec3(targetActor:GetLocation())
    if location == nil then
        return
    end

    presenter:ShowDamageAt(location, amount or 0.0)
end

return DamageNumberSystem
