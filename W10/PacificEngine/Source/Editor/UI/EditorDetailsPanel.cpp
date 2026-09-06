// 에디터 영역의 세부 동작을 구현합니다.
#include "Editor/UI/EditorDetailsPanel.h"

#include "Editor/EditorEngine.h"
#include "Editor/Viewport/EditorViewportClient.h"
#include "Editor/Viewport/LevelEditorViewportClient.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"

#include "ImGui/imgui.h"
#include "Component/GizmoComponent.h"
#include "Component/AmbientLightComponent.h"
#include "Component/DecalComponent.h"
#include "Component/DirectionalLightComponent.h"
#include "Component/HeightFogComponent.h"
#include "Component/LightComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Component/SkeletalMeshComponent.h"
#include "Component/SceneComponent.h"
#include "Component/ScriptComponent.h"
#include "Core/PropertyTypes.h"
#include "Core/ClassTypes.h"
#include "Resource/FontManager.h"
#include "Resource/SpriteVfxManager.h"
#include "Resource/TextureManager.h"
#include "Object/FName.h"
#include "Object/ObjectIterator.h"
#include "Materials/Material.h"
#include "Mesh/ObjManager.h"
#include "Mesh/StaticMesh.h"
#include "Platform/Paths.h"

#include <Windows.h>
#include <commdlg.h>
#include <cctype>
#include <cstring>
#include <cmath>
#include <filesystem>
#include <functional>

#include "Materials/MaterialManager.h"
#include "Render/Execute/Passes/Scene/ShadowMapPass.h"
#include "Render/Execute/Registry/RenderPassRegistry.h"
#include "Render/Resources/Shadows/ShadowFilterSettings.h"
#include "Render/Resources/Shadows/ShadowResolutionSettings.h"
#include "Render/Scene/Proxies/Light/LightProxy.h"

namespace
{
constexpr const char* ContentAssetPayloadType = "CRASH_CONTENT_ASSET_PATH";
constexpr const char* LuaScriptPayloadType = "CRASH_LUA_SCRIPT_PATH";

static bool IsWorldTransformSpace(const UGizmoComponent* Gizmo)
{
    return Gizmo && Gizmo->IsWorldSpace();
}

static FVector GetDisplayedLocation(USceneComponent* SceneComp, bool bWorldSpace)
{
    if (!SceneComp)
    {
        return FVector();
    }

    return bWorldSpace ? SceneComp->GetWorldLocation() : SceneComp->GetRelativeLocation();
}

static FRotator GetDisplayedRotation(USceneComponent* SceneComp, bool bWorldSpace)
{
    if (!SceneComp)
    {
        return FRotator();
    }

    return bWorldSpace ? SceneComp->GetWorldRotation().ToRotator() : SceneComp->GetCachedEditRotator();
}

static void SetDisplayedLocation(USceneComponent* SceneComp, const FVector& NewLocation, bool bWorldSpace)
{
    if (!SceneComp)
    {
        return;
    }

    if (bWorldSpace)
    {
        SceneComp->SetWorldLocation(NewLocation);
    }
    else
    {
        SceneComp->SetRelativeLocation(NewLocation);
    }
}

static void SetDisplayedRotation(USceneComponent* SceneComp, const FRotator& NewRotation, bool bWorldSpace)
{
    if (!SceneComp)
    {
        return;
    }

    if (bWorldSpace)
    {
        SceneComp->SetWorldRotationWithEulerHint(NewRotation.ToQuaternion(), NewRotation.GetClamped());
    }
    else
    {
        SceneComp->SetRelativeRotationWithEulerHint(NewRotation.ToQuaternion(), NewRotation.GetClamped());
    }
}

static void ApplyRotationDeltaToActor(AActor* Actor, const FQuat& DeltaDisplayRotation, bool bWorldSpace)
{
    if (!Actor || !Actor->GetRootComponent())
    {
        return;
    }

    USceneComponent* Root = Actor->GetRootComponent();
    if (bWorldSpace)
    {
        FQuat NewWorldRotation = DeltaDisplayRotation * Root->GetWorldRotation();
        NewWorldRotation.Normalize();
        Root->SetWorldRotationWithEulerHint(NewWorldRotation, NewWorldRotation.ToRotator());
    }
    else
    {
        FQuat NewLocalRotation = Root->GetRelativeQuat() * DeltaDisplayRotation;
        NewLocalRotation.Normalize();
        Root->SetRelativeRotationWithEulerHint(NewLocalRotation, NewLocalRotation.ToRotator());
    }
}

static void ApplyLocationDeltaToActor(AActor* Actor, const FVector& Delta, bool bWorldSpace)
{
    if (!Actor || !Actor->GetRootComponent())
    {
        return;
    }

    USceneComponent* Root = Actor->GetRootComponent();
    if (bWorldSpace)
    {
        Root->SetWorldLocation(Root->GetWorldLocation() + Delta);
    }
    else
    {
        Root->SetRelativeLocation(Root->GetRelativeLocation() + Delta);
    }
}
}

#define SEPARATOR()     \
    ;                   \
    ImGui::Spacing();   \
    ImGui::Spacing();   \
    ImGui::Separator(); \
    ImGui::Spacing();   \
    ImGui::Spacing();

static FString RemoveExtension(const FString& Path)
{
    size_t DotPos = Path.find_last_of('.');
    if (DotPos == FString::npos)
    {
        return Path;
    }
    return Path.substr(0, DotPos);
}

static FString GetStemFromPath(const FString& Path)
{
    size_t SlashPos = Path.find_last_of("/\\");
    FString FileName = (SlashPos == FString::npos) ? Path : Path.substr(SlashPos + 1);
    return RemoveExtension(FileName);
}

static FString BuildComponentDisplayLabel(UActorComponent* Comp)
{
    if (!Comp)
    {
        return "None";
    }

    const FString TypeName = Comp->GetClass()->GetName();
    FString Name = Comp->GetFName().ToString();
    if (Name.empty())
    {
        Name = TypeName;
    }
    return TypeName + "(" + Name + ")";
}

static int32 GetShadowResolutionTierIndex(EShadowResolution ResolutionTier)
{
    for (int32 Index = 0; Index < static_cast<int32>(GShadowResolutionOptions.size()); ++Index)
    {
        if (GShadowResolutionOptions[Index] == ResolutionTier)
        {
            return Index;
        }
    }

    return 0;
}

static TArray<EShadowResolution> GetAllowedShadowResolutionTiers(uint32 LightType)
{
    if (LightType == static_cast<uint32>(ELightType::Point))
    {
        return { EShadowResolution::R256, EShadowResolution::R512, EShadowResolution::R1024 };
    }

    if (LightType == static_cast<uint32>(ELightType::Spot))
    {
        return { EShadowResolution::R256, EShadowResolution::R512, EShadowResolution::R1024, EShadowResolution::R2048 };
    }

    return { EShadowResolution::R256, EShadowResolution::R512, EShadowResolution::R1024, EShadowResolution::R2048, EShadowResolution::R4096 };
}

struct FShadowAtlasOwnerInfo
{
    UActorComponent* Component = nullptr;
    int32 CascadeIndex = -1;
    int32 FaceIndex = -1;
};

static bool MatchesShadowAllocation(const FShadowMapData& A, const FShadowMapData& B)
{
    return A.bAllocated == B.bAllocated &&
           A.AtlasPageIndex == B.AtlasPageIndex &&
           A.SliceIndex == B.SliceIndex &&
           A.Rect.X == B.Rect.X &&
           A.Rect.Y == B.Rect.Y &&
           A.Rect.Width == B.Rect.Width &&
           A.Rect.Height == B.Rect.Height &&
           A.ViewportRect.X == B.ViewportRect.X &&
           A.ViewportRect.Y == B.ViewportRect.Y &&
           A.ViewportRect.Width == B.ViewportRect.Width &&
           A.ViewportRect.Height == B.ViewportRect.Height;
}

static const FMatrix* FindShadowAtlasAllocationViewProj(UWorld* World, const FShadowMapData& Allocation)
{
    if (!World || !Allocation.bAllocated)
    {
        return nullptr;
    }

    const TArray<FLightProxy*>& LightProxies = World->GetScene().GetLightProxies();
    for (FLightProxy* LightProxy : LightProxies)
    {
        if (!LightProxy || !LightProxy->Owner)
        {
            continue;
        }

        if (const FCascadeShadowMapData* CascadeShadowMapData = LightProxy->GetCascadeShadowMapData())
        {
            const uint32 CascadeCount = std::min(CascadeShadowMapData->CascadeCount, static_cast<uint32>(ShadowAtlas::MaxCascades));
            for (uint32 CascadeIndex = 0; CascadeIndex < CascadeCount; ++CascadeIndex)
            {
                if (MatchesShadowAllocation(CascadeShadowMapData->Cascades[CascadeIndex], Allocation))
                {
                    return &CascadeShadowMapData->CascadeViewProj[CascadeIndex];
                }
            }
        }

        if (const FShadowMapData* SpotShadowMapData = LightProxy->GetSpotShadowMapData())
        {
            if (MatchesShadowAllocation(*SpotShadowMapData, Allocation))
            {
                return &LightProxy->LightViewProj;
            }
        }

        if (const FCubeShadowMapData* CubeShadowMapData = LightProxy->GetCubeShadowMapData())
        {
            for (uint32 FaceIndex = 0; FaceIndex < ShadowAtlas::MaxPointFaces; ++FaceIndex)
            {
                if (MatchesShadowAllocation(CubeShadowMapData->Faces[FaceIndex], Allocation))
                {
                    return &CubeShadowMapData->FaceViewProj[FaceIndex];
                }
            }
        }
    }

    return nullptr;
}

static FShadowAtlasOwnerInfo FindShadowAtlasOwnerInfo(UWorld* World, const FShadowMapData& Allocation)
{
    if (!World || !Allocation.bAllocated)
    {
        return {};
    }

    const TArray<FLightProxy*>& LightProxies = World->GetScene().GetLightProxies();
    for (FLightProxy* LightProxy : LightProxies)
    {
        if (!LightProxy || !LightProxy->Owner)
        {
            continue;
        }

        if (const FCascadeShadowMapData* CascadeShadowMapData = LightProxy->GetCascadeShadowMapData())
        {
            const uint32 CascadeCount = std::min(CascadeShadowMapData->CascadeCount, static_cast<uint32>(ShadowAtlas::MaxCascades));
            for (uint32 CascadeIndex = 0; CascadeIndex < CascadeCount; ++CascadeIndex)
            {
                if (MatchesShadowAllocation(CascadeShadowMapData->Cascades[CascadeIndex], Allocation))
                {
                    FShadowAtlasOwnerInfo Info = {};
                    Info.Component = LightProxy->Owner;
                    Info.CascadeIndex = static_cast<int32>(CascadeIndex);
                    return Info;
                }
            }
        }

        if (const FShadowMapData* SpotShadowMapData = LightProxy->GetSpotShadowMapData())
        {
            if (MatchesShadowAllocation(*SpotShadowMapData, Allocation))
            {
                FShadowAtlasOwnerInfo Info = {};
                Info.Component = LightProxy->Owner;
                return Info;
            }
        }

        if (const FCubeShadowMapData* CubeShadowMapData = LightProxy->GetCubeShadowMapData())
        {
            for (uint32 FaceIndex = 0; FaceIndex < ShadowAtlas::MaxPointFaces; ++FaceIndex)
            {
                if (MatchesShadowAllocation(CubeShadowMapData->Faces[FaceIndex], Allocation))
                {
                    FShadowAtlasOwnerInfo Info = {};
                    Info.Component = LightProxy->Owner;
                    Info.FaceIndex = static_cast<int32>(FaceIndex);
                    return Info;
                }
            }
        }
    }

    return {};
}

