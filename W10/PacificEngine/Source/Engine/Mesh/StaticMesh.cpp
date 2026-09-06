// 메시 영역의 세부 동작을 구현합니다.
#include "Mesh/StaticMesh.h"
#include "Object/ObjectFactory.h"
#include "Mesh/ObjManager.h"
#include "Serialization/WindowsArchive.h"
#include "Mesh/ObjImporter.h"
#include "Texture/Texture2D.h"
#include "Engine/Profiling/MemoryStats.h"
#include "Mesh/MeshSimplifier.h"
#include "Render/RHI/D3D11/Buffers/RuntimeVertexPacker.h"
#include "Render/RHI/D3D11/Buffers/RuntimeVertexWideningPolicy.h"
#include "Render/RHI/D3D11/Shaders/GraphicsProgram.h"

#include <string.h>

IMPLEMENT_CLASS(UStaticMesh, UObject)

static const FString EmptyPath;

namespace
{
bool AreRuntimeVertexRequestListsEqual(
    const FRuntimeVertexElementRequestList& A,
    const FRuntimeVertexElementRequestList& B)
{
    if (A.Elements.size() != B.Elements.size())
    {
        return false;
    }

    for (size_t Index = 0; Index < A.Elements.size(); ++Index)
    {
        const FRuntimeVertexElementRequest& Left = A.Elements[Index];
        const FRuntimeVertexElementRequest& Right = B.Elements[Index];
        if (_stricmp(Left.SemanticName.c_str(), Right.SemanticName.c_str()) != 0 ||
            Left.SemanticIndex != Right.SemanticIndex ||
            Left.ComponentCount != Right.ComponentCount ||
            Left.ScalarType != Right.ScalarType)
        {
            return false;
        }
    }

    return true;
}

bool BuildRuntimeVertexRequestListFromShader(
    const FGraphicsProgram* InShader,
    FRuntimeVertexElementRequestList& OutRequestList)
{
    OutRequestList.Elements.clear();
    if (!InShader)
    {
        return false;
    }

    const TArray<FGraphicsProgram::FVertexInputElement>& VertexInputs = InShader->GetVertexInputs();
    OutRequestList.Elements.reserve(VertexInputs.size());
    for (const FGraphicsProgram::FVertexInputElement& VertexInput : VertexInputs)
    {
        FRuntimeVertexElementRequest Request;
        if (!BuildRuntimeVertexElementRequest(
                VertexInput.SemanticName,
                VertexInput.SemanticIndex,
                VertexInput.Format,
                Request))
        {
            return false;
        }

        OutRequestList.Elements.push_back(Request);
    }

    return !OutRequestList.IsEmpty();
}

void BuildLegacyStaticMeshRenderData(const FStaticMesh& InMesh, FRuntimePackedMeshData& OutPackedMeshData)
{
    if (InMesh.HasValidUV1Data())
    {
        TMeshData<FVertexPNCT_T_UV1> RenderMeshData;
        RenderMeshData.Vertices.reserve(InMesh.Vertices.size());

        for (size_t VertexIndex = 0; VertexIndex < InMesh.Vertices.size(); ++VertexIndex)
        {
            const FVertexPNCT_T& RawVert = InMesh.Vertices[VertexIndex];
            FVertexPNCT_T_UV1 RenderVert;
            RenderVert.Position = RawVert.Position;
            RenderVert.Normal = RawVert.Normal;
            RenderVert.Color = RawVert.Color;
            RenderVert.UV = RawVert.UV;
            RenderVert.Tangent = RawVert.Tangent;
            RenderVert.UV1 = InMesh.UV1s[VertexIndex];
            RenderMeshData.Vertices.push_back(RenderVert);
        }
        RenderMeshData.Indices = InMesh.Indices;
        OutPackedMeshData = BuildRuntimePackedMeshData(RenderMeshData, FVertexSemanticDescriptorPreset::PNCTTUV1);
        return;
    }

    TMeshData<FVertexPNCT_T> RenderMeshData;
    RenderMeshData.Vertices.reserve(InMesh.Vertices.size());
    for (const FVertexPNCT_T& RawVert : InMesh.Vertices)
    {
        FVertexPNCT_T RenderVert;
        RenderVert.Position = RawVert.Position;
        RenderVert.Normal = RawVert.Normal;
        RenderVert.Color = RawVert.Color;
        RenderVert.UV = RawVert.UV;
        RenderVert.Tangent = RawVert.Tangent;
        RenderMeshData.Vertices.push_back(RenderVert);
    }
    RenderMeshData.Indices = InMesh.Indices;
    OutPackedMeshData = BuildRuntimePackedMeshData(RenderMeshData, FVertexSemanticDescriptorPreset::PNCTT);
}
} // namespace

