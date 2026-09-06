// 렌더 영역의 세부 동작을 구현합니다.
#include "Render/Submission/Command/BuildDrawCommand.h"

#include "Component/TextRenderComponent.h"
#include "Core/Logging/LogMacros.h"
#include "Render/Execute/Context/RenderPipelineContext.h"
#include "Render/Execute/Context/Scene/SceneView.h"
#include "Render/Execute/Context/ViewMode/ShadingModel.h"
#include "Render/Execute/Context/Viewport/ViewportRenderTargets.h"
#include "Render/Execute/Registry/RenderPassPresets.h"
#include "Render/Execute/Registry/ViewModePassRegistry.h"
#include "Render/Renderer.h"
#include "Render/Resources/Bindings/RenderBindingSlots.h"
#include "Render/Resources/Buffers/ConstantBufferCache.h"
#include "Render/Resources/Buffers/ConstantBufferData.h"
#include "Render/Resources/FrameResources.h"
#include "Render/Resources/Shadows/ShadowFilterSettings.h"
#include "Render/Resources/Shaders/MeshPassOverrideSettings.h"
#include "Render/Resources/Shaders/ShaderManager.h"
#include "Render/Scene/Proxies/Primitive/PrimitiveProxy.h"
#include "Render/Scene/Proxies/Primitive/TextRenderSceneProxy.h"
#include "Render/Scene/Proxies/UI/UIProxy.h"
#include "Render/Scene/Scene.h"
#include "Render/Resources/State/RenderStateTypes.h"
#include "Render/Submission/Command/DrawCommand.h"
#include "Render/Submission/Command/DrawCommandList.h"
#include "Mesh/ObjManager.h"
#include "Mesh/StaticMesh.h"
#include "Render/RHI/D3D11/Buffers/StaticMeshBuffer.h"
#include "Render/RHI/D3D11/Buffers/VertexTypes.h"
#include "Render/RHI/D3D11/Buffers/RuntimeVertexWideningPolicy.h"
#include "Engine/Platform/Paths.h"
#include "Resource/FontManager.h"

#include <algorithm>
#include <unordered_set>
#include <vector>