static FString BuildShadowAtlasOwnerLabel(const FShadowAtlasOwnerInfo& Info)
{
    if (!Info.Component)
    {
        return "Unresolved allocation";
    }

    FString Label = BuildComponentDisplayLabel(Info.Component);
    if (AActor* OwnerActor = Info.Component->GetOwner())
    {
        FString ActorName = OwnerActor->GetFName().ToString();
        if (ActorName.empty())
        {
            ActorName = OwnerActor->GetClass()->GetName();
        }
        Label = ActorName + " / " + Label;
    }

    if (Info.FaceIndex >= 0)
    {
        Label += " / Face " + std::to_string(Info.FaceIndex);
    }
    else if (Info.CascadeIndex >= 0)
    {
        Label += " / Cascade " + std::to_string(Info.CascadeIndex);
    }

    return Label;
}

static FString GetEditorFriendlyPropertyName(const FString& RawName)
{
    constexpr const char* LuaPropertyPrefix = "Lua.";
    FString Name = RawName;
    if (Name.rfind(LuaPropertyPrefix, 0) == 0)
    {
        Name = Name.substr(std::strlen(LuaPropertyPrefix));
    }

    if (Name == "bTickEnable")
    {
        return "Tick Enabled";
    }
    if (Name == "bAffectsWorld")
    {
        return "Affects World";
    }
    if (Name == "Cast Shadows")
    {
        return "Cast Shadows";
    }
    if (Name == "CSM Max Distance")
    {
        return "CSM Max Distance";
    }

    if (Name.size() > 1 && Name[0] == 'b' && std::isupper(static_cast<unsigned char>(Name[1])))
    {
        Name.erase(Name.begin());
    }

    FString Result;
    Result.reserve(Name.size() + 8);
    for (size_t Index = 0; Index < Name.size(); ++Index)
    {
        const char Current = Name[Index];
        const bool bHasPrev = (Index > 0);
        const bool bHasNext = (Index + 1 < Name.size());
        const char Prev = bHasPrev ? Name[Index - 1] : '\0';
        const char Next = bHasNext ? Name[Index + 1] : '\0';

        const bool bBreakBeforeUpper =
            bHasPrev && std::isupper(static_cast<unsigned char>(Current)) &&
            (std::islower(static_cast<unsigned char>(Prev)) ||
             (bHasNext && std::islower(static_cast<unsigned char>(Next))));
        const bool bBreakBeforeDigit =
            bHasPrev && std::isdigit(static_cast<unsigned char>(Current)) &&
            !std::isdigit(static_cast<unsigned char>(Prev));

        if ((bBreakBeforeUpper || bBreakBeforeDigit) && !Result.empty() && Result.back() != ' ')
        {
            Result.push_back(' ');
        }

        Result.push_back(Current);
    }

    return Result;
}

static bool BeginEditorSection(const char* Label, ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_DefaultOpen)
{
    return ImGui::CollapsingHeader(Label, Flags);
}

static void EndEditorSection(bool bWasOpen)
{
    if (bWasOpen)
    {
        ImGui::Dummy(ImVec2(0.0f, 6.0f));
    }
}

static int32 GetClosestWorldAlignedPointShadowFace(const UEditorEngine* InEditorEngine)
{
    if (!InEditorEngine)
    {
        return 0;
    }

    const auto* ActiveViewport = InEditorEngine->GetActiveViewport();
    const UCameraComponent* ActiveCamera = ActiveViewport ? ActiveViewport->GetCamera() : InEditorEngine->GetCamera();
    if (!ActiveCamera)
    {
        return 0;
    }

    const FVector Forward = ActiveCamera->GetForwardVector().Normalized();
    static const FVector FaceDirections[6] = {
        FVector(1.0f, 0.0f, 0.0f),
        FVector(-1.0f, 0.0f, 0.0f),
        FVector(0.0f, 1.0f, 0.0f),
        FVector(0.0f, -1.0f, 0.0f),
        FVector(0.0f, 0.0f, 1.0f),
        FVector(0.0f, 0.0f, -1.0f),
    };

    int32 BestFace = 0;
    float BestDot = -FLT_MAX;
    for (int32 FaceIndex = 0; FaceIndex < 6; ++FaceIndex)
    {
        const float DotValue = Forward.Dot(FaceDirections[FaceIndex]);
        if (DotValue > BestDot)
        {
            BestDot = DotValue;
            BestFace = FaceIndex;
        }
    }

    return BestFace;
}

static ImU32 GetShadowAtlasDebugColor(uint32 Resolution)
{
    switch (Resolution)
    {
    case 256:
        return IM_COL32(90, 200, 255, 255);
    case 512:
        return IM_COL32(110, 230, 140, 255);
    case 1024:
        return IM_COL32(255, 215, 90, 255);
    case 2048:
        return IM_COL32(255, 150, 80, 255);
    case 4096:
        return IM_COL32(255, 90, 90, 255);
    default:
        return IM_COL32(220, 220, 220, 255);
    }
}

static uint32 GetLightCascadeCountForEvictionDebug(const FLightProxy& LightProxy)
{
    if (LightProxy.LightProxyInfo.LightType != static_cast<uint32>(ELightType::Directional))
    {
        return 1;
    }

    if (const FCascadeShadowMapData* CascadeShadowMapData = LightProxy.GetCascadeShadowMapData())
    {
        return std::max(1u, std::min(CascadeShadowMapData->CascadeCount, static_cast<uint32>(ShadowAtlas::MaxCascades)));
    }

    return std::max(1u, std::min(LightProxy.GetCascadeCountSetting(), static_cast<uint32>(ShadowAtlas::MaxCascades)));
}

static void RenderShadowEvictionDebugInfo(const FLightProxy& LightProxy, uint32 Resolution, uint32 CascadeCount)
{
    const FLightShadowAllocationRegistry::FShadowEvictionDebugInfo DebugInfo =
        FLightShadowAllocationRegistry::BuildEvictionDebugInfo(
            LightProxy,
            Resolution,
            CascadeCount,
            LightProxy.LightProxyInfo.LightType);

    ImGui::SeparatorText("Eviction Score");
    ImGui::Text("Priority: %.2f", DebugInfo.Priority);
    ImGui::Text("Cost: %.0f", DebugInfo.Cost);
    ImGui::Text("Score: %.6f", DebugInfo.FinalScore);
}

static void DrawHatchedBand(ImDrawList* DrawList, const ImVec2& Min, const ImVec2& Max, ImU32 FillColor, ImU32 LineColor)
{
    if (!DrawList || Max.x <= Min.x || Max.y <= Min.y)
    {
        return;
    }

    DrawList->AddRectFilled(Min, Max, FillColor);
    DrawList->PushClipRect(Min, Max, true);
    constexpr float HatchSpacing = 8.0f;
    const float Height = Max.y - Min.y;
    for (float X = Min.x - Height; X < Max.x; X += HatchSpacing)
    {
        DrawList->AddLine(ImVec2(X, Max.y), ImVec2(X + Height, Min.y), LineColor, 1.0f);
    }
    DrawList->PopClipRect();
}

static void DrawInactiveShadowAtlasPadding(ImDrawList* DrawList, const ImVec2& RectMin, const ImVec2& RectMax, const ImVec2& ViewMin, const ImVec2& ViewMax)
{
    const ImU32 FillColor = IM_COL32(48, 112, 220, 84);
    const ImU32 LineColor = IM_COL32(132, 194, 255, 220);

    constexpr float PaddingEpsilon = 0.5f;
    if (std::fabs(RectMin.x - ViewMin.x) <= PaddingEpsilon &&
        std::fabs(RectMin.y - ViewMin.y) <= PaddingEpsilon &&
        std::fabs(RectMax.x - ViewMax.x) <= PaddingEpsilon &&
        std::fabs(RectMax.y - ViewMax.y) <= PaddingEpsilon)
    {
        return;
    }

    DrawHatchedBand(DrawList, RectMin, ImVec2(RectMax.x, ViewMin.y), FillColor, LineColor);
    DrawHatchedBand(DrawList, ImVec2(RectMin.x, ViewMax.y), RectMax, FillColor, LineColor);
    DrawHatchedBand(DrawList, ImVec2(RectMin.x, ViewMin.y), ImVec2(ViewMin.x, ViewMax.y), FillColor, LineColor);
    DrawHatchedBand(DrawList, ImVec2(ViewMax.x, ViewMin.y), ImVec2(RectMax.x, ViewMax.y), FillColor, LineColor);
}

static bool HasInactiveShadowAtlasPadding(const FShadowMapData& Allocation)
{
    return Allocation.Rect.X != Allocation.ViewportRect.X ||
           Allocation.Rect.Y != Allocation.ViewportRect.Y ||
           Allocation.Rect.Width != Allocation.ViewportRect.Width ||
           Allocation.Rect.Height != Allocation.ViewportRect.Height;
}

static ImVec2 InsetRectMin(const ImVec2& Min, const ImVec2& Max, float Inset)
{
    return ImVec2((std::min)(Min.x + Inset, Max.x), (std::min)(Min.y + Inset, Max.y));
}

static ImVec2 InsetRectMax(const ImVec2& Min, const ImVec2& Max, float Inset)
{
    return ImVec2((std::max)(Max.x - Inset, Min.x), (std::max)(Max.y - Inset, Min.y));
}

static uint64 HashShadowViewProj(const FMatrix& Matrix)
{
    constexpr uint64 FnvOffset = 1469598103934665603ull;
    constexpr uint64 FnvPrime = 1099511628211ull;

    uint64 Hash = FnvOffset;
    const uint8* Bytes = reinterpret_cast<const uint8*>(Matrix.Data);
    for (size_t ByteIndex = 0; ByteIndex < sizeof(Matrix.Data); ++ByteIndex)
    {
        Hash ^= static_cast<uint64>(Bytes[ByteIndex]);
        Hash *= FnvPrime;
    }
    return Hash;
}

static bool CompareShadowMapDataStable(const FShadowMapData& A, const FShadowMapData& B)
{
    if (A.AtlasPageIndex != B.AtlasPageIndex)
    {
        return A.AtlasPageIndex < B.AtlasPageIndex;
    }
    if (A.SliceIndex != B.SliceIndex)
    {
        return A.SliceIndex < B.SliceIndex;
    }
    if (A.Rect.Y != B.Rect.Y)
    {
        return A.Rect.Y < B.Rect.Y;
    }
    if (A.Rect.X != B.Rect.X)
    {
        return A.Rect.X < B.Rect.X;
    }
    if (A.Rect.Width != B.Rect.Width)
    {
        return A.Rect.Width < B.Rect.Width;
    }
    return A.Rect.Height < B.Rect.Height;
}