UStaticMesh::~UStaticMesh()
{
    if (StaticMeshAsset)
    {
        const uint32 CPUSize =
            static_cast<uint32>(StaticMeshAsset->Vertices.size() * sizeof(FVertexPNCT_T)) +
            static_cast<uint32>(StaticMeshAsset->UV1s.size() * sizeof(FVector2)) +
            static_cast<uint32>(StaticMeshAsset->Indices.size() * sizeof(uint32));

        MemoryStats::SubStaticMeshCPUMemory(CPUSize);
    }
}

void UStaticMesh::Serialize(FArchive& Ar)
{
    // 에셋이 비어있으면 로드용으로 생성
    if (Ar.IsLoading() && !StaticMeshAsset)
    {
        StaticMeshAsset = new FStaticMesh();
    }

    // 1. 지오메트리 데이터 직렬화
    StaticMeshAsset->Serialize(Ar);

    // 2. 머티리얼 데이터 직렬화 (필수!)
    Ar << StaticMaterials;

    // 3. 로딩 시 Section → MaterialIndex 매핑 캐싱 (매 프레임 문자열 비교 방지)
    if (Ar.IsLoading())
    {
        for (FStaticMeshSection& Section : StaticMeshAsset->Sections)
        {
            Section.MaterialIndex = -1;
            for (int32 i = 0; i < (int32)StaticMaterials.size(); ++i)
            {
                if (StaticMaterials[i].MaterialSlotName == Section.MaterialSlotName)
                {
                    Section.MaterialIndex = i;
                    break;
                }
            }
        }
    }
}

void UStaticMesh::InitResources(ID3D11Device* InDevice)
{
    if (!InDevice || !StaticMeshAsset)
        return;

    // CPU 메모리 추적
    const uint32 CPUSize =
        static_cast<uint32>(StaticMeshAsset->Vertices.size() * sizeof(FVertexPNCT_T)) +
        static_cast<uint32>(StaticMeshAsset->UV1s.size() * sizeof(FVector2)) +
        static_cast<uint32>(StaticMeshAsset->Indices.size() * sizeof(uint32));
    MemoryStats::AddStaticMeshCPUMemory(CPUSize);

    StaticMeshAsset->RenderBuffer = std::make_unique<FStaticMeshBuffer>();
    StaticMeshAsset->RuntimeRenderBufferCache.clear();

    FRuntimePackedMeshData PackedMeshData;
    if (TryPackStaticMeshVertices(
            *StaticMeshAsset,
            StaticMeshAsset->BuildRuntimeUploadRequestList(),
            PackedMeshData))
    {
        StaticMeshAsset->RenderBuffer->Create(InDevice, PackedMeshData);
    }
    else
    {
        BuildLegacyStaticMeshRenderData(*StaticMeshAsset, PackedMeshData);
        StaticMeshAsset->RenderBuffer->Create(InDevice, PackedMeshData);
    }

    // ── LOD 생성 (LOD1: 90%, LOD2: 55%, LOD3: 15%) ──
    /*if (StaticMeshAsset->Vertices.size() >= 100)
    {
        static const float LODRatios[] = { 0.9f, 0.55f, 0.15f };
        for (int lod = 0; lod < 3; ++lod)
        {
            FSimplifiedMesh Simplified = FMeshSimplifier::Simplify(
                StaticMeshAsset->Vertices, StaticMeshAsset->Indices,
                StaticMeshAsset->Sections, LODRatios[lod]);

            AdditionalLODs[lod].Sections = std::move(Simplified.Sections);

            TMeshData<FVertexPNCT_T> LODRenderData;
            LODRenderData.Vertices.reserve(Simplified.Vertices.size());
            for (const FVertexPNCT_T& RawVert : Simplified.Vertices)
            {
                FVertexPNCT_T RenderVert;
                RenderVert.Position = RawVert.Position;
                RenderVert.Normal = RawVert.Normal;
                RenderVert.Color = RawVert.Color;
                RenderVert.UV = RawVert.UV;
                LODRenderData.Vertices.push_back(RenderVert);
            }
            LODRenderData.Indices = std::move(Simplified.Indices);

            AdditionalLODs[lod].RenderBuffer = std::make_unique<FMeshBuffer>();
            AdditionalLODs[lod].RenderBuffer->Create(InDevice, LODRenderData);
        }
        bHasLOD = true;
    }*/
}

