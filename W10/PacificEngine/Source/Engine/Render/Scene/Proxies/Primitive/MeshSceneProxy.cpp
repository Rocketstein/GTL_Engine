#include "Render/Resources/State/RenderStateTypes.h"
#include "Render/Execute/Registry/RenderPassTypes.h"
#include "Render/Resources/Buffers/ConstantBufferData.h"
#include "Render/Scene/Proxies/Primitive/MeshSceneProxy.h"
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

FMeshSceneProxy::FMeshSceneProxy(UMeshComponent* InComponent)
    : FPrimitiveProxy(InComponent) 
{
    bAllowViewModeShaderOverride = true;
    //UpdateShadow();
}

void FMeshSceneProxy::UpdateMaterial()
{
    RebuildSectionRenderData();
}

void FMeshSceneProxy::UpdateMesh()
{
    MeshBuffer = Owner->GetMeshBuffer();
    // Mesh shading is selected by the active render pass/view mode registry.
    Shader = nullptr;
    Pass   = ERenderPass::Opaque;

    RebuildSectionRenderData();
}

bool FMeshSceneProxy::SectionMaterialLess(const FMeshSectionRenderData& A, const FMeshSectionRenderData& B) 
{
    const uintptr_t ACB0 = reinterpret_cast<uintptr_t>(A.MaterialCB[0]);
    const uintptr_t BCB0 = reinterpret_cast<uintptr_t>(B.MaterialCB[0]);
    if (ACB0 != BCB0)
    {
        return ACB0 < BCB0;
    }

    const uintptr_t ACB1 = reinterpret_cast<uintptr_t>(A.MaterialCB[1]);
    const uintptr_t BCB1 = reinterpret_cast<uintptr_t>(B.MaterialCB[1]);
    if (ACB1 != BCB1)
    {
        return ACB1 < BCB1;
    }

    const uintptr_t ABaseSRV = reinterpret_cast<uintptr_t>(A.DiffuseSRV);
    const uintptr_t BBaseSRV = reinterpret_cast<uintptr_t>(B.DiffuseSRV);
    if (ABaseSRV != BBaseSRV)
    {
        return ABaseSRV < BBaseSRV;
    }

    const uintptr_t ANormalSRV = reinterpret_cast<uintptr_t>(A.NormalSRV);
    const uintptr_t BNormalSRV = reinterpret_cast<uintptr_t>(B.NormalSRV);
    if (ANormalSRV != BNormalSRV)
    {
        return ANormalSRV < BNormalSRV;
    }

    const uintptr_t ASpecularSRV = reinterpret_cast<uintptr_t>(A.SpecularSRV);
    const uintptr_t BSpecularSRV = reinterpret_cast<uintptr_t>(B.SpecularSRV);
    if (ASpecularSRV != BSpecularSRV)
    {
        return ASpecularSRV < BSpecularSRV;
    }

    return A.FirstIndex < B.FirstIndex;
}

bool FMeshSceneProxy::TryGetTextureSRV(UMaterial* Material, std::initializer_list<const char*> SlotNames, ID3D11ShaderResourceView*& OutSRV)
{
    OutSRV = nullptr;
    if (!Material)
    {
        return false;
    }

    for (const char* SlotName : SlotNames)
    {
        UTexture2D* Texture = nullptr;
        if (Material->GetTextureParameter(SlotName, Texture) && Texture)
        {
            OutSRV = Texture->GetSRV();
            if (OutSRV)
            {
                return true;
            }
        }
    }

    return false;
}

TArray<FShaderResourceBinding> FMeshSceneProxy::BuildReflectedTextureBindings(const UMaterial* Material, const FGraphicsProgram* GraphicsProgram)
{
    TArray<FShaderResourceBinding> Bindings;
    if (Material == nullptr || GraphicsProgram == nullptr)
    {
        return Bindings;
    }

    const TArray<FShaderTextureBindingInfo>& ReflectedBindings = GraphicsProgram->GetTextureBindings();
    Bindings.reserve(ReflectedBindings.size());

    for (const FShaderTextureBindingInfo& ReflectedBinding : ReflectedBindings)
    {
        UTexture2D* Texture = nullptr;
        const FString& SlotName = ReflectedBinding.CanonicalSlotName.empty() ? ReflectedBinding.ResourceName : ReflectedBinding.CanonicalSlotName;
        if (!Material->GetTextureParameter(SlotName, Texture) || Texture == nullptr || Texture->GetSRV() == nullptr)
        {
            continue;
        }

        FShaderResourceBinding Binding;
        Binding.SlotIndex = ReflectedBinding.SlotIndex;
        Binding.SRV = Texture->GetSRV();
        Bindings.push_back(Binding);
    }

    return Bindings;
}

float FMeshSceneProxy::GetScalarOrDefault(const UMaterial* Material, const char* ParamName, float DefaultValue)
{
    if (!Material)
    {
        return DefaultValue;
    }

    float Value = DefaultValue;
    return Material->GetScalarParameter(ParamName, Value) ? Value : DefaultValue;
}

FVector4 FMeshSceneProxy::GetVector4OrDefault(const UMaterial* Material, const char* ParamName, const FVector4& DefaultValue)
{
    if (!Material)
    {
        return DefaultValue;
    }

    FVector4 Value = DefaultValue;
    return Material->GetVector4Parameter(ParamName, Value) ? Value : DefaultValue;
}

std::unique_ptr<FMaterialConstantBuffer> FMeshSceneProxy::BuildMeshMaterialCB(const UMaterial* Material, ID3D11Device* Device, ID3D11DeviceContext* Context,
                                                                   ID3D11ShaderResourceView* DiffuseSRV, ID3D11ShaderResourceView* NormalSRV,
                                                                   ID3D11ShaderResourceView* SpecularSRV)
{
    if (!Device || !Context)
    {
        return nullptr;
    }

    auto Buffer = std::make_unique<FMaterialConstantBuffer>();
    Buffer->Init(Device, sizeof(FMeshMaterialViewCBData), ECBSlot::PerShader0);

    FMeshMaterialViewCBData Constants;
    Constants.SectionColor  = GetVector4OrDefault(Material, MaterialSemantics::SectionColorParameter, MaterialSemantics::GetDefaultSectionColor());
    Constants.MaterialParam = FVector4(
        GetScalarOrDefault(Material, MaterialSemantics::SpecularPowerParameter, MaterialSemantics::DefaultSpecularPower),
        GetScalarOrDefault(Material, MaterialSemantics::SpecularStrengthParameter, MaterialSemantics::DefaultSpecularStrength),
        0.0f,
        1.0f);
    Constants.HasBaseTexture     = DiffuseSRV ? 1u : 0u;
    Constants.HasNormalTexture   = NormalSRV ? 1u : 0u;
    Constants.HasSpecularTexture = SpecularSRV ? 1u : 0u;

    Buffer->SetData(&Constants, sizeof(Constants));
    Buffer->Upload(Context);
    return Buffer;
}

void FMeshSceneProxy::SortSectionRenderDataByMaterial(TArray<FMeshSectionRenderData>& Draws)
{
    if (Draws.size() > 1)
    {
        std::sort(Draws.begin(), Draws.end(), SectionMaterialLess);
    }
}