namespace
{
enum class EShaderOverridePolicy : uint8
{
    None = 0,
    Required,
    Legacy,
};

struct FMeshPassShaderSelection
{
    FGraphicsProgram*     Shader       = nullptr;
    EShaderOverridePolicy Policy       = EShaderOverridePolicy::None;
    const char*           ShaderSource = "ProxyShader";
    const char*           OverrideTag  = "Override::None";
};

struct FExpectedVertexSemantic
{
    const char* SemanticName = nullptr;
    uint32      SemanticIndex = 0;
};

FString JoinVertexSemanticDescriptor(const FVertexSemanticDescriptor& Descriptor)
{
    FString Result;
    for (const FVertexSemantic& Semantic : Descriptor.Elements)
    {
        if (!Result.empty())
        {
            Result += ",";
        }

        Result += Semantic.SemanticName;
        if (Semantic.SemanticIndex > 0)
        {
            Result += std::to_string(Semantic.SemanticIndex);
        }
    }

    return Result.empty() ? "<none>" : Result;
}

const char* GetShaderDebugName(const FGraphicsProgram* Shader)
{
    if (!Shader)
    {
        return "<null>";
    }

    const FString& DebugName = Shader->GetDebugName();
    return DebugName.empty() ? "<unnamed>" : DebugName.c_str();
}

const char* GetShaderOverridePolicyName(EShaderOverridePolicy Policy)
{
    switch (Policy)
    {
    case EShaderOverridePolicy::Required:
        return "RequiredOverride";
    case EShaderOverridePolicy::Legacy:
        return "LegacyOverride";
    case EShaderOverridePolicy::None:
    default:
        return "NoOverride";
    }
}

const TArray<FExpectedVertexSemantic>* GetSpecialPassVertexSemantics(ERenderPass Pass)
{
    static const TArray<FExpectedVertexSemantic> PositionOnly = {
        { "POSITION", 0 },
    };

    /*
        DepthOnly / ShadowEncode currently share VS_Input_PNCT_T in HLSL, but the
        compiled vertex path only consumes POSITION. Keep that minimum contract
        fixed here so validation does not drift with shared input struct names.
    */
    if (Pass == ERenderPass::DepthPre || Pass == ERenderPass::SelectionMask || Pass == ERenderPass::ShadowMap)
    {
        return &PositionOnly;
    }

    return nullptr;
}

/*
    Mesh pass shader selection policy

    1. Required override
       - DepthPre / SelectionMask: depth-only input/output contract is fixed.
       - ShadowMap: shadow filter mode decides the encoding shader.

    2. Legacy override
       - Opaque: editor view mode registry may still replace the proxy/material shader.

    Any future cleanup should preserve the required overrides first, then decide
    whether the legacy opaque override can be reduced or removed.
*/
FMeshPassShaderSelection ResolveMeshPassShader(const FPrimitiveProxy& Proxy,
                                               const FGraphicsProgram* PreferredShader,
                                               ERenderPass Pass,
                                               const FRenderPipelineContext& Context)
{
    FMeshPassShaderSelection Selection;
    Selection.Shader = const_cast<FGraphicsProgram*>(PreferredShader ? PreferredShader : Proxy.Shader);
    Selection.ShaderSource = PreferredShader ? "MaterialShader" : "ProxyShader";

    const bool bIsMaskLikePass = (Pass == ERenderPass::DepthPre || Pass == ERenderPass::SelectionMask || Pass == ERenderPass::ShadowMap);

    // Required override: shadow pass output format depends on the active shadow filter.
    if (Pass == ERenderPass::ShadowMap)
    {
        const EShadowFilterMethod ShadowFilterMethod = GetShadowFilterMethod();
        if (ShadowFilterMethod == EShadowFilterMethod::ESM)
        {
            Selection.Shader       = FShaderManager::Get().GetShader(EShaderType::ShadowEncodeESM);
            Selection.Policy       = EShaderOverridePolicy::Required;
            Selection.ShaderSource = "BuiltIn::ShadowEncodeESM";
            Selection.OverrideTag  = "Required::ShadowMapESM";
        }
        else if (ShadowFilterMethod == EShadowFilterMethod::VSM)
        {
            Selection.Shader       = FShaderManager::Get().GetShader(EShaderType::ShadowEncodeVSM);
            Selection.Policy       = EShaderOverridePolicy::Required;
            Selection.ShaderSource = "BuiltIn::ShadowEncodeVSM";
            Selection.OverrideTag  = "Required::ShadowMapVSM";
        }
        else
        {
            Selection.Shader       = FShaderManager::Get().GetShader(EShaderType::DepthOnly);
            Selection.Policy       = EShaderOverridePolicy::Required;
            Selection.ShaderSource = "BuiltIn::DepthOnly";
            Selection.OverrideTag  = "Required::ShadowMapDepthOnly";
        }
    }
    // Required override: mask-like passes share the fixed depth-only contract.
    else if (bIsMaskLikePass)
    {
        Selection.Shader       = FShaderManager::Get().GetShader(EShaderType::DepthOnly);
        Selection.Policy       = EShaderOverridePolicy::Required;
        Selection.ShaderSource = "BuiltIn::DepthOnly";
        Selection.OverrideTag  = "Required::MaskLikeDepthOnly";
    }
    // Legacy override: editor/view-mode path can still replace the proxy shader for opaque.
    else if (Pass == ERenderPass::Opaque &&
             PreferredShader == nullptr &&
             !IsLegacyOpaqueViewModeOverrideBypassed() &&
             Context.SceneView &&
             Proxy.bAllowViewModeShaderOverride &&
             Context.ViewMode.Registry &&
             Context.ViewMode.Registry->HasConfig(Context.ViewMode.ActiveViewMode))
    {
        const FViewModePassDesc* Desc =
            Context.ViewMode.Registry->FindOpaquePassDesc(Context.ViewMode.ActiveViewMode);
        if (Desc && Desc->CompiledShader)
        {
            Selection.Shader       = Desc->CompiledShader;
            Selection.Policy       = EShaderOverridePolicy::Legacy;
            Selection.ShaderSource = "ViewModeRegistry::Opaque";
            Selection.OverrideTag  = "Legacy::ViewModeOpaque";
        }
    }

    return Selection;
}

void LogMeshShaderSelectionOnce(ERenderPass Pass,
                                const FPrimitiveProxy& Proxy,
                                const FMeshPassShaderSelection& Selection,
                                const FString& MaterialPath,
                                uint32 FirstIndex,
                                uint32 IndexCount)
{
    static std::unordered_set<std::string> LoggedKeys;

    const char* MaterialLabel = MaterialPath.empty() ? "<none>" : MaterialPath.c_str();
    const char* ShaderLabel   = GetShaderDebugName(Selection.Shader);
    const char* PolicyLabel   = GetShaderOverridePolicyName(Selection.Policy);
    const char* SourceLabel   = Selection.ShaderSource ? Selection.ShaderSource : "Unknown";
    const char* OverrideLabel = Selection.OverrideTag ? Selection.OverrideTag : "None";

    std::string Key = std::string(GetRenderPassName(Pass)) + "|" +
                      PolicyLabel + "|" +
                      SourceLabel + "|" +
                      OverrideLabel + "|" +
                      ShaderLabel + "|" +
                      MaterialLabel + "|" +
                      std::to_string(reinterpret_cast<uintptr_t>(Proxy.MeshBuffer)) + "|" +
                      std::to_string(FirstIndex) + "|" +
                      std::to_string(IndexCount);

    if (!LoggedKeys.insert(Key).second)
    {
        return;
    }

    UE_LOG(
        Render,
        Debug,
        "MeshDrawCommand ShaderTrace Pass=%s Policy=%s Shader=%s Source=%s Override=%s Material=%s ProxyShader=%s SectionFirstIndex=%u SectionIndexCount=%u",
        GetRenderPassName(Pass),
        PolicyLabel,
        ShaderLabel,
        SourceLabel,
        OverrideLabel,
        MaterialLabel,
        GetShaderDebugName(Proxy.Shader),
        FirstIndex,
        IndexCount);
}

FVertexSemanticDescriptor BuildVertexSemanticDescriptor(const TArray<FExpectedVertexSemantic>& ExpectedSemantics)
{
    FVertexSemanticDescriptor Descriptor;
    Descriptor.Elements.reserve(ExpectedSemantics.size());
    for (const FExpectedVertexSemantic& Semantic : ExpectedSemantics)
    {
        Descriptor.Elements.push_back({ Semantic.SemanticName, Semantic.SemanticIndex });
    }
    return Descriptor;
}

bool HasExpectedVertexSemantic(const FVertexSemanticDescriptor& ExpectedSemantics, const FVertexSemantic& InputElement)
{
    for (const FVertexSemantic& Expected : ExpectedSemantics.Elements)
    {
        if (_stricmp(Expected.SemanticName.c_str(), InputElement.SemanticName.c_str()) == 0 &&
            Expected.SemanticIndex == InputElement.SemanticIndex)
        {
            return true;
        }
    }

    return false;
}

bool BuildCompatibilityRequestList(
    ERenderPass Pass,
    const FGraphicsProgram* Shader,
    FRuntimeVertexElementRequestList& OutRequestList,
    bool& bOutUsesSpecialPassSemantics,
    FString* OutUnsupportedInputs = nullptr)
{
    OutRequestList.Elements.clear();
    bOutUsesSpecialPassSemantics = false;
    if (OutUnsupportedInputs)
    {
        OutUnsupportedInputs->clear();
    }

    if (!Shader)
    {
        return false;
    }

    if (const TArray<FExpectedVertexSemantic>* SpecialPassSemantics = GetSpecialPassVertexSemantics(Pass))
    {
        bOutUsesSpecialPassSemantics = true;
        OutRequestList.Elements.reserve(SpecialPassSemantics->size());
        for (const FExpectedVertexSemantic& Semantic : *SpecialPassSemantics)
        {
            FRuntimeVertexSemanticSourceDesc CanonicalSourceDesc;
            if (!BuildCanonicalRuntimeVertexSourceDesc(Semantic.SemanticName, Semantic.SemanticIndex, CanonicalSourceDesc))
            {
                if (OutUnsupportedInputs)
                {
                    if (!OutUnsupportedInputs->empty())
                    {
                        *OutUnsupportedInputs += ",";
                    }
                    *OutUnsupportedInputs += Semantic.SemanticName;
                    if (Semantic.SemanticIndex > 0)
                    {
                        *OutUnsupportedInputs += std::to_string(Semantic.SemanticIndex);
                    }
                }
                return false;
            }

            FRuntimeVertexElementRequest Request;
            Request.SemanticName = Semantic.SemanticName;
            Request.SemanticIndex = Semantic.SemanticIndex;
            Request.ComponentCount = CanonicalSourceDesc.ComponentCount;
            Request.ScalarType = CanonicalSourceDesc.ScalarType;
            OutRequestList.Elements.push_back(Request);
        }
        return true;
    }

    const TArray<FGraphicsProgram::FVertexInputElement>& ShaderInputs = Shader->GetVertexInputs();
    OutRequestList.Elements.reserve(ShaderInputs.size());
    for (const FGraphicsProgram::FVertexInputElement& Input : ShaderInputs)
    {
        FRuntimeVertexElementRequest Request;
        if (!BuildRuntimeVertexElementRequest(Input.SemanticName, Input.SemanticIndex, Input.Format, Request))
        {
            if (OutUnsupportedInputs)
            {
                if (!OutUnsupportedInputs->empty())
                {
                    *OutUnsupportedInputs += ",";
                }
                *OutUnsupportedInputs += Input.SemanticName;
                if (Input.SemanticIndex > 0)
                {
                    *OutUnsupportedInputs += std::to_string(Input.SemanticIndex);
                }
            }
            return false;
        }
        OutRequestList.Elements.push_back(Request);
    }

    return !OutRequestList.IsEmpty();
}

void ValidateMeshShaderInputCompatibilityOnce(ERenderPass Pass,
                                              const FGraphicsProgram* Shader,
                                              const FMeshBuffer* MeshBuffer,
                                              const FString& MaterialPath)
{
    if (!Shader || !MeshBuffer)
    {
        return;
    }

    static std::unordered_set<std::string> LoggedCompatibilityKeys;

    const uint32 VertexStride = MeshBuffer->GetVertexStride();
    const FVertexSemanticDescriptor& MeshSemantics = MeshBuffer->GetVertexSemanticDescriptor();
    const char* MaterialLabel = MaterialPath.empty() ? "<none>" : MaterialPath.c_str();
    const char* ShaderLabel   = GetShaderDebugName(Shader);
    const std::string Key = std::string(GetRenderPassName(Pass)) + "|" +
                            ShaderLabel + "|" +
                            MaterialLabel + "|" +
                            std::to_string(VertexStride) + "|" +
                            JoinVertexSemanticDescriptor(MeshSemantics).c_str();

    if (!LoggedCompatibilityKeys.insert(Key).second)
    {
        return;
    }

    FRuntimeVertexElementRequestList RequiredElements;
    bool bUsesSpecialPassSemantics = false;
    FString UnsupportedInputs;
    if (!BuildCompatibilityRequestList(Pass, Shader, RequiredElements, bUsesSpecialPassSemantics, &UnsupportedInputs))
    {
        UE_LOG(
            Render,
            Warning,
            "Mesh/Shader input contract has no compatibility request list. Pass=%s Shader=%s Material=%s VertexStride=%u ReflectedShaderInputs=%s UnsupportedInputs=%s",
            GetRenderPassName(Pass),
            ShaderLabel,
            MaterialLabel,
            VertexStride,
            JoinVertexSemanticDescriptor(Shader->GetVertexInputDescriptor()).c_str(),
            UnsupportedInputs.empty() ? "<none>" : UnsupportedInputs.c_str());
        return;
    }

    const FVertexSemanticDescriptor& ShaderInputs = Shader->GetVertexInputDescriptor();
    const FVertexSemanticDescriptor& AvailableSemantics = MeshSemantics;
    FVertexSemanticDescriptor RequiredSemantics;
    RequiredSemantics.Elements.reserve(RequiredElements.Elements.size());
    for (const FRuntimeVertexElementRequest& RequiredElement : RequiredElements.Elements)
    {
        RequiredSemantics.Elements.push_back({ RequiredElement.SemanticName, RequiredElement.SemanticIndex });
    }

    FString MissingSemantics;
    FString IncompatibleSemantics;
    FString UnsupportedMeshSemantics;

    for (const FRuntimeVertexElementRequest& RequiredElement : RequiredElements.Elements)
    {
        const FVertexSemantic RequiredSemantic = { RequiredElement.SemanticName, RequiredElement.SemanticIndex };
        const FVertexSemantic* AvailableSemanticMatch = nullptr;

        for (const FVertexSemantic& AvailableSemantic : AvailableSemantics.Elements)
        {
            if (_stricmp(RequiredSemantic.SemanticName.c_str(), AvailableSemantic.SemanticName.c_str()) == 0 &&
                RequiredSemantic.SemanticIndex == AvailableSemantic.SemanticIndex)
            {
                AvailableSemanticMatch = &AvailableSemantic;
                break;
            }
        }

        if (!AvailableSemanticMatch)
        {
            if (CanDefaultFillRuntimeVertexElement(RequiredElement))
            {
                continue;
            }

            if (!MissingSemantics.empty())
            {
                MissingSemantics += ",";
            }

            MissingSemantics += RequiredSemantic.SemanticName;
            if (RequiredSemantic.SemanticIndex > 0)
            {
                MissingSemantics += std::to_string(RequiredSemantic.SemanticIndex);
            }
            continue;
        }

        FRuntimeVertexSemanticSourceDesc AvailableSourceDesc;
        if (!BuildCanonicalRuntimeVertexSourceDesc(*AvailableSemanticMatch, AvailableSourceDesc))
        {
            if (!UnsupportedMeshSemantics.empty())
            {
                UnsupportedMeshSemantics += ",";
            }

            UnsupportedMeshSemantics += RequiredSemantic.SemanticName;
            if (RequiredSemantic.SemanticIndex > 0)
            {
                UnsupportedMeshSemantics += std::to_string(RequiredSemantic.SemanticIndex);
            }
            continue;
        }

        const bool bCompatible =
            IsRuntimeVertexElementExactMatch(AvailableSourceDesc, RequiredElement) ||
            IsRuntimeVertexWideningCompatible(AvailableSourceDesc, RequiredElement);
        if (!bCompatible)
        {
            if (!IncompatibleSemantics.empty())
            {
                IncompatibleSemantics += ",";
            }

            IncompatibleSemantics += RequiredSemantic.SemanticName;
            if (RequiredSemantic.SemanticIndex > 0)
            {
                IncompatibleSemantics += std::to_string(RequiredSemantic.SemanticIndex);
            }
        }
    }

    if (!MissingSemantics.empty() || !IncompatibleSemantics.empty() || !UnsupportedMeshSemantics.empty())
    {
        UE_LOG(
            Render,
            Warning,
            "Mesh/Shader input mismatch detected. Pass=%s Shader=%s Material=%s VertexStride=%u MeshSemantics=%s RequiredSemantics=%s ReflectedShaderInputs=%s MissingSemantics=%s IncompatibleSemantics=%s UnsupportedMeshSemantics=%s ValidationPolicy=%s",
            GetRenderPassName(Pass),
            ShaderLabel,
            MaterialLabel,
            VertexStride,
            JoinVertexSemanticDescriptor(MeshSemantics).c_str(),
            JoinVertexSemanticDescriptor(RequiredSemantics).c_str(),
            JoinVertexSemanticDescriptor(ShaderInputs).c_str(),
            MissingSemantics.empty() ? "<none>" : MissingSemantics.c_str(),
            IncompatibleSemantics.empty() ? "<none>" : IncompatibleSemantics.c_str(),
            UnsupportedMeshSemantics.empty() ? "<none>" : UnsupportedMeshSemantics.c_str(),
            bUsesSpecialPassSemantics ? "SpecialPassSemanticPolicy" : "ReflectedCompatibilityPolicy");
    }
}
} // namespace

