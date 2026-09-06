// 에디터 영역의 세부 동작을 구현합니다.
#include "Editor/UI/EditorToolbarPanel.h"

#include "Component/CameraComponent.h"
#include "Component/GizmoComponent.h"
#include "Component/SceneComponent.h"
#include "Editor/EditorEngine.h"
#include "Editor/PIE/PIETypes.h"
#include "Editor/Selection/SelectionManager.h"
#include "Editor/Viewport/FLevelViewportLayout.h"
#include "Editor/Viewport/LevelEditorViewportClient.h"
#include "GameFramework/AActor.h"
#include "GameFramework/AmbientLightActor.h"
#include "GameFramework/ButtonActor.h"
#include "GameFramework/DecalActor.h"
#include "GameFramework/DirectionalLightActor.h"
#include "GameFramework/FakeLightActor.h"
#include "GameFramework/FireballActor.h"
#include "GameFramework/HeightFogActor.h"
#include "GameFramework/PointLightActor.h"
#include "GameFramework/SpotLightActor.h"
#include "GameFramework/StaticMeshActor.h"
#include "GameFramework/GamejamActor/EnemySpawnerActor.h"
#include "GameFramework/TextUIActor.h"
#include "GameFramework/TextureUIActor.h"
#include "GameFramework/UIActor.h"
#include "GameFramework/World.h"
#include "GameFramework/TankActor.h"
#include "ImGui/imgui.h"
#include "Math/MathUtils.h"
#include "Platform/Paths.h"
#include "Render/Resources/Shadows/ShadowFilterSettings.h"
#include "Render/Resources/Shadows/ShadowMapSettings.h"
#include "Render/Submission/Atlas/ShadowResolutionPolicy.h"
#include "WICTextureLoader.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <d3d11.h>
#include <filesystem>
#include <string>
#include <utility>

#include "GameFramework/ExpBarActor.h"
#include "Engine/Classes/Camera/CameraManager.h"

