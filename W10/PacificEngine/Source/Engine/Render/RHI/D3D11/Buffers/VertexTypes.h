#pragma once

#include "Core/CoreTypes.h"
#include "Math/Vector.h"
#include <cassert>
#include <cstring>
#include <string>

/*
    렌더러가 사용하는 기본 정점 형식과 CPU 메시 데이터 뷰를 정의합니다.
    기본 프리미티브, 폰트, 디버그 드로우, 정적 메시가 함께 사용합니다.
*/

#include "Math/Vector.h"
#include <cassert>

namespace SkeletalMeshLimits
{
inline constexpr int32 MaxBone = 8;
}

// FVertex는 렌더 처리에 필요한 데이터를 묶는 구조체입니다.
struct FVertex
{
    FVector  Position;
    FVector4 Color;
    int      SubID;
};

// FOverlayVertex는 렌더 처리에 필요한 데이터를 묶는 구조체입니다.
struct FOverlayVertex
{
    float X, Y;
    float _pad[2];
};

// Position + TexCoord 범용 버텍스 (FFontGeometry 등 텍스처 기반 동적 지오메트리 공용)
struct FTextureVertex
{
    FVector  Position;
    FVector2 TexCoord;
    float    _pad[3];
};

struct FUIVertex
{
    FVector  Position;
    FVector2 UV;
    FVector4 Color;
};

// Position + Normal + Color + UV (NormalMap이 없는 StaticMesh GPU용 정점 형식)
struct FVertexPNCT
{
    FVector  Position;
    FVector  Normal;
    FVector4 Color;
    FVector2 UV;
};

// Position + Normal + Color + UV + Tangent (NormalMap이 있는 StaticMesh GPU용 정점 형식)
struct FVertexPNCT_T
{
    FVector  Position;
    FVector  Normal;
    FVector4 Color;
    FVector2 UV;
    FVector4 Tangent;
};

struct FVertexPNCT_T_UV1
{
    FVector  Position;
    FVector  Normal;
    FVector4 Color;
    FVector2 UV;
    FVector4 Tangent;
    FVector2 UV1;
};

// Position + Normal + Color + UV + Tangent + BoneIndices + BoneWeights (Skeletal Mesh GPU용 정점 형식)
struct FVertexSkinned
{
    FVector  Position;
    FVector  Normal;
    FVector4 Color;
    FVector2 UV;
    FVector4 Tangent;
    uint16   BoneIndices[SkeletalMeshLimits::MaxBone] = {};
    float    BoneWeights[SkeletalMeshLimits::MaxBone] = {};
};

struct FVertexSemantic
{
    FString SemanticName;
    uint32  SemanticIndex = 0;
};

struct FVertexSemanticDescriptor
{
    TArray<FVertexSemantic> Elements;

    bool IsEmpty() const
    {
        return Elements.empty();
    }
};

enum class ERuntimeVertexSemanticSource : uint8
{
    Position = 0,
    Normal,
    Color,
    UV0,
    UV1,
    Tangent,
    BoneIndices,
    BoneWeights,
};

enum class ERuntimeVertexScalarType : uint8
{
    Unknown = 0,
    Float32,
    UInt32,
    SInt32,
    UInt16,
};

inline const char* GetRuntimeVertexSemanticSourceName(ERuntimeVertexSemanticSource Source)
{
    switch (Source)
    {
    case ERuntimeVertexSemanticSource::Position:
        return "Position";
    case ERuntimeVertexSemanticSource::Normal:
        return "Normal";
    case ERuntimeVertexSemanticSource::Color:
        return "Color";
    case ERuntimeVertexSemanticSource::UV0:
        return "UV0";
    case ERuntimeVertexSemanticSource::UV1:
        return "UV1";
    case ERuntimeVertexSemanticSource::Tangent:
        return "Tangent";
    case ERuntimeVertexSemanticSource::BoneIndices:
        return "BoneIndices";
    case ERuntimeVertexSemanticSource::BoneWeights:
        return "BoneWeights";
    default:
        return "Unknown";
    }
}

struct FRuntimeVertexSemanticSourceDesc
{
    ERuntimeVertexSemanticSource Source = ERuntimeVertexSemanticSource::Position;
    FString                     SemanticName;
    uint32                      SemanticIndex = 0;
    uint32                      ComponentCount = 0;
    ERuntimeVertexScalarType    ScalarType = ERuntimeVertexScalarType::Float32;
};