void DrawCommandBuild::BuildMeshDrawCommand(const FPrimitiveProxy& Proxy, ERenderPass Pass, FRenderPipelineContext& Context, FDrawCommandList& OutList, uint16 UserBits)
{
    const bool bProxyMeshValid    = Proxy.MeshBuffer && Proxy.MeshBuffer->IsValid();
    const bool bHasSectionBuffers = std::any_of(
        Proxy.SectionRenderData.begin(),
        Proxy.SectionRenderData.end(),
        [](const FMeshSectionRenderData& S)
        {
            return S.MeshBuffer && S.MeshBuffer->IsValid();
        });

    if (!bProxyMeshValid && !bHasSectionBuffers)
    {
        return;
    }

    ID3D11DeviceContext* Ctx             = Context.Context;
    const bool                     bIsMaskLikePass = (Pass == ERenderPass::DepthPre || Pass == ERenderPass::SelectionMask || Pass == ERenderPass::ShadowMap);

    const FRenderPassDrawPreset& PassPreset = Context.GetRenderPassDrawPreset(Pass);

    FConstantBuffer* PerObjCB = Context.Renderer ? Context.Renderer->AcquirePerObjectCBForProxy(Proxy) : nullptr;
    if (!PerObjCB && Context.Resources)
    {
        PerObjCB = &Context.Resources->PerObjectConstantBuffer;
    }

    if (PerObjCB && Ctx)
    {
        PerObjCB->Update(Ctx, &Proxy.PerObjectConstants, sizeof(FPerObjectCBData));
        Proxy.ClearPerObjectCBDirty();
    }

    FConstantBuffer* ExtraCB0 = nullptr;
    FConstantBuffer* ExtraCB1 = nullptr;
    if (!bIsMaskLikePass && Proxy.ExtraCB.Buffer && Proxy.ExtraCB.Size > 0 && Ctx)
    {
        Proxy.ExtraCB.Buffer->Update(Ctx, Proxy.ExtraCB.Data, Proxy.ExtraCB.Size);

        if (Proxy.ExtraCB.Slot == ECBSlot::PerShader0)
        {
            ExtraCB0 = Proxy.ExtraCB.Buffer;
        }
        else if (Proxy.ExtraCB.Slot == ECBSlot::PerShader1)
        {
            ExtraCB1 = Proxy.ExtraCB.Buffer;
        }
    }

    auto AddSection = [&](uint32 FirstIndex, uint32 IndexCount, ID3D11ShaderResourceView* BaseSRV, ID3D11ShaderResourceView* InNormalSRV,
                          ID3D11ShaderResourceView* InSpecularSRV,
                          FConstantBuffer* CB0, FConstantBuffer* CB1,
                          EBlendState SectionBlend, EDepthStencilState SectionDepthStencil, ERasterizerState SectionRasterizer,
                          FMeshBuffer* MeshBuffer = nullptr,
                          FGraphicsProgram* SectionShader = nullptr,
                          const TArray<FShaderResourceBinding>* ShaderResourceBindings = nullptr,
                          const FString* MaterialPath = nullptr)
    {
        if (IndexCount == 0)
        {
            return;
        }

        const FMeshPassShaderSelection ShaderSelection = ResolveMeshPassShader(Proxy, SectionShader, Pass, Context);
        FGraphicsProgram*              Shader          = ShaderSelection.Shader;
        if (!Shader)
        {
            return;
        }

        LogMeshShaderSelectionOnce(
            Pass,
            Proxy,
            ShaderSelection,
            MaterialPath ? *MaterialPath : FString(),
            FirstIndex,
            IndexCount);

        ValidateMeshShaderInputCompatibilityOnce(
            Pass,
            Shader,
            MeshBuffer ? MeshBuffer : Proxy.MeshBuffer,
            MaterialPath ? *MaterialPath : FString());

        FDrawCommand& Cmd = OutList.AddCommand();
        Cmd.Shader        = Shader;
        Cmd.MeshBuffer    = MeshBuffer ? MeshBuffer : Proxy.MeshBuffer;
        Cmd.FirstIndex    = FirstIndex;
        Cmd.IndexCount    = IndexCount;

        if (bIsMaskLikePass)
        {
            Cmd.DepthStencil = PassPreset.DepthStencil;
            Cmd.Blend        = PassPreset.Blend;
            Cmd.Rasterizer   = PassPreset.Rasterizer;
        }
        else
        {
            const bool bUsesDepthPreForViewMode =
                (Pass == ERenderPass::Opaque) &&
                Context.ViewMode.Registry != nullptr &&
                Context.ViewMode.Registry->HasConfig(Context.ViewMode.ActiveViewMode) &&
                Context.ViewMode.Registry->UsesDepthPrePass(Context.ViewMode.ActiveViewMode);

            const bool bUsesViewModeOpaque =
                (Pass == ERenderPass::Opaque) &&
                ShaderSelection.Policy == EShaderOverridePolicy::Legacy;

            // Reversed-Z depth pre-pass writes the same geometry depth first.
            // Any opaque color pass that follows depth pre must keep GREATER_EQUAL
            // style read-only depth so exact-depth fragments survive the second pass.
            if (SectionDepthStencil != EDepthStencilState::Default)
            {
                Cmd.DepthStencil = SectionDepthStencil;
            }
            else if (bUsesDepthPreForViewMode || bUsesViewModeOpaque)
            {
                Cmd.DepthStencil = EDepthStencilState::DepthReadOnly;
            }
            else
            {
                Cmd.DepthStencil = PassPreset.DepthStencil;
            }

            if (SectionBlend != EBlendState::Opaque || Pass != ERenderPass::Opaque)
            {
                Cmd.Blend = SectionBlend;
            }
            else
            {
                Cmd.Blend = PassPreset.Blend;
            }

            if (SectionRasterizer != ERasterizerState::SolidBackCull)
            {
                Cmd.Rasterizer = SectionRasterizer;
            }
            else
            {
                Cmd.Rasterizer = PassPreset.Rasterizer;
            }
        }

        if ((Pass == ERenderPass::Opaque ||
             Pass == ERenderPass::AlphaBlend ||
             Pass == ERenderPass::SubUV ||
             Pass == ERenderPass::OverlayBillboard ||
             Pass == ERenderPass::OverlayTextWorld) &&
            Context.ViewMode.ActiveViewMode == EViewMode::Wireframe)
        {
            Cmd.Rasterizer = ERasterizerState::WireFrame;
        }

        Cmd.Topology       = PassPreset.Topology;
        Cmd.PerObjectCB    = PerObjCB;
        Cmd.PerShaderCB[0] = bIsMaskLikePass ? nullptr : (CB0 ? CB0 : (Proxy.MaterialCB[0] ? Proxy.MaterialCB[0] : ExtraCB0));
        Cmd.PerShaderCB[1] = bIsMaskLikePass ? nullptr : (CB1 ? CB1 : (Proxy.MaterialCB[1] ? Proxy.MaterialCB[1] : ExtraCB1));
        Cmd.LightCB        = (bIsMaskLikePass || !Context.Resources) ? nullptr : &Context.Resources->GlobalLightBuffer;
        Cmd.LocalLightSRV  = (bIsMaskLikePass || !Context.Resources) ? nullptr : Context.Resources->LocalLightSRV;
        Cmd.DiffuseSRV     = bIsMaskLikePass ? nullptr : BaseSRV;
        Cmd.NormalSRV      = bIsMaskLikePass ? nullptr : InNormalSRV;
        Cmd.SpecularSRV    = bIsMaskLikePass ? nullptr : InSpecularSRV;
        if (!bIsMaskLikePass && ShaderResourceBindings)
        {
            Cmd.ShaderResourceBindings = *ShaderResourceBindings;
        }
        Cmd.Pass           = Pass;
        Cmd.DebugName      = MaterialPath && !MaterialPath->empty() ? MaterialPath->c_str() : nullptr;
        const uintptr_t MaterialHash =
            (reinterpret_cast<uintptr_t>(Cmd.PerShaderCB[0]) >> 4) ^
            (reinterpret_cast<uintptr_t>(Cmd.PerShaderCB[1]) >> 9) ^
            (reinterpret_cast<uintptr_t>(Cmd.DiffuseSRV) >> 14) ^
            (reinterpret_cast<uintptr_t>(Cmd.NormalSRV) >> 19) ^
            (reinterpret_cast<uintptr_t>(Cmd.SpecularSRV) >> 24);
        uint32 GeneralBindingHash = 0;
        for (const FShaderResourceBinding& Binding : Cmd.ShaderResourceBindings)
        {
            GeneralBindingHash ^= (Binding.SlotIndex * 131u);
            GeneralBindingHash ^= static_cast<uint32>((reinterpret_cast<uintptr_t>(Binding.SRV) >> (Binding.SlotIndex & 15u)) & 0xFFFFu);
        }
        
        const uint8 FinalUserBits = static_cast<uint8>(UserBits & 0xFFu);
        const uint32 FinalMaterialHash = static_cast<uint32>((MaterialHash ^ GeneralBindingHash) & 0xFFFFu);

        Cmd.SortKey = FDrawCommand::BuildSortKey(
            Pass,
            FinalUserBits,
            Cmd.Shader,
            Cmd.MeshBuffer,
            FinalMaterialHash);
    };

    if (!Proxy.SectionRenderData.empty())
    {
        for (const FMeshSectionRenderData& S : Proxy.SectionRenderData)
        {
            AddSection(
                S.FirstIndex,
                S.IndexCount,
                S.DiffuseSRV,
                S.NormalSRV,
                S.SpecularSRV,
                S.MaterialCB[0],
                S.MaterialCB[1],
                S.Blend,
                S.DepthStencil,
                S.Rasterizer,
				S.MeshBuffer,
                S.Shader,
                &S.ShaderResourceBindings,
                &S.MaterialPath);
        }
    }
    else
    {
        AddSection(
            0,
            Proxy.MeshBuffer->GetIndexCount(),
            Proxy.DiffuseSRV,
            Proxy.NormalSRV,
            Proxy.SpecularSRV,
            Proxy.MaterialCB[0],
            Proxy.MaterialCB[1],
            Proxy.Blend,
            Proxy.DepthStencil,
            Proxy.Rasterizer);
    }
}