namespace
{
template <typename T>
void BeginDisabledUnless(bool bEnabled, T&& Fn)
{
    if (!bEnabled)
    {
        ImGui::BeginDisabled();
    }

    Fn();

    if (!bEnabled)
    {
        ImGui::EndDisabled();
    }
}

ID3D11ShaderResourceView* GPlayStartIcon = nullptr;
ID3D11ShaderResourceView* GPauseIcon = nullptr;
ID3D11ShaderResourceView* GStopIcon = nullptr;
ID3D11ShaderResourceView* GToolbarDirectionalLightIcon = nullptr;
ID3D11ShaderResourceView* GToolbarPointLightIcon = nullptr;
ID3D11ShaderResourceView* GToolbarSpotLightIcon = nullptr;
ID3D11ShaderResourceView* GToolbarAmbientLightIcon = nullptr;
ID3D11ShaderResourceView* GToolbarDecalIcon = nullptr;
ID3D11ShaderResourceView* GToolbarHeightFogIcon = nullptr;
char GAddActorSearch[128] = {};

enum class EAddActorIcon
{
    Empty,
    Cube,
    Sphere,
    AmbientLight,
    DirectionalLight,
    PointLight,
    SpotLight,
    Decal,
    HeightFog,
    UI,
    Effect
};

struct FAddActorEntry
{
    const char* Label;
    EAddActorIcon Icon;
    AActor* (*Spawn)(UWorld* World, const FVector& SpawnPoint);
};

template <typename TActor, typename... TArgs>
AActor* SpawnToolbarActor(UWorld* World, const FVector& SpawnPoint, bool bInsertToOctree, TArgs&&... Args)
{
    if (!World)
    {
        return nullptr;
    }

    TActor* Actor = World->SpawnActor<TActor>(std::forward<TArgs>(Args)...);
    if (!Actor)
    {
        return nullptr;
    }

    Actor->SetActorLocation(SpawnPoint);

    if (bInsertToOctree)
    {
        World->InsertActorToOctree(Actor);
    }

    return Actor;
}

AActor* SpawnEmptyToolbarActor(UWorld* World, const FVector& SpawnPoint)
{
    if (!World)
    {
        return nullptr;
    }

    AActor* Actor = World->SpawnActor<AActor>();
    if (!Actor)
    {
        return nullptr;
    }

    USceneComponent* RootComponent = Actor->AddComponent<USceneComponent>();
    if (RootComponent)
    {
        Actor->SetRootComponent(RootComponent);
    }

    Actor->SetActorLocation(SpawnPoint);
    return Actor;
}

// Add Actor 메뉴 등록 가이드
// 1. 새 Actor 헤더를 이 파일 상단에 include한다.
// 2. 메뉴에 보여줄 아이콘이 기존 분류와 맞지 않으면 EAddActorIcon에 타입을 추가하고 DrawAddActorIcon()에 그리기 처리를 추가한다.
// 3. 아래 BasicAddActors / LightAddActors / EffectAddActors 중 맞는 카테고리에 한 줄 추가한다.
//    - 일반 Actor: ADD_ACTOR("메뉴 이름", EAddActorIcon::아이콘, AMyActor, bInsertToOctree)
//    - StaticMesh 프리셋: ADD_MESH("메뉴 이름", EAddActorIcon::아이콘, "모델 경로")
//    - InitDefaultComponents 인자가 다르거나 추가 세팅이 필요하면 Empty Actor처럼 직접 lambda를 작성한다.
// 4. bInsertToOctree는 viewport picking/공간 관리가 필요한 primitive actor면 true, light/fog/helper처럼 별도 proxy 중심이면 false로 둔다.
// Outliner 전용 아이콘/분류는 EditorWorldOutlinerPanel.cpp의 ResolveActorIconType()에서 별도로 맞춘다.
#define ADD_MESH(Label, Icon, Path) { Label, Icon, [](UWorld* W, const FVector& P) -> AActor* { return SpawnToolbarActor<AStaticMeshActor>(W, P, true, Path); } }
#define ADD_ACTOR(Label, Icon, Type, bOctree) { Label, Icon, [](UWorld* W, const FVector& P) -> AActor* { return SpawnToolbarActor<Type>(W, P, bOctree); } }

constexpr FAddActorEntry BasicAddActors[] = {
    { "Empty Actor", EAddActorIcon::Empty, [](UWorld* W, const FVector& P) -> AActor* { return SpawnEmptyToolbarActor(W, P); } },
    ADD_MESH("Cube", EAddActorIcon::Cube, FPaths::ContentRelativePath("Models/_Basic/Cube.OBJ")),
    ADD_MESH("Sphere", EAddActorIcon::Sphere, FPaths::ContentRelativePath("Models/_Basic/Sphere.OBJ")),
    ADD_ACTOR("UI", EAddActorIcon::UI, AUIActor, false),
    ADD_ACTOR("Texture UI", EAddActorIcon::UI, ATextureUIActor, false),
    ADD_ACTOR("Text UI", EAddActorIcon::UI, ATextUIActor, false),
    ADD_ACTOR("Button", EAddActorIcon::UI, AButtonActor, false),
};

constexpr FAddActorEntry LightAddActors[] = {
    ADD_ACTOR("Ambient Light", EAddActorIcon::AmbientLight, AAmbientLightActor, false),
    ADD_ACTOR("Directional Light", EAddActorIcon::DirectionalLight, ADirectionalLightActor, false),
    ADD_ACTOR("Point Light", EAddActorIcon::PointLight, APointLightActor, false),
    ADD_ACTOR("Spot Light", EAddActorIcon::SpotLight, ASpotLightActor, false),
    ADD_ACTOR("Fake Light", EAddActorIcon::PointLight, AFakeLightActor, false),
};

constexpr FAddActorEntry EffectAddActors[] = {
    ADD_ACTOR("Decal", EAddActorIcon::Decal, ADecalActor, true),
    ADD_ACTOR("Height Fog", EAddActorIcon::HeightFog, AHeightFogActor, false),
    ADD_ACTOR("Fireball", EAddActorIcon::Effect, AFireballActor, false),
};

constexpr FAddActorEntry GameAddActors[] = {
    ADD_ACTOR("Enemy Spawner", EAddActorIcon::Empty, AEnemySpawnerActor, false),
    ADD_ACTOR("Tank", EAddActorIcon::Empty, ATankActor, false),
	ADD_ACTOR("Exp Bar UI", EAddActorIcon::Empty, AExpBarActor, false),
    ADD_ACTOR("CameraManager", EAddActorIcon::Empty, APlayerCameraManager, false),
};

#undef ADD_MESH
#undef ADD_ACTOR

void SetFixedPopupPosBelowLastItem(float YOffset)
{
    const ImVec2 ItemMin = ImGui::GetItemRectMin();
    ImGui::SetNextWindowPos(ImVec2(ItemMin.x, ItemMin.y + YOffset), ImGuiCond_Appearing);
}

std::wstring ResolveEditorIconPath(const std::wstring& FileName)
{
    const std::filesystem::path RootCandidate = std::filesystem::path(FPaths::RootDir()) / L"Asset/Editor/Icons" / FileName;
    if (std::filesystem::exists(RootCandidate))
    {
        return RootCandidate.wstring();
    }

    WCHAR Buffer[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, Buffer, MAX_PATH);
    const std::filesystem::path ExeDir = std::filesystem::path(Buffer).parent_path();
    const std::filesystem::path ExeCandidate = ExeDir / L"Asset/Editor/Icons" / FileName;
    if (std::filesystem::exists(ExeCandidate))
    {
        return ExeCandidate.wstring();
    }

    return (std::filesystem::current_path() / L"Asset/Editor/Icons" / FileName).wstring();
}

void LoadToolbarIcon(ID3D11Device* Device, const std::wstring& FileName, ID3D11ShaderResourceView** OutIcon)
{
    if (!Device || !OutIcon || *OutIcon)
    {
        return;
    }

    DirectX::CreateWICTextureFromFile(
        Device,
        ResolveEditorIconPath(FileName).c_str(),
        nullptr,
        OutIcon);
}

void ReleaseToolbarIcon(ID3D11ShaderResourceView*& Icon)
{
    if (Icon)
    {
        Icon->Release();
        Icon = nullptr;
    }
}

std::string ToLowerAscii(const char* Text)
{
    std::string Lower = Text ? Text : "";
    std::transform(Lower.begin(), Lower.end(), Lower.begin(), [](unsigned char Ch)
                   {
        return static_cast<char>(std::tolower(Ch));
    });
    return Lower;
}

bool AddActorMatchesFilter(const FAddActorEntry& Entry, const char* Filter)
{
    const std::string Needle = ToLowerAscii(Filter);
    if (Needle.empty())
    {
        return true;
    }

    return ToLowerAscii(Entry.Label).find(Needle) != std::string::npos;
}

ID3D11ShaderResourceView* GetAddActorIconTexture(EAddActorIcon Icon)
{
    switch (Icon)
    {
    case EAddActorIcon::AmbientLight:
        return GToolbarAmbientLightIcon;
    case EAddActorIcon::DirectionalLight:
        return GToolbarDirectionalLightIcon;
    case EAddActorIcon::PointLight:
        return GToolbarPointLightIcon;
    case EAddActorIcon::SpotLight:
        return GToolbarSpotLightIcon;
    case EAddActorIcon::Decal:
        return GToolbarDecalIcon;
    case EAddActorIcon::HeightFog:
        return GToolbarHeightFogIcon;
    default:
        return nullptr;
    }
}

void DrawCubeGlyph(ImDrawList* DrawList, const ImVec2& Min, const ImVec2& Max, ImU32 Color)
{
    const float W = Max.x - Min.x;
    const float H = Max.y - Min.y;
    const ImVec2 FrontMin(Min.x + W * 0.25f, Min.y + H * 0.34f);
    const ImVec2 FrontMax(Min.x + W * 0.70f, Min.y + H * 0.78f);
    const ImVec2 Offset(W * 0.16f, -H * 0.14f);

    DrawList->AddRect(FrontMin, FrontMax, Color, 1.5f, 0, 1.6f);
    DrawList->AddLine(FrontMin, ImVec2(FrontMin.x + Offset.x, FrontMin.y + Offset.y), Color, 1.6f);
    DrawList->AddLine(ImVec2(FrontMax.x, FrontMin.y), ImVec2(FrontMax.x + Offset.x, FrontMin.y + Offset.y), Color, 1.6f);
    DrawList->AddLine(ImVec2(FrontMax.x, FrontMax.y), ImVec2(FrontMax.x + Offset.x, FrontMax.y + Offset.y), Color, 1.6f);
    DrawList->AddLine(ImVec2(FrontMin.x + Offset.x, FrontMin.y + Offset.y),
                      ImVec2(FrontMax.x + Offset.x, FrontMin.y + Offset.y),
                      Color,
                      1.6f);
    DrawList->AddLine(ImVec2(FrontMax.x + Offset.x, FrontMin.y + Offset.y),
                      ImVec2(FrontMax.x + Offset.x, FrontMax.y + Offset.y),
                      Color,
                      1.6f);
}

void DrawSphereGlyph(ImDrawList* DrawList, const ImVec2& Min, const ImVec2& Max, ImU32 Color)
{
    const ImVec2 Center((Min.x + Max.x) * 0.5f, (Min.y + Max.y) * 0.5f);
    const float Radius = (Max.x - Min.x) * 0.27f;
    DrawList->AddCircle(Center, Radius, Color, 28, 1.6f);
    DrawList->AddLine(ImVec2(Center.x - Radius * 0.85f, Center.y), ImVec2(Center.x + Radius * 0.85f, Center.y), Color, 1.2f);
    DrawList->AddLine(ImVec2(Center.x, Center.y - Radius * 0.85f), ImVec2(Center.x, Center.y + Radius * 0.85f), Color, 1.2f);
}

void DrawEffectGlyph(ImDrawList* DrawList, const ImVec2& Min, const ImVec2& Max, ImU32 Color)
{
    const ImVec2 Center((Min.x + Max.x) * 0.5f, (Min.y + Max.y) * 0.5f);
    const float Radius = (Max.x - Min.x) * 0.18f;
    DrawList->AddCircleFilled(Center, Radius, Color, 16);
    DrawList->AddCircle(Center, Radius * 1.75f, Color, 18, 1.3f);
    DrawList->AddLine(ImVec2(Center.x - Radius * 2.1f, Center.y), ImVec2(Center.x - Radius * 1.15f, Center.y), Color, 1.3f);
    DrawList->AddLine(ImVec2(Center.x + Radius * 1.15f, Center.y), ImVec2(Center.x + Radius * 2.1f, Center.y), Color, 1.3f);
    DrawList->AddLine(ImVec2(Center.x, Center.y - Radius * 2.1f), ImVec2(Center.x, Center.y - Radius * 1.15f), Color, 1.3f);
    DrawList->AddLine(ImVec2(Center.x, Center.y + Radius * 1.15f), ImVec2(Center.x, Center.y + Radius * 2.1f), Color, 1.3f);
}

void DrawEmptyActorGlyph(ImDrawList* DrawList, const ImVec2& Min, const ImVec2& Max, ImU32 Color)
{
    const ImVec2 Center((Min.x + Max.x) * 0.5f, (Min.y + Max.y) * 0.5f);
    const float Half = (Max.x - Min.x) * 0.24f;
    DrawList->AddCircle(Center, Half * 0.95f, Color, 18, 1.4f);
    DrawList->AddLine(ImVec2(Center.x - Half, Center.y), ImVec2(Center.x + Half, Center.y), Color, 1.3f);
    DrawList->AddLine(ImVec2(Center.x, Center.y - Half), ImVec2(Center.x, Center.y + Half), Color, 1.3f);
}

void DrawUIGlyph(ImDrawList* DrawList, const ImVec2& Min, const ImVec2& Max, ImU32 Color)
{
    const float W = Max.x - Min.x;
    const float H = Max.y - Min.y;
    const ImVec2 PanelMin(Min.x + W * 0.22f, Min.y + H * 0.28f);
    const ImVec2 PanelMax(Min.x + W * 0.78f, Min.y + H * 0.72f);
    DrawList->AddRect(PanelMin, PanelMax, Color, 2.0f, 0, 1.6f);
    DrawList->AddLine(
        ImVec2(PanelMin.x + W * 0.08f, PanelMin.y + H * 0.12f),
        ImVec2(PanelMax.x - W * 0.08f, PanelMin.y + H * 0.12f),
        Color,
        1.3f);
    DrawList->AddCircleFilled(ImVec2(PanelMin.x + W * 0.12f, PanelMax.y - H * 0.12f), 1.6f, Color, 8);
    DrawList->AddCircleFilled(ImVec2(PanelMin.x + W * 0.24f, PanelMax.y - H * 0.12f), 1.6f, Color, 8);
}

void DrawAddActorIcon(ImDrawList* DrawList, const ImVec2& Min, const ImVec2& Max, EAddActorIcon Icon, bool bEnabled)
{
    const ImU32 BgColor = bEnabled ? IM_COL32(30, 34, 40, 235) : IM_COL32(30, 32, 36, 150);
    const ImU32 BorderColor = bEnabled ? IM_COL32(60, 66, 76, 255) : IM_COL32(52, 54, 58, 180);
    DrawList->AddRectFilled(Min, Max, BgColor, 3.0f);
    DrawList->AddRect(Min, Max, BorderColor, 3.0f);

    const ImVec2 IconMin(Min.x + 3.0f, Min.y + 3.0f);
    const ImVec2 IconMax(Max.x - 3.0f, Max.y - 3.0f);
    if (ID3D11ShaderResourceView* Texture = GetAddActorIconTexture(Icon))
    {
        DrawList->AddImage(
            reinterpret_cast<ImTextureID>(Texture),
            IconMin,
            IconMax,
            ImVec2(0.0f, 0.0f),
            ImVec2(1.0f, 1.0f),
            bEnabled ? IM_COL32(255, 255, 255, 255) : IM_COL32(150, 150, 150, 170));
        return;
    }

    const ImU32 GlyphColor = bEnabled ? IM_COL32(128, 184, 240, 255) : IM_COL32(104, 112, 122, 180);
    switch (Icon)
    {
    case EAddActorIcon::Empty:
        DrawEmptyActorGlyph(DrawList, Min, Max, bEnabled ? IM_COL32(176, 188, 204, 255) : GlyphColor);
        break;
    case EAddActorIcon::Cube:
        DrawCubeGlyph(DrawList, Min, Max, GlyphColor);
        break;
    case EAddActorIcon::Sphere:
        DrawSphereGlyph(DrawList, Min, Max, GlyphColor);
        break;
    case EAddActorIcon::UI:
        DrawUIGlyph(DrawList, Min, Max, bEnabled ? IM_COL32(116, 210, 186, 255) : GlyphColor);
        break;
    default:
        DrawEffectGlyph(DrawList, Min, Max, bEnabled ? IM_COL32(255, 160, 82, 255) : GlyphColor);
        break;
    }
}

void DrawPlusGlyph(ImDrawList* DrawList, const ImVec2& Min, const ImVec2& Max, ImU32 Color)
{
    const ImVec2 Center((Min.x + Max.x) * 0.5f, (Min.y + Max.y) * 0.5f);
    const float Half = (Max.x - Min.x) * 0.25f;
    DrawList->AddLine(ImVec2(Center.x - Half, Center.y), ImVec2(Center.x + Half, Center.y), Color, 1.7f);
    DrawList->AddLine(ImVec2(Center.x, Center.y - Half), ImVec2(Center.x, Center.y + Half), Color, 1.7f);
}

void DrawChevronDown(ImDrawList* DrawList, const ImVec2& Center, ImU32 Color)
{
    DrawList->AddLine(ImVec2(Center.x - 3.0f, Center.y - 1.5f), ImVec2(Center.x, Center.y + 2.0f), Color, 1.4f);
    DrawList->AddLine(ImVec2(Center.x, Center.y + 2.0f), ImVec2(Center.x + 3.0f, Center.y - 1.5f), Color, 1.4f);
}

bool DrawAddActorToolbarButton(const char* Id, bool bOpen)
{
    constexpr float Width = 82.0f;
    constexpr float Height = 28.0f;

    ImGui::PushID(Id);
    ImGui::InvisibleButton("##AddActorToolbarButton", ImVec2(Width, Height));
    const bool bClicked = ImGui::IsItemClicked();
    const bool bHovered = ImGui::IsItemHovered();

    const ImVec2 Min = ImGui::GetItemRectMin();
    const ImVec2 Max = ImGui::GetItemRectMax();
    ImDrawList* DrawList = ImGui::GetWindowDrawList();

    const ImU32 BgColor = bOpen ? IM_COL32(63, 68, 76, 255) : (bHovered ? IM_COL32(56, 60, 68, 255) : IM_COL32(43, 47, 54, 255));
    const ImU32 BorderColor = bOpen || bHovered ? IM_COL32(94, 116, 148, 255) : IM_COL32(62, 67, 76, 255);
    const ImU32 TextColor = IM_COL32(224, 228, 234, 255);
    const ImU32 AccentColor = IM_COL32(86, 196, 116, 255);

    DrawList->AddRectFilled(Min, Max, BgColor, 4.0f);
    DrawList->AddRect(Min, Max, BorderColor, 4.0f);

    const ImVec2 IconMin(Min.x + 8.0f, Min.y + 6.0f);
    const ImVec2 IconMax(IconMin.x + 16.0f, IconMin.y + 16.0f);
    DrawList->AddCircleFilled(ImVec2((IconMin.x + IconMax.x) * 0.5f, (IconMin.y + IconMax.y) * 0.5f), 7.0f, IM_COL32(45, 104, 59, 255), 18);
    DrawPlusGlyph(DrawList, IconMin, IconMax, AccentColor);

    DrawList->AddText(ImVec2(Min.x + 30.0f, Min.y + 6.0f), TextColor, "Add");
    DrawChevronDown(DrawList, ImVec2(Max.x - 12.0f, Min.y + Height * 0.5f), IM_COL32(172, 178, 188, 255));

    ImGui::PopID();
    return bClicked;
}

FVector GetToolbarSpawnPoint(FLevelEditorViewportClient* ViewportClient)
{
    UCameraComponent* Camera = ViewportClient ? ViewportClient->GetCamera() : nullptr;
    if (!Camera)
    {
        return FVector(0.0f, 0.0f, 0.0f);
    }

    return Camera->GetWorldLocation() + Camera->GetForwardVector().Normalized() * 5.0f;
}

void SpawnFromToolbarEntry(UEditorEngine* Editor, FLevelEditorViewportClient* ViewportClient, const FAddActorEntry& Entry)
{
    if (!Editor)
    {
        return;
    }

    UWorld* World = Editor->GetWorld();
    if (!World)
    {
        return;
    }

    Editor->SetActiveViewport(ViewportClient);
    if (AActor* Actor = Entry.Spawn(World, GetToolbarSpawnPoint(ViewportClient)))
    {
        Editor->GetSelectionManager().Select(Actor);
    }
}

bool DrawAddActorRow(const FAddActorEntry& Entry,
                     UEditorEngine* Editor,
                     FLevelEditorViewportClient* ViewportClient,
                     bool bCanAddActor)
{
    constexpr float RowHeight = 30.0f;
    constexpr float IconSize = 22.0f;

    ImGui::PushID(Entry.Label);
    const float RowWidth = ImGui::GetContentRegionAvail().x;
    ImGui::InvisibleButton("##AddActorRow", ImVec2(RowWidth, RowHeight));

    const bool bHovered = bCanAddActor && ImGui::IsItemHovered();
    const bool bClicked = bCanAddActor && ImGui::IsItemClicked();
    const ImVec2 RowMin = ImGui::GetItemRectMin();
    const ImVec2 RowMax = ImGui::GetItemRectMax();
    ImDrawList* DrawList = ImGui::GetWindowDrawList();

    if (bHovered)
    {
        DrawList->AddRectFilled(RowMin, RowMax, IM_COL32(56, 63, 74, 255), 3.0f);
    }

    const ImVec2 IconMin(RowMin.x + 6.0f, RowMin.y + (RowHeight - IconSize) * 0.5f);
    const ImVec2 IconMax(IconMin.x + IconSize, IconMin.y + IconSize);
    DrawAddActorIcon(DrawList, IconMin, IconMax, Entry.Icon, bCanAddActor);

    const ImU32 TextColor = bCanAddActor ? IM_COL32(226, 229, 234, 255) : IM_COL32(126, 130, 138, 190);
    DrawList->AddText(ImVec2(IconMax.x + 8.0f, RowMin.y + 7.0f), TextColor, Entry.Label);

    ImGui::PopID();
    return bClicked;
}

void DrawAddActorSectionHeader(const char* Label)
{
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.52f, 0.58f, 0.66f, 1.0f));
    ImGui::TextUnformatted(Label);
    ImGui::PopStyleColor();
    ImGui::Separator();
}

