#pragma once

#include "Core/CoreTypes.h"
#include "Math/Vector.h"
#include "Render/RHI/D3D11/Buffers/VertexTypes.h"
#include <functional>

// Blender 스타일 Forward 축 선택
enum class EForwardAxis : uint8
{
    X,
    NegX, // +X, -X
    Y,
    NegY, // +Y, -Y
    Z,
    NegZ // +Z, -Z
};

enum class EUpAxis : uint8
{
    X,
    NegX,
    Y,
    NegY,
    Z,
    NegZ
};

// EWindingOrder는 메시 처리에서 사용할 선택지를 정의합니다.
enum class EWindingOrder : uint8
{
    CCW_to_CW, // OBJ CCW → DX CW (인덱스 [0,2,1]) — 기본값
    Keep       // 원본 유지 [0,1,2]
};

// FImportOptions는 메시 처리에 필요한 데이터를 묶는 구조체입니다.
struct FImportOptions
{
    float Scale = 1.0f;
    EForwardAxis ForwardAxis = EForwardAxis::NegY; // Blender 기본: Z-up, -Y Forward
    EUpAxis UpAxis = EUpAxis::Z;
    EWindingOrder WindingOrder = EWindingOrder::CCW_to_CW;
    static FImportOptions Default()
    {
        // NOTE: 
        // FBX SDK에서 축 보정하면서 CCW_to_CW 변환까지 자동으로 적용해주는 경우가 있어서 
        // Default Option은 WindingOrder 항목을 Keep으로 둡니다.
        
        FImportOptions Options;
        Options.WindingOrder = EWindingOrder::Keep;
        return Options;
    }
};

namespace MeshImporterUtils
{
    // Helper to combine hashes (Boost-style)
    template <class T>
    inline void hash_combine(std::size_t& seed, const T& v)
    {
        std::hash<T> hasher;
        seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    }

    struct FVectorHasher
    {
        size_t operator()(const FVector& v) const
        {
            size_t seed = 0;
            hash_combine(seed, v.X);
            hash_combine(seed, v.Y);
            hash_combine(seed, v.Z);
            return seed;
        }
    };

    struct FVector2Hasher
    {
        size_t operator()(const FVector2& v) const
        {
            size_t seed = 0;
            hash_combine(seed, v.X);
            hash_combine(seed, v.Y);
            return seed;
        }
    };

    struct FVector4Hasher
    {
        size_t operator()(const FVector4& v) const
        {
            size_t seed = 0;
            hash_combine(seed, v.X);
            hash_combine(seed, v.Y);
            hash_combine(seed, v.Z);
            hash_combine(seed, v.W);
            return seed;
        }
    };

    struct FStaticVertexKey
    {
        FVertexPNCT_T Vertex;
        FVector2 UV1;
        bool bHasUV1;

        bool operator==(const FStaticVertexKey& Other) const
        {
            auto IsVecEqual = [](const FVector& a, const FVector& b) { return a.X == b.X && a.Y == b.Y && a.Z == b.Z; };
            auto IsVec2Equal = [](const FVector2& a, const FVector2& b) { return a.X == b.X && a.Y == b.Y; };
            auto IsVec4Equal = [](const FVector4& a, const FVector4& b) { return a.X == b.X && a.Y == b.Y && a.Z == b.Z && a.W == b.W; };

            if (!IsVecEqual(Vertex.Position, Other.Vertex.Position)) return false;
            if (!IsVecEqual(Vertex.Normal, Other.Vertex.Normal)) return false;
            if (!IsVec2Equal(Vertex.UV, Other.Vertex.UV)) return false;
            if (!IsVec4Equal(Vertex.Tangent, Other.Vertex.Tangent)) return false;
            if (!IsVec4Equal(Vertex.Color, Other.Vertex.Color)) return false;
            
            if (bHasUV1 != Other.bHasUV1) return false;
            if (bHasUV1 && !IsVec2Equal(UV1, Other.UV1)) return false;

            return true;
        }
    };

    struct FStaticVertexHasher
    {
        size_t operator()(const FStaticVertexKey& Key) const
        {
            size_t seed = 0;
            FVectorHasher vecHasher;
            FVector2Hasher vec2Hasher;
            FVector4Hasher vec4Hasher;

            hash_combine(seed, vecHasher(Key.Vertex.Position));
            hash_combine(seed, vecHasher(Key.Vertex.Normal));
            hash_combine(seed, vec2Hasher(Key.Vertex.UV));
            hash_combine(seed, vec4Hasher(Key.Vertex.Tangent));
            hash_combine(seed, vec4Hasher(Key.Vertex.Color));
            if (Key.bHasUV1)
            {
                hash_combine(seed, vec2Hasher(Key.UV1));
            }
            return seed;
        }
    };