struct FRuntimeVertexSemanticSourceModel
{
    TArray<FRuntimeVertexSemanticSourceDesc> Sources;

    void AddSource(
        ERuntimeVertexSemanticSource Source,
        const char* SemanticName,
        uint32 SemanticIndex,
        uint32 ComponentCount,
        ERuntimeVertexScalarType ScalarType = ERuntimeVertexScalarType::Float32)
    {
        FRuntimeVertexSemanticSourceDesc Entry;
        Entry.Source = Source;
        Entry.SemanticName = SemanticName;
        Entry.SemanticIndex = SemanticIndex;
        Entry.ComponentCount = ComponentCount;
        Entry.ScalarType = ScalarType;
        Sources.push_back(Entry);
    }

    bool HasSource(ERuntimeVertexSemanticSource Source) const
    {
        return FindSource(Source) != nullptr;
    }

    const FRuntimeVertexSemanticSourceDesc* FindSource(ERuntimeVertexSemanticSource Source) const
    {
        for (const FRuntimeVertexSemanticSourceDesc& Entry : Sources)
        {
            if (Entry.Source == Source)
            {
                return &Entry;
            }
        }

        return nullptr;
    }

    FString ToDebugString() const
    {
        FString Result;
        for (const FRuntimeVertexSemanticSourceDesc& Entry : Sources)
        {
            if (!Result.empty())
            {
                Result += ",";
            }

            Result += GetRuntimeVertexSemanticSourceName(Entry.Source);
            Result += "->";
            Result += Entry.SemanticName;
            if (Entry.SemanticIndex > 0)
            {
                Result += std::to_string(Entry.SemanticIndex);
            }
        }

        return Result.empty() ? "<none>" : Result;
    }
};

struct FRuntimeVertexElementRequest
{
    FString                  SemanticName;
    uint32                   SemanticIndex = 0;
    uint32                   ComponentCount = 0;
    ERuntimeVertexScalarType ScalarType = ERuntimeVertexScalarType::Unknown;
};

struct FRuntimeVertexElementRequestList
{
    TArray<FRuntimeVertexElementRequest> Elements;

    bool IsEmpty() const
    {
        return Elements.empty();
    }
};

struct FRuntimePackedVertexData
{
    TArray<uint8>              VertexBlob;
    uint32                     VertexStride = 0;
    uint32                     VertexCount = 0;
    FVertexSemanticDescriptor  VertexSemanticDescriptor;

    bool IsValid() const
    {
        return VertexStride > 0 &&
               VertexCount > 0 &&
               VertexBlob.size() == static_cast<size_t>(VertexStride) * VertexCount;
    }

    uint32 GetVertexBufferByteWidth() const
    {
        return VertexStride * VertexCount;
    }

    const void* GetVertexData() const
    {
        return VertexBlob.empty() ? nullptr : VertexBlob.data();
    }
};

struct FRuntimePackedMeshData
{
    FRuntimePackedVertexData Vertices;
    TArray<uint32>           Indices;

    bool HasIndices() const
    {
        return !Indices.empty();
    }
};

template <typename VertexType>
struct TMeshData;

template <typename VertexType>
// TMeshData는 메시 데이터와 렌더 제출 정보를 다룹니다.
struct TMeshData
{
    TArray<VertexType> Vertices;
    TArray<uint32>     Indices;
};

using FMeshData = TMeshData<FVertex>;

struct FVertexSemanticDescriptorPreset
{
    inline static const FVertexSemanticDescriptor PC = {
        {
            { "POSITION", 0 },
            { "COLOR", 0 },
        }
    };

    inline static const FVertexSemanticDescriptor PT = {
        {
            { "POSITION", 0 },
            { "TEXCOORD", 0 },
        }
    };

    inline static const FVertexSemanticDescriptor PUVC = {
        {
            { "POSITION", 0 },
            { "TEXCOORD", 0 },
            { "COLOR", 0 },
        }
    };

    inline static const FVertexSemanticDescriptor PNCT = {
        {
            { "POSITION", 0 },
            { "NORMAL", 0 },
            { "COLOR", 0 },
            { "TEXCOORD", 0 },
        }
    };