template <size_t Count>
bool DrawAddActorSection(const char* Label,
                         const FAddActorEntry (&Entries)[Count],
                         UEditorEngine* Editor,
                         FLevelEditorViewportClient* ViewportClient,
                         const char* Filter,
                         bool bCanAddActor)
{
    bool bDrewHeader = false;
    bool bDrewAny = false;

    for (const FAddActorEntry& Entry : Entries)
    {
        if (!AddActorMatchesFilter(Entry, Filter))
        {
            continue;
        }

        if (!bDrewHeader)
        {
            DrawAddActorSectionHeader(Label);
            bDrewHeader = true;
        }

        if (DrawAddActorRow(Entry, Editor, ViewportClient, bCanAddActor))
        {
            SpawnFromToolbarEntry(Editor, ViewportClient, Entry);
            ImGui::CloseCurrentPopup();
        }
        bDrewAny = true;
    }

    return bDrewAny;
}

void DrawAddActorMenu(UEditorEngine* Editor, FLevelEditorViewportClient* ViewportClient)
{
    const bool bCanAddActor = Editor && !Editor->IsPlayingInEditor();
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 4.0f));

    if (ImGui::IsWindowAppearing())
    {
        ImGui::SetKeyboardFocusHere();
    }

    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    ImGui::InputTextWithHint("##AddActorSearch", "Search Actors", GAddActorSearch, sizeof(GAddActorSearch));

    if (!bCanAddActor)
    {
        ImGui::TextDisabled("Actor creation is disabled during PIE.");
    }

    const bool bAnyBasic = DrawAddActorSection("BASIC", BasicAddActors, Editor, ViewportClient, GAddActorSearch, bCanAddActor);
    const bool bAnyLights = DrawAddActorSection("LIGHTS", LightAddActors, Editor, ViewportClient, GAddActorSearch, bCanAddActor);
    const bool bAnyEffects = DrawAddActorSection("EFFECTS", EffectAddActors, Editor, ViewportClient, GAddActorSearch, bCanAddActor);
    const bool bAnyGames = DrawAddActorSection("GAMES", GameAddActors, Editor, ViewportClient, GAddActorSearch, bCanAddActor);

    if (!bAnyBasic && !bAnyLights && !bAnyEffects)
    {
        ImGui::Spacing();
        ImGui::TextDisabled("No actors found.");
    }

    ImGui::PopStyleVar();
}
} // namespace

