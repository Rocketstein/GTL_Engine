// 렌더 영역의 세부 동작을 구현합니다.
#include "Render/Resources/State/RenderStateTypes.h"
#include "Render/Execute/Registry/RenderPassTypes.h"
#include "Render/Resources/Buffers/ConstantBufferData.h"
#include "Render/Scene/Proxies/Primitive/StaticMeshSceneProxy.h"
#include "Component/StaticMeshComponent.h"
#include "Mesh/StaticMesh.h"
#include "Mesh/StaticMeshAsset.h"
#include "Materials/Material.h"
#include "Materials/MaterialSemantics.h"
#include "Texture/Texture2D.h"
#include "Engine/Runtime/Engine.h"
#include "Render/Renderer.h"

#include <algorithm>
#include <initializer_list>
#include <memory>

FStaticMeshSceneProxy::FStaticMeshSceneProxy(UStaticMeshComponent* InComponent)
    : FMeshSceneProxy(InComponent)
{
    UpdateShadow();
}

UMeshComponent* FStaticMeshSceneProxy::GetMeshComponent() const
{
    return static_cast<UStaticMeshComponent*>(Owner);
}

void FStaticMeshSceneProxy::UpdateShadow()
{
    UStaticMeshComponent* StaticMesh = static_cast<UStaticMeshComponent*>(GetMeshComponent());
    bCastShadow = StaticMesh ? StaticMesh->ShouldCastShadow() : true;
}

void FStaticMeshSceneProxy::UpdateLOD(uint32 LODLevel)
{
    if (LODLevel >= LODCount)
        LODLevel = LODCount - 1;
    if (LODLevel == CurrentLOD)
        return;

    std::swap(MeshBuffer, LODData[CurrentLOD].MeshBuffer);
    std::swap(SectionRenderData, LODData[CurrentLOD].SectionRenderData);
    std::swap(ActiveOwnedMaterialCBs, LODData[CurrentLOD].OwnedMaterialCBs);

    CurrentLOD = LODLevel;
    std::swap(MeshBuffer, LODData[LODLevel].MeshBuffer);
    std::swap(SectionRenderData, LODData[LODLevel].SectionRenderData);
    std::swap(ActiveOwnedMaterialCBs, LODData[LODLevel].OwnedMaterialCBs);
}

// Rebuilds the render-ready draw metadata for every section of every LOD in a UStaticMeshComponent.
void FStaticMeshSceneProxy::RebuildSectionRenderData()
{
    UStaticMeshComponent* SMC  = static_cast<UStaticMeshComponent*>(GetMeshComponent());
    UStaticMesh*          Mesh = SMC->GetStaticMesh();
    if (!Mesh || !Mesh->GetStaticMeshAsset())
    {
        for (uint32 lod = 0; lod < MAX_LOD; ++lod)
        {
            LODData[lod].MeshBuffer = nullptr;
            LODData[lod].SectionRenderData.clear();
            LODData[lod].OwnedMaterialCBs.clear();
        }

        LODCount   = 1;
        CurrentLOD = 0;
        MeshBuffer = nullptr;
        SectionRenderData.clear();
        ActiveOwnedMaterialCBs.clear();
        return;
    }

    ID3D11Device*        Device  = GEngine ? GEngine->GetRenderer().GetFD3DDevice().GetDevice() : nullptr;
    ID3D11DeviceContext* Context = GEngine ? GEngine->GetRenderer().GetFD3DDevice().GetDeviceContext() : nullptr;

    const auto& Slots     = Mesh->GetStaticMaterials();
    const auto& Overrides = SMC->GetOverrideMaterials();
    LODCount              = Mesh->GetLODCount();

    for (uint32 lod = 0; lod < LODCount; ++lod)
    {
        const auto& Sections    = Mesh->GetLODSections(lod);
        LODData[lod].MeshBuffer = Mesh->GetLODMeshBuffer(lod);
        LODData[lod].SectionRenderData.clear();
        LODData[lod].OwnedMaterialCBs.clear();
        LODData[lod].SectionRenderData.reserve(Sections.size());
        LODData[lod].OwnedMaterialCBs.reserve(Sections.size());

        for (const FStaticMeshSection& Section : Sections)
        {
            FMeshSectionRenderData Draw;
            Draw.FirstIndex    = Section.FirstIndex;
            Draw.IndexCount    = Section.NumTriangles * 3;
            Draw.Blend         = EBlendState::Opaque;
            Draw.DepthStencil  = EDepthStencilState::Default;
            Draw.Rasterizer    = ERasterizerState::SolidBackCull;
            Draw.MaterialCB[0] = nullptr;
            Draw.MaterialCB[1] = nullptr;

            UMaterial*  Mat           = nullptr;
            const int32 MaterialIndex = Section.MaterialIndex;
            if (MaterialIndex >= 0 && MaterialIndex < static_cast<int32>(Slots.size()))
            {
                if (MaterialIndex < static_cast<int32>(Overrides.size()) && Overrides[MaterialIndex])
                {
                    Mat = Overrides[MaterialIndex];
                }
                else if (Slots[MaterialIndex].MaterialInterface)
                {
                    Mat = Slots[MaterialIndex].MaterialInterface;
                }
            }

            if (Mat)
            {
                Draw.MaterialPath = Mat->GetAssetPathFileName();
                Draw.Shader = Mat->GetGraphicsProgram();
                TryGetTextureSRV(Mat, { MaterialSemantics::DiffuseTextureSlot, "BaseColorTexture", "AlbedoTexture", "BaseTexture", "DiffuseMap" }, Draw.DiffuseSRV);
                TryGetTextureSRV(Mat, { MaterialSemantics::NormalTextureSlot, "NormalMap", "NormalMapTexture", "BumpTexture", "BumpMap" }, Draw.NormalSRV);
                TryGetTextureSRV(Mat, { MaterialSemantics::SpecularTextureSlot, "SpecularMap", "SpecularMapTexture", "SpecularMask", "SpecularMaskTexture", "GlossMap" }, Draw.SpecularSRV);
                Draw.ShaderResourceBindings = BuildReflectedTextureBindings(Mat, Draw.Shader);
                if (lod == 0 && Device && Draw.Shader)
                {
                    Draw.MeshBuffer = Mesh->GetOrCreateRenderBufferForShader(
                        Device,
                        Draw.Shader);
                }
            }

            auto MaterialCB = BuildMeshMaterialCB(Mat, Device, Context, Draw.DiffuseSRV, Draw.NormalSRV, Draw.SpecularSRV);
            if (MaterialCB)
            {
                Draw.MaterialCB[0] = MaterialCB->GetConstantBuffer();
                LODData[lod].OwnedMaterialCBs.push_back(std::move(MaterialCB));
            }

            if (!Draw.MeshBuffer)
            {
                Draw.MeshBuffer = LODData[lod].MeshBuffer;
            }

            LODData[lod].SectionRenderData.push_back(Draw);
        }

        SortSectionRenderDataByMaterial(LODData[lod].SectionRenderData);
    }

    CurrentLOD = 0;
    std::swap(MeshBuffer, LODData[0].MeshBuffer);
    std::swap(SectionRenderData, LODData[0].SectionRenderData);
    std::swap(ActiveOwnedMaterialCBs, LODData[0].OwnedMaterialCBs);
}

