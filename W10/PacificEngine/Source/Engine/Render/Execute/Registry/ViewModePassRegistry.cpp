// 렌더 영역의 세부 동작을 구현합니다.
#include "Render/Execute/Registry/ViewModePassRegistry.h"
#include "Render/Resources/Shadows/ShadowFilterSettings.h"
#include "Render/Resources/Shaders/ShaderManager.h"

namespace ViewModePassConfigUtils
{
// ========== Define Helpers ==========

void AddDefine(TArray<FShaderMacroDefine>& Defines, const char* Name, const char* Value)
{
    Defines.push_back({ Name, Value });
}

} // namespace ViewModePassConfigUtils

namespace
{
FViewModePassDesc* FindMatchingPassDesc(FViewModePassConfig& Config, ERenderPass RenderPass, ERenderShadingPath ShadingPath)
{
    for (FViewModePassDesc& Pass : Config.Passes)
    {
        if (Pass.RenderPass == RenderPass && Pass.ShadingPath == ShadingPath)
        {
            return &Pass;
        }
    }

    return nullptr;
}

void UpsertShaderDefine(TArray<FShaderMacroDefine>& Defines, const char* Name, const char* Value)
{
    for (FShaderMacroDefine& Define : Defines)
    {
        if (Define.Name == Name)
        {
            Define.Value = Value;
            return;
        }
    }

    Defines.push_back({ Name, Value });
}

void ApplyShadowFilterDefine(FViewModePassDesc& Pass)
{
    if (Pass.RenderPass != ERenderPass::Opaque)
    {
        return;
    }

    UpsertShaderDefine(
        Pass.ShaderVariant.Defines,
        "SHADOW_FILTER_METHOD",
        GetShadowFilterMethodDefineValue(GetShadowFilterMethod()));
}

FViewModePassDesc BuildViewModeForwardOpaquePassDesc(EShadingModel ShadingModel)
{
    FViewModePassDesc Pass      = {};
    Pass.RenderPass             = ERenderPass::Opaque;
    Pass.ShadingPath            = ERenderShadingPath::Forward;
    Pass.bFullscreenPass        = false;
    Pass.ShaderVariant.FilePath = "Shaders/Passes/Scene/Opaque/OpaquePass.hlsl";
    Pass.ShaderVariant.VSEntry  = "VSmain";
    Pass.ShaderVariant.PSEntry  = "PSmain";

    switch (ShadingModel)
    {
    case EShadingModel::Lambert:
        ViewModePassConfigUtils::AddDefine(Pass.ShaderVariant.Defines, "LIGHTING_MODEL_LAMBERT");
        ViewModePassConfigUtils::AddDefine(Pass.ShaderVariant.Defines, "FORWARD_ENABLE_LIGHTING");
        ViewModePassConfigUtils::AddDefine(Pass.ShaderVariant.Defines, "USE_NORMAL_MAP");
        break;
    case EShadingModel::BlinnPhong:
        ViewModePassConfigUtils::AddDefine(Pass.ShaderVariant.Defines, "LIGHTING_MODEL_BLINNPHONG");
        ViewModePassConfigUtils::AddDefine(Pass.ShaderVariant.Defines, "FORWARD_ENABLE_LIGHTING");
        ViewModePassConfigUtils::AddDefine(Pass.ShaderVariant.Defines, "USE_NORMAL_MAP");
        break;
    case EShadingModel::WorldNormal:
        ViewModePassConfigUtils::AddDefine(Pass.ShaderVariant.Defines, "LIGHTING_MODEL_WORLDNORMAL");
        ViewModePassConfigUtils::AddDefine(Pass.ShaderVariant.Defines, "USE_NORMAL_MAP");
        break;
    case EShadingModel::Unlit:
    default:
        ViewModePassConfigUtils::AddDefine(Pass.ShaderVariant.Defines, "LIGHTING_MODEL_UNLIT");
        break;
    }

    return Pass;
}

void BuildViewModePasses(FViewModePassConfig& Config)
{
    Config.Passes.clear();

    if (Config.bEnableOpaque)
    {
        Config.Passes.push_back(BuildViewModeForwardOpaquePassDesc(Config.ShadingModel));
    }
}

void InitializeViewModePassConfig(FViewModePassConfig& Config, EViewMode InViewMode, FShaderVariantCache& VariantCache)
{
    (void)VariantCache;
    Config              = {};
    Config.ViewMode     = InViewMode;
    Config.ShadingModel = GetShadingModelFromViewMode(InViewMode);

    switch (InViewMode)
    {
    case EViewMode::Wireframe:
        Config.bEnableDepthPre       = false;
        Config.bEnableOpaque         = true;
        Config.bEnableLighting       = false;
        Config.bEnableAdditiveDecal  = false;
        Config.bEnableAlphaBlend     = false;
        Config.bEnableNonLitViewMode = false;
        Config.bEnableHeightFog      = false;
        Config.bEnableFXAA           = false;
        Config.PostProcessVariant    = EViewModePostProcessVariant::None;
        break;
    case EViewMode::SceneDepth:
        Config.bEnableDepthPre       = true;
        Config.bEnableOpaque         = false;
        Config.bEnableLighting       = false;
        Config.bEnableAdditiveDecal  = false;
        Config.bEnableAlphaBlend     = false;
        Config.bEnableNonLitViewMode = true;
        Config.bEnableHeightFog      = false;
        Config.bEnableFXAA           = false;
        Config.PostProcessVariant    = EViewModePostProcessVariant::SceneDepth;
        break;
    case EViewMode::WorldNormal:
        Config.bEnableDepthPre       = true;
        Config.bEnableOpaque         = true;
        Config.bEnableLighting       = false;
        Config.bEnableAdditiveDecal  = false;
        Config.bEnableAlphaBlend     = false;
        Config.bEnableNonLitViewMode = false;
        Config.bEnableHeightFog      = false;
        Config.bEnableFXAA           = false;
        Config.PostProcessVariant    = EViewModePostProcessVariant::None;
        break;
    case EViewMode::Unlit:
        Config.bEnableDepthPre       = true;
        Config.bEnableOpaque         = true;
        Config.bEnableLighting       = false;
        Config.bEnableAdditiveDecal  = false;
        Config.bEnableAlphaBlend     = true;
        Config.bEnableNonLitViewMode = false;
        Config.bEnableHeightFog      = true;
        Config.bEnableFXAA           = true;
        Config.PostProcessVariant    = EViewModePostProcessVariant::None;
        break;
    case EViewMode::Lit_Lambert:
    case EViewMode::Lit_Phong:
    default:
        Config.bEnableDepthPre       = true;
        Config.bEnableOpaque         = true;
        Config.bEnableLighting       = true;
        Config.bEnableAdditiveDecal  = false;
        Config.bEnableAlphaBlend     = true;
        Config.bEnableNonLitViewMode = false;
        Config.bEnableHeightFog      = true;
        Config.bEnableFXAA           = true;
        Config.PostProcessVariant    = EViewModePostProcessVariant::None;
        break;
    }

    BuildViewModePasses(Config);
    for (FViewModePassDesc& Pass : Config.Passes)
    {
        Pass.CompiledShader = nullptr;
    }
}
} // namespace

