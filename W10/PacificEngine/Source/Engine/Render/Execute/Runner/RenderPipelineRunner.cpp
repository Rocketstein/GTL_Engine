// 렌더 영역의 세부 동작을 구현합니다.
#include "Render/Execute/Runner/RenderPipelineRunner.h"

#include "Render/Execute/Context/RenderPipelineContext.h"
#include "Render/Execute/Registry/RenderPassRegistry.h"
#include "Render/Execute/Registry/RenderPipelineRegistry.h"
#include "Render/Execute/Registry/ViewModePassRegistry.h"
#include "Render/Execute/Runner/RenderMarkers.h"

namespace
{
const wchar_t* GetRenderPassMarkerName(ERenderPassNodeType PassType)
{
    switch (PassType)
    {
    case ERenderPassNodeType::GridPass:
        return L"GridPass";
    case ERenderPassNodeType::DepthPrePass:
        return L"DepthPrePass";
    case ERenderPassNodeType::ShadowMapPass:
        return L"ShadowMapPass";
    case ERenderPassNodeType::ForwardOpaquePass:
        return L"ForwardOpaquePass";
    case ERenderPassNodeType::AdditiveDecalPass:
        return L"AdditiveDecalPass";
    case ERenderPassNodeType::AlphaBlendPass:
        return L"AlphaBlendPass";
    case ERenderPassNodeType::NonLitViewModePass:
        return L"NonLitViewModePass";
    case ERenderPassNodeType::SelectionMaskPass:
        return L"SelectionMaskPass";
    case ERenderPassNodeType::OutlinePass:
        return L"OutlinePass";
    case ERenderPassNodeType::DebugLinePass:
        return L"DebugLinePass";
    case ERenderPassNodeType::OverlayBillboardPass:
        return L"OverlayBillboardPass";
    case ERenderPassNodeType::GizmoPass:
        return L"GizmoPass";
    case ERenderPassNodeType::OverlayTextPass:
        return L"OverlayTextPass";
    case ERenderPassNodeType::HeightFogPass:
        return L"HeightFogPass";
    case ERenderPassNodeType::FXAAPass:
        return L"FXAAPass";
    case ERenderPassNodeType::FinalPostProcessCompositePass:
        return L"FinalPostProcessCompositePass";
    case ERenderPassNodeType::UIPass:
        return L"UIPass";
    case ERenderPassNodeType::PresentPass:
        return L"PresentPass";
    case ERenderPassNodeType::LightHitMapPass:
        return L"LightHitMapPass";
    default:
        return L"RenderPass";
    }
}

bool ShouldExecutePass(const FRenderPipelineContext& Context, ERenderPassNodeType PassType)
{
    const FViewModePassRegistry* Registry = Context.ViewMode.Registry;
    if (!Registry || !Registry->HasConfig(Context.ViewMode.ActiveViewMode))
    {
        return true;
    }

    switch (PassType)
    {
    case ERenderPassNodeType::DepthPrePass:
        return Registry->UsesDepthPrePass(Context.ViewMode.ActiveViewMode);
    case ERenderPassNodeType::ForwardOpaquePass:
        return Registry->UsesOpaque(Context.ViewMode.ActiveViewMode);
    case ERenderPassNodeType::AdditiveDecalPass:
        return Registry->UsesAdditiveDecal(Context.ViewMode.ActiveViewMode);
    case ERenderPassNodeType::AlphaBlendPass:
        return Registry->UsesAlphaBlend(Context.ViewMode.ActiveViewMode);
    case ERenderPassNodeType::NonLitViewModePass:
        return Registry->UsesNonLitViewMode(Context.ViewMode.ActiveViewMode);
    case ERenderPassNodeType::HeightFogPass:
        return Registry->UsesHeightFog(Context.ViewMode.ActiveViewMode);
    case ERenderPassNodeType::FXAAPass:
        return Registry->UsesFXAA(Context.ViewMode.ActiveViewMode);
    case ERenderPassNodeType::FinalPostProcessCompositePass:
        return Context.SceneView &&
               ((Context.SceneView->ShowFlags.bVignetting && Context.SceneView->PostProcessSettings.Vignetting.bEnabled) ||
                (Context.SceneView->ShowFlags.bGammaCorrection && Context.SceneView->PostProcessSettings.GammaCorrection.bEnabled) ||
                Context.SceneView->PostProcessSettings.Fade.bEnabled ||
                Context.SceneView->PostProcessSettings.Letterbox.bEnabled);
    default:
        return true;
    }
}

bool ShouldExecutePipeline(const FRenderPipelineContext& Context, ERenderPipelineType PipelineType)
{
    const FViewModePassRegistry* Registry = Context.ViewMode.Registry;
    if (!Registry || !Registry->HasConfig(Context.ViewMode.ActiveViewMode))
    {
        return true;
    }

    const bool bUsesLighting = Registry->UsesLightingPass(Context.ViewMode.ActiveViewMode);

    switch (PipelineType)
    {
    case ERenderPipelineType::ForwardPipeline:
        return true;
    case ERenderPipelineType::ForwardLitPipeline:
        return bUsesLighting;
    case ERenderPipelineType::ForwardUnlitPipeline:
        return Context.ViewMode.ActiveViewMode == EViewMode::Unlit || Context.ViewMode.ActiveViewMode == EViewMode::Wireframe;
    case ERenderPipelineType::ForwardWorldNormalPipeline:
        return Context.ViewMode.ActiveViewMode == EViewMode::WorldNormal;
    case ERenderPipelineType::ForwardSceneDepthPipeline:
        return Context.ViewMode.ActiveViewMode == EViewMode::SceneDepth;
    default:
        return true;
    }
}
} // namespace

void FRenderPipelineRunner::ExecutePipeline(
    ERenderPipelineType            Root,
    FRenderPipelineContext&        Context,
    const FSceneView&              SceneView,
    const FRenderPipelineRegistry& PipelineRegistry,
    const FRenderPassRegistry&     PassRegistry) const
{
    ExecutePipelineRecursive(Root, Context, SceneView, PipelineRegistry, PassRegistry);
}

void FRenderPipelineRunner::ExecutePipelineRecursive(
    ERenderPipelineType            Type,
    FRenderPipelineContext&        Context,
    const FSceneView&              SceneView,
    const FRenderPipelineRegistry& PipelineRegistry,
    const FRenderPassRegistry&     PassRegistry) const
{
    const FRenderPipelineDesc* Desc = PipelineRegistry.FindPipeline(Type);
    if (!Desc)
    {
        return;
    }

    for (const FRenderNodeRef& Child : Desc->Children)
    {
        if (Child.Kind == ERenderNodeKind::Pipeline)
        {
            const ERenderPipelineType ChildPipelineType = (ERenderPipelineType)Child.TypeValue;
            if (ShouldExecutePipeline(Context, ChildPipelineType))
            {
                ExecutePipelineRecursive(ChildPipelineType, Context, SceneView, PipelineRegistry, PassRegistry);
            }
            continue;
        }

        const ERenderPassNodeType PassType = (ERenderPassNodeType)Child.TypeValue;
        if (!ShouldExecutePass(Context, PassType))
        {
            continue;
        }

        if (FRenderPass* Pass = PassRegistry.FindPass(PassType))
        {
#if WITH_RENDER_MARKERS
            FScopedGpuEvent Event(*Context.Renderer, GetRenderPassMarkerName(PassType));
#endif
            Pass->Execute(Context);
        }
    }
}
