// 메시 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "Core/CoreTypes.h"
#include "Math/Vector.h"
#include "Engine/Object/Object.h"
#include "Render/RHI/D3D11/Buffers/Buffers.h"
#include "Serialization/Archive.h"
#include "Engine/Object/FName.h"
#include "Materials/Material.h"
#include "Materials/MaterialManager.h"
#include "Render/RHI/D3D11/Buffers/VertexTypes.h"
#include <memory>
#include <algorithm>

// FStaticMeshSection는 메시 데이터와 렌더 제출 정보를 다룹니다.
struct FStaticMeshSection
{
    int32 MaterialIndex = -1; // Index into UStaticMesh's FStaticMaterial array. Cached to avoid per-frame string comparison.
    FString MaterialSlotName;
    uint32 FirstIndex = 0;
    uint32 NumTriangles = 0;

    friend FArchive& operator<<(FArchive& Ar, FStaticMeshSection& Section)
    {
        Ar << Section.MaterialSlotName << Section.FirstIndex << Section.NumTriangles;
        return Ar;
    }
};

// FStaticMaterial는 머티리얼 파라미터와 렌더 리소스를 다룹니다.
struct FStaticMaterial
{
    // std::shared_ptr<class UMaterialInterface> MaterialInterface;
    UMaterial* MaterialInterface = nullptr;
    FString MaterialSlotName = "None"; // "None"은 특별한 슬롯 이름으로, OBJ 파일에서 머티리얼이 지정되지 않은 섹션에 할당됩니다.
    FString MaterialAssetPath;

    void SetMaterialInterface(UMaterial* InMaterial)
    {
        MaterialInterface = InMaterial;
        MaterialAssetPath = InMaterial ? InMaterial->GetAssetPathFileName() : FString();
    }

    friend FArchive& operator<<(FArchive& Ar, FStaticMaterial& Mat)
    {
        // 1. 슬롯 이름 직렬화 (메시 섹션과 매핑용)
        Ar << Mat.MaterialSlotName;

        // 2. Material JSON 경로 직렬화 (Source of Truth = Asset/**/Materials/*.json)
        if (Ar.IsSaving() && Mat.MaterialAssetPath.empty() && Mat.MaterialInterface)
        {
            Mat.MaterialAssetPath = Mat.MaterialInterface->GetAssetPathFileName();
        }
        Ar << Mat.MaterialAssetPath;

        // 3. 로딩 시 FMaterialManager를 통해 머티리얼 복원
        if (Ar.IsLoading())
        {
            if (!Mat.MaterialAssetPath.empty())
            {
                Mat.MaterialInterface = FMaterialManager::Get().GetOrCreateStaticMeshMaterial(Mat.MaterialAssetPath);
            }
            else
            {
                Mat.MaterialInterface = nullptr;
            }
        }

        return Ar;
    }
};

// Cooked Data — GPU용 정점/인덱스
// FStaticMeshLODResources in UE5
struct FStaticMesh
{
    struct FRuntimeRenderBufferEntry
    {
        FRuntimeVertexElementRequestList RequestedElements;
        std::unique_ptr<FStaticMeshBuffer> RenderBuffer;
    };

    FString PathFileName;
    TArray<FVertexPNCT_T> Vertices;
    TArray<FVector2> UV1s;
    TArray<uint32> Indices;

    TArray<FStaticMeshSection> Sections;

    std::unique_ptr<FStaticMeshBuffer> RenderBuffer;
    TArray<FRuntimeRenderBufferEntry> RuntimeRenderBufferCache;

    // 메시 로컬 바운드 캐시 (정점 순회 1회로 계산)
    FVector BoundsCenter = FVector(0, 0, 0);
    FVector BoundsExtent = FVector(0, 0, 0);
    bool bBoundsValid = false;

    bool HasValidUV1Data() const
    {
        return !UV1s.empty() && UV1s.size() == Vertices.size();
    }

    FRuntimeVertexSemanticSourceModel BuildRuntimeSemanticSourceModel() const
    {
        FRuntimeVertexSemanticSourceModel SourceModel;
        SourceModel.Sources.reserve(HasValidUV1Data() ? 6 : 5);
        SourceModel.AddSource(ERuntimeVertexSemanticSource::Position, "POSITION", 0, 3);
        SourceModel.AddSource(ERuntimeVertexSemanticSource::Normal, "NORMAL", 0, 3);
        SourceModel.AddSource(ERuntimeVertexSemanticSource::Color, "COLOR", 0, 4);
        SourceModel.AddSource(ERuntimeVertexSemanticSource::UV0, "TEXCOORD", 0, 2);
        SourceModel.AddSource(ERuntimeVertexSemanticSource::Tangent, "TANGENT", 0, 4);
        if (HasValidUV1Data())
        {
            SourceModel.AddSource(ERuntimeVertexSemanticSource::UV1, "TEXCOORD", 1, 2);
        }

        return SourceModel;
    }

    FVertexSemanticDescriptor BuildRuntimeUploadSemanticDescriptor() const
    {
        FVertexSemanticDescriptor Descriptor;
        const FRuntimeVertexSemanticSourceModel SourceModel = BuildRuntimeSemanticSourceModel();
        Descriptor.Elements.reserve(SourceModel.Sources.size());
        for (const FRuntimeVertexSemanticSourceDesc& SourceDesc : SourceModel.Sources)
        {
            Descriptor.Elements.push_back({ SourceDesc.SemanticName, SourceDesc.SemanticIndex });
        }

        return Descriptor;
    }

    FRuntimeVertexElementRequestList BuildRuntimeUploadRequestList() const
    {
        FRuntimeVertexElementRequestList RequestList;
        const FRuntimeVertexSemanticSourceModel SourceModel = BuildRuntimeSemanticSourceModel();
        RequestList.Elements.reserve(SourceModel.Sources.size());
        for (const FRuntimeVertexSemanticSourceDesc& SourceDesc : SourceModel.Sources)
        {
            FRuntimeVertexElementRequest Request;
            Request.SemanticName = SourceDesc.SemanticName;
            Request.SemanticIndex = SourceDesc.SemanticIndex;
            Request.ComponentCount = SourceDesc.ComponentCount;
            Request.ScalarType = SourceDesc.ScalarType;
            RequestList.Elements.push_back(Request);
        }

        return RequestList;
    }

    void CacheBounds()
    {
        bBoundsValid = false;
        if (Vertices.empty())
            return;

        FVector LocalMin = Vertices[0].Position;
        FVector LocalMax = Vertices[0].Position;
        for (const FVertexPNCT_T& V : Vertices)
        {
            LocalMin.X = (std::min)(LocalMin.X, V.Position.X);
            LocalMin.Y = (std::min)(LocalMin.Y, V.Position.Y);
            LocalMin.Z = (std::min)(LocalMin.Z, V.Position.Z);
            LocalMax.X = (std::max)(LocalMax.X, V.Position.X);
            LocalMax.Y = (std::max)(LocalMax.Y, V.Position.Y);
            LocalMax.Z = (std::max)(LocalMax.Z, V.Position.Z);
        }

        BoundsCenter = (LocalMin + LocalMax) * 0.5f;
        BoundsExtent = (LocalMax - LocalMin) * 0.5f;
        bBoundsValid = true;
    }

    void Serialize(FArchive& Ar)
    {
        Ar << PathFileName;
        Ar << Vertices;
        Ar << UV1s;
        Ar << Indices;
        Ar << Sections;

        if (Ar.IsLoading() && UV1s.size() != Vertices.size())
        {
            UV1s.clear();
        }
    }
};