void FEditorToolbarPanel::Initialize(UEditorEngine* InEditor, ID3D11Device* InDevice)
{
    Editor = InEditor;
    if (!InDevice)
    {
        return;
    }

    if (!GPlayStartIcon)
    {
        DirectX::CreateWICTextureFromFile(
            InDevice,
            ResolveEditorIconPath(L"icon_playInSelectedViewport_16x.png").c_str(),
            nullptr,
            &GPlayStartIcon);
    }

    if (!GPauseIcon)
    {
        DirectX::CreateWICTextureFromFile(
            InDevice,
            ResolveEditorIconPath(L"icon_pause_40x.png").c_str(),
            nullptr,
            &GPauseIcon);
    }

    if (!GStopIcon)
    {
        DirectX::CreateWICTextureFromFile(
            InDevice,
            ResolveEditorIconPath(L"generic_stop_16x.png").c_str(),
            nullptr,
            &GStopIcon);
    }

    LoadToolbarIcon(InDevice, L"DirectionalLight_64x.png", &GToolbarDirectionalLightIcon);
    LoadToolbarIcon(InDevice, L"PointLight_64x.png", &GToolbarPointLightIcon);
    LoadToolbarIcon(InDevice, L"SpotLight_64x.png", &GToolbarSpotLightIcon);
    LoadToolbarIcon(InDevice, L"SkyLightComponent_64x.png", &GToolbarAmbientLightIcon);
    LoadToolbarIcon(InDevice, L"S_DecalActorIcon.png", &GToolbarDecalIcon);
    LoadToolbarIcon(InDevice, L"S_ExpoHeightFog.png", &GToolbarHeightFogIcon);

    PlayIcon = GPlayStartIcon;
    StopIcon = GStopIcon;
}