// ========== Config Queries ==========

uint16 ToPostProcessUserBits(EViewModePostProcessVariant Variant)
{
    return static_cast<uint16>(Variant);
}


EShadingModel GetViewModeShadingModel(const FViewModePassConfig* Config)
{
    return Config ? Config->ShadingModel : EShadingModel::Unlit;
}

bool UsesViewModeDepthPre(const FViewModePassConfig* Config)
{
    return Config ? Config->bEnableDepthPre : false;
}

bool UsesViewModeOpaque(const FViewModePassConfig* Config)
{
    return Config ? Config->bEnableOpaque : false;
}

bool UsesViewModeLighting(const FViewModePassConfig* Config)
{
    return Config ? Config->bEnableLighting : false;
}

bool UsesViewModeAdditiveDecal(const FViewModePassConfig* Config)
{
    return Config ? Config->bEnableAdditiveDecal : false;
}

bool UsesViewModeAlphaBlend(const FViewModePassConfig* Config)
{
    return Config ? Config->bEnableAlphaBlend : false;
}

bool UsesNonLitViewMode(const FViewModePassConfig* Config)
{
    return Config ? Config->bEnableNonLitViewMode : false;
}

bool UsesViewModeHeightFog(const FViewModePassConfig* Config)
{
    return Config ? Config->bEnableHeightFog : false;
}

bool UsesViewModeFXAA(const FViewModePassConfig* Config)
{
    return Config ? Config->bEnableFXAA : false;
}

EViewModePostProcessVariant GetViewModePostProcessVariant(const FViewModePassConfig* Config)
{
    return Config ? Config->PostProcessVariant : EViewModePostProcessVariant::None;
}

// ========== Registry Lifecycle ==========

void FViewModePassRegistry::Initialize(ID3D11Device* Device)
{
    VariantCache.Initialize(Device);
    Configs.clear();

    const EViewMode Modes[] = {
        EViewMode::Lit_Lambert,
        EViewMode::Lit_Phong,
        EViewMode::Unlit,
        EViewMode::Wireframe,
        EViewMode::SceneDepth,
        EViewMode::WorldNormal,
    };

    for (EViewMode Mode : Modes)
    {
        FViewModePassConfig Config = {};
        InitializeViewModePassConfig(Config, Mode, VariantCache);
        Configs.emplace(Mode, std::move(Config));
    }
}


