local Vec = {}

local function NewVector(X, Y, Z)
    return FVector.new(X or 0.0, Y or 0.0, Z or 0.0)
end

local function Components(V)
    return V.X or V.x or V[1] or 0.0,
           V.Y or V.y or V[2] or 0.0,
           V.Z or V.z or V[3] or 0.0
end

function Vec.New(X, Y, Z)
    return NewVector(X, Y, Z)
end

function Vec.Zero()
    return NewVector(0.0, 0.0, 0.0)
end

function Vec.Add(A, B)
    local AX, AY, AZ = Components(A)
    local BX, BY, BZ = Components(B)
    return NewVector(AX + BX, AY + BY, AZ + BZ)
end

function Vec.Sub(A, B)
    local AX, AY, AZ = Components(A)
    local BX, BY, BZ = Components(B)
    return NewVector(AX - BX, AY - BY, AZ - BZ)
end

function Vec.Mul(V, Scalar)
    local X, Y, Z = Components(V)
    return NewVector(X * Scalar, Y * Scalar, Z * Scalar)
end

function Vec.LengthSquared(V)
    local X, Y, Z = Components(V)
    return X * X + Y * Y + Z * Z
end

function Vec.Length(V)
    return math.sqrt(Vec.LengthSquared(V))
end

function Vec.Normalized(V)
    local Length = Vec.Length(V)
    if Length <= 0.000001 then
        return Vec.Zero()
    end

    return Vec.Mul(V, 1.0 / Length)
end

function Vec.DirectionTo(From, To)
    return Vec.Normalized(Vec.Sub(To, From))
end

function Vec.DistanceSquared(A, B)
    return Vec.LengthSquared(Vec.Sub(B, A))
end

function Vec.Lerp(A, B, Alpha)
    local AX, AY, AZ = Components(A)
    local BX, BY, BZ = Components(B)
    local InverseAlpha = 1.0 - Alpha
    return NewVector(AX * InverseAlpha + BX * Alpha,
                     AY * InverseAlpha + BY * Alpha,
                     AZ * InverseAlpha + BZ * Alpha)
end

function Vec.Cross(A, B)
    local AX, AY, AZ = Components(A)
    local BX, BY, BZ = Components(B)
    return NewVector(AY * BZ - AZ * BY,
                     AZ * BX - AX * BZ,
                     AX * BY - AY * BX)
end

return Vec