void DrawCommandBuild::BuildFullscreenDrawCommand(ERenderPass Pass, FRenderPipelineContext& Context, FDrawCommandList& OutList, EViewModePostProcessVariant PostProcessVariant)
{
    const FViewportRenderTargets* Targets = Context.Targets;
    FGraphicsProgram*             Shader  = nullptr;

    if (Pass == ERenderPass::FXAA)
    {
        Shader = FShaderManager::Get().GetShader(EShaderType::FXAA);
    }
    else if (Pass == ERenderPass::FinalPostProcess)
    {
        Shader = FShaderManager::Get().GetShader(EShaderType::FinalPostProcessComposite);
    }
    else if (Pass == ERenderPass::PostProcess)
    {
        switch (PostProcessVariant)
        {
        case EViewModePostProcessVariant::Outline:
            Shader = FShaderManager::Get().GetShader(EShaderType::OutlinePostProcess);
            break;
        case EViewModePostProcessVariant::SceneDepth:
            Shader = FShaderManager::Get().GetShader(EShaderType::SceneDepth);
            break;
        case EViewModePostProcessVariant::WorldNormal:
            Shader = FShaderManager::Get().GetShader(EShaderType::NormalView);
            break;
        case EViewModePostProcessVariant::LightHitMap:
            Shader = FShaderManager::Get().GetShader(EShaderType::LightHitMap);
            break;
        default:
            Shader = FShaderManager::Get().GetShader(EShaderType::HeightFog);
            break;
        }
    }

    if (!Shader)
        return;

    const FRenderPassDrawPreset& S   = Context.GetRenderPassDrawPreset(Pass);
    FDrawCommand&                Cmd = OutList.AddCommand();
    Cmd.Shader                       = Shader;
    Cmd.DepthStencil                 = S.DepthStencil;
    Cmd.Blend                        = S.Blend;
    Cmd.Rasterizer                   = S.Rasterizer;
    Cmd.Topology                     = S.Topology;
    Cmd.VertexCount                  = 3;
    Cmd.Pass                         = Pass;

    if (Pass == ERenderPass::FXAA && Context.SceneView)
    {
        // FXAA prepares SceneColor on t0 before submission; keep the command in sync
        // so SubmitCommand does not overwrite it with nullptr on a forced bind.
        Cmd.DiffuseSRV = Targets ? Targets->SceneColorCopySRV : nullptr;
    }
    else if (Pass == ERenderPass::FinalPostProcess)
    {
        Cmd.DiffuseSRV = Targets ? Targets->SceneColorCopySRV : nullptr;
    }
    else if (Pass == ERenderPass::PostProcess)
    {
        // PostProcess is kept for fog and view-mode debug fullscreen variants.
        // Final color adjustment lives in FinalPostProcess so it does not share this bucket.
    }

    auto GetPtrHash = [](const void* Ptr) -> uint32
    {
        uintptr_t Val = reinterpret_cast<uintptr_t>(Ptr);
        return static_cast<uint32>((Val >> 4) ^ (Val >> 20));
    };

    Cmd.LightCB       = nullptr;
    Cmd.LocalLightSRV = nullptr;
    Cmd.SortKey       = FDrawCommand::BuildSortKey(Pass, static_cast<uint8>(ToPostProcessUserBits(PostProcessVariant)), Shader, nullptr, GetPtrHash(Cmd.DiffuseSRV));
}


