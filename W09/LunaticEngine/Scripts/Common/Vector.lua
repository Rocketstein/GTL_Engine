local Math = require("Common.Math")

------------------------------------------------------------------------------
-- Vector Helper Class
------------------------------------------------------------------------------

local VectorUtils = {}

-- vector 생성 함수
function VectorUtils.Make(x, y, z)
    if vec3 then
        return vec3(x, y, z)
    end

    if type(Vector) == "function" then
        return Vector(x, y, z)
    end

    if Vector and Vector.new then
        return Vector.new(x, y, z)
    end

    if Vector3 then
        return Vector3(x, y, z)
    end

    return { x = x, y = y, z = z }
end

-- Vector에서 X, Y, Z 축 가져오는 함수
function VectorUtils.GetX(v)
    if not v then
        return 0.0
    end

    return v.x or v.X or v[1] or 0.0
end

function VectorUtils.GetY(v)
    if not v then
        return 0.0
    end

    return v.y or v.Y or v[2] or 0.0
end

function VectorUtils.GetZ(v)
    if not v then
        return 0.0
    end

    return v.z or v.Z or v[3] or 0.0
end

-- vector간 선형 보간
function VectorUtils.Lerp(a, b, t)
    return VectorUtils.Make(
        Math.Lerp(VectorUtils.GetX(a), VectorUtils.GetX(b), t),
        Math.Lerp(VectorUtils.GetY(a), VectorUtils.GetY(b), t),
        Math.Lerp(VectorUtils.GetZ(a), VectorUtils.GetZ(b), t)
    )
end

-- vector 복사 함수
function VectorUtils.Copy(v)
    return VectorUtils.Make(
        VectorUtils.GetX(v),
        VectorUtils.GetY(v),
        VectorUtils.GetZ(v)
    )
end

return VectorUtils