void FEditorToolbarPanel::Release()
{
    if (GPlayStartIcon)
    {
        GPlayStartIcon->Release();
        GPlayStartIcon = nullptr;
    }

    if (GPauseIcon)
    {
        GPauseIcon->Release();
        GPauseIcon = nullptr;
    }

    if (GStopIcon)
    {
        GStopIcon->Release();
        GStopIcon = nullptr;
    }

    ReleaseToolbarIcon(GToolbarDirectionalLightIcon);
    ReleaseToolbarIcon(GToolbarPointLightIcon);
    ReleaseToolbarIcon(GToolbarSpotLightIcon);
    ReleaseToolbarIcon(GToolbarAmbientLightIcon);
    ReleaseToolbarIcon(GToolbarDecalIcon);
    ReleaseToolbarIcon(GToolbarHeightFogIcon);

    PlayIcon = nullptr;
    StopIcon = nullptr;
    Editor = nullptr;
}

bool FEditorToolbarPanel::DrawIconButton(const char* Id,
                                         ID3D11ShaderResourceView* Icon,
                                         const char* FallbackLabel,
                                         uint32 TintColor) const
{
    bool bClicked = false;

    if (Icon)
    {
        ImGui::PushID(Id);
        ImGui::InvisibleButton("##IconButton", ImVec2(IconSize, IconSize));

        const ImVec2 Min = ImGui::GetItemRectMin();
        const ImVec2 Max = ImGui::GetItemRectMax();

        if (ImGui::IsItemHovered())
        {
            ImGui::GetWindowDrawList()->AddRectFilled(Min, Max, IM_COL32(255, 255, 255, 24), 4.0f);
        }

        ImGui::GetWindowDrawList()->AddImage(
            reinterpret_cast<ImTextureID>(Icon),
            Min,
            Max,
            ImVec2(0.0f, 0.0f),
            ImVec2(1.0f, 1.0f),
            TintColor);

        bClicked = ImGui::IsItemClicked();
        ImGui::PopID();
    }
    else
    {
        bClicked = ImGui::Button(FallbackLabel, ImVec2(IconSize, IconSize));
    }

    return bClicked;
}

void FEditorToolbarPanel::PushPopupStyle() const
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.0f, 6.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 4.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0f, 4.0f));
}

void FEditorToolbarPanel::PopPopupStyle() const
{
    ImGui::PopStyleVar(3);
}