void DrawCommandBuild::BuildLineDrawCommand(FRenderPipelineContext& Context, FDrawCommandList& OutList)
{
    if (!Context.Renderer || !Context.Scene || !Context.SceneView)
    {
        return;
    }

    FLineBatch& EditorLines = Context.Renderer->GetEditorLineBatch();
    FLineBatch& GridLines   = Context.Renderer->GetGridLineBatch();
    EditorLines.Clear();
    GridLines.Clear();

    const FCollectedOverlayData* OverlayData = Context.Submission.OverlayData;

    if (OverlayData && OverlayData->HasGrid())
    {
        GridLines.AddWorldHelpers(
            Context.SceneView->ShowFlags,
            OverlayData->GetGridSpacing(),
            OverlayData->GetGridHalfLineCount(),
            Context.SceneView->CameraPosition,
            Context.SceneView->CameraForward,
            Context.SceneView->bIsOrtho);
    }

    if (OverlayData)
    {
        for (const FSceneDebugLine& Line : OverlayData->GetDebugLines())
        {
            EditorLines.AddLine(Line.Start, Line.End, Line.Color.ToVector4());
        }
    }

    if (OverlayData)
    {
        for (const FSceneDebugAABB& Box : OverlayData->GetDebugAABBs())
        {
            const FVector& Min  = Box.Min;
            const FVector& Max  = Box.Max;
            const FVector  V[8] = {
                FVector(Min.X, Min.Y, Min.Z),
                FVector(Max.X, Min.Y, Min.Z),
                FVector(Max.X, Max.Y, Min.Z),
                FVector(Min.X, Max.Y, Min.Z),
                FVector(Min.X, Min.Y, Max.Z),
                FVector(Max.X, Min.Y, Max.Z),
                FVector(Max.X, Max.Y, Max.Z),
                FVector(Min.X, Max.Y, Max.Z),
            };
            static constexpr int32 Edges[][2] = {
                { 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 }, { 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 4 }, { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 }
            };
            for (const auto& Edge : Edges)
            {
                EditorLines.AddLine(V[Edge[0]], V[Edge[1]], Box.Color.ToVector4());
            }
        }
    }

    if (const FGraphicsProgram* Shader = FShaderManager::Get().GetShader(EShaderType::Editor))
    {
        const FRenderPassDrawPreset& State = Context.GetRenderPassDrawPreset(ERenderPass::EditorLines);

        auto AddBatch = [&](FLineBatch& Batch, const char* DebugName)
        {
            if (Batch.GetIndexCount() == 0 || !Batch.UploadBuffers(Context.Context))
            {
                return;
            }

            FDrawCommand& Cmd = OutList.AddCommand();
            Cmd.Shader        = const_cast<FGraphicsProgram*>(Shader);
            Cmd.DepthStencil  = State.DepthStencil;
            Cmd.Blend         = State.Blend;
            Cmd.Rasterizer    = ERasterizerState::SolidNoCull;
            Cmd.Topology      = State.Topology;
            Cmd.RawVB         = Batch.GetVBBuffer();
            Cmd.RawVBStride   = Batch.GetVBStride();
            Cmd.RawIB         = Batch.GetIBBuffer();
            Cmd.IndexCount    = Batch.GetIndexCount();
            Cmd.Pass          = ERenderPass::EditorLines;
            Cmd.DebugName     = DebugName;
            Cmd.SortKey       = FDrawCommand::BuildSortKey(Cmd.Pass, 0, Cmd.Shader, nullptr, 0);
        };

        AddBatch(GridLines, "GridLines");
        AddBatch(EditorLines, "DebugLines");
    }
}

namespace
{
constexpr float kBoneConeHalfHeight = 0.2f;
constexpr float kBoneConeBaseRadius = 0.2f;
constexpr float kBoneConeRadialScaleRatio = 0.1f;

FStaticMeshBuffer* GetBonePyramidMeshBuffer(ID3D11Device* Device)
{
    static FStaticMeshBuffer PyramidBuffer;
    if (PyramidBuffer.IsValid())
    {
        return &PyramidBuffer;
    }

    if (!Device)
    {
        return nullptr;
    }

    const float H = kBoneConeHalfHeight;
    const float R = kBoneConeBaseRadius;
    const FVector4 White(1.0f, 1.0f, 1.0f, 1.0f);

    TMeshData<FVertexPNCT_T> MeshData;
    MeshData.Vertices = {
        { FVector(-R, -R, -H), FVector(0, 0, -1), White, FVector2(0, 0), FVector4(1, 0, 0, 1) },
        { FVector( R, -R, -H), FVector(0, 0, -1), White, FVector2(1, 0), FVector4(1, 0, 0, 1) },
        { FVector( R,  R, -H), FVector(0, 0, -1), White, FVector2(1, 1), FVector4(1, 0, 0, 1) },
        { FVector(-R,  R, -H), FVector(0, 0, -1), White, FVector2(0, 1), FVector4(1, 0, 0, 1) },
        { FVector(0, 0, H), FVector(0, 0, 1), White, FVector2(0.5f, 0.5f), FVector4(1, 0, 0, 1) },
    };
    MeshData.Indices = {
        0, 2, 1,
        0, 3, 2,
        0, 1, 4,
        1, 2, 4,
        2, 3, 4,
        3, 0, 4,
    };

    PyramidBuffer.Create(Device, BuildRuntimePackedMeshData(MeshData, FVertexSemanticDescriptorPreset::PNCTT));
    return PyramidBuffer.IsValid() ? &PyramidBuffer : nullptr;
}

// Build a row-vector world matrix that maps the cone's local +Z onto the Tail->Head world direction
FMatrix MakeBoneConeWorld(const FVector& Head, const FVector& Tail)
{
    const FVector Delta  = Tail - Head;
    const float   Length = Delta.Length();
    if (Length <= 1e-6f)
    {
        return FMatrix::MakeTranslationMatrix(Head);
    }

    const FVector Forward = Delta / Length;

    // Pick a stable reference axis to derive the perpendicular basis.
    FVector RefUp = FVector(0.0f, 0.0f, 1.0f);
    if (fabsf(Forward.Z) > 0.999f)
    {
        RefUp = FVector(1.0f, 0.0f, 0.0f);
    }

    FVector Right = RefUp.Cross(Forward);
    Right.Normalize();
    FVector Up = Forward.Cross(Right);

    const float ScaleZ = 0.15f * Length / (2.0f * kBoneConeHalfHeight);
    const float RadiusScale = 0.15f * (Length * kBoneConeRadialScaleRatio) / kBoneConeBaseRadius;

    // Local origin sits at the cone's midpoint, so translate to (Head + Tail) / 2.
    FMatrix Result;
    Result.M[0][0] = Right.X * RadiusScale;
    Result.M[0][1] = Right.Y * RadiusScale;
    Result.M[0][2] = Right.Z * RadiusScale;
    Result.M[0][3] = 0.0f;

    Result.M[1][0] = Up.X * RadiusScale;
    Result.M[1][1] = Up.Y * RadiusScale;
    Result.M[1][2] = Up.Z * RadiusScale;
    Result.M[1][3] = 0.0f;

    Result.M[2][0] = Forward.X * ScaleZ;
    Result.M[2][1] = Forward.Y * ScaleZ;
    Result.M[2][2] = Forward.Z * ScaleZ;
    Result.M[2][3] = 0.0f;

    const FVector Mid = (Head + Tail) * 0.5f;
    Result.M[3][0] = Mid.X;
    Result.M[3][1] = Mid.Y;
    Result.M[3][2] = Mid.Z;
    Result.M[3][3] = 1.0f;
    return Result;
}
} // namespace