    struct FSkinnedVertexKey
    {
        FVertexSkinned Vertex;
        FVector2 UV1;
        bool bHasUV1;
        int CtrlPointIndex;

        bool operator==(const FSkinnedVertexKey& Other) const
        {
            auto IsVecEqual = [](const FVector& a, const FVector& b) { return a.X == b.X && a.Y == b.Y && a.Z == b.Z; };
            auto IsVec2Equal = [](const FVector2& a, const FVector2& b) { return a.X == b.X && a.Y == b.Y; };
            auto IsVec4Equal = [](const FVector4& a, const FVector4& b) { return a.X == b.X && a.Y == b.Y && a.Z == b.Z && a.W == b.W; };

            if (CtrlPointIndex != Other.CtrlPointIndex) return false;
            if (!IsVecEqual(Vertex.Position, Other.Vertex.Position)) return false;
            if (!IsVecEqual(Vertex.Normal, Other.Vertex.Normal)) return false;
            if (!IsVec2Equal(Vertex.UV, Other.Vertex.UV)) return false;
            if (!IsVec4Equal(Vertex.Tangent, Other.Vertex.Tangent)) return false;
            if (!IsVec4Equal(Vertex.Color, Other.Vertex.Color)) return false;
            
            if (bHasUV1 != Other.bHasUV1) return false;
            if (bHasUV1 && !IsVec2Equal(UV1, Other.UV1)) return false;

            return true;
        }
    };

    struct FSkinnedVertexHasher
    {
        size_t operator()(const FSkinnedVertexKey& Key) const
        {
            size_t seed = 0;
            FVectorHasher vecHasher;
            FVector2Hasher vec2Hasher;
            FVector4Hasher vec4Hasher;

            hash_combine(seed, Key.CtrlPointIndex);
            hash_combine(seed, vecHasher(Key.Vertex.Position));
            hash_combine(seed, vecHasher(Key.Vertex.Normal));
            hash_combine(seed, vec2Hasher(Key.Vertex.UV));
            hash_combine(seed, vec4Hasher(Key.Vertex.Tangent));
            hash_combine(seed, vec4Hasher(Key.Vertex.Color));
            if (Key.bHasUV1)
            {
                hash_combine(seed, vec2Hasher(Key.UV1));
            }
            return seed;
        }
    };
}

class FMeshImporterUtils
{
public:
    static FVector AxisToVector(EForwardAxis Axis)
    {
        switch (Axis)
        {
        case EForwardAxis::X:
            return FVector(1.0f, 0.0f, 0.0f);
        case EForwardAxis::NegX:
            return FVector(-1.0f, 0.0f, 0.0f);
        case EForwardAxis::Y:
            return FVector(0.0f, 1.0f, 0.0f);
        case EForwardAxis::NegY:
            return FVector(0.0f, -1.0f, 0.0f);
        case EForwardAxis::Z:
            return FVector(0.0f, 0.0f, 1.0f);
        case EForwardAxis::NegZ:
            return FVector(0.0f, 0.0f, -1.0f);
        default:
            return FVector(0.0f, -1.0f, 0.0f);
        }
    }

    static FVector AxisToVector(EUpAxis Axis)
    {
        switch (Axis)
        {
        case EUpAxis::X:
            return FVector(1.0f, 0.0f, 0.0f);
        case EUpAxis::NegX:
            return FVector(-1.0f, 0.0f, 0.0f);
        case EUpAxis::Y:
            return FVector(0.0f, 1.0f, 0.0f);
        case EUpAxis::NegY:
            return FVector(0.0f, -1.0f, 0.0f);
        case EUpAxis::Z:
            return FVector(0.0f, 0.0f, 1.0f);
        case EUpAxis::NegZ:
            return FVector(0.0f, 0.0f, -1.0f);
        default:
            return FVector(0.0f, 0.0f, 1.0f);
        }
    }

    static FVector RemapPosition(const FVector& InPos, EForwardAxis ForwardAxis, EUpAxis UpAxis)
    {
        const FVector Forward = AxisToVector(ForwardAxis);
        FVector Up = AxisToVector(UpAxis);

        const bool bAxesAreCollinear =
            (Forward.X == Up.X && Forward.Y == Up.Y && Forward.Z == Up.Z) ||
            (Forward.X == -Up.X && Forward.Y == -Up.Y && Forward.Z == -Up.Z);
        if (bAxesAreCollinear)
        {
            Up = (Forward.Z == 0.0f) ? FVector(0.0f, 0.0f, 1.0f) : FVector(0.0f, 1.0f, 0.0f);
        }

        const FVector Right = Up.Cross(Forward);
        return FVector(InPos.Dot(Forward), InPos.Dot(Right), InPos.Dot(Up));
    }
};