void FViewModePassRegistry::Release()
{
    Configs.clear();
    VariantCache.Release();
}

void FViewModePassRegistry::TickHotReload()
{
    if (FShaderManager::IsHotReloadEnabled())
    {
        VariantCache.TickHotReload();
    }
}


// ========== Registry Lookup ==========

bool FViewModePassRegistry::HasConfig(EViewMode ViewMode) const
{
    return Configs.find(ViewMode) != Configs.end();
}


const FViewModePassConfig* FViewModePassRegistry::GetConfig(EViewMode ViewMode) const
{
    return FindConfig(ViewMode);
}

const FViewModePassDesc* FViewModePassRegistry::FindOpaquePassDesc(EViewMode ViewMode) const
{
    return FindPassDesc(ViewMode, ERenderPass::Opaque, ERenderShadingPath::Forward);
}

EShadingModel FViewModePassRegistry::GetShadingModel(EViewMode ViewMode) const
{
    return GetViewModeShadingModel(GetConfig(ViewMode));
}

bool FViewModePassRegistry::UsesDepthPrePass(EViewMode ViewMode) const
{
    return UsesViewModeDepthPre(GetConfig(ViewMode));
}

bool FViewModePassRegistry::UsesOpaque(EViewMode ViewMode) const
{
    return UsesViewModeOpaque(GetConfig(ViewMode));
}

bool FViewModePassRegistry::UsesLightingPass(EViewMode ViewMode) const
{
    return UsesViewModeLighting(GetConfig(ViewMode));
}

bool FViewModePassRegistry::UsesAdditiveDecal(EViewMode ViewMode) const
{
    return UsesViewModeAdditiveDecal(GetConfig(ViewMode));
}

bool FViewModePassRegistry::UsesAlphaBlend(EViewMode ViewMode) const
{
    return UsesViewModeAlphaBlend(GetConfig(ViewMode));
}

bool FViewModePassRegistry::UsesNonLitViewMode(EViewMode ViewMode) const
{
    return ::UsesNonLitViewMode(GetConfig(ViewMode));
}

bool FViewModePassRegistry::UsesHeightFog(EViewMode ViewMode) const
{
    return UsesViewModeHeightFog(GetConfig(ViewMode));
}

bool FViewModePassRegistry::UsesFXAA(EViewMode ViewMode) const
{
    return UsesViewModeFXAA(GetConfig(ViewMode));
}

EViewModePostProcessVariant FViewModePassRegistry::GetPostProcessVariant(EViewMode ViewMode) const
{
    return GetViewModePostProcessVariant(GetConfig(ViewMode));
}


const FViewModePassConfig* FViewModePassRegistry::FindConfig(EViewMode ViewMode) const
{
    auto It = Configs.find(ViewMode);
    if (It == Configs.end())
    {
        return nullptr;
    }

    return &It->second;
}

FViewModePassDesc* FViewModePassRegistry::FindPassDescMutable(EViewMode ViewMode, ERenderPass RenderPass, ERenderShadingPath ShadingPath) const
{
    auto It = Configs.find(ViewMode);
    if (It == Configs.end())
    {
        return nullptr;
    }

    FViewModePassConfig& MutableConfig = It->second;
    FViewModePassDesc*   Pass          = FindMatchingPassDesc(MutableConfig, RenderPass, ShadingPath);
    if (!Pass)
    {
        return nullptr;
    }

    return Pass;
}

FGraphicsProgram* FViewModePassRegistry::CompilePassVariant(FViewModePassDesc& Pass) const
{
    ApplyShadowFilterDefine(Pass);
    Pass.CompiledShader = VariantCache.GetOrCreate(Pass.ShaderVariant);
    return Pass.CompiledShader;
}

const FViewModePassDesc* FViewModePassRegistry::FindPassDesc(EViewMode ViewMode, ERenderPass RenderPass, ERenderShadingPath ShadingPath) const
{
    FViewModePassDesc* Pass = FindPassDescMutable(ViewMode, RenderPass, ShadingPath);
    if (!Pass)
    {
        return nullptr;
    }

    CompilePassVariant(*Pass);
    return Pass;
}

void FViewModePassRegistry::WarmUpViewMode(EViewMode ViewMode) const
{
    const FViewModePassConfig* Config = FindConfig(ViewMode);
    if (!Config)
    {
        return;
    }

    if (Config->bEnableOpaque)
    {
        if (FViewModePassDesc* OpaquePass = FindPassDescMutable(ViewMode, ERenderPass::Opaque, ERenderShadingPath::Forward))
        {
            CompilePassVariant(*OpaquePass);
        }
    }
}