void DrawCommandBuild::BuildSkeletalDebugDrawCommand(FRenderPipelineContext& Context, FDrawCommandList& OutList)
{
    const FCollectedOverlayData* OverlayData = Context.Submission.OverlayData;
    if (!OverlayData)
    {
        return;
    }

    FGraphicsProgram* LineShader = FShaderManager::Get().GetShader(EShaderType::Editor);
    FGraphicsProgram* ConeShader = FShaderManager::Get().GetShader(EShaderType::SkeletalDebug);
    FStaticMeshBuffer* ConeMesh = GetBonePyramidMeshBuffer(Context.Device ? Context.Device->GetDevice() : nullptr);
    if (!LineShader || !ConeShader || !ConeMesh || !ConeMesh->IsValid())
    {
        return;
    }

    FLineBatch& SkeletonLines = Context.Renderer->GetSkeletonLineBatch();
    SkeletonLines.Clear();

    const auto& SkeletalDebugInstances = OverlayData->GetSkeletalDebugInstances();

    // Pre-reserve the bone CB pool: growing the std::vector inside the loop
    // would invalidate every FConstantBuffer* already stored in earlier draw commands.
    uint32 TotalBoneCount = 0;
    for (const auto& Instance : SkeletalDebugInstances)
    {
        TotalBoneCount += static_cast<uint32>(Instance.Bones.size());
    }
    Context.Resources->EnsurePerBoneDebugCBCapacity(
        Context.Device->GetDevice(),
        Context.Resources->BoneDebugCBCursor + TotalBoneCount);

    const FRenderPassDrawPreset& ConePreset = Context.GetRenderPassDrawPreset(ERenderPass::SkeletalDebug);

    for (const auto& Instance : SkeletalDebugInstances)
    {
        for (uint32 i = 0; i < Instance.Bones.size(); i++)
        {
            const auto& Bone = Instance.Bones[i];
            if (!Bone.bDraw)
            {
                continue;
            }

            const FVector BonePos = Bone.WorldMatrix.GetLocation();

            FMatrix ConeWorld;
            if (Bone.ParentIndex != -1 && static_cast<uint32>(Bone.ParentIndex) < Instance.Bones.size())
            {
                const FSkeletalDebugBone& ParentBone = Instance.Bones[Bone.ParentIndex];
                if (!ParentBone.bDraw)
                {
                    continue;
                }

                const FVector ParentPos = ParentBone.WorldMatrix.GetLocation();
                ConeWorld = MakeBoneConeWorld(ParentPos, BonePos);

                SkeletonLines.AddLine(ParentPos, BonePos, Bone.Color.ToVector4(), Bone.Color.ToVector4());
            }
            else
            {
                // Root bone
                ConeWorld = FMatrix::MakeScaleMatrix(FVector(1, 1, 1)) * Bone.WorldMatrix;
            }

            FPerObjectCBData PerObject = FPerObjectCBData::FromWorldMatrix(ConeWorld);
            PerObject.Color            = Bone.Color.ToVector4();

            FConstantBuffer* CB = Context.Resources->AcquirePerBoneDebugCB(Context.Device->GetDevice());
            CB->Update(Context.Context, &PerObject, sizeof(FPerObjectCBData));

            FDrawCommand& Cmd = OutList.AddCommand();
            Cmd.Shader        = ConeShader;
            Cmd.MeshBuffer    = ConeMesh;
            Cmd.FirstIndex    = 0;
            Cmd.IndexCount    = ConeMesh->GetIndexCount();
            Cmd.DepthStencil  = ConePreset.DepthStencil;
            Cmd.Blend         = ConePreset.Blend;
            Cmd.Rasterizer    = ConePreset.Rasterizer;
            Cmd.Topology      = ConePreset.Topology;
            Cmd.PerObjectCB   = CB;
            Cmd.Pass          = ERenderPass::SkeletalDebug;
            Cmd.DebugName     = "SkeletalDebugCone";
            Cmd.SortKey       = FDrawCommand::BuildSortKey(Cmd.Pass, 0, Cmd.Shader, Cmd.MeshBuffer, 0);
        }
    }

    // Skeleton line batch shares the SkeletalDebug pass, but uses a different
    // shader (Editor.hlsl, VS_Input_PC) and the EditorLines preset for line topology.
    if (SkeletonLines.GetIndexCount() > 0 && SkeletonLines.UploadBuffers(Context.Context))
    {
        const FRenderPassDrawPreset& LinePreset = Context.GetRenderPassDrawPreset(ERenderPass::EditorLines);

        FDrawCommand& Cmd = OutList.AddCommand();
        Cmd.Shader        = LineShader;
        Cmd.DepthStencil  = EDepthStencilState::NoDepth;
        Cmd.Blend         = LinePreset.Blend;
        Cmd.Rasterizer    = ERasterizerState::SolidNoCull;
        Cmd.Topology      = LinePreset.Topology;
        Cmd.RawVB         = SkeletonLines.GetVBBuffer();
        Cmd.RawVBStride   = SkeletonLines.GetVBStride();
        Cmd.RawIB         = SkeletonLines.GetIBBuffer();
        Cmd.IndexCount    = SkeletonLines.GetIndexCount();
        Cmd.Pass          = ERenderPass::SkeletalDebug;
        Cmd.DebugName     = "SkeletalDebugLines";
        Cmd.SortKey       = FDrawCommand::BuildSortKey(Cmd.Pass, 0, Cmd.Shader, nullptr, 0);
    }
}

void DrawCommandBuild::BuildOverlayBillboardDrawCommand(FRenderPipelineContext& Context, FDrawCommandList& OutList)
{
    const FCollectedOverlayData* OverlayData = Context.Submission.OverlayData;
    if (!OverlayData)
    {
        return;
    }

    for (FPrimitiveProxy* Proxy : OverlayData->GetEditorHelperBillboards())
    {
        if (!Proxy)
        {
            continue;
        }

        BuildMeshDrawCommand(*Proxy, ERenderPass::OverlayBillboard, Context, OutList);
    }
}