void FEditorToolbarPanel::RenderPaneToolbar(FLevelViewportLayout* Layout,
                                            int32 SlotIndex,
                                            FLevelEditorViewportClient* VC)
{
    if (!Editor || !Layout || !VC)
    {
        return;
    }

    const FRect& PaneRect = Layout->GetViewportPaneRect(SlotIndex);
    if (PaneRect.Width <= 0 || PaneRect.Height <= 0)
    {
        return;
    }

    char OverlayID[64];
    std::snprintf(OverlayID, sizeof(OverlayID), "##PaneToolbar_%d", SlotIndex);

    ImGui::SetNextWindowPos(ImVec2(PaneRect.X, PaneRect.Y));
    ImGui::SetNextWindowBgAlpha(1.0f);
    ImGui::SetNextWindowSize(ImVec2(PaneRect.Width, ToolbarHeight), ImGuiCond_Always);

    const ImGuiWindowFlags OverlayFlags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse;

    if (!ImGui::Begin(OverlayID, nullptr, OverlayFlags))
    {
        ImGui::End();
        return;
    }

    const float ToolbarHeightPx = ToolbarHeight;
    const float IconSizePx = 24.0f;
    const float ButtonSpacingPx = 8.0f;
    const float PlayStopSpacingPx = 18.0f;
    const float PopupOffsetY = ToolbarHeightPx + 2.0f;
    const float TextButtonHeight = 28.0f;
    const float TextButtonYOffset = -1.0f;

    ImGui::PushID(SlotIndex);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0f, 4.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f, 6.0f));

    const ImVec2 CursorStart = ImGui::GetCursorScreenPos();
    const float Width = PaneRect.Width;

    ImGui::GetWindowDrawList()->AddRectFilled(
        CursorStart,
        ImVec2(CursorStart.x + Width, CursorStart.y + ToolbarHeightPx),
        IM_COL32(40, 40, 40, 255));

    const float PrevIconSize = IconSize;
    const float PrevToolbarHeight = ToolbarHeight;
    const float PrevButtonSpacing = ButtonSpacing;

    const_cast<FEditorToolbarPanel*>(this)->IconSize = IconSizePx;
    const_cast<FEditorToolbarPanel*>(this)->ToolbarHeight = ToolbarHeightPx;
    const_cast<FEditorToolbarPanel*>(this)->ButtonSpacing = ButtonSpacingPx;

    const float ButtonPaddingX = 8.0f;
    const float IconButtonY = CursorStart.y + (ToolbarHeightPx - IconSizePx) * 0.5f;
    const float TextButtonY = CursorStart.y + (ToolbarHeightPx - TextButtonHeight) * 0.5f + TextButtonYOffset;

    ImGui::SetCursorScreenPos(ImVec2(CursorStart.x + ButtonPaddingX, IconButtonY));

    const bool bPlaying = Editor->IsPlayingInEditor();
    const bool bPaused = Editor->IsPausedInEditor();
    ID3D11ShaderResourceView* CurrentPlayIcon = bPlaying ? GPauseIcon : GPlayStartIcon;

    const uint32 PlayTint = !bPlaying
                                ? IM_COL32(70, 210, 90, 255)
                                : (bPaused ? IM_COL32(255, 230, 80, 255) : IM_COL32(255, 255, 255, 255));
    const uint32 StopTint = bPlaying ? IM_COL32(220, 70, 70, 255) : IM_COL32(255, 255, 255, 255);

    if (DrawIconButton("PIE_Play", CurrentPlayIcon, bPlaying ? (bPaused ? "Resume" : "Pause") : "Play", PlayTint))
    {
        Editor->SetActiveViewport(VC);

        if (!bPlaying)
        {
            FRequestPlaySessionParams Params;
            Params.PlayMode = EPIEPlayMode::PlayInViewport;
            Params.DestinationViewportClient = VC;
            Editor->RequestPlaySession(Params);
        }
        else if (VC->GetPlayState() != EEditorViewportPlayState::Stopped)
        {
            Editor->TogglePausePlayInEditor();
        }
    }

    ImGui::SameLine(0.0f, PlayStopSpacingPx);

    if (DrawIconButton("PIE_Stop", GStopIcon, "Stop", StopTint) && bPlaying)
    {
        Editor->StopPlayInEditorImmediate();
    }

    auto OpenToolbarPopup = [&](const char* ButtonLabel, const char* PopupId, auto&& Body)
    {
        ImGui::SameLine(0.0f, ButtonSpacingPx);

        const ImVec2 CurrentPos = ImGui::GetCursorScreenPos();
        ImGui::SetCursorScreenPos(ImVec2(CurrentPos.x, TextButtonY));

        if (ImGui::Button(ButtonLabel, ImVec2(0.0f, TextButtonHeight)))
        {
            ImGui::OpenPopup(PopupId);
        }

        SetFixedPopupPosBelowLastItem(PopupOffsetY);
        PushPopupStyle();
        const bool bOpened = ImGui::BeginPopup(PopupId);
        PopPopupStyle();

        if (bOpened)
        {
            Body();
            ImGui::EndPopup();
        }
    };

    ImGui::SameLine(0.0f, ButtonSpacingPx);
    {
        const ImVec2 CurrentPos = ImGui::GetCursorScreenPos();
        ImGui::SetCursorScreenPos(ImVec2(CurrentPos.x, TextButtonY));

        const bool bAddPopupOpen = ImGui::IsPopupOpen("AddActorPopup");
        if (DrawAddActorToolbarButton("AddActor", bAddPopupOpen))
        {
            ImGui::OpenPopup("AddActorPopup");
        }

        SetFixedPopupPosBelowLastItem(PopupOffsetY);
        ImGui::SetNextWindowSizeConstraints(ImVec2(286.0f, 0.0f), ImVec2(286.0f, 420.0f));
        PushPopupStyle();
        const bool bOpened = ImGui::BeginPopup("AddActorPopup");
        PopPopupStyle();

        if (bOpened)
        {
            DrawAddActorMenu(Editor, VC);
            ImGui::EndPopup();
        }
    }

    OpenToolbarPopup("Layout", "LayoutPopup", [&]()
                     {
        constexpr int32 LayoutCount = static_cast<int32>(EViewportLayout::MAX);
        constexpr int32 Columns = 4;
        constexpr float LayoutIconSize = 28.0f;

        for (int32 i = 0; i < LayoutCount; ++i)
        {
            ImGui::PushID(i);

            const bool bSelected = (static_cast<EViewportLayout>(i) == Layout->GetLayout());
            if (bSelected)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.35f, 0.50f, 1.0f));
            }

            bool bClicked = false;
            if (ID3D11ShaderResourceView* Icon = Layout->GetLayoutIcon(static_cast<EViewportLayout>(i)))
            {
                bClicked = ImGui::ImageButton("##LayoutIcon",
                                              reinterpret_cast<ImTextureID>(Icon),
                                              ImVec2(LayoutIconSize, LayoutIconSize));
            }
            else
            {
                char Label[8];
                std::snprintf(Label, sizeof(Label), "%d", i);
                bClicked = ImGui::Button(Label, ImVec2(LayoutIconSize + 4.0f, LayoutIconSize + 4.0f));
            }

            if (bSelected)
            {
                ImGui::PopStyleColor();
            }

            if (bClicked)
            {
                Layout->SetLayout(static_cast<EViewportLayout>(i));
                ImGui::CloseCurrentPopup();
            }

            if (((i + 1) % Columns) != 0 && (i + 1) < LayoutCount)
            {
                ImGui::SameLine();
            }

            ImGui::PopID();
        } });

    FViewportRenderOptions& Opts = VC->GetRenderOptions();

    OpenToolbarPopup("View", "ViewportTypePopup", [&]()
                     {
        ImGui::SeparatorText("Perspective");
        if (ImGui::Selectable("Perspective", Opts.ViewportType == ELevelViewportType::Perspective))
        {
            VC->SetViewportType(ELevelViewportType::Perspective);
        }

        ImGui::SeparatorText("Orthographic");

        const struct
        {
            const char* Label;
            ELevelViewportType Type;
        } OrthoTypes[] = {
            { "Free",   ELevelViewportType::FreeOrthographic },
            { "Top",    ELevelViewportType::Top },
            { "Bottom", ELevelViewportType::Bottom },
            { "Left",   ELevelViewportType::Left },
            { "Right",  ELevelViewportType::Right },
            { "Front",  ELevelViewportType::Front },
            { "Back",   ELevelViewportType::Back },
        };

        for (const auto& Entry : OrthoTypes)
        {
            if (ImGui::Selectable(Entry.Label, Opts.ViewportType == Entry.Type))
            {
                VC->SetViewportType(Entry.Type);
            }
        }

        if (UCameraComponent* Camera = VC->GetCamera())
        {
            ImGui::Separator();

            float OrthoWidth = Camera->GetOrthoWidth();
            if (ImGui::DragFloat("Ortho Width", &OrthoWidth, 0.1f, 0.1f, 1000.0f))
            {
                Camera->SetOrthoWidth(Clamp(OrthoWidth, 0.1f, 1000.0f));
            }
        } });

    OpenToolbarPopup("Pilot", "PilotPopup", [&]()
                     {
        FSelectionManager& Selection = Editor->GetSelectionManager();
        AActor* SelectedActor = Selection.GetPrimarySelection();
        const bool bCanPilotSelectedActor = (SelectedActor != nullptr);

        BeginDisabledUnless(bCanPilotSelectedActor, [&]()
        {
            FString ActorName = SelectedActor ? SelectedActor->GetFName().ToString() : FString();
            if (ActorName.empty() && SelectedActor && SelectedActor->GetClass())
            {
                ActorName = SelectedActor->GetClass()->GetName();
            }

            FString PilotLabel = "Pilot Selected Actor";
            if (!ActorName.empty())
            {
                PilotLabel += " (" + ActorName + ")";
            }

            if (ImGui::Selectable(PilotLabel.c_str(), false))
            {
                VC->PilotSelectedActor(SelectedActor);
                ImGui::CloseCurrentPopup();
            }
        });

        const bool bIsPilotingActor = VC->IsPilotingActor();
        BeginDisabledUnless(bIsPilotingActor, [&]()
        {
            if (ImGui::Selectable("Stop Piloting Actor", false))
            {
                VC->StopPilotingActor();
                ImGui::CloseCurrentPopup();
            }
        }); });

    if (UGizmoComponent* Gizmo = Editor->GetGizmo())
    {
        OpenToolbarPopup("Gizmo", "GizmoModePopup", [&]()
                         {
            int32 CurrentGizmoMode = static_cast<int32>(Gizmo->GetMode());

            if (ImGui::RadioButton("Translate", &CurrentGizmoMode, static_cast<int32>(EGizmoMode::Translate)))
            {
                Gizmo->SetTranslateMode();
            }

            if (ImGui::RadioButton("Rotate", &CurrentGizmoMode, static_cast<int32>(EGizmoMode::Rotate)))
            {
                Gizmo->SetRotateMode();
            }

            if (ImGui::RadioButton("Scale", &CurrentGizmoMode, static_cast<int32>(EGizmoMode::Scale)))
            {
                Gizmo->SetScaleMode();
            } });

        OpenToolbarPopup("Space", "GizmoSpacePopup", [&]()
                         {
            int32 CurrentSpace = Gizmo->IsWorldSpace() ? 1 : 0;
            if (ImGui::RadioButton("World", &CurrentSpace, 1))
            {
                Gizmo->SetWorldSpace(true);
            }
            if (ImGui::RadioButton("Local", &CurrentSpace, 0))
            {
                Gizmo->SetWorldSpace(false);
            }
            ImGui::Separator();
            ImGui::TextDisabled("Scale stays Local");
        });
    }

    OpenToolbarPopup("ViewMode", "ViewModePopup", [&]()
                     {
        int32 CurrentMode = static_cast<int32>(Opts.ViewMode);

        ImGui::RadioButton("Lit_Lambert", &CurrentMode, static_cast<int32>(EViewMode::Lit_Lambert));
        ImGui::RadioButton("Lit_Phong", &CurrentMode, static_cast<int32>(EViewMode::Lit_Phong));
        ImGui::RadioButton("Unlit", &CurrentMode, static_cast<int32>(EViewMode::Unlit));
        ImGui::RadioButton("WorldNormal", &CurrentMode, static_cast<int32>(EViewMode::WorldNormal));
        ImGui::RadioButton("Wireframe", &CurrentMode, static_cast<int32>(EViewMode::Wireframe));
        ImGui::RadioButton("SceneDepth", &CurrentMode, static_cast<int32>(EViewMode::SceneDepth));

        Opts.ViewMode = static_cast<EViewMode>(CurrentMode);

        if (Opts.ViewMode == EViewMode::SceneDepth)
        {
            ImGui::Separator();
            ImGui::DragFloat("Exponent", &Opts.Exponent, 0.05f, 0.1f, 10.0f, "%.2f");
            ImGui::DragFloat("Range", &Opts.Range, 0.5f, 0.1f, 1000.0f, "%.1f");
            ImGui::Combo("Mode", &Opts.SceneDepthVisMode, "Power\0Linear\0");
        } });

    OpenToolbarPopup("Show", "ShowPopup", [&]()
                     {
        if (ImGui::CollapsingHeader("Common Show Flags", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Checkbox("Primitives", &Opts.ShowFlags.bPrimitives);
            ImGui::Checkbox("UI", &Opts.ShowFlags.bUI);
            ImGui::Checkbox("Text", &Opts.ShowFlags.bText);
			ImGui::Checkbox("Skeleton", &Opts.ShowFlags.bSkeletalDebug);
        }

        if (ImGui::CollapsingHeader("Actor Helpers", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Checkbox("UUID Text", &Opts.ShowFlags.bUUIDText);
            ImGui::Checkbox("Grid", &Opts.ShowFlags.bGrid);

            BeginDisabledUnless(Opts.ShowFlags.bGrid, [&]()
            {
                ImGui::Indent();
                ImGui::SliderFloat("Spacing", &Opts.GridSpacing, 0.1f, 10.0f, "%.1f");
                ImGui::SliderInt("Half Line Count", &Opts.GridHalfLineCount, 10, 500);
                ImGui::Unindent();
            });

            ImGui::Checkbox("World Axis", &Opts.ShowFlags.bWorldAxis);
            ImGui::Checkbox("Gizmo", &Opts.ShowFlags.bGizmo);
        }

        if (ImGui::CollapsingHeader("Debug", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Checkbox("Scene BVH (Green)", &Opts.ShowFlags.bSceneBVH);
            ImGui::Checkbox("Scene Octree (Cyan)", &Opts.ShowFlags.bSceneOctree);
            ImGui::Checkbox("World Bound (Magenta)", &Opts.ShowFlags.bWorldBound);
            ImGui::Checkbox("Light Visualization", &Opts.ShowFlags.bLightDebugLines);
            BeginDisabledUnless(Opts.ShowFlags.bLightDebugLines, [&]()
            {
                ImGui::Indent();
                ImGui::SliderFloat("Directional Scale", &Opts.ShowFlags.DirectionalLightDebugScale, 0.5f, 4.0f, "%.2f");
                ImGui::SliderFloat("Point Scale", &Opts.ShowFlags.PointLightDebugScale, 0.5f, 4.0f, "%.2f");
                ImGui::SliderFloat("Spot Scale", &Opts.ShowFlags.SpotLightDebugScale, 0.5f, 4.0f, "%.2f");
                ImGui::Unindent();
            });
            ImGui::Checkbox("Light Hit Map", &Opts.ShowFlags.bLightHitMap);

        }

        if (ImGui::CollapsingHeader("Post-Processing Show Flags", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Checkbox("Height Distance Fog", &Opts.ShowFlags.bFog);
            ImGui::Checkbox("Anti-Aliasing (FXAA)", &Opts.ShowFlags.bFXAA);

            BeginDisabledUnless(Opts.ShowFlags.bFXAA, [&]()
            {
                ImGui::Indent();
                ImGui::SliderFloat("Edge Threshold", &Opts.EdgeThreshold, 0.06f, 0.333f, "%.3f");
                ImGui::SliderFloat("Edge Threshold Min", &Opts.EdgeThresholdMin, 0.0312f, 0.0833f, "%.4f");
                ImGui::Unindent();
            });

            ImGui::Checkbox("Vignette", &Opts.ShowFlags.bVignetting);
            BeginDisabledUnless(Opts.ShowFlags.bVignetting, [&]()
            {
                ImGui::Indent();
                ImGui::SliderFloat("Vignette Intensity", &Opts.Vignetting.Intensity, 0.0f, 10.0f, "%.2f");
                ImGui::SliderFloat("Vignette Radius", &Opts.Vignetting.Radius, 0.0f, 1.0f, "%.2f");
                ImGui::SliderFloat("Vignette Softness", &Opts.Vignetting.Softness, 0.001f, 1.0f, "%.3f");
                ImGui::ColorEdit3("Vignette Color", &Opts.Vignetting.Color.X);
                ImGui::Unindent();
            });

            ImGui::Checkbox("Gamma Correction", &Opts.ShowFlags.bGammaCorrection);
            BeginDisabledUnless(Opts.ShowFlags.bGammaCorrection, [&]()
            {
                ImGui::Indent();
                ImGui::DragFloat("Display Gamma", &Opts.GammaCorrection.DisplayGamma, 0.01f, 0.1f, 5.0f, "%.2f");
                ImGui::Unindent();
            });
        }
		if (ImGui::CollapsingHeader("Light Culling Options", ImGuiTreeNodeFlags_DefaultOpen))
        {
            int mode = Opts.ShowFlags.b25DCulling ? 1 : 0;

            ImGui::RadioButton("2D Tile Culling", &mode, 0);
            ImGui::SameLine();
            ImGui::RadioButton("2.5D Tile Culling", &mode, 1);

            Opts.ShowFlags.b25DCulling = (mode == 1);
        } });

    OpenToolbarPopup("Shadows", "ShadowPopup", [&]()
                     {
        static const char* ShadowMapMethodLabels[] = {
            "Standard",
            "PSM (Perspective Shadow Map)",
            "Cascade"
        };
        int32 SelectedShadowMapMethod = static_cast<int32>(GetShadowMapMethod());
        if (SelectedShadowMapMethod < 0 || SelectedShadowMapMethod >= IM_ARRAYSIZE(ShadowMapMethodLabels))
        {
            SelectedShadowMapMethod = 0;
        }

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 6.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 10.0f));

        if (ImGui::Combo("Map Method", &SelectedShadowMapMethod, ShadowMapMethodLabels, IM_ARRAYSIZE(ShadowMapMethodLabels)))
        {
            SetShadowMapMethod(static_cast<EShadowMapMethod>(SelectedShadowMapMethod));
        }

        static const char* ShadowFilterLabels[] = {
            "None (Single Compare)",
            "PCF (Percentage-Closer Filtering)",
            "VSM (Variance Shadow Map)",
            "ESM (Exponential Shadow Map)"
        };
        int32 SelectedShadowFilter = static_cast<int32>(GetShadowFilterMethod());
        if (SelectedShadowFilter < 0 || SelectedShadowFilter >= IM_ARRAYSIZE(ShadowFilterLabels))
        {
            SelectedShadowFilter = 0;
        }

        if (ImGui::Combo("Filter", &SelectedShadowFilter, ShadowFilterLabels, IM_ARRAYSIZE(ShadowFilterLabels)))
        {
            SetShadowFilterMethod(static_cast<EShadowFilterMethod>(SelectedShadowFilter));
        }

        static const char* AllocationPolicyLabels[] = {
            "Prefer Downgrade",
            "Prefer Eviction",
            "Downgrade Only"
        };
        int32 SelectedAllocationPolicy = static_cast<int32>(GetShadowAllocationPolicy());
        if (SelectedAllocationPolicy < 0 || SelectedAllocationPolicy >= IM_ARRAYSIZE(AllocationPolicyLabels))
        {
            SelectedAllocationPolicy = 0;
        }

        if (ImGui::Combo("Eviction Policy", &SelectedAllocationPolicy, AllocationPolicyLabels, IM_ARRAYSIZE(AllocationPolicyLabels)))
        {
            SetShadowAllocationPolicy(static_cast<EShadowAllocationPolicy>(SelectedAllocationPolicy));
        }

        ImGui::PopStyleVar(2);

        ImGui::TextDisabled("Global renderer settings.");
        ImGui::TextDisabled("Applied to all shadow-casting lights.");
    });

    const_cast<FEditorToolbarPanel*>(this)->IconSize = PrevIconSize;
    const_cast<FEditorToolbarPanel*>(this)->ToolbarHeight = PrevToolbarHeight;
    const_cast<FEditorToolbarPanel*>(this)->ButtonSpacing = PrevButtonSpacing;

    ImGui::PopStyleVar(2);
    ImGui::PopID();

    ImGui::SetCursorScreenPos(ImVec2(CursorStart.x, CursorStart.y));
    ImGui::Dummy(ImVec2(Width, ToolbarHeightPx));
    ImGui::End();
}