FStaticMeshBuffer* UStaticMesh::GetOrCreateRenderBufferForShader(
    ID3D11Device* InDevice,
    const FGraphicsProgram* InShader)
{
    if (!InDevice || !StaticMeshAsset)
    {
        return nullptr;
    }

    FRuntimeVertexElementRequestList RequestedElements;
    if (!BuildRuntimeVertexRequestListFromShader(InShader, RequestedElements))
    {
        return StaticMeshAsset->RenderBuffer.get();
    }

    if (StaticMeshAsset->RenderBuffer &&
        AreRuntimeVertexRequestListsEqual(
            StaticMeshAsset->BuildRuntimeUploadRequestList(),
            RequestedElements))
    {
        return StaticMeshAsset->RenderBuffer.get();
    }

    for (FStaticMesh::FRuntimeRenderBufferEntry& Entry : StaticMeshAsset->RuntimeRenderBufferCache)
    {
        if (Entry.RenderBuffer &&
            AreRuntimeVertexRequestListsEqual(Entry.RequestedElements, RequestedElements))
        {
            return Entry.RenderBuffer.get();
        }
    }

    FRuntimePackedMeshData PackedMeshData;
    if (!TryPackStaticMeshVertices(*StaticMeshAsset, RequestedElements, PackedMeshData))
    {
        return StaticMeshAsset->RenderBuffer.get();
    }

    FStaticMesh::FRuntimeRenderBufferEntry Entry;
    Entry.RequestedElements = RequestedElements;
    Entry.RenderBuffer = std::make_unique<FStaticMeshBuffer>();
    Entry.RenderBuffer->Create(InDevice, PackedMeshData);
    if (!Entry.RenderBuffer->IsValid())
    {
        return StaticMeshAsset->RenderBuffer.get();
    }

    StaticMeshAsset->RuntimeRenderBufferCache.push_back(std::move(Entry));
    return StaticMeshAsset->RuntimeRenderBufferCache.back().RenderBuffer.get();
}

const FString& UStaticMesh::GetAssetPathFileName() const
{
    if (StaticMeshAsset)
    {
        return StaticMeshAsset->PathFileName;
    }
    return EmptyPath;
}

void UStaticMesh::SetStaticMeshAsset(FStaticMesh* InMesh)
{
    StaticMeshAsset = InMesh;
    // 현재는 static mesh asset이 로드 후 고정된다고 보고, 메시 변경 dirty 갱신은 비활성화합니다.
    // MarkMeshTrianglePickingBVHDirty();

    // Section → MaterialIndex 캐싱 갱신
    if (StaticMeshAsset)
    {
        for (FStaticMeshSection& Section : StaticMeshAsset->Sections)
        {
            Section.MaterialIndex = -1;
            for (int32 i = 0; i < (int32)StaticMaterials.size(); ++i)
            {
                if (StaticMaterials[i].MaterialSlotName == Section.MaterialSlotName)
                {
                    Section.MaterialIndex = i;
                    break;
                }
            }
        }
        EnsureMeshTrianglePickingBVHBuilt();
    }
}

FStaticMesh* UStaticMesh::GetStaticMeshAsset() const
{
    return StaticMeshAsset;
}

void UStaticMesh::SetStaticMaterials(TArray<FStaticMaterial>&& InMaterials)
{
    StaticMaterials = InMaterials;
    for (FStaticMaterial& Material : StaticMaterials)
    {
        if (Material.MaterialInterface && Material.MaterialAssetPath.empty())
        {
            Material.MaterialAssetPath = Material.MaterialInterface->GetAssetPathFileName();
        }
        else if (!Material.MaterialInterface && !Material.MaterialAssetPath.empty())
        {
            Material.MaterialInterface = FMaterialManager::Get().GetOrCreateStaticMeshMaterial(Material.MaterialAssetPath);
        }
    }
}

const TArray<FStaticMaterial>& UStaticMesh::GetStaticMaterials() const
{
    return StaticMaterials;
}

void UStaticMesh::EnsureMeshTrianglePickingBVHBuilt() const
{
    if (!StaticMeshAsset)
    {
        return;
    }

    MeshTrianglePickingBVH.EnsureBuilt(*StaticMeshAsset);
}

bool UStaticMesh::RaycastMeshTrianglesWithBVHLocal(const FVector& LocalOrigin, const FVector& LocalDirection, FHitResult& OutHitResult) const
{
    if (!StaticMeshAsset)
    {
        return false;
    }

    EnsureMeshTrianglePickingBVHBuilt();
    return MeshTrianglePickingBVH.RaycastLocal(LocalOrigin, LocalDirection, *StaticMeshAsset, OutHitResult);
}

FStaticMeshBuffer* UStaticMesh::GetLODMeshBuffer(uint32 LODLevel) const
{
    if (LODLevel == 0 && StaticMeshAsset)
        return StaticMeshAsset->RenderBuffer.get();
    if (LODLevel >= 1 && LODLevel <= 3 && bHasLOD)
        return AdditionalLODs[LODLevel - 1].RenderBuffer.get();
    return StaticMeshAsset ? StaticMeshAsset->RenderBuffer.get() : nullptr;
}

static const TArray<FStaticMeshSection> EmptySections;

const TArray<FStaticMeshSection>& UStaticMesh::GetLODSections(uint32 LODLevel) const
{
    if (LODLevel == 0 && StaticMeshAsset)
        return StaticMeshAsset->Sections;
    if (LODLevel >= 1 && LODLevel <= 3 && bHasLOD)
        return AdditionalLODs[LODLevel - 1].Sections;
    return StaticMeshAsset ? StaticMeshAsset->Sections : EmptySections;
}
