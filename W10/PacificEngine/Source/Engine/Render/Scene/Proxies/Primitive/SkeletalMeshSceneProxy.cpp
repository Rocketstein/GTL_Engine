#include "SkeletalMeshSceneProxy.h"
#include "Component/SkeletalMeshComponent.h"
#include "Component/SkinnedMeshComponent.h"
#include "Render/Resources/State/RenderStateTypes.h"
#include "Render/Execute/Registry/RenderPassTypes.h"
#include "Render/Resources/Buffers/ConstantBufferData.h"
#include "Mesh/StaticMeshAsset.h"
#include "Materials/Material.h"
#include "Materials/MaterialSemantics.h"
#include "Texture/Texture2D.h"
#include "Engine/Runtime/Engine.h"
#include "Render/Renderer.h"

FSkeletalMeshSceneProxy::FSkeletalMeshSceneProxy(USkeletalMeshComponent* InComponent)
    : FMeshSceneProxy(InComponent)
{
    UpdateShadow();
}

void FSkeletalMeshSceneProxy::UpdateMaterial()
{
    if (USkinnedMeshComponent* SMC = static_cast<USkinnedMeshComponent*>(GetMeshComponent()))
    {
        SMC->UpdateSkinnedVertices();
        FMeshSceneProxy::UpdateMaterial();
        SMC->PruneUnusedSkinnedRuntimeRenderBuffers();
        return;
    }

    FMeshSceneProxy::UpdateMaterial();
}

void FSkeletalMeshSceneProxy::UpdateMesh()
{
    if (USkinnedMeshComponent* SMC = static_cast<USkinnedMeshComponent*>(GetMeshComponent()))
    {
        SMC->UpdateSkinnedVertices();
        FMeshSceneProxy::UpdateMesh();
        SMC->PruneUnusedSkinnedRuntimeRenderBuffers();
        return;
    }

    FMeshSceneProxy::UpdateMesh();
}

void FSkeletalMeshSceneProxy::BuildSkeletalDebugInstance(FSkeletalDebugInstance& OutInstance) const
{
    OutInstance.Bones.clear();
    const USkinnedMeshComponent* Skinned = static_cast<const USkinnedMeshComponent*>(Owner);
    if (!Skinned)
        return;

    // TODO: Add ParentIndex once the concrete data type is set
    const int32 BoneCount = Skinned->GetNumBones();
    OutInstance.Bones.reserve(BoneCount);

    for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
    {
        FSkeletalDebugBone Bone;
        Bone.Color       = FColor(255, 255, 0);
        Bone.WorldMatrix = Skinned->GetBoneWorldMatrix(BoneIndex);

        if (const FBoneInfo* Info = Skinned->GetBoneInfo(BoneIndex))
        {
            Bone.ParentIndex = Info->ParentIndex;
        }

        OutInstance.Bones.push_back(Bone);
    }
}

void FSkeletalMeshSceneProxy::UpdateShadow()
{
    UMeshComponent* Mesh = GetMeshComponent();
    bCastShadow          = Mesh ? Mesh->ShouldCastShadow() : true;
}

UMeshComponent* FSkeletalMeshSceneProxy::GetMeshComponent() const
{
    return static_cast<USkeletalMeshComponent*>(Owner);
}

// Note: SkeletalMesh does not LOD for now
void FSkeletalMeshSceneProxy::RebuildSectionRenderData()
{
    USkinnedMeshComponent* SMC = static_cast<USkinnedMeshComponent*>(GetMeshComponent());
    if (!SMC)
    {
        SectionRenderData.clear();
        ActiveOwnedMaterialCBs.clear();
        MeshBuffer = nullptr;
        return;
    }
    USkeletalMesh* Mesh = SMC->GetSkeletalMesh();
    if (!Mesh)
    {
        SectionRenderData.clear();
        ActiveOwnedMaterialCBs.clear();
        MeshBuffer = nullptr;
        return;
    }

    LODCount        = 1;
    CurrentLOD      = 0;
    auto& Lod0      = LODData[0];
    Lod0.MeshBuffer = nullptr; // proxy-level buffer unused. Sections carry their own
    Lod0.SectionRenderData.clear();
    Lod0.OwnedMaterialCBs.clear();

    int32                GlobalMaterialBase = 0;
    ID3D11Device*        Device             = GEngine ? GEngine->GetRenderer().GetFD3DDevice().GetDevice() : nullptr;
    ID3D11DeviceContext* Context            = GEngine ? GEngine->GetRenderer().GetFD3DDevice().GetDeviceContext() : nullptr;
    for (uint32 SubMeshIndex = 0; SubMeshIndex < Mesh->GetSubMeshes().size(); ++SubMeshIndex)
    {
        USkeletalSubMesh* SubMesh = Mesh->GetSubMeshes()[SubMeshIndex];
        if (!SubMesh || !SubMesh->GetSkeletalSubMeshAsset())
            continue;

        FSkeletalSubMesh*    Asset     = SubMesh->GetSkeletalSubMeshAsset();
        FSkeletalMeshBuffer* SubBuffer = SMC->GetSkinnedRenderBuffer(static_cast<int32>(SubMeshIndex));
        if (!SubBuffer)
        {
            SubBuffer = Asset->RenderBuffer.get();
        }
        const auto&          Slots     = SubMesh->GetStaticMaterials();
        const auto&          OverAll   = SMC->GetOverrideMaterials();

        if (!SubBuffer || !SubBuffer->IsValid())
        {
            GlobalMaterialBase += static_cast<int32>(Slots.size());
            continue;
        }

        for (const FSkeletalMeshSection& Section : Asset->Sections)
        {
            FMeshSectionRenderData Draw;
            Draw.MeshBuffer    = SubBuffer;
            Draw.FirstIndex    = Section.FirstIndex;
            Draw.IndexCount    = Section.NumTriangles * 3;
            Draw.MaterialCB[0] = nullptr;
            Draw.MaterialCB[1] = nullptr;

            UMaterial*  Mat           = nullptr;
            const int32 MaterialIndex = Section.MaterialIndex;
            if (MaterialIndex >= 0 && MaterialIndex < static_cast<int32>(Slots.size()))
            {
                const int32 GlobalIndex = GlobalMaterialBase + MaterialIndex;
                if (GlobalIndex < static_cast<int32>(OverAll.size()) && OverAll[GlobalIndex])
                {
                    Mat = OverAll[GlobalIndex];
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
                if (Draw.Shader)
                {
                    Draw.MeshBuffer = SMC->GetSkinnedRenderBufferForShader(
                        static_cast<int32>(SubMeshIndex),
                        Draw.Shader);
                }
            }

            auto MaterialCB = BuildMeshMaterialCB(Mat, Device, Context, Draw.DiffuseSRV, Draw.NormalSRV, Draw.SpecularSRV);
            if (MaterialCB)
            {
                Draw.MaterialCB[0] = MaterialCB->GetConstantBuffer();
                Lod0.OwnedMaterialCBs.push_back(std::move(MaterialCB));
            }

            Lod0.SectionRenderData.push_back(Draw);
        }

        GlobalMaterialBase += static_cast<int32>(Slots.size());
    }


    SortSectionRenderDataByMaterial(Lod0.SectionRenderData);
    std::swap(MeshBuffer, Lod0.MeshBuffer);
    std::swap(SectionRenderData, Lod0.SectionRenderData);
    std::swap(ActiveOwnedMaterialCBs, Lod0.OwnedMaterialCBs);
}