static uint64 HashShadowAllocationsStable(TArray<FShadowMapData> Allocations)
{
    std::sort(Allocations.begin(), Allocations.end(), CompareShadowMapDataStable);

    uint64 Hash = 1469598103934665603ull;
    auto Mix = [&Hash](uint32 Value)
    {
        Hash ^= static_cast<uint64>(Value);
        Hash *= 1099511628211ull;
    };

    Mix(static_cast<uint32>(Allocations.size()));
    for (const FShadowMapData& Allocation : Allocations)
    {
        Mix(Allocation.AtlasPageIndex);
        Mix(Allocation.SliceIndex);
        Mix(Allocation.Resolution);
        Mix(Allocation.Rect.X);
        Mix(Allocation.Rect.Y);
        Mix(Allocation.Rect.Width);
        Mix(Allocation.Rect.Height);
        Mix(Allocation.ViewportRect.X);
        Mix(Allocation.ViewportRect.Y);
        Mix(Allocation.ViewportRect.Width);
        Mix(Allocation.ViewportRect.Height);
        Mix(Allocation.NodeIndex);
    }

    return Hash;
}

static void NormalizeShadowAllocationsForDisplay(TArray<FShadowMapData>& Allocations)
{
    std::sort(Allocations.begin(), Allocations.end(), CompareShadowMapDataStable);
}

FString FEditorDetailsPanel::OpenObjFileDialog()
{
    wchar_t FilePath[MAX_PATH] = {};

    OPENFILENAMEW Ofn = {};
    Ofn.lStructSize = sizeof(Ofn);
    Ofn.hwndOwner = nullptr;
    Ofn.lpstrFilter = L"OBJ Files (*.obj)\0*.obj\0All Files (*.*)\0*.*\0";
    Ofn.lpstrFile = FilePath;
    Ofn.nMaxFile = MAX_PATH;
    Ofn.lpstrTitle = L"Import Mesh";
    Ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameW(&Ofn))
    {
        std::filesystem::path AbsPath = std::filesystem::path(FilePath).lexically_normal();
        std::filesystem::path RootPath = std::filesystem::path(FPaths::RootDir());
        std::filesystem::path RelPath = AbsPath.lexically_relative(RootPath);

        // 상대 경로 변환 실패 시 (드라이브가 다른 경우 등) 절대 경로를 그대로 반환
        if (RelPath.empty() || RelPath.wstring().starts_with(L".."))
        {
            return FPaths::ToUtf8(AbsPath.generic_wstring());
        }
        return FPaths::ToUtf8(RelPath.generic_wstring());
    }

    return FString();
}

void FEditorDetailsPanel::FocusComponentDetails(UActorComponent* Component, int32 CascadeIndex, int32 FaceIndex)
{
    if (!EditorEngine || !Component)
    {
        return;
    }

    AActor* OwnerActor = Component->GetOwner();
    if (!OwnerActor)
    {
        return;
    }

    EditorEngine->GetSelectionManager().Select(OwnerActor);
    LastSelectedActor = OwnerActor;
    SelectedComponent = Component;
    bActorSelected = false;

    if (CascadeIndex >= 0)
    {
        SelectedDirectionalCascade = CascadeIndex;
    }
    if (FaceIndex >= 0)
    {
        SelectedPointLightShadowFace = FaceIndex;
    }
}

void FEditorDetailsPanel::Render(float DeltaTime)
{
    (void)DeltaTime;

    ImGui::SetNextWindowSize(ImVec2(350.0f, 500.0f), ImGuiCond_Once);

    ImGui::Begin("Details");

    FSelectionManager& Selection = EditorEngine->GetSelectionManager();
    AActor* PrimaryActor = Selection.GetPrimarySelection();
    TArray<AActor*> FallbackActors;
    if (!PrimaryActor)
    {
        FLevelEditorViewportClient* ActiveViewport = EditorEngine->GetActiveViewport();
        if (ActiveViewport && ActiveViewport->IsPilotingActor())
        {
            if (AActor* PilotedActor = ActiveViewport->GetPilotedActor())
            {
                PrimaryActor = PilotedActor;
                FallbackActors.push_back(PilotedActor);
            }
        }
    }

    if (!PrimaryActor)
    {
        SelectedComponent = nullptr;
        LastSelectedActor = nullptr;
        bActorSelected = true;
        ImGui::TextDisabled("No actor selected.");
        ImGui::End();
        return;
    }

    // Actor 선택이 바뀌면 초기화
    if (PrimaryActor != LastSelectedActor)
    {
        SelectedComponent = nullptr;
        LastSelectedActor = PrimaryActor;
        bActorSelected = true;
    }

    const TArray<AActor*>& SelectedActors = FallbackActors.empty() ? Selection.GetSelectedActors() : FallbackActors;
    const int32 SelectionCount = static_cast<int32>(SelectedActors.size());

    if (SelectionCount > 1)
    {
        ImGui::Text("Selected: %d Actors", SelectionCount);
    }
    else
    {
        ImGui::Text("Class: %s", PrimaryActor->GetClass()->GetName());
    }

    // ========== 고정 영역: Component Tree ==========
    SEPARATOR();
    RenderComponentTree(PrimaryActor);

    // ========== 스크롤 영역: Details ==========
    SEPARATOR();
    ImGui::Text("Details");
    ImGui::Separator();

    float ScrollHeight = ImGui::GetContentRegionAvail().y;
    if (ScrollHeight < 50.0f)
        ScrollHeight = 50.0f;

    ImGui::BeginChild("##Details", ImVec2(0, ScrollHeight), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);
    {
        RenderDetails(PrimaryActor, SelectedActors);
    }
    ImGui::EndChild();

    ImGui::End();
}

void FEditorDetailsPanel::RenderDetails(AActor* PrimaryActor, const TArray<AActor*>& SelectedActors)
{
    if (bActorSelected)
    {
        RenderActorProperties(PrimaryActor, SelectedActors);
    }
    else if (SelectedComponent)
    {
        RenderComponentProperties(PrimaryActor);
    }
    else
    {
        ImGui::TextDisabled("Select an actor or component to view details.");
    }
}

void FEditorDetailsPanel::RenderActorProperties(AActor* PrimaryActor, const TArray<AActor*>& SelectedActors)
{
    ImGui::Text("Actor: %s", PrimaryActor->GetClass()->GetName());
    ImGui::Text("Name: %s", PrimaryActor->GetFName().ToString().c_str());

    char TagBuf[256];
    strncpy_s(TagBuf, sizeof(TagBuf), PrimaryActor->GetActorTag().ToString().c_str(), _TRUNCATE);
    if (ImGui::InputText("Tag", TagBuf, sizeof(TagBuf)))
    {
        FName NewTag(TagBuf);
        PrimaryActor->SetActorTag(NewTag);
        for (AActor* Actor : SelectedActors)
        {
            if (Actor && Actor != PrimaryActor)
            {
                Actor->SetActorTag(NewTag);
            }
        }
    }

    if (PrimaryActor->GetRootComponent())
    {
        ImGui::Separator();
        ImGui::Text("Transform");
        ImGui::Spacing();

        UGizmoComponent* Gizmo = EditorEngine ? EditorEngine->GetGizmo() : nullptr;
        const bool bWorldSpace = IsWorldTransformSpace(Gizmo);
        ImGui::Text("Space: %s", bWorldSpace ? "World" : "Local");
        ImGui::SameLine();
        ImGui::TextDisabled("(Scale stays Local)");

        USceneComponent* RootComp = PrimaryActor->GetRootComponent();

        FVector Pos = GetDisplayedLocation(RootComp, bWorldSpace);
        float PosArray[3] = { Pos.X, Pos.Y, Pos.Z };

        FVector Scale = PrimaryActor->GetActorScale();
        float ScaleArray[3] = { Scale.X, Scale.Y, Scale.Z };

        if (ImGui::DragFloat3("Location", PosArray, 0.1f))
        {
            const FVector NewLocation(PosArray[0], PosArray[1], PosArray[2]);
            const FVector Delta = NewLocation - Pos;
            for (AActor* Actor : SelectedActors)
            {
                if (Actor)
                {
                    ApplyLocationDeltaToActor(Actor, Delta, bWorldSpace);
                }
            }
            if (Gizmo)
            {
                Gizmo->UpdateGizmoTransform();
            }
        }
        {
            FRotator DisplayRot = GetDisplayedRotation(RootComp, bWorldSpace);
            FRotator PrevRot = DisplayRot;
            float RotXYZ[3] = { DisplayRot.Roll, DisplayRot.Pitch, DisplayRot.Yaw };

            if (ImGui::DragFloat3("Rotation", RotXYZ, 0.1f))
            {
                FRotator NewDisplayRot(RotXYZ[1], RotXYZ[2], RotXYZ[0]);
                FQuat DisplayDelta = bWorldSpace
                                         ? (NewDisplayRot.ToQuaternion() * PrevRot.ToQuaternion().Inverse())
                                         : (PrevRot.ToQuaternion().Inverse() * NewDisplayRot.ToQuaternion());
                DisplayDelta.Normalize();

                for (AActor* Actor : SelectedActors)
                {
                    if (!Actor || !Actor->GetRootComponent())
                    {
                        continue;
                    }

                    if (Actor == PrimaryActor)
                    {
                        USceneComponent* Root = Actor->GetRootComponent();
                        if (bWorldSpace)
                        {
                            Root->SetWorldRotationWithEulerHint(NewDisplayRot.ToQuaternion(), NewDisplayRot.GetClamped());
                        }
                        else
                        {
                            Root->SetRelativeRotationWithEulerHint(NewDisplayRot.ToQuaternion(), NewDisplayRot.GetClamped());
                        }
                    }
                    else
                    {
                        ApplyRotationDeltaToActor(Actor, DisplayDelta, bWorldSpace);
                    }
                }
                if (Gizmo)
                {
                    Gizmo->UpdateGizmoTransform();
                }
            }
        }
        if (ImGui::DragFloat3("Scale", ScaleArray, 0.1f))
        {
            FVector Delta = FVector(ScaleArray[0], ScaleArray[1], ScaleArray[2]) - Scale;
            for (AActor* Actor : SelectedActors)
            {
                if (Actor)
                {
                    Actor->SetActorScale(Actor->GetActorScale() + Delta);
                }
            }
        }
    }

    ImGui::Separator();
    bool bVisible = PrimaryActor->IsVisible();
    if (ImGui::Checkbox("Visible", &bVisible))
    {
        PrimaryActor->SetVisible(bVisible);
    }

    TArray<FPropertyDescriptor> ActorProps;
    PrimaryActor->GetEditableProperties(ActorProps);
    if (!ActorProps.empty())
    {
        ImGui::Separator();
        const bool bActorPropertiesOpen = BeginEditorSection("Post Process");
        if (bActorPropertiesOpen)
        {
            for (int32 i = 0; i < static_cast<int32>(ActorProps.size()); ++i)
            {
                if (RenderDetailsPanel(ActorProps, i))
                {
                    PrimaryActor->PostEditProperty(ActorProps[i].Name.c_str());
                }
            }
        }
        EndEditorSection(bActorPropertiesOpen);
    }
}

