#pragma once

#include "Object/Object.h"
#include "Mesh/StaticMeshAsset.h"
#include "Render/RHI/D3D11/Buffers/VertexTypes.h"
#include "Mesh/Skeleton.h"
#include "Math/Matrix.h"

namespace SkeletalMeshPrefix
{
    inline const FString Mesh = "Mesh_";
    inline const FString Skeleton = "Skeleton_";
}

inline FArchive& operator<<(FArchive& Ar, FMatrix& Matrix)
{
    for (float& Value : Matrix.Data)
    {
        Ar << Value;
    }
    return Ar;
}

struct FSkeletalMeshSection
{
    int32 MaterialIndex = -1;
    FString MaterialSlotName;
    uint32 FirstIndex = 0;
    uint32 NumTriangles = 0;

    friend FArchive& operator<<(FArchive& Ar, FSkeletalMeshSection& Section)
    {
        Ar << Section.MaterialSlotName << Section.FirstIndex << Section.NumTriangles;
        return Ar;
    }
};

struct FSkeletalSubMesh
{
    FString PathFileName;
    TArray<FVertexSkinned> Vertices;
    TArray<FVector2> UV1s;
    TArray<uint32> Indices;
    TArray<FSkeletalMeshSection> Sections;
    FVector MeshBindInverseScale = FVector(1.0f);
    TArray<FMatrix> InverseBindPoseMatrices;
    TArray<FMatrix> BoneBindGlobalMatrices;

    std::unique_ptr<FSkeletalMeshBuffer> RenderBuffer;

    bool HasValidUV1Data() const
    {
        return !UV1s.empty() && UV1s.size() == Vertices.size();
    }

    FRuntimeVertexSemanticSourceModel BuildRuntimeSemanticSourceModel() const
    {
        FRuntimeVertexSemanticSourceModel SourceModel;
        SourceModel.Sources.reserve(HasValidUV1Data() ? 8 : 7);
        SourceModel.AddSource(ERuntimeVertexSemanticSource::Position, "POSITION", 0, 3);
        SourceModel.AddSource(ERuntimeVertexSemanticSource::Normal, "NORMAL", 0, 3);
        SourceModel.AddSource(ERuntimeVertexSemanticSource::Color, "COLOR", 0, 4);
        SourceModel.AddSource(ERuntimeVertexSemanticSource::UV0, "TEXCOORD", 0, 2);
        SourceModel.AddSource(ERuntimeVertexSemanticSource::Tangent, "TANGENT", 0, 4);
        if (HasValidUV1Data())
        {
            SourceModel.AddSource(ERuntimeVertexSemanticSource::UV1, "TEXCOORD", 1, 2);
        }
        SourceModel.AddSource(
            ERuntimeVertexSemanticSource::BoneIndices,
            "BLENDINDICES",
            0,
            SkeletalMeshLimits::MaxBone,
            ERuntimeVertexScalarType::UInt16);
        SourceModel.AddSource(
            ERuntimeVertexSemanticSource::BoneWeights,
            "BLENDWEIGHT",
            0,
            SkeletalMeshLimits::MaxBone,
            ERuntimeVertexScalarType::Float32);
        return SourceModel;
    }

    FRuntimeVertexElementRequestList BuildRuntimeUploadRequestList() const
    {
        FRuntimeVertexElementRequestList RequestList;
        RequestList.Elements.reserve(HasValidUV1Data() ? 6 : 5);

        auto AddRequest = [&RequestList](
                              const char* SemanticName,
                              uint32 SemanticIndex,
                              uint32 ComponentCount,
                              ERuntimeVertexScalarType ScalarType = ERuntimeVertexScalarType::Float32)
        {
            FRuntimeVertexElementRequest Request;
            Request.SemanticName = SemanticName;
            Request.SemanticIndex = SemanticIndex;
            Request.ComponentCount = ComponentCount;
            Request.ScalarType = ScalarType;
            RequestList.Elements.push_back(Request);
        };

        AddRequest("POSITION", 0, 3);
        AddRequest("NORMAL", 0, 3);
        AddRequest("COLOR", 0, 4);
        AddRequest("TEXCOORD", 0, 2);
        AddRequest("TANGENT", 0, 4);
        if (HasValidUV1Data())
        {
            AddRequest("TEXCOORD", 1, 2);
        }

        return RequestList;
    }

    void Serialize(FArchive& Ar)
    {
        Ar << PathFileName;
        Ar << Vertices;
        Ar << UV1s;
        Ar << Indices;
        Ar << Sections;
        Ar << MeshBindInverseScale;
        Ar << InverseBindPoseMatrices;
        Ar << BoneBindGlobalMatrices;

        if (Ar.IsLoading() && UV1s.size() != Vertices.size())
        {
            UV1s.clear();
        }
    }
};

class USkeletalSubMesh : public UObject
{
public:
    DECLARE_CLASS(USkeletalSubMesh, UObject)

    USkeletalSubMesh();
    ~USkeletalSubMesh() override;

    void Serialize(FArchive& Ar) override;

    const FString& GetAssetPathFileName() const;
    void SetSkeletalSubMeshAsset(FSkeletalSubMesh* InMesh);
    FSkeletalSubMesh* GetSkeletalSubMeshAsset() const { return SkeletalSubMeshAsset; }

    void SetSkeleton(USkeleton* InSkeleton) { Skeleton = InSkeleton; }
    USkeleton* GetSkeleton() const { return Skeleton; }

    void SetStaticMaterials(TArray<FStaticMaterial>&& InMaterials);
    const TArray<FStaticMaterial>& GetStaticMaterials() const { return StaticMaterials; }

    void InitResources(ID3D11Device* InDevice);

private:
    FSkeletalSubMesh* SkeletalSubMeshAsset = nullptr;
    
    // 메시에 연결된 스켈레톤 포인터.
    // 직렬화되지 않으며, 로드 시에 부모(USkeletalMesh)에게 주입받는 런타임 전용 필드.
    USkeleton* Skeleton = nullptr;
    
    TArray<FStaticMaterial> StaticMaterials;
};

// 새로운 컨테이너 클래스
class USkeletalMesh : public UObject
{
public:
    DECLARE_CLASS(USkeletalMesh, UObject)

    USkeletalMesh() = default;
    ~USkeletalMesh() override = default;

    void SetSkeleton(USkeleton* InSkeleton) { Skeleton = InSkeleton; }
    USkeleton* GetSkeleton() const { return Skeleton; }

    void AddSubMesh(USkeletalSubMesh* InSubMesh) { SubMeshes.push_back(InSubMesh); }
    const TArray<USkeletalSubMesh*>& GetSubMeshes() const { return SubMeshes; }

    void SetAssetPathFileName(const FString& InPath) { PathFileName = InPath; }
    const FString& GetAssetPathFileName() const { return PathFileName; }

    void Serialize(FArchive& Ar) override;

private:
    USkeleton* Skeleton = nullptr;
    TArray<USkeletalSubMesh*> SubMeshes;
    FString PathFileName;
};