void DrawCommandBuild::BuildOverlayTextDrawCommand(FRenderPipelineContext& Context, FDrawCommandList& OutList)
{
    if (!Context.Renderer || !Context.SceneView)
    {
        return;
    }

    const FCollectedOverlayData* OverlayData = Context.Submission.OverlayData;
    const FCollectedSceneData*   SceneData   = Context.Submission.SceneData;

    if (OverlayData)
    {
        struct FTextRange
        {
            uint32 StartIndex = 0;
            uint32 IndexCount = 0;
            ID3D11ShaderResourceView* FontSRV = nullptr;
        };

        std::vector<FTextRange> OverlayWorldRanges;
        OverlayWorldRanges.reserve(OverlayData->GetEditorHelperTexts().size());

        FFontBatch& FontBatch = Context.Renderer->GetTextBatch();
        FontBatch.ClearOverlayWorld();

        for (FPrimitiveProxy* Proxy : OverlayData->GetEditorHelperTexts())
        {
            if (!Proxy)
            {
                continue;
            }

            const FTextRenderSceneProxy* TextProxy = static_cast<const FTextRenderSceneProxy*>(Proxy);
            if (!TextProxy->CachedText.empty())
            {
                const UTextRenderComponent* TextComp = static_cast<const UTextRenderComponent*>(TextProxy->Owner);

                const FFontResource* FontRes = TextComp ? TextComp->GetFont() : nullptr;
                if (!FontRes || !FontRes->IsLoaded())
                {
                    FontRes = FFontManager::Get().FindFont(FName("Default"));
                }
                if (!FontRes || !FontRes->IsLoaded())
                {
                    continue;
                }

                const FVector WorldScale = TextComp ? TextComp->GetWorldScale() : FVector(1.0f, 1.0f, 1.0f);
                FontBatch.EnsureCharInfoMap(FontRes);

                const uint32 StartIndex = FontBatch.GetOverlayWorldIndexCount();
                FontBatch.AddOverlayWorldText(
                    TextProxy->CachedText,
                    TextComp ? TextComp->GetWorldLocation() : TextProxy->CachedWorldPos,
                    TextProxy->CachedTextRight,
                    TextProxy->CachedTextUp,
                    WorldScale,
                    TextProxy->CachedFontScale);

                const uint32 EndIndex = FontBatch.GetOverlayWorldIndexCount();
                if (EndIndex > StartIndex)
                {
                    OverlayWorldRanges.push_back({ StartIndex, EndIndex - StartIndex, FontRes->SRV });
                }
            }
        }

        if (!OverlayWorldRanges.empty() && FontBatch.UploadOverlayWorldBuffers(Context.Context))
        {
            FGraphicsProgram* Shader = FShaderManager::Get().GetShader(EShaderType::Font);
            if (Shader)
            {
                auto GetPtrHash = [](const void* Ptr) -> uint32
                {
                    uintptr_t Val = reinterpret_cast<uintptr_t>(Ptr);
                    return static_cast<uint32>((Val >> 4) ^ (Val >> 20));
                };

                const FRenderPassDrawPreset& State = Context.GetRenderPassDrawPreset(ERenderPass::OverlayTextWorld);

                for (const FTextRange& Range : OverlayWorldRanges)
                {
                    FDrawCommand& Cmd = OutList.AddCommand();
                    Cmd.Shader        = Shader;
                    Cmd.DepthStencil  = State.DepthStencil;
                    Cmd.Blend         = State.Blend;
                    Cmd.Rasterizer    = Context.ViewMode.ActiveViewMode == EViewMode::Wireframe ? ERasterizerState::WireFrame : ERasterizerState::SolidNoCull;
                    Cmd.Topology      = State.Topology;
                    Cmd.RawVB         = FontBatch.GetOverlayWorldVBBuffer();
                    Cmd.RawVBStride   = FontBatch.GetOverlayWorldVBStride();
                    Cmd.RawIB         = FontBatch.GetOverlayWorldIBBuffer();
                    Cmd.FirstIndex    = Range.StartIndex;
                    Cmd.IndexCount    = Range.IndexCount;
                    Cmd.DiffuseSRV    = Range.FontSRV;
                    Cmd.Pass          = ERenderPass::OverlayTextWorld;
                    Cmd.DebugName     = "OverlayWorldText";
                    Cmd.SortKey       = FDrawCommand::BuildSortKey(Cmd.Pass, 0, Cmd.Shader, nullptr, GetPtrHash(Cmd.DiffuseSRV));
                }
            }
        }
    }

    if (!SceneData)
    {
        return;
    }

    const FFontResource* FontRes = FFontManager::Get().FindFont(FName("Default"));
    if (!FontRes || !FontRes->IsLoaded())
    {
        return;
    }

    FFontBatch& FontBatch = Context.Renderer->GetTextBatch();
    FontBatch.EnsureCharInfoMap(FontRes);
    FontBatch.ClearScreen();

    for (const FSceneOverlayText& Text : SceneData->Primitives.OverlayTexts)
    {
        if (!Text.Text.empty())
        {
            FontBatch.AddScreenText(
                Text.Text,
                Text.Position.X,
                Text.Position.Y,
                Context.SceneView->ViewportWidth,
                Context.SceneView->ViewportHeight,
                Text.Scale);
        }
    }

    if (FontBatch.GetScreenIndexCount() == 0 || !FontBatch.UploadScreenBuffers(Context.Context))
    {
        return;
    }

    FGraphicsProgram* Shader = FShaderManager::Get().GetShader(EShaderType::OverlayFont);
    if (!Shader)
    {
        return;
    }

    auto GetPtrHash = [](const void* Ptr) -> uint32
    {
        uintptr_t Val = reinterpret_cast<uintptr_t>(Ptr);
        return static_cast<uint32>((Val >> 4) ^ (Val >> 20));
    };

    const FRenderPassDrawPreset& State = Context.GetRenderPassDrawPreset(ERenderPass::OverlayFont);
    FDrawCommand&                Cmd   = OutList.AddCommand();
    Cmd.Shader                         = Shader;
    Cmd.DepthStencil                   = State.DepthStencil;
    Cmd.Blend                          = State.Blend;
    Cmd.Rasterizer                     = Context.ViewMode.ActiveViewMode == EViewMode::Wireframe ? ERasterizerState::WireFrame : ERasterizerState::SolidNoCull;
    Cmd.Topology                       = State.Topology;
    Cmd.RawVB                          = FontBatch.GetScreenVBBuffer();
    Cmd.RawVBStride                    = FontBatch.GetScreenVBStride();
    Cmd.RawIB                          = FontBatch.GetScreenIBBuffer();
    Cmd.IndexCount                     = FontBatch.GetScreenIndexCount();
    Cmd.DiffuseSRV                     = FontRes->SRV;
    Cmd.Pass                           = ERenderPass::OverlayFont;
    Cmd.DebugName                      = "OverlayText";
    Cmd.SortKey                        = FDrawCommand::BuildSortKey(Cmd.Pass, 0, Cmd.Shader, nullptr, GetPtrHash(Cmd.DiffuseSRV));
}

void DrawCommandBuild::BuildBatchedWorldTextDrawCommands(FRenderPipelineContext& Context, FDrawCommandList& OutList)
{
    if (!Context.Renderer || !Context.SceneView || !Context.Submission.SceneData)
    {
        return;
    }

    FFontBatch& FontBatch = Context.Renderer->GetTextBatch();
    FontBatch.ClearWorld();

    struct FTextRange
    {
        uint32 StartIndex = 0;
        uint32 IndexCount = 0;
        ID3D11ShaderResourceView* FontSRV = nullptr;
    };

    std::vector<FTextRange> WorldRanges;
    WorldRanges.reserve(Context.Submission.SceneData->Primitives.VisibleProxies.size());

    for (FPrimitiveProxy* Proxy : Context.Submission.SceneData->Primitives.VisibleProxies)
    {
        if (!Proxy || !Proxy->bFontBatched)
        {
            continue;
        }

        const FTextRenderSceneProxy* TextProxy = static_cast<const FTextRenderSceneProxy*>(Proxy);
        if (TextProxy->CachedText.empty())
        {
            continue;
        }

        const UTextRenderComponent* TextComp = static_cast<const UTextRenderComponent*>(TextProxy->Owner);

        const FFontResource* FontRes = TextComp ? TextComp->GetFont() : nullptr;
        if (!FontRes || !FontRes->IsLoaded())
        {
            FontRes = FFontManager::Get().FindFont(FName("Default"));
        }
        if (!FontRes || !FontRes->IsLoaded())
        {
            continue;
        }

        const FVector WorldScale = TextComp ? TextComp->GetWorldScale() : FVector(1.0f, 1.0f, 1.0f);
        FontBatch.EnsureCharInfoMap(FontRes);

        const uint32 StartIndex = FontBatch.GetWorldIndexCount();
        FontBatch.AddWorldText(
            TextProxy->CachedText,
            TextComp ? TextComp->GetWorldLocation() : TextProxy->CachedWorldPos,
            TextProxy->CachedTextRight,
            TextProxy->CachedTextUp,
            WorldScale,
            TextProxy->CachedFontScale);

        const uint32 EndIndex = FontBatch.GetWorldIndexCount();
        if (EndIndex > StartIndex)
        {
            WorldRanges.push_back({ StartIndex, EndIndex - StartIndex, FontRes->SRV });
        }
    }

    if (WorldRanges.empty() || !FontBatch.UploadWorldBuffers(Context.Context))
    {
        return;
    }

    FGraphicsProgram* Shader = FShaderManager::Get().GetShader(EShaderType::Font);
    if (!Shader)
    {
        return;
    }

    auto GetPtrHash = [](const void* Ptr) -> uint32
    {
        uintptr_t Val = reinterpret_cast<uintptr_t>(Ptr);
        return static_cast<uint32>((Val >> 4) ^ (Val >> 20));
    };

    const FRenderPassDrawPreset& State = Context.GetRenderPassDrawPreset(ERenderPass::AlphaBlend);
    for (const FTextRange& Range : WorldRanges)
    {
        FDrawCommand& Cmd = OutList.AddCommand();
        Cmd.Shader        = Shader;
        Cmd.DepthStencil  = State.DepthStencil;
        Cmd.Blend         = State.Blend;
        Cmd.Rasterizer    = Context.ViewMode.ActiveViewMode == EViewMode::Wireframe ? ERasterizerState::WireFrame : ERasterizerState::SolidNoCull;
        Cmd.Topology      = State.Topology;
        Cmd.RawVB         = FontBatch.GetWorldVBBuffer();
        Cmd.RawVBStride   = FontBatch.GetWorldVBStride();
        Cmd.RawIB         = FontBatch.GetWorldIBBuffer();
        Cmd.FirstIndex    = Range.StartIndex;
        Cmd.IndexCount    = Range.IndexCount;
        Cmd.DiffuseSRV    = Range.FontSRV;
        Cmd.Pass          = ERenderPass::AlphaBlend;
        Cmd.DebugName     = "WorldText";
        Cmd.SortKey       = FDrawCommand::BuildSortKey(Cmd.Pass, 0, Cmd.Shader, nullptr, GetPtrHash(Cmd.DiffuseSRV));
    }
}

