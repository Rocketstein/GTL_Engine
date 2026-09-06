local Vector = require("Core.Vector")

---@class CollisionTest : ScriptComponent
---@class CollisionTest : Vector

local Script = {
    properties = {
        MoveSpeed = { type = "float", default = 1.0 }
    }
}

function Script:BeginPlay()
    -- empty
end

function Script:Tick(deltaTime)
    -- empty
end

function Script:OnCollision(other)
    Log("OnCollision")
    --Log("OnCollision with ".. other)
end

function Script:OnOverlapBegin(other)
    Log("OnOverlapBegin")
    Log("Type of other: " .. type(other))
    
    -- 기본 컴포넌트 메서드 테스트
    if other.GetName then
        Log("Other Name: " .. other:GetName())
        Log("Other Class: " .. other:GetClassName())
    end

    -- 구체적인 콜라이더 타입(Box/Sphere)이 필요하다면 Cast해서 사용
    local boxCollider = other:Cast("UBoxCollider2DComponent")
    if boxCollider then
        local extent = boxCollider:GetBoxExtent()
        Log("Box Extent: " .. extent.x .. ", " .. extent.y)
    else
        Log("this is not BoxCollider2D")
    end

    -- alias로도 Cast 가능 UBoxCollider2DComponent == 
    local boxCollider = other:Cast("BoxCollider2D")
    if boxCollider then
        local extent = boxCollider:GetBoxExtent()
        Log("Box Extent: " .. extent.x .. ", " .. extent.y)
    else
        Log("this is not BoxCollider2D")
    end

    local circleCollider = other:Cast("CircleCollider2D")
    if circleCollider then
        local radius = circleCollider:GetRadius()
        Log("Circle radius: " .. radius)
    else
        Log("this is not CircleCollider2D")
    end

    -- 등록되지 않은 타입으로는 캐스팅 불가
    local otherCastResult = other:Cast("Cat")
    if otherCastResult then
        Log("this is a cat")
    else
        Log("this is not a cat")
    end
end

function Script:OnOverlapEnd(other)
    Log("OnOverlapEnd")
    --Log("OnOverlapEnd with ".. other)
end

function Script:EndPlay()
    -- empty
end

return Script