void FEditorDetailsPanel::RenderComponentTree(AActor* Actor)
{
    ImGui::Text("Component List");

    // Get All Component Classes
    TArray<UClass*>& AllClasses = UClass::GetAllClasses();

    TArray<UClass*> ComponentClasses;
    for (UClass* Cls : AllClasses)
    {
        if (Cls->IsA(UActorComponent::StaticClass()) && !Cls->HasAnyClassFlags(CF_Abstract))
            ComponentClasses.push_back(Cls);
    }

    static int SelectedIndex = 0;
    if (ComponentClasses.empty())
    {
        SelectedIndex = 0;
    }
    else if (SelectedIndex >= static_cast<int>(ComponentClasses.size()))
    {
        SelectedIndex = static_cast<int>(ComponentClasses.size()) - 1;
    }
    const char* Preview = ComponentClasses.empty() ? "None" : ComponentClasses[SelectedIndex]->GetName();

    USceneComponent* Root = Actor->GetRootComponent();

    if (!ComponentClasses.empty() && ImGui::Button("Add"))
    {
        UActorComponent* Comp = Actor->AddComponentByClass(ComponentClasses[SelectedIndex]);
        if (!Comp)
        {
            return;
        }

        if (ComponentClasses[SelectedIndex]->IsA(USceneComponent::StaticClass()))
        {
            if (SelectedComponent != nullptr && SelectedComponent->GetClass()->IsA(USceneComponent::StaticClass()))
                Cast<USceneComponent>(Comp)->AttachToComponent(Cast<USceneComponent>(SelectedComponent));
            else
                Cast<USceneComponent>(Comp)->AttachToComponent(Root);
        }
    }

    ImGui::SameLine();
    // ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::BeginCombo("Type", Preview))
    {
        for (int i = 0; i < (int)ComponentClasses.size(); ++i)
        {
            bool bSelected = (SelectedIndex == i);
            if (ImGui::Selectable(ComponentClasses[i]->GetName(), bSelected))
                SelectedIndex = i;
            if (bSelected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::Separator();

    // Actor Node
    ImGuiTreeNodeFlags ActorFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen;
    if (bActorSelected)
        ActorFlags |= ImGuiTreeNodeFlags_Selected;

    FString ActorLabel = Actor->GetFName().ToString();
    if (ActorLabel.empty())
        ActorLabel = Actor->GetClass()->GetName();

    bool bActorOpen = ImGui::TreeNodeEx(Actor, ActorFlags, "[Actor] %s", ActorLabel.c_str());
    if (ImGui::IsItemClicked())
    {
        SelectedComponent = nullptr;
        bActorSelected = true;
    }
    FString DroppedActorScriptPath;
    if (AcceptLuaScriptDrop(DroppedActorScriptPath))
    {
        AssignLuaScriptToActor(Actor, DroppedActorScriptPath);
    }

    if (bActorOpen)
    {
        if (Root)
        {
            RenderSceneComponentNode(Root);
        }

        // Non-scene ActorComponents
        TArray<UActorComponent*> Comps = Actor->GetComponents();
        UActorComponent* ComponentToDelete = nullptr;

        for (UActorComponent* Comp : Comps)
        {
            if (!Comp)
                continue;
            if (Comp->IsA<USceneComponent>())
                continue;

            FString Label = BuildComponentDisplayLabel(Comp);

            ImGui::Indent(12.0f);
            ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
            if (!bActorSelected && SelectedComponent == Comp)
                Flags |= ImGuiTreeNodeFlags_Selected;

            const bool bIsRenaming = (RenamingComponent == Comp);
            ImGui::TreeNodeEx(Comp, Flags, "%s", bIsRenaming ? "" : Label.c_str());
            if (bIsRenaming)
            {
                ImGui::SameLine();
                RenderComponentRenameInput(Comp);
            }
            if (!bIsRenaming && ImGui::IsItemClicked())
            {
                SelectedComponent = Comp;
                bActorSelected = false;
            }
            if (UScriptComponent* ScriptComponent = Cast<UScriptComponent>(Comp))
            {
                FString DroppedScriptPath;
                if (AcceptLuaScriptDrop(DroppedScriptPath))
                {
                    AssignLuaScriptToComponent(ScriptComponent, DroppedScriptPath);
                }
            }

            if (ImGui::BeginPopupContextItem())
            {
                if (ImGui::MenuItem("Rename"))
                {
                    BeginComponentRename(Comp);
                }
                if (ImGui::MenuItem("Delete"))
                {
                    ComponentToDelete = Comp;
                }
                ImGui::EndPopup();
            }

            ImGui::Unindent(12.0f);
        }

        if (ComponentToDelete)
        {
            Actor->RemoveComponent(ComponentToDelete);
            if (RenamingComponent == ComponentToDelete)
            {
                RenamingComponent = nullptr;
            }
            if (SelectedComponent == ComponentToDelete)
            {
                SelectedComponent = nullptr;
                bActorSelected = true;
            }
        }
        ImGui::TreePop();
    }
}

void FEditorDetailsPanel::RenderSceneComponentNode(USceneComponent* Comp)
{
    if (!Comp)
        return;

    FString Label = BuildComponentDisplayLabel(Comp);

    const auto& Children = Comp->GetChildren();
    bool bHasChildren = !Children.empty();

    ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen;
    if (!bHasChildren)
        Flags |= ImGuiTreeNodeFlags_Leaf;
    if (!bActorSelected && SelectedComponent == Comp)
        Flags |= ImGuiTreeNodeFlags_Selected;

    bool bIsRoot = (Comp->GetParent() == nullptr);
    const bool bIsRenaming = (RenamingComponent == Comp);
    bool bOpen = ImGui::TreeNodeEx(
        Comp, Flags, "%s%s",
        bIsRoot ? "[Root] " : "",
        bIsRenaming ? "" : Label.c_str());
    if (bIsRenaming)
    {
        ImGui::SameLine();
        RenderComponentRenameInput(Comp);
    }

    if (!bIsRenaming && ImGui::IsItemClicked())
    {
        SelectedComponent = Comp;
        bActorSelected = false;
    }

    bool bDeleted = false;
    if (ImGui::BeginPopupContextItem())
    {
        if (ImGui::MenuItem("Rename"))
        {
            BeginComponentRename(Comp);
        }
        // Root Component는 삭제할 수 없도록 예외 처리
        if (ImGui::MenuItem("Delete", nullptr, false, !bIsRoot))
        {
            bDeleted = true;
        }
        if (bIsRoot && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        {
            ImGui::SetTooltip("Root Component cannot be deleted.");
        }
        ImGui::EndPopup();
    }

    if (bDeleted)
    {
        AActor* Actor = Comp->GetOwner();
        if (Actor)
        {
            Actor->RemoveComponent(Comp);
            if (RenamingComponent == Comp)
            {
                RenamingComponent = nullptr;
            }
            if (SelectedComponent == Comp)
            {
                SelectedComponent = nullptr;
                bActorSelected = true;
            }
        }
        if (bOpen)
            ImGui::TreePop();
        return;
    }

    if (bOpen)
    {
        for (USceneComponent* Child : Children)
        {
            RenderSceneComponentNode(Child);
        }
        ImGui::TreePop();
    }
}

void FEditorDetailsPanel::BeginComponentRename(UActorComponent* Component)
{
    if (!Component)
    {
        return;
    }

    RenamingComponent = Component;
    strncpy_s(ComponentRenameBuffer, Component->GetFName().ToString().c_str(), _TRUNCATE);
}

bool FEditorDetailsPanel::RenderComponentRenameInput(UActorComponent* Component)
{
    if (!Component)
    {
        RenamingComponent = nullptr;
        return false;
    }

    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SetKeyboardFocusHere();
    if (ImGui::InputText("##ComponentRename", ComponentRenameBuffer, sizeof(ComponentRenameBuffer), ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll))
    {
        if (ComponentRenameBuffer[0] != '\0')
        {
            Component->SetFName(FName(ComponentRenameBuffer));
        }
        RenamingComponent = nullptr;
        return true;
    }

    if (!ImGui::IsItemActive() && (ImGui::IsMouseClicked(0) || ImGui::IsKeyPressed(ImGuiKey_Escape)))
    {
        RenamingComponent = nullptr;
    }

    return false;
}

bool FEditorDetailsPanel::AcceptLuaScriptDrop(FString& OutScriptPath)
{
    if (!ImGui::BeginDragDropTarget())
    {
        return false;
    }

    bool bAccepted = false;
    if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload(LuaScriptPayloadType))
    {
        const char* ScriptPath = static_cast<const char*>(Payload->Data);
        if (ScriptPath && Payload->DataSize > 0)
        {
            OutScriptPath = ScriptPath;
            bAccepted = !OutScriptPath.empty();
        }
    }

    ImGui::EndDragDropTarget();
    return bAccepted;
}

UScriptComponent* FEditorDetailsPanel::FindOrAddScriptComponent(AActor* Actor)
{
    if (!Actor)
    {
        return nullptr;
    }

    for (UActorComponent* Component : Actor->GetComponents())
    {
        if (UScriptComponent* ScriptComponent = Cast<UScriptComponent>(Component))
        {
            return ScriptComponent;
        }
    }

    return Cast<UScriptComponent>(Actor->AddComponentByClass(UScriptComponent::StaticClass()));
}

bool FEditorDetailsPanel::AssignLuaScriptToActor(AActor* Actor, const FString& ScriptPath)
{
    UScriptComponent* ScriptComponent = FindOrAddScriptComponent(Actor);
    return AssignLuaScriptToComponent(ScriptComponent, ScriptPath);
}

bool FEditorDetailsPanel::AssignLuaScriptToComponent(UScriptComponent* ScriptComponent, const FString& ScriptPath)
{
    if (!ScriptComponent || ScriptPath.empty())
    {
        return false;
    }

    ScriptComponent->SetScriptPath(ScriptPath);
    SelectedComponent = ScriptComponent;
    LastSelectedActor = ScriptComponent->GetOwner();
    bActorSelected = false;
    return true;
}

void FEditorDetailsPanel::RenderComponentProperties(AActor* Actor)
{
    (void)Actor;
    const FString ComponentLabel = BuildComponentDisplayLabel(SelectedComponent);
    ImGui::Text("Component: %s", ComponentLabel.c_str());
    ImGui::Separator();

    TArray<FPropertyDescriptor> Props;
    SelectedComponent->GetEditableProperties(Props);

    auto IsTransformProp = [](const FString& Name)
    {
        return Name == "Location" || Name == "Rotation" || Name == "Scale";
    };
    auto IsStaticMeshProp = [](const FString& Name, EPropertyType Type)
    {
        return Type == EPropertyType::StaticMeshRef || Name == "Static Mesh" || Name == "StaticMesh";
    };
    auto IsSkeletalMeshProp = [](const FString& Name, EPropertyType Type)
    {
        return Type == EPropertyType::SkeletalMeshRef || Name == "Skeletal Mesh" || Name == "SkeletalMesh";
    };
    auto IsMaterialProp = [](const FString& Name, EPropertyType Type)
    {
        return Type == EPropertyType::MaterialSlot || Name.rfind("Element ", 0) == 0;
    };
    auto IsVisibilityProp = [](const FString& Name)
    {
        return Name == "Visible" || Name == "Visible In Editor" || Name == "Visible In Game" || Name == "Is Editor Helper";
    };
    auto IsBehaviorProp = [](const FString& Name)
    {
        return Name == "bTickEnable";
    };
    auto IsShadowProp = [](const FString& Name)
    {
        return Name == "Cast Shadows" || Name == "Depth Bias" || Name == "Slope Bias" || Name == "Normal Bias" || Name == "Shadow Sharpen" || Name == "ESM Exponent" || Name == "Cascade Count" || Name == "CSM Max Distance" || Name == "Cascade Distribution" || Name == "bAffectsWorld";
    };

    bool bIsRoot = false;
    if (SelectedComponent->IsA<USceneComponent>())
    {
        USceneComponent* SceneComp = static_cast<USceneComponent*>(SelectedComponent);
        bIsRoot = (SceneComp->GetParent() == nullptr);
    }

    const bool bIsDecalComponent = SelectedComponent->IsA<UDecalComponent>();
    const bool bIsFogComponent = SelectedComponent->IsA<UHeightFogComponent>();
    const bool bIsLightComponent = SelectedComponent->IsA<ULightComponent>();

    const bool bTransformOpen = SelectedComponent->IsA<USceneComponent>() && BeginEditorSection("Transform");
    if (bTransformOpen)
    {
        USceneComponent* SceneComp = static_cast<USceneComponent*>(SelectedComponent);
        UGizmoComponent* Gizmo = EditorEngine ? EditorEngine->GetGizmo() : nullptr;
        const bool bWorldSpace = IsWorldTransformSpace(Gizmo);
        ImGui::Text("Space: %s", bWorldSpace ? "World" : "Local");
        ImGui::SameLine();
        ImGui::TextDisabled("(Scale stays Local)");

        FVector Pos = GetDisplayedLocation(SceneComp, bWorldSpace);
        float PosArray[3] = { Pos.X, Pos.Y, Pos.Z };

        if (ImGui::DragFloat3("Location", PosArray, 0.1f))
        {
            const FVector NewLocation(PosArray[0], PosArray[1], PosArray[2]);
            SetDisplayedLocation(SceneComp, NewLocation, bWorldSpace);
            if (Gizmo)
            {
                Gizmo->UpdateGizmoTransform();
            }
        }

        FRotator DisplayRot = GetDisplayedRotation(SceneComp, bWorldSpace);
        float RotXYZ[3] = { DisplayRot.Roll, DisplayRot.Pitch, DisplayRot.Yaw };
        if (ImGui::DragFloat3("Rotation", RotXYZ, 0.1f))
        {
            const FRotator NewRotation(RotXYZ[1], RotXYZ[2], RotXYZ[0]);
            SetDisplayedRotation(SceneComp, NewRotation, bWorldSpace);
            if (Gizmo)
            {
                Gizmo->UpdateGizmoTransform();
            }
        }

        FVector Scale = SceneComp->GetRelativeScale();
        float ScaleArray[3] = { Scale.X, Scale.Y, Scale.Z };
        if (ImGui::DragFloat3("Scale", ScaleArray, 0.1f))
        {
            SceneComp->SetRelativeScale(FVector(ScaleArray[0], ScaleArray[1], ScaleArray[2]));
        }
    }
    EndEditorSection(bTransformOpen);

    const bool bIsStaticMeshComponent = SelectedComponent->IsA<UStaticMeshComponent>();
    const bool bIsSkeletalMeshComponent = SelectedComponent->IsA<USkeletalMeshComponent>();
    UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(SelectedComponent);
    const bool bStaticMeshOpen = bIsStaticMeshComponent && BeginEditorSection("Static Mesh");
    if (bStaticMeshOpen)
    {
        for (int32 i = 0; i < (int32)Props.size(); ++i)
        {
            if (IsStaticMeshProp(Props[i].Name, Props[i].Type))
            {
                bool bChanged = RenderDetailsPanel(Props, i);
                if (bChanged && Props[i].Type == EPropertyType::StaticMeshRef)
                {
                    if (SelectedComponent->IsA<USceneComponent>())
                    {
                        static_cast<USceneComponent*>(SelectedComponent)->MarkTransformDirty();
                    }
                    return;
                }
            }
        }
    }
    EndEditorSection(bStaticMeshOpen);

    const bool bSkeletalMeshOpen = bIsSkeletalMeshComponent && BeginEditorSection("Skeletal Mesh");
    if (bSkeletalMeshOpen)
    {
        for (int32 i = 0; i < (int32)Props.size(); ++i)
        {
            if (IsSkeletalMeshProp(Props[i].Name, Props[i].Type))
            {
                const bool bChanged = RenderDetailsPanel(Props, i);
                if (bChanged && Props[i].Type == EPropertyType::SkeletalMeshRef)
                {
                    if (SelectedComponent->IsA<USceneComponent>())
                    {
                        static_cast<USceneComponent*>(SelectedComponent)->MarkTransformDirty();
                    }
                    return;
                }
            }
        }
    }
    EndEditorSection(bSkeletalMeshOpen);

    bool bHasMaterialProp = false;
    for (const FPropertyDescriptor& Prop : Props)
    {
        if (IsMaterialProp(Prop.Name, Prop.Type))
        {
            bHasMaterialProp = true;
            break;
        }
    }

    const bool bMaterialsOpen = bHasMaterialProp && BeginEditorSection("Materials");
    if (bMaterialsOpen)
    {
        for (int32 i = 0; i < (int32)Props.size(); ++i)
        {
            if (IsMaterialProp(Props[i].Name, Props[i].Type))
            {
                RenderDetailsPanel(Props, i);
            }
        }
    }
    EndEditorSection(bMaterialsOpen);

    const bool bVisibilityOpen = PrimitiveComponent && BeginEditorSection("Visibility");
    if (bVisibilityOpen)
    {
        bool bVisible = PrimitiveComponent->IsVisible();
        if (ImGui::Checkbox("Visible", &bVisible))
        {
            PrimitiveComponent->SetVisibility(bVisible);
            PrimitiveComponent->PostEditProperty("Visible");
        }

        ImGui::Indent();
        bool bVisibleInEditor = PrimitiveComponent->IsVisibleInEditor();
        bool bVisibleInGame = PrimitiveComponent->IsVisibleInGame();

        if (!PrimitiveComponent->IsVisible())
        {
            bVisibleInEditor = false;
            bVisibleInGame = false;
        }

        ImGui::BeginDisabled(!PrimitiveComponent->IsVisible());
        if (ImGui::Checkbox("Visible In Editor", &bVisibleInEditor))
        {
            PrimitiveComponent->SetVisibleInEditor(bVisibleInEditor);
            PrimitiveComponent->PostEditProperty("Visible In Editor");
        }
        if (ImGui::Checkbox("Visible In Game", &bVisibleInGame))
        {
            PrimitiveComponent->SetVisibleInGame(bVisibleInGame);
            PrimitiveComponent->PostEditProperty("Visible In Game");
        }
        ImGui::EndDisabled();
        ImGui::Unindent();
    }
    EndEditorSection(bVisibilityOpen);

    const char* PropertySectionLabel = "Component";
    if (bIsLightComponent)
    {
        PropertySectionLabel = "Light";
    }
    else if (bIsDecalComponent)
    {
        PropertySectionLabel = "Decal";
    }
    else if (bIsFogComponent)
    {
        PropertySectionLabel = "Fog";
    }

    bool bShowLightShadowSection = true;
    bool bHasPropertySectionContent = false;
    if (bIsLightComponent)
    {
        bHasPropertySectionContent = true; // Affects World is always surfaced at the top for lights.
    }
    else
    {
        for (const FPropertyDescriptor& Prop : Props)
        {
            if (IsTransformProp(Prop.Name) || IsStaticMeshProp(Prop.Name, Prop.Type) || IsSkeletalMeshProp(Prop.Name, Prop.Type) || IsMaterialProp(Prop.Name, Prop.Type) || IsVisibilityProp(Prop.Name) || IsBehaviorProp(Prop.Name))
            {
                continue;
            }
            bHasPropertySectionContent = true;
            break;
        }
    }

    const bool bPropertySectionOpen = bHasPropertySectionContent && BeginEditorSection(PropertySectionLabel);
    if (bPropertySectionOpen)
    {
        ULightComponent* LightComponent = Cast<ULightComponent>(SelectedComponent);
        if (LightComponent)
        {
            for (int32 i = 0; i < (int32)Props.size(); ++i)
            {
                if (Props[i].Name == "bAffectsWorld")
                {
                    const bool bChanged = RenderDetailsPanel(Props, i);
                    if (bChanged)
                    {
                        LightComponent->PostEditProperty("bAffectsWorld");
                    }
                    break;
                }
            }

            if (!LightComponent->AffectsWorld())
            {
                bShowLightShadowSection = false;
            }
        }

        for (int32 i = 0; i < (int32)Props.size(); ++i)
        {
            if (IsTransformProp(Props[i].Name) || IsStaticMeshProp(Props[i].Name, Props[i].Type) || IsSkeletalMeshProp(Props[i].Name, Props[i].Type) || IsMaterialProp(Props[i].Name, Props[i].Type) || IsVisibilityProp(Props[i].Name) || IsBehaviorProp(Props[i].Name))
            {
                continue;
            }
            if (LightComponent && IsShadowProp(Props[i].Name))
            {
                continue;
            }
            if (LightComponent && Props[i].Name == "bAffectsWorld")
            {
                continue;
            }
            if (LightComponent && !LightComponent->AffectsWorld())
            {
                continue;
            }
            RenderDetailsPanel(Props, i);
        }
    }
    EndEditorSection(bPropertySectionOpen);

    bool bHasBehaviorSectionContent = false;
    for (const FPropertyDescriptor& Prop : Props)
    {
        if (IsBehaviorProp(Prop.Name))
        {
            bHasBehaviorSectionContent = true;
            break;
        }
    }

    const bool bBehaviorSectionOpen = bHasBehaviorSectionContent && BeginEditorSection("Behavior");
    if (bBehaviorSectionOpen)
    {
        for (int32 i = 0; i < (int32)Props.size(); ++i)
        {
            if (IsBehaviorProp(Props[i].Name))
            {
                RenderDetailsPanel(Props, i);
            }
        }
    }
    EndEditorSection(bBehaviorSectionOpen);

    ULightComponent* LightComponent = Cast<ULightComponent>(SelectedComponent);
    if (LightComponent && bShowLightShadowSection)
    {
        RenderLightShadowSettings(LightComponent);
    }

    if (SelectedComponent->IsA<USceneComponent>())
    {
        static_cast<USceneComponent*>(SelectedComponent)->MarkTransformDirty();
    }
}

void FEditorDetailsPanel::RenderLightShadowSettings(ULightComponent* LightComponent)
{
    if (!LightComponent || LightComponent->IsA<UAmbientLightComponent>())
    {
        return;
    }

    if (!BeginEditorSection("Shadow"))
    {
        return;
    }

    TArray<FPropertyDescriptor> ShadowProps;
    LightComponent->GetEditableProperties(ShadowProps);

    auto RenderShadowPropertyByName = [&](const char* PropertyName)
    {
        for (int32 PropIndex = 0; PropIndex < static_cast<int32>(ShadowProps.size()); ++PropIndex)
        {
            if (ShadowProps[PropIndex].Name == PropertyName)
            {
                bool bChanged = false;
                if (ShadowProps[PropIndex].Name == "Cast Shadows" && ShadowProps[PropIndex].Type == EPropertyType::Bool)
                {
                    bool* ValuePtr = static_cast<bool*>(ShadowProps[PropIndex].ValuePtr);
                    bChanged = ImGui::Checkbox("Cast Shadows", ValuePtr);
                }
                else
                {
                    bChanged = RenderDetailsPanel(ShadowProps, PropIndex);
                }

                if (bChanged)
                {
                    LightComponent->PostEditProperty(PropertyName);
                }
                return true;
            }
        }
        return false;
    };

    ImGui::Dummy(ImVec2(0.0f, 4.0f));
    RenderShadowPropertyByName("Cast Shadows");
    if (!LightComponent->DoesCastShadows())
    {
        return;
    }

    ImGui::Dummy(ImVec2(0.0f, 6.0f));
    ImGui::SeparatorText("Common");
    ImGui::Dummy(ImVec2(0.0f, 4.0f));

    FLightProxy* LightProxy = LightComponent->GetLightProxy();
    if (!LightProxy)
    {
        ImGui::TextDisabled("Light proxy is not ready.");
        return;
    }

    if (!GEngine)
    {
        ImGui::TextDisabled("Renderer is unavailable.");
        return;
    }

    FRenderPass* Pass = GEngine->GetRenderer().GetPassRegistry().FindPass(ERenderPassNodeType::ShadowMapPass);
    FShadowMapPass* ShadowPass = static_cast<FShadowMapPass*>(Pass);
    if (!ShadowPass)
    {
        ImGui::TextDisabled("Shadow pass is unavailable.");
        return;
    }

    const TArray<EShadowResolution> AllowedResolutionTiers = GetAllowedShadowResolutionTiers(LightProxy->LightProxyInfo.LightType);
    int32 ResolutionTierIndex = 0;
    for (int32 OptionIndex = 0; OptionIndex < static_cast<int32>(AllowedResolutionTiers.size()); ++OptionIndex)
    {
        if (AllowedResolutionTiers[OptionIndex] == LightComponent->GetShadowResolutionTier())
        {
            ResolutionTierIndex = OptionIndex;
            break;
        }
    }

    TArray<const char*> ResolutionLabels;
    ResolutionLabels.reserve(AllowedResolutionTiers.size());
    for (EShadowResolution ResolutionTier : AllowedResolutionTiers)
    {
        ResolutionLabels.push_back(GetShadowResolutionLabel(ResolutionTier));
    }

    if (ImGui::Combo("Shadow Resolution", &ResolutionTierIndex, ResolutionLabels.data(), static_cast<int32>(ResolutionLabels.size())))
    {
        ResolutionTierIndex = std::clamp(ResolutionTierIndex, 0, static_cast<int32>(AllowedResolutionTiers.size()) - 1);
        LightComponent->SetShadowResolution(AllowedResolutionTiers[ResolutionTierIndex]);
        LightComponent->PostEditProperty("Shadow Resolution");
    }
    ImGui::TextDisabled("Allowed Range: %u - %u",
        GetShadowResolutionValue(AllowedResolutionTiers.front()),
        GetShadowResolutionValue(AllowedResolutionTiers.back()));

    RenderShadowPropertyByName("Depth Bias");
    RenderShadowPropertyByName("Slope Bias");
    RenderShadowPropertyByName("Normal Bias");
    RenderShadowPropertyByName("Shadow Sharpen");
    RenderShadowPropertyByName("ESM Exponent");

    if (UDirectionalLightComponent* DirectionalLight = Cast<UDirectionalLightComponent>(LightComponent))
    {
        (void)DirectionalLight;
        ImGui::Dummy(ImVec2(0.0f, 6.0f));
        ImGui::SeparatorText("Cascade Settings");
        ImGui::Dummy(ImVec2(0.0f, 4.0f));
        RenderShadowPropertyByName("Cascade Count");
        RenderShadowPropertyByName("CSM Max Distance");
        RenderShadowPropertyByName("Cascade Distribution");
    }

    uint32 PreviewFace = 0;
    bool bIsCubeShadow = (LightProxy->LightProxyInfo.LightType == static_cast<uint32>(ELightType::Point));
    if (bIsCubeShadow)
    {
        static const char* FaceLabels[6] = { "World +X", "World -X", "World +Y", "World -Y", "World +Z", "World -Z" };
        static const char* FaceButtonLabels[6] = { "+X", "-X", "+Y", "-Y", "+Z", "-Z" };
        const int32 ClosestFace = GetClosestWorldAlignedPointShadowFace(EditorEngine);

        ImGui::TextDisabled("Point light shadow faces are world-aligned.");
        ImGui::TextDisabled("Closest to current view: %s", FaceLabels[ClosestFace]);
        ImGui::SameLine();
        if (ImGui::Button("Use Closest Face"))
        {
            SelectedPointLightShadowFace = ClosestFace;
        }

        static const int32 FaceButtonOrder[6] = { 0, 1, 2, 3, 4, 5 };
        const float AvailableWidth = ImGui::GetContentRegionAvail().x;
        const float ButtonSpacing = ImGui::GetStyle().ItemSpacing.x;
        const float FaceButtonWidth = (std::max)(64.0f, (AvailableWidth - ButtonSpacing) / 2.0f);

        ImGui::Text("Face");
        for (int32 ButtonIndex = 0; ButtonIndex < 6; ++ButtonIndex)
        {
            if ((ButtonIndex % 2) != 0)
            {
                ImGui::SameLine();
            }

            const int32 FaceIndex = FaceButtonOrder[ButtonIndex];
            const bool bSelectedFace = (SelectedPointLightShadowFace == FaceIndex);
            if (bSelectedFace)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
            }

            if (ImGui::Button(FaceButtonLabels[FaceIndex], ImVec2(FaceButtonWidth, 0.0f)))
            {
                SelectedPointLightShadowFace = FaceIndex;
            }

            if (bSelectedFace)
            {
                ImGui::PopStyleColor(2);
            }
        }

        PreviewFace = static_cast<uint32>(std::clamp(SelectedPointLightShadowFace, 0, 5));
    }

    const FShadowMapData* PreviewShadowData = nullptr;
    const FMatrix* PreviewViewProj = nullptr;
    if (bIsCubeShadow)
    {
        if (const FCubeShadowMapData* CubeShadowMapData = LightProxy->GetCubeShadowMapData())
        {
            PreviewShadowData = &CubeShadowMapData->Faces[PreviewFace];
            PreviewViewProj = &CubeShadowMapData->FaceViewProj[PreviewFace];
        }
    }
    else if (LightProxy->LightProxyInfo.LightType == static_cast<uint32>(ELightType::Directional))
    {
        if (const FCascadeShadowMapData* CascadeShadowMapData = LightProxy->GetCascadeShadowMapData())
        {
            const uint32 CascadeCount = std::min(CascadeShadowMapData->CascadeCount, static_cast<uint32>(ShadowAtlas::MaxCascades));
            if (CascadeCount > 0)
            {
                SelectedDirectionalCascade = std::clamp(SelectedDirectionalCascade, 0, static_cast<int32>(CascadeCount) - 1);
                ImGui::TextDisabled("Lower cascade index is closer to the camera. Cascade 0 is the nearest range.");
                ImGui::SetNextItemWidth(150.0f);
                ImGui::SliderInt("Cascade", &SelectedDirectionalCascade, 0, static_cast<int32>(CascadeCount) - 1);
                PreviewShadowData = &CascadeShadowMapData->Cascades[SelectedDirectionalCascade];
                PreviewViewProj = &CascadeShadowMapData->CascadeViewProj[SelectedDirectionalCascade];
            }
        }
    }
    else
    {
        PreviewShadowData = LightProxy->GetSpotShadowMapData();
        PreviewViewProj = &LightProxy->LightViewProj;
    }

    ImGui::Dummy(ImVec2(0.0f, 6.0f));
    const uint32 EvictionDebugResolution = (PreviewShadowData && PreviewShadowData->bAllocated) ? PreviewShadowData->Resolution : LightProxy->ShadowResolution;
    RenderShadowEvictionDebugInfo(*LightProxy, EvictionDebugResolution, GetLightCascadeCountForEvictionDebug(*LightProxy));

    ImGui::Dummy(ImVec2(0.0f, 6.0f));
    ImGui::SeparatorText("Shadow Map Preview");
    ImGui::Dummy(ImVec2(0.0f, 4.0f));

    const float MaxWidth = ImGui::GetContentRegionAvail().x;
    const float PreviewSize = (MaxWidth > 0.0f) ? (std::min)(MaxWidth, 256.0f) : 256.0f;

    if (!PreviewShadowData || !PreviewViewProj || !PreviewShadowData->bAllocated)
    {
        ImGui::TextDisabled("No shadow map assigned this frame.");
        ImGui::Dummy(ImVec2(PreviewSize, PreviewSize));
        return;
    }

    static const char* PreviewModeLabels[] = {
        "Raw Depth",
        "Linearized Depth",
        "Moments",
    };
    int32 PreviewModeIndex = static_cast<int32>(ShadowDepthPreviewMode);
    if (ImGui::Combo("Preview Mode", &PreviewModeIndex, PreviewModeLabels, IM_ARRAYSIZE(PreviewModeLabels)))
    {
        ShadowDepthPreviewMode = static_cast<EShadowDepthPreviewMode>(std::clamp(PreviewModeIndex, 0, 2));
    }

    ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
    ID3D11DeviceContext* DeviceContext = GEngine->GetRenderer().GetFD3DDevice().GetDeviceContext();

    ID3D11ShaderResourceView* PreviewSRV = nullptr;
    bool bPreviewUsesStandaloneTexture = false;
    if (ShadowDepthPreviewMode == EShadowDepthPreviewMode::Moments)
    {
        PreviewSRV = ShadowPass->GetShadowDebugPreviewSRV(
            *PreviewShadowData,
            *PreviewViewProj,
            ShadowDepthPreviewMode,
            LightProxy->ShadowESMExponent,
            Device,
            DeviceContext);
        bPreviewUsesStandaloneTexture = (PreviewSRV != nullptr);
    }
    else
    {
        if (ShadowDepthPreviewMode == EShadowDepthPreviewMode::LinearizedDepth)
        {
            PreviewSRV = ShadowPass->GetShadowDebugPreviewSRV(
                *PreviewShadowData,
                *PreviewViewProj,
                ShadowDepthPreviewMode,
                LightProxy->ShadowESMExponent,
                Device,
                DeviceContext);
            bPreviewUsesStandaloneTexture = (PreviewSRV != nullptr);
        }

        if (!PreviewSRV)
        {
            PreviewSRV = ShadowPass->GetShadowPreviewSRV(*PreviewShadowData);
        }
    }

    if (!PreviewSRV)
    {
        ImGui::TextDisabled("Shadow preview is unavailable.");
        ImGui::Dummy(ImVec2(PreviewSize, PreviewSize));
        return;
    }

    const ImVec2 PreviewMin = ImGui::GetCursorScreenPos();
    const ImVec2 PreviewMax = ImVec2(PreviewMin.x + PreviewSize, PreviewMin.y + PreviewSize);
    ImDrawList* DrawList = ImGui::GetWindowDrawList();
    DrawList->AddImageQuad(
        reinterpret_cast<ImTextureID>(PreviewSRV),
        PreviewMin,
        ImVec2(PreviewMax.x, PreviewMin.y),
        PreviewMax,
        ImVec2(PreviewMin.x, PreviewMax.y),
        bPreviewUsesStandaloneTexture
            ? ImVec2(0.0f, 0.0f)
            : ImVec2(PreviewShadowData->UVScaleOffset.Z, PreviewShadowData->UVScaleOffset.W),
        bPreviewUsesStandaloneTexture
            ? ImVec2(1.0f, 0.0f)
            : ImVec2(PreviewShadowData->UVScaleOffset.Z + PreviewShadowData->UVScaleOffset.X,
                     PreviewShadowData->UVScaleOffset.W),
        bPreviewUsesStandaloneTexture
            ? ImVec2(1.0f, 1.0f)
            : ImVec2(PreviewShadowData->UVScaleOffset.Z + PreviewShadowData->UVScaleOffset.X,
                     PreviewShadowData->UVScaleOffset.W + PreviewShadowData->UVScaleOffset.Y),
        bPreviewUsesStandaloneTexture
            ? ImVec2(0.0f, 1.0f)
            : ImVec2(PreviewShadowData->UVScaleOffset.Z,
                     PreviewShadowData->UVScaleOffset.W + PreviewShadowData->UVScaleOffset.Y));
    ImGui::Dummy(ImVec2(PreviewSize, PreviewSize));

    const FLevelEditorViewportClient* ActiveViewport = EditorEngine ? EditorEngine->GetActiveViewport() : nullptr;
    const UCameraComponent* ActiveCamera = ActiveViewport ? ActiveViewport->GetCamera() : (EditorEngine ? EditorEngine->GetCamera() : nullptr);
    const uint64 ShadowViewProjHash = HashShadowViewProj(*PreviewViewProj);
    const uint64 LightViewProjHash = HashShadowViewProj(LightProxy->LightViewProj);
    const uint64 CameraViewProjHash = ActiveCamera ? HashShadowViewProj(ActiveCamera->GetViewProjectionMatrix()) : 0ull;

    ImGui::TextDisabled("Atlas Page: %u  Slice: %u", PreviewShadowData->AtlasPageIndex, PreviewShadowData->SliceIndex);
    ImGui::TextDisabled(
        "Rect: (%u, %u) %u x %u",
        PreviewShadowData->Rect.X,
        PreviewShadowData->Rect.Y,
        PreviewShadowData->Rect.Width,
        PreviewShadowData->Rect.Height);
    ImGui::TextDisabled(
        "Viewport: (%u, %u) %u x %u",
        PreviewShadowData->ViewportRect.X,
        PreviewShadowData->ViewportRect.Y,
        PreviewShadowData->ViewportRect.Width,
        PreviewShadowData->ViewportRect.Height);
    ImGui::TextDisabled("Camera ViewProj Hash: 0x%016llX", CameraViewProjHash);
    ImGui::TextDisabled("Shadow ViewProj Hash: 0x%016llX", ShadowViewProjHash);
    ImGui::TextDisabled("Light ViewProj Hash:  0x%016llX", LightViewProjHash);
}

void FEditorDetailsPanel::RenderShadowAtlasDebugWindow()
{
    RenderEditorShadowAtlasDebugWindow(*this);
}

bool FEditorDetailsPanel::RenderDetailsPanel(TArray<FPropertyDescriptor>& Props, int32& Index)
{
    ImGui::PushID(Index);
    FPropertyDescriptor& Prop = Props[Index];
    bool bChanged = false;
    const FString DisplayName = GetEditorFriendlyPropertyName(Prop.Name);
    const FString WidgetLabel = DisplayName + "##" + Prop.Name;

    switch (Prop.Type)
    {
    case EPropertyType::Bool:
    {
        bool* Val = static_cast<bool*>(Prop.ValuePtr);
        bChanged = ImGui::Checkbox(WidgetLabel.c_str(), Val);
        break;
    }
    case EPropertyType::ByteBool:
    {
        uint8* Val = static_cast<uint8*>(Prop.ValuePtr);
        bool bVal = (*Val != 0);
        if (ImGui::Checkbox(WidgetLabel.c_str(), &bVal))
        {
            *Val = bVal ? 1 : 0;
            bChanged = true;
        }
        break;
    }
    case EPropertyType::Int:
    {
        int32* Val = static_cast<int32*>(Prop.ValuePtr);
        bChanged = ImGui::DragInt(WidgetLabel.c_str(), Val);
        break;
    }
    case EPropertyType::Enum:
    {
        int32* Val = static_cast<int32*>(Prop.ValuePtr);
        const FEnumPropertyMeta* Meta = Prop.EnumMeta;
        const char* Preview = "Unknown";

        if (Meta)
        {
            for (int32 OptionIndex = 0; OptionIndex < Meta->NumOptions; ++OptionIndex)
            {
                const FEnumPropertyOption& Option = Meta->Options[OptionIndex];
                if (Option.Value == *Val)
                {
                    Preview = Option.Label;
                    break;
                }
            }
        }

        ImGui::Text("%s", DisplayName.c_str());
        ImGui::SameLine(120);
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::BeginCombo(("##enum_" + Prop.Name).c_str(), Preview))
        {
            if (Meta)
            {
                for (int32 OptionIndex = 0; OptionIndex < Meta->NumOptions; ++OptionIndex)
                {
                    const FEnumPropertyOption& Option = Meta->Options[OptionIndex];
                    const bool bSelected = (Option.Value == *Val);
                    if (ImGui::Selectable(Option.Label, bSelected))
                    {
                        *Val = Option.Value;
                        bChanged = true;
                    }

                    if (bSelected)
                    {
                        ImGui::SetItemDefaultFocus();
                    }
                }
            }
            ImGui::EndCombo();
        }
        break;
    }
    case EPropertyType::Float:
    {
        float* Val = static_cast<float*>(Prop.ValuePtr);
        if (Prop.Min != 0.0f || Prop.Max != 0.0f)
            bChanged = ImGui::DragFloat(WidgetLabel.c_str(), Val, Prop.Speed, Prop.Min, Prop.Max);
        else
            bChanged = ImGui::DragFloat(WidgetLabel.c_str(), Val, Prop.Speed);
        break;
    }
    case EPropertyType::Vec2:
    {
        float* Val = static_cast<float*>(Prop.ValuePtr);
        bChanged = ImGui::DragFloat2(WidgetLabel.c_str(), Val, Prop.Speed);
        break;
    }
    case EPropertyType::Vec3:
    {
        float* Val = static_cast<float*>(Prop.ValuePtr);
        bChanged = ImGui::DragFloat3(WidgetLabel.c_str(), Val, Prop.Speed);
        break;
    }
    case EPropertyType::Rotator:
    {
        // FRotator 메모리 레이아웃 [Pitch,Yaw,Roll] → UI X=Roll(X축), Y=Pitch(Y축), Z=Yaw(Z축)
        FRotator* Rot = static_cast<FRotator*>(Prop.ValuePtr);
        float RotXYZ[3] = { Rot->Roll, Rot->Pitch, Rot->Yaw };
        bChanged = ImGui::DragFloat3(WidgetLabel.c_str(), RotXYZ, Prop.Speed);
        if (bChanged)
        {
            Rot->Roll = RotXYZ[0];
            Rot->Pitch = RotXYZ[1];
            Rot->Yaw = RotXYZ[2];
            if (SelectedComponent && SelectedComponent->IsA<USceneComponent>())
            {
                static_cast<USceneComponent*>(SelectedComponent)->ApplyCachedEditRotator();
            }
        }
        break;
    }
    case EPropertyType::Vec4:
    {
        float* Val = static_cast<float*>(Prop.ValuePtr);
        bChanged = ImGui::DragFloat4(WidgetLabel.c_str(), Val, Prop.Speed);
        break;
    }
    case EPropertyType::Color4:
    {
        float* Val = static_cast<float*>(Prop.ValuePtr);
        bChanged = ImGui::ColorEdit4(WidgetLabel.c_str(), Val);
        break;
    }
    case EPropertyType::String:
    {
        FString* Val = static_cast<FString*>(Prop.ValuePtr);
        const bool bAcceptLuaScriptDrop = Prop.Name == "Script";
        const bool bAcceptContentAssetDrop = Prop.Name == "Texture Path";
        char Buf[512];
        strncpy_s(Buf, sizeof(Buf), Val->c_str(), _TRUNCATE);
        if (ImGui::InputText(WidgetLabel.c_str(), Buf, sizeof(Buf)))
        {
            *Val = Buf;
            bChanged = true;
        }
        if ((bAcceptLuaScriptDrop || bAcceptContentAssetDrop) && ImGui::BeginDragDropTarget())
        {
            if (bAcceptLuaScriptDrop)
            {
                if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload(LuaScriptPayloadType))
                {
                    const char* ScriptPath = static_cast<const char*>(Payload->Data);
                    if (ScriptPath && Payload->DataSize > 0)
                    {
                        *Val = ScriptPath;
                        bChanged = true;
                    }
                }
            }
            if (bAcceptContentAssetDrop)
            {
                if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload(ContentAssetPayloadType))
                {
                    const char* AssetPath = static_cast<const char*>(Payload->Data);
                    if (AssetPath && Payload->DataSize > 0)
                    {
                        *Val = AssetPath;
                        bChanged = true;
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }
        break;
    }
    case EPropertyType::StaticMeshRef:
    {
        FString* Val = static_cast<FString*>(Prop.ValuePtr);

        FString Preview = Val->empty() ? "None" : GetStemFromPath(*Val);
        if (*Val == "None")
            Preview = "None";

        ImGui::Text("%s", DisplayName.c_str());
        ImGui::SameLine(120);
        ImGui::SetNextItemWidth(-1.0f);

        float ButtonWidth = ImGui::CalcTextSize("Import OBJ").x + ImGui::GetStyle().FramePadding.x * 2.0f;

        if (ImGui::BeginCombo("##Mesh", Preview.c_str()))
        {
            FObjManager::Get().ScanMeshCacheFiles();
            FObjManager::Get().ScanObjSourceFiles();

            bool bSelectedNone = (*Val == "None");
            if (ImGui::Selectable("None", bSelectedNone))
            {
                *Val = "None";
                bChanged = true;
            }
            if (bSelectedNone)
                ImGui::SetItemDefaultFocus();

            ImGui::TextDisabled("OBJ Source");
            const TArray<FMeshAssetListItem>& ObjFiles = FObjManager::Get().GetAvailableObjFiles();
            for (const FMeshAssetListItem& Item : ObjFiles)
            {
                const FString Label = Item.DisplayName + "##obj_" + Item.FullPath;
                bool bSelected = (*Val == Item.FullPath);
                if (ImGui::Selectable(Label.c_str(), bSelected))
                {
                    *Val = Item.FullPath;
                    bChanged = true;
                }
                if (bSelected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::Separator();
            ImGui::TextDisabled("Cached Mesh");
            const TArray<FMeshAssetListItem>& MeshFiles = FObjManager::Get().GetAvailableMeshFiles();
            for (const FMeshAssetListItem& Item : MeshFiles)
            {
                const FString Label = Item.DisplayName + "##bin_" + Item.FullPath;
                bool bSelected = (*Val == Item.FullPath);
                if (ImGui::Selectable(Label.c_str(), bSelected))
                {
                    *Val = Item.FullPath;
                    bChanged = true;
                }
                if (bSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        // .obj 임포트 버튼은 다음 줄 우측 정렬
        ImGui::Dummy(ImVec2(0.0f, ImGui::GetStyle().ItemSpacing.y));
        ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMax().x - ButtonWidth);
        if (ImGui::Button("Import OBJ"))
        {
            FString ObjPath = OpenObjFileDialog();
            if (!ObjPath.empty())
            {
                ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
                UStaticMesh* Loaded = FObjManager::Get().Load(ObjPath);
                if (Loaded)
                {
                    *Val = ObjPath;
                    bChanged = true;
                }
            }
        }
        break;
    }
    case EPropertyType::SkeletalMeshRef:
    {
        FString* Val = static_cast<FString*>(Prop.ValuePtr);

        FString Preview = Val->empty() ? "None" : GetStemFromPath(*Val);
        if (*Val == "None")
            Preview = "None";

        ImGui::Text("%s", DisplayName.c_str());
        ImGui::SameLine(120);
        ImGui::SetNextItemWidth(-1.0f);

        if (ImGui::BeginCombo("##SkeletalMesh", Preview.c_str()))
        {
            FSkeletalMeshManager::Get().ScanFBXSourceFiles();
            FSkeletalMeshManager::Get().ScanMeshCacheFiles();

            bool bSelectedNone = (*Val == "None");
            if (ImGui::Selectable("None", bSelectedNone))
            {
                *Val = "None";
                bChanged = true;
            }
            if (bSelectedNone)
                ImGui::SetItemDefaultFocus();

            ImGui::TextDisabled("FBX Source");
            const TArray<FSkeletalMeshAssetListItem>& FBXFiles = FSkeletalMeshManager::Get().GetAvailableFBXFiles();
            for (const FSkeletalMeshAssetListItem& Item : FBXFiles)
            {
                const FString Label = Item.DisplayName + "##fbx_" + Item.FullPath;
                bool bSelected = (*Val == Item.FullPath);
                if (ImGui::Selectable(Label.c_str(), bSelected))
                {
                    *Val = Item.FullPath;
                    bChanged = true;
                }
                if (bSelected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::Separator();
            ImGui::TextDisabled("Skeletal Mesh");
            const TArray<FSkeletalMeshAssetListItem>& SkelMeshFiles = FSkeletalMeshManager::Get().GetAvailableMeshFiles();
            for (const FSkeletalMeshAssetListItem& Item : SkelMeshFiles)
            {
                const FString Label = Item.DisplayName + "##skelmesh_" + Item.FullPath;
                bool bSelected = (*Val == Item.FullPath);
                if (ImGui::Selectable(Label.c_str(), bSelected))
                {
                    *Val = Item.FullPath;
                    bChanged = true;
                }
                if (bSelected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::Separator();
            ImGui::TextDisabled("Skeletons");
            const TArray<FSkeletalMeshAssetListItem>& SkelFiles = FSkeletalMeshManager::Get().GetAvailableSkeletonFiles();
            for (const FSkeletalMeshAssetListItem& Item : SkelFiles)
            {
                const FString Label = Item.DisplayName + "##skeleton_" + Item.FullPath;
                bool bSelected = (*Val == Item.FullPath);
                if (ImGui::Selectable(Label.c_str(), bSelected))
                {
                    *Val = Item.FullPath;
                    bChanged = true;
                }
                if (bSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        break;
    }
    case EPropertyType::ComponentRef:
    {
        FString* Val = static_cast<FString*>(Prop.ValuePtr);

        // 현재 선택 라벨 계산
        FString Preview = "None";
        AActor* RefActor = SelectedComponent ? SelectedComponent->GetOwner() : nullptr;
        if (RefActor && RefActor->GetRootComponent())
        {
            if (Val->empty())
            {
                Preview = "(auto)";
            }
            else if (*Val == "Root")
            {
                Preview = FString("Root - ") + RefActor->GetRootComponent()->GetClass()->GetName();
            }
            else
            {
                // 경로 탐색으로 라벨 구성
                USceneComponent* Resolved = RefActor->GetRootComponent();
                size_t Start = 0;
                bool bValid = true;
                while (Start < Val->size() && bValid)
                {
                    size_t End = Val->find('/', Start);
                    size_t Len = (End == FString::npos) ? (Val->size() - Start) : (End - Start);
                    int32 Idx = std::stoi(Val->substr(Start, Len));
                    const TArray<USceneComponent*>& Ch = Resolved->GetChildren();
                    if (Idx >= 0 && Idx < static_cast<int32>(Ch.size()))
                        Resolved = Ch[Idx];
                    else
                        bValid = false;
                    Start = (End == FString::npos) ? Val->size() : (End + 1);
                }
                Preview = bValid ? (*Val + " - " + Resolved->GetClass()->GetName()) : "(invalid)";
            }
        }

        ImGui::Text("%s", DisplayName.c_str());
        ImGui::SameLine(140);
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::BeginCombo(("##compref_" + Prop.Name).c_str(), Preview.c_str()))
        {
            // "(auto)" 선택지
            bool bAutoSel = Val->empty();
            if (ImGui::Selectable("(auto)", bAutoSel))
            {
                Val->clear();
                bChanged = true;
            }
            if (bAutoSel)
                ImGui::SetItemDefaultFocus();

            if (RefActor && RefActor->GetRootComponent())
            {
                // DFS로 SceneComponent 열거
                struct FEntry
                {
                    FString Path;
                    FString Label;
                };
                TArray<FEntry> Entries;
                std::function<void(USceneComponent*, const FString&, int32)> Collect =
                    [&](USceneComponent* Comp, const FString& CurPath, int32 Depth)
                {
                    if (!Comp)
                        return;
                    FString Indent(static_cast<size_t>(Depth * 2), ' ');
                    Entries.push_back({ CurPath, Indent + Comp->GetClass()->GetName() });
                    const TArray<USceneComponent*>& Ch = Comp->GetChildren();
                    for (int32 ci = 0; ci < static_cast<int32>(Ch.size()); ++ci)
                    {
                        FString ChildPath = (CurPath == "Root")
                                                ? std::to_string(ci)
                                                : (CurPath + "/" + std::to_string(ci));
                        Collect(Ch[ci], ChildPath, Depth + 1);
                    }
                };
                Collect(RefActor->GetRootComponent(), "Root", 0);

                for (const FEntry& Entry : Entries)
                {
                    bool bSel = (*Val == Entry.Path);
                    FString SelectLabel = Entry.Label + "##" + Entry.Path;
                    if (ImGui::Selectable(SelectLabel.c_str(), bSel))
                    {
                        *Val = Entry.Path;
                        bChanged = true;
                    }
                    if (bSel)
                        ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        break;
    }
    case EPropertyType::MaterialSlot:
    {
        FMaterialSlot* Slot = static_cast<FMaterialSlot*>(Prop.ValuePtr);
        const bool bHasElementPrefix = (strncmp(Prop.Name.c_str(), "Element ", 8) == 0);
        const int32 ElemIdx = bHasElementPrefix ? atoi(&Prop.Name[8]) : -1;

        FString SlotName = "None";
        if (ElemIdx != -1 && SelectedComponent && SelectedComponent->IsA<UStaticMeshComponent>())
        {
            UStaticMeshComponent* SMC = static_cast<UStaticMeshComponent*>(SelectedComponent);
            if (SMC->GetStaticMesh() && ElemIdx < (int32)SMC->GetStaticMesh()->GetStaticMaterials().size())
                SlotName = SMC->GetStaticMesh()->GetStaticMaterials()[ElemIdx].MaterialSlotName;
        }
        else if (ElemIdx != -1 && SelectedComponent && SelectedComponent->IsA<USkeletalMeshComponent>())
        {
            USkeletalMeshComponent* SKC = static_cast<USkeletalMeshComponent*>(SelectedComponent);
            if (USkeletalMesh* SkeletalMesh = SKC->GetSkeletalMesh())
            {
                int32 GlobalSlotIndex = 0;
                for (USkeletalSubMesh* SubMesh : SkeletalMesh->GetSubMeshes())
                {
                    if (!SubMesh)
                    {
                        continue;
                    }

                    const TArray<FStaticMaterial>& StaticMaterials = SubMesh->GetStaticMaterials();
                    if (ElemIdx < GlobalSlotIndex + static_cast<int32>(StaticMaterials.size()))
                    {
                        SlotName = StaticMaterials[ElemIdx - GlobalSlotIndex].MaterialSlotName;
                        break;
                    }

                    GlobalSlotIndex += static_cast<int32>(StaticMaterials.size());
                }
            }
        }

        const bool bIsArrayElementSlot = (ElemIdx != -1);

        // 좌측: Element 인덱스 + 슬롯 이름, 또는 단일 Material 라벨
        ImGui::BeginGroup();
        if (bIsArrayElementSlot)
        {
            ImGui::Text("Element %d", ElemIdx);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
            ImGui::TextUnformatted(SlotName.c_str());
            ImGui::PopStyleColor();
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", SlotName.c_str());
        }
        else
        {
            ImGui::TextUnformatted(DisplayName.c_str());
        }
        ImGui::EndGroup();

        ImGui::SameLine(120);

        // 우측: Material 콤보
        ImGui::BeginGroup();
        ImGui::SetNextItemWidth(-1);

        FString Preview = (Slot->Path.empty() || Slot->Path == "None") ? "None" : GetStemFromPath(Slot->Path);
        FString ComboId = bIsArrayElementSlot ? "##Mat" : "##SingleMaterial";
        if (ImGui::BeginCombo(ComboId.c_str(), Preview.c_str()))
        {
            FMaterialManager::Get().ScanMaterialAssets();

            // "None" 선택지 기본 제공
            bool bSelectedNone = (Slot->Path == "None" || Slot->Path.empty());
            if (ImGui::Selectable("None", bSelectedNone))
            {
                Slot->Path = "None";
                bChanged = true;
            }
            if (bSelectedNone)
                ImGui::SetItemDefaultFocus();

            // TObjectIterator 대신 FMaterialManager 파일 목록 스캔 데이터 사용
            const TArray<FMaterialAssetListItem>& MatFiles = FMaterialManager::Get().GetAvailableRuntimeMaterialFiles();
            for (const FMaterialAssetListItem& Item : MatFiles)
            {
                bool bSelected = (Slot->Path == Item.FullPath);
                if (ImGui::Selectable(Item.DisplayName.c_str(), bSelected))
                {
                    Slot->Path = Item.FullPath; // 데이터는 전체 경로로 저장
                    bChanged = true;
                }
                if (bSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::EndGroup();
        break;
    }
    case EPropertyType::Name:
    {
        FName* Val = static_cast<FName*>(Prop.ValuePtr);
        FString Current = Val->ToString();

        // 리소스 키와 매칭되는 프로퍼티면 콤보 박스로 렌더링
        TArray<FString> Names;
        if (strcmp(Prop.Name.c_str(), "Font") == 0)
            Names = FFontManager::Get().GetFontNames();
        else if (strcmp(Prop.Name.c_str(), "SpriteVfx") == 0)
            Names = FSpriteVfxManager::Get().GetSpriteVfxNames();
        else if (strcmp(Prop.Name.c_str(), "Texture") == 0)
            Names = FTextureManager::Get().GetTextureNames();

        if (!Names.empty())
        {
            if (ImGui::BeginCombo(WidgetLabel.c_str(), Current.c_str()))
            {
                for (const auto& Name : Names)
                {
                    bool bSelected = (Current == Name);
                    if (ImGui::Selectable(Name.c_str(), bSelected))
                    {
                        *Val = FName(Name);
                        bChanged = true;
                    }
                    if (bSelected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
        }
        else
        {
            char Buf[256];
            strncpy_s(Buf, sizeof(Buf), Current.c_str(), _TRUNCATE);
            if (ImGui::InputText(WidgetLabel.c_str(), Buf, sizeof(Buf)))
            {
                *Val = FName(Buf);
                bChanged = true;
            }
        }
        break;
    }
    }

    if (bChanged && SelectedComponent)
    {
        SelectedComponent->PostEditProperty(Prop.Name.c_str());
    }

    ImGui::PopID();
    return bChanged;
}
