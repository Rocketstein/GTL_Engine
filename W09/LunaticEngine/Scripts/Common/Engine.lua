------------------------------------------------------------------------------
-- Actor 와 Component 관련 함수
------------------------------------------------------------------------------
local Engine = {}

-- Object, Actor, Component Valid 체크 함수
function Engine.IsValidObject(value)
    return value ~= nil and value.IsValid ~= nil and value:IsValid()
end

function Engine.IsValidActor(actor)
    return Engine.IsValidObject(actor)
end

function Engine.IsValidComponent(component)
    return Engine.IsValidObject(component)
end

-- Component 가져오는 함수
function Engine.GetComponent(owner, ...)
    if not owner or not owner.GetComponent then
        return nil
    end

    local names = { ... }

    for index = 1, #names do
        local name = names[index]
        local component = owner:GetComponent(name)

        if Engine.IsValidComponent(component) then
            return component
        end
    end

    return nil
end

-- 요청된 Component를 가져오는 함수
function Engine.GetRequiredComponent(owner, missing_message, ...)
    local component = Engine.GetComponent(owner, ...)
    if component then
        return component
    end

    local names = { ... }
    warn(missing_message or "Component missing:", table.concat(names, ", "))
    return nil
end

-- type으로 component를 가져오는 함수
function Engine.GetComponentByType(owner, type_name)
    if not Engine.IsValidObject(owner) or not owner.GetComponentByType then
        return nil
    end

    local component = owner:GetComponentByType(type_name)
    if Engine.IsValidComponent(component) then
        return component
    end

    return nil
end

-- static mesh component를 가져오는 함수 (sliding 시 크기 조절)
function Engine.GetStaticMeshComponent(actor)
    if not Engine.IsValidActor(actor) or not actor.GetStaticMeshComponent then
        return nil
    end

    local mesh = actor:GetStaticMeshComponent()
    if Engine.IsValidComponent(mesh) then
        return mesh
    end

    return nil
end

-- ShapeComponent 가져오는 함수
function Engine.FindCollisionShape(actor)
    local shape = Engine.GetComponentByType(actor, "UBoxComponent")
    if shape then
        return shape
    end

    shape = Engine.GetComponentByType(actor, "USphereComponent")
    if shape then
        return shape
    end

    return Engine.GetComponentByType(actor, "UCapsuleComponent")
end

return Engine