    inline static const FVertexSemanticDescriptor PNCTT = {
        {
            { "POSITION", 0 },
            { "NORMAL", 0 },
            { "COLOR", 0 },
            { "TEXCOORD", 0 },
            { "TANGENT", 0 },
        }
    };

    inline static const FVertexSemanticDescriptor PNCTTUV1 = {
        {
            { "POSITION", 0 },
            { "NORMAL", 0 },
            { "COLOR", 0 },
            { "TEXCOORD", 0 },
            { "TANGENT", 0 },
            { "TEXCOORD", 1 },
        }
    };

    inline static const FVertexSemanticDescriptor Skinned = {
        {
            { "POSITION", 0 },
            { "NORMAL", 0 },
            { "COLOR", 0 },
            { "TEXCOORD", 0 },
            { "TANGENT", 0 },
            { "BLENDINDICES", 0 },
            { "BLENDWEIGHT", 0 },
        }
    };
};

template <typename VertexType>
FRuntimePackedVertexData BuildRuntimePackedVertexData(
    const TArray<VertexType>& InVertices,
    const FVertexSemanticDescriptor& InVertexSemanticDescriptor)
{
    FRuntimePackedVertexData PackedData;
    PackedData.VertexStride = sizeof(VertexType);
    PackedData.VertexCount = static_cast<uint32>(InVertices.size());
    PackedData.VertexSemanticDescriptor = InVertexSemanticDescriptor;
    if (!InVertices.empty())
    {
        const uint32 ByteWidth = PackedData.GetVertexBufferByteWidth();
        PackedData.VertexBlob.resize(ByteWidth);
        std::memcpy(PackedData.VertexBlob.data(), InVertices.data(), ByteWidth);
    }

    return PackedData;
}

template <typename VertexType>
FRuntimePackedMeshData BuildRuntimePackedMeshData(
    const TMeshData<VertexType>& InMeshData,
    const FVertexSemanticDescriptor& InVertexSemanticDescriptor)
{
    FRuntimePackedMeshData PackedMeshData;
    PackedMeshData.Vertices = BuildRuntimePackedVertexData(InMeshData.Vertices, InVertexSemanticDescriptor);
    PackedMeshData.Indices = InMeshData.Indices;
    return PackedMeshData;
}

// 정점 타입에 무관하게 메시 데이터를 참조하는 뷰.
struct FMeshDataView
{
    const void*   VertexData  = nullptr;
    const uint32* IndexData   = nullptr;
    uint32        VertexCount = 0;
    uint32        IndexCount  = 0;
    uint32        Stride      = 0;

    bool   IsValid() const { return VertexData && IndexCount > 0; }
    uint32 GetTriangleCount() const { return IndexCount / 3; }

    // N번째 정점을 T 타입으로 반환
    template <typename T>
    const T& GetVertex(uint32 Index) const
    {
        assert(sizeof(T) == Stride && "GetVertex<T>: sizeof(T) must match Stride");
        return *reinterpret_cast<const T*>(
            static_cast<const uint8*>(VertexData) + Index * Stride);
    }

    // Position은 모든 정점 타입의 offset 0에 있으므로 타입 없이 접근 가능
    const FVector& GetPosition(uint32 Index) const
    {
        return *reinterpret_cast<const FVector*>(
            static_cast<const uint8*>(VertexData) + Index * Stride);
    }

    // N번째 삼각형의 세 정점 인덱스를 반환
    void GetTriangleIndices(uint32 TriIndex, uint32& OutI0, uint32& OutI1, uint32& OutI2) const
    {
        assert(TriIndex * 3 + 2 < IndexCount && "GetTriangleIndices: TriIndex out of range");
        OutI0 = IndexData[TriIndex * 3];
        OutI1 = IndexData[TriIndex * 3 + 1];
        OutI2 = IndexData[TriIndex * 3 + 2];
    }

    template <typename VertexType>
    static FMeshDataView FromMeshData(const TMeshData<VertexType>& Data)
    {
        FMeshDataView View;
        if (!Data.Vertices.empty())
        {
            View.VertexData  = Data.Vertices.data();
            View.VertexCount = (uint32)Data.Vertices.size();
            View.Stride      = sizeof(VertexType);
        }
        if (!Data.Indices.empty())
        {
            View.IndexData  = Data.Indices.data();
            View.IndexCount = (uint32)Data.Indices.size();
        }
        return View;
    }
};