void DrawCommandBuild::BuildUIDrawCommands(FRenderPipelineContext& Context, FDrawCommandList& OutList)
{
    if (!Context.Scene || !Context.SceneView || !Context.Resources || !Context.Context)
    {
        return;
    }

    if (!Context.SceneView->ShowFlags.bUI)
    {
        return;
    }

    struct FSortedUIProxy
    {
        const FUIProxy* Proxy = nullptr;
    };

    TArray<FSortedUIProxy> SortedProxies;
    for (const FUIProxy* Proxy : Context.Scene->GetUIProxies())
    {
        if (!Proxy ||
            !Proxy->bVisible)
        {
            continue;
        }

        if (Proxy->ElementType == EUIElementType::Image)
        {
            if (Proxy->GeometryType != EUIGeometryType::Quad)
            {
                continue;
            }
        }
        else if (Proxy->ElementType == EUIElementType::Text)
        {
            if (Proxy->Text.empty() || !Proxy->FontResource || !Proxy->FontResource->IsLoaded())
            {
                continue;
            }
        }
        else
        {
            continue;
        }

        SortedProxies.push_back({ Proxy });
    }

    if (SortedProxies.empty())
    {
        return;
    }

    std::sort(
        SortedProxies.begin(),
        SortedProxies.end(),
        [](const FSortedUIProxy& A, const FSortedUIProxy& B)
        {
            if (A.Proxy->RenderSpace != B.Proxy->RenderSpace)
            {
                return A.Proxy->RenderSpace == EUIRenderSpace::WorldSpace;
            }
            if (A.Proxy->Layer != B.Proxy->Layer)
            {
                return A.Proxy->Layer < B.Proxy->Layer;
            }
            if (A.Proxy->ZOrder != B.Proxy->ZOrder)
            {
                return A.Proxy->ZOrder < B.Proxy->ZOrder;
            }
            return A.Proxy->ProxyId < B.Proxy->ProxyId;
        });

    FUIBatch& UIBatch = Context.Resources->UIBatch;
    UIBatch.Clear();

    struct FUICommandRange
    {
        const FUIProxy* Proxy = nullptr;
        FUIBatchRange Range;
    };

    TArray<FUICommandRange> Ranges;
    Ranges.reserve(SortedProxies.size());

    for (const FSortedUIProxy& Entry : SortedProxies)
    {
        FUIBatchRange Range = {};
        if (Entry.Proxy->ElementType == EUIElementType::Text)
        {
            if (Entry.Proxy->RenderSpace == EUIRenderSpace::WorldSpace)
            {
                Range = UIBatch.AddWorldText(
                    *Entry.Proxy,
                    Entry.Proxy->FontResource,
                    Context.SceneView->CameraRight,
                    Context.SceneView->CameraUp);
            }
            else
            {
                Range = UIBatch.AddScreenText(
                    *Entry.Proxy,
                    Entry.Proxy->FontResource,
                    Context.SceneView->ViewportWidth,
                    Context.SceneView->ViewportHeight);
            }
        }
        else
        {
            if (Entry.Proxy->RenderSpace == EUIRenderSpace::WorldSpace)
            {
                Range = UIBatch.AddWorldQuad(
                    *Entry.Proxy,
                    Context.SceneView->CameraRight,
                    Context.SceneView->CameraUp);
            }
            else
            {
                Range = UIBatch.AddScreenQuad(
                    *Entry.Proxy,
                    Context.SceneView->ViewportWidth,
                    Context.SceneView->ViewportHeight);
            }
        }

        if (Range.IndexCount > 0)
        {
            Ranges.push_back({ Entry.Proxy, Range });
        }
    }

    if (Ranges.empty() || !UIBatch.UploadBuffers(Context.Context))
    {
        return;
    }

    UIBatch.UpdateParams(Context.Context);

    FGraphicsProgram* Shader = FShaderManager::Get().GetShader(EShaderType::UI);
    if (!Shader)
    {
        return;
    }

    const FRenderPassDrawPreset& State = Context.GetRenderPassDrawPreset(ERenderPass::UI);

    auto BuildUIOrderSortKey = [](uint32 Order) -> uint64
    {
        return (static_cast<uint64>(ERenderPass::UI) << 56) |
               (static_cast<uint64>(Order) & 0x00FFFFFFFFFFFFFFull);
    };

    uint32 Order = 0;
    for (const FUICommandRange& Entry : Ranges)
    {
        FDrawCommand& Cmd = OutList.AddCommand();
        const bool bWorldSpace = Entry.Proxy->RenderSpace == EUIRenderSpace::WorldSpace;
        Cmd.Shader        = Shader;
        Cmd.DepthStencil  = bWorldSpace ? EDepthStencilState::NoDepth : State.DepthStencil;
        Cmd.Blend         = State.Blend;
        Cmd.Rasterizer    = State.Rasterizer;
        Cmd.Topology      = State.Topology;
        Cmd.RawVB         = UIBatch.GetVBBuffer();
        Cmd.RawVBStride   = UIBatch.GetVBStride();
        Cmd.RawIB         = UIBatch.GetIBBuffer();
        Cmd.FirstIndex    = Entry.Range.FirstIndex;
        Cmd.IndexCount    = Entry.Range.IndexCount;
        Cmd.DiffuseSRV    = Entry.Proxy->bUseTexture ? Entry.Proxy->TextureSRV : nullptr;
        Cmd.bForceSRVBind = Entry.Proxy->ElementType == EUIElementType::Image && !Entry.Proxy->bUseTexture;
        if (Entry.Proxy->ElementType == EUIElementType::Text)
        {
            Cmd.PerShaderCB[0] = bWorldSpace ? UIBatch.GetWorldFontParamsCB() : UIBatch.GetFontParamsCB();
        }
        else
        {
            Cmd.PerShaderCB[0] = Entry.Proxy->bUseTexture
                ? (bWorldSpace ? UIBatch.GetWorldTextureParamsCB() : UIBatch.GetTextureParamsCB())
                : (bWorldSpace ? UIBatch.GetWorldSolidParamsCB() : UIBatch.GetSolidParamsCB());
        }
        Cmd.Pass      = ERenderPass::UI;
        Cmd.DebugName = Entry.Proxy->ElementType == EUIElementType::Text
            ? (bWorldSpace ? "WorldUIText" : "UIText")
            : (bWorldSpace ? "WorldUI" : "UI");
        Cmd.SortKey   = BuildUIOrderSortKey(Order++);
    }
}

