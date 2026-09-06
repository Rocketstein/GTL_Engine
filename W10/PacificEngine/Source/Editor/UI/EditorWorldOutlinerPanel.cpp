// Implements the editor World Outliner panel.
#include "Editor/UI/EditorWorldOutlinerPanel.h"

#include "Component/ActorComponent.h"
#include "Editor/EditorEngine.h"
#include "Editor/Selection/SelectionManager.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "ImGui/imgui.h"
#include "Object/FName.h"
#include "Platform/Paths.h"
#include "WICTextureLoader.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <d3d11.h>
#include <filesystem>

namespace
{
constexpr float OutlinerHeaderHeight = 22.0f;
constexpr float OutlinerRowHeight = 22.0f;
constexpr float OutlinerTreeGutterWidth = 20.0f;
constexpr float OutlinerIconSize = 16.0f;
constexpr float OutlinerVisibilityWidth = 28.0f;

enum class EOutlinerToolbarIcon
{
    Folder,
    Filter,
    Settings
};

enum class EOutlinerActorIcon
{
    GenericActor,
    StaticMesh,
    Camera,
    DirectionalLight,
    PointLight,
    SpotLight,
    AmbientLight,
    Decal,
    HeightFog,
    UI,
    Effect
};

FString ToLowerAscii(FString Value)
{
    std::transform(Value.begin(), Value.end(), Value.begin(),
                   [](unsigned char Ch) { return static_cast<char>(std::tolower(Ch)); });
    return Value;
}

FString TrimAscii(const FString& Value)
{
    size_t First = 0;
    while (First < Value.size() && std::isspace(static_cast<unsigned char>(Value[First])))
    {
        ++First;
    }

    size_t Last = Value.size();
    while (Last > First && std::isspace(static_cast<unsigned char>(Value[Last - 1])))
    {
        --Last;
    }

    return Value.substr(First, Last - First);
}

bool ContainsCaseInsensitive(const FString& Value, const FString& Query)
{
    if (Query.empty())
    {
        return true;
    }

    return ToLowerAscii(Value).find(ToLowerAscii(Query)) != FString::npos;
}

bool ClassNameContains(UClass* Class, const char* Token)
{
    return Class && ContainsCaseInsensitive(Class->GetName(), Token);
}

bool ActorClassContains(AActor* Actor, const char* Token)
{
    return Actor && ClassNameContains(Actor->GetClass(), Token);
}

bool ActorComponentClassContains(AActor* Actor, const char* Token)
{
    if (!Actor)
    {
        return false;
    }

    for (UActorComponent* Component : Actor->GetComponents())
    {
        if (Component && ClassNameContains(Component->GetClass(), Token))
        {
            return true;
        }
    }

    return false;
}

EOutlinerActorIcon ResolveActorIconType(AActor* Actor)
{
    if (!Actor)
    {
        return EOutlinerActorIcon::GenericActor;
    }

    if (ActorClassContains(Actor, "Fireball"))
    {
        return EOutlinerActorIcon::Effect;
    }
    if (ActorClassContains(Actor, "FakeLight"))
    {
        return EOutlinerActorIcon::PointLight;
    }
    if (ActorClassContains(Actor, "DirectionalLight") || ActorComponentClassContains(Actor, "DirectionalLight"))
    {
        return EOutlinerActorIcon::DirectionalLight;
    }
    if (ActorClassContains(Actor, "PointLight") || ActorComponentClassContains(Actor, "PointLight"))
    {
        return EOutlinerActorIcon::PointLight;
    }
    if (ActorClassContains(Actor, "SpotLight") || ActorComponentClassContains(Actor, "SpotLight"))
    {
        return EOutlinerActorIcon::SpotLight;
    }
    if (ActorClassContains(Actor, "AmbientLight") || ActorClassContains(Actor, "SkyLight") ||
        ActorComponentClassContains(Actor, "AmbientLight") || ActorComponentClassContains(Actor, "SkyLight"))
    {
        return EOutlinerActorIcon::AmbientLight;
    }
    if (ActorClassContains(Actor, "HeightFog") || ActorComponentClassContains(Actor, "HeightFog"))
    {
        return EOutlinerActorIcon::HeightFog;
    }
    if (ActorClassContains(Actor, "Decal") || ActorComponentClassContains(Actor, "Decal"))
    {
        return EOutlinerActorIcon::Decal;
    }
    if (ActorClassContains(Actor, "UI") || ActorComponentClassContains(Actor, "UI"))
    {
        return EOutlinerActorIcon::UI;
    }
    if (ActorClassContains(Actor, "Camera") || ActorComponentClassContains(Actor, "Camera"))
    {
        return EOutlinerActorIcon::Camera;
    }
    if (ActorClassContains(Actor, "StaticMesh") || ActorComponentClassContains(Actor, "StaticMesh") ||
        ActorClassContains(Actor, "Mesh") || ActorComponentClassContains(Actor, "Mesh"))
    {
        return EOutlinerActorIcon::StaticMesh;
    }

    return EOutlinerActorIcon::GenericActor;
}

bool IsMouseInRect(const ImVec2& Min, const ImVec2& Max)
{
    const ImVec2 MousePos = ImGui::GetIO().MousePos;
    return MousePos.x >= Min.x && MousePos.x <= Max.x && MousePos.y >= Min.y && MousePos.y <= Max.y;
}

FString GetFolderParentPath(const FString& FolderPath)
{
    const size_t Slash = FolderPath.rfind('/');
    if (Slash == FString::npos)
    {
        return FString();
    }
    return FolderPath.substr(0, Slash);
}

FString GetFolderLeafName(const FString& FolderPath)
{
    const size_t Slash = FolderPath.rfind('/');
    if (Slash == FString::npos)
    {
        return FolderPath;
    }
    return FolderPath.substr(Slash + 1);
}

int32 GetFolderDepth(const FString& FolderPath)
{
    int32 Depth = 0;
    for (char Ch : FolderPath)
    {
        if (Ch == '/')
        {
            ++Depth;
        }
    }
    return Depth;
}

bool IsFolderPathWithin(const FString& FolderPath, const FString& ParentPath)
{
    if (FolderPath == ParentPath)
    {
        return true;
    }
    if (ParentPath.empty() || FolderPath.size() <= ParentPath.size())
    {
        return false;
    }
    return FolderPath.compare(0, ParentPath.size(), ParentPath) == 0 && FolderPath[ParentPath.size()] == '/';
}

FString ReplaceFolderPrefix(const FString& FolderPath, const FString& OldPrefix, const FString& NewPrefix)
{
    if (FolderPath == OldPrefix)
    {
        return NewPrefix;
    }
    return NewPrefix + FolderPath.substr(OldPrefix.size());
}

void AddFolderAndParents(TSet<FString>& SeenFolders, TArray<FString>& OutFolders, const FString& FolderPath)
{
    if (FolderPath.empty())
    {
        return;
    }

    FString Current;
    size_t Start = 0;
    while (Start < FolderPath.size())
    {
        const size_t Slash = FolderPath.find('/', Start);
        const size_t End = Slash == FString::npos ? FolderPath.size() : Slash;
        if (End > Start)
        {
            if (!Current.empty())
            {
                Current += "/";
            }
            Current += FolderPath.substr(Start, End - Start);
            if (SeenFolders.find(Current) == SeenFolders.end())
            {
                SeenFolders.insert(Current);
                OutFolders.push_back(Current);
            }
        }
        if (Slash == FString::npos)
        {
            break;
        }
        Start = Slash + 1;
    }
}

void AddWorldFolderAndParents(UWorld* World, const FString& FolderPath)
{
    if (!World || FolderPath.empty())
    {
        return;
    }

    TSet<FString> SeenFolders;
    TArray<FString> FolderPaths;
    AddFolderAndParents(SeenFolders, FolderPaths, FolderPath);
    for (const FString& Path : FolderPaths)
    {
        World->AddEditorActorFolder(Path);
    }
}

void OpenFolderAndParents(TSet<FString>& OpenFolderPaths, const FString& FolderPath)
{
    TSet<FString> SeenFolders;
    TArray<FString> FolderPaths;
    AddFolderAndParents(SeenFolders, FolderPaths, FolderPath);
    for (const FString& Path : FolderPaths)
    {
        OpenFolderPaths.insert(Path);
    }
}

TArray<FString> BuildChildFolderPaths(const FString& ParentPath, const TArray<FString>& Folders)
{
    TArray<FString> Result;
    for (const FString& FolderPath : Folders)
    {
        if (GetFolderParentPath(FolderPath) == ParentPath)
        {
            Result.push_back(FolderPath);
        }
    }
    std::sort(Result.begin(), Result.end(), [](const FString& A, const FString& B)
    {
        return ToLowerAscii(GetFolderLeafName(A)) < ToLowerAscii(GetFolderLeafName(B));
    });
    return Result;
}

void DrawOutlinerToolbarGlyph(ImDrawList* DrawList, EOutlinerToolbarIcon Icon, const ImVec2& Min, const ImVec2& Max, ImU32 Color)
{
    const ImVec2 Center((Min.x + Max.x) * 0.5f, (Min.y + Max.y) * 0.5f);
    if (Icon == EOutlinerToolbarIcon::Folder)
    {
        const ImVec2 FolderMin(Min.x + 6.0f, Min.y + 9.0f);
        const ImVec2 FolderMax(Max.x - 5.0f, Max.y - 6.0f);
        DrawList->AddRect(FolderMin, FolderMax, Color, 2.0f, 0, 1.3f);
        DrawList->AddLine(ImVec2(FolderMin.x + 2.0f, FolderMin.y),
                          ImVec2(FolderMin.x + 5.0f, FolderMin.y - 3.0f),
                          Color,
                          1.3f);
        DrawList->AddLine(ImVec2(FolderMin.x + 5.0f, FolderMin.y - 3.0f),
                          ImVec2(FolderMin.x + 11.0f, FolderMin.y - 3.0f),
                          Color,
                          1.3f);
        DrawList->AddLine(ImVec2(FolderMax.x - 4.0f, Center.y), ImVec2(FolderMax.x + 2.0f, Center.y), Color, 1.3f);
        DrawList->AddLine(ImVec2(FolderMax.x - 1.0f, Center.y - 3.0f), ImVec2(FolderMax.x - 1.0f, Center.y + 3.0f), Color, 1.3f);
        return;
    }

    if (Icon == EOutlinerToolbarIcon::Filter)
    {
        const float TopY = Center.y - 5.0f;
        const float MidY = Center.y;
        const float BottomY = Center.y + 5.0f;
        DrawList->AddLine(ImVec2(Center.x - 7.0f, TopY), ImVec2(Center.x + 7.0f, TopY), Color, 1.4f);
        DrawList->AddLine(ImVec2(Center.x - 4.5f, MidY), ImVec2(Center.x + 4.5f, MidY), Color, 1.4f);
        DrawList->AddLine(ImVec2(Center.x - 2.0f, BottomY), ImVec2(Center.x + 2.0f, BottomY), Color, 1.4f);
        return;
    }

    DrawList->AddCircle(Center, 5.2f, Color, 12, 1.3f);
    DrawList->AddCircleFilled(Center, 1.6f, Color, 8);
    DrawList->AddLine(ImVec2(Center.x, Center.y - 8.0f), ImVec2(Center.x, Center.y - 5.9f), Color, 1.2f);
    DrawList->AddLine(ImVec2(Center.x, Center.y + 5.9f), ImVec2(Center.x, Center.y + 8.0f), Color, 1.2f);
    DrawList->AddLine(ImVec2(Center.x - 8.0f, Center.y), ImVec2(Center.x - 5.9f, Center.y), Color, 1.2f);
    DrawList->AddLine(ImVec2(Center.x + 5.9f, Center.y), ImVec2(Center.x + 8.0f, Center.y), Color, 1.2f);
}

bool DrawOutlinerToolbarButton(const char* Id, EOutlinerToolbarIcon Icon, const char* Tooltip)
{
    const float Size = ImGui::GetFrameHeight();
    const ImVec2 Min = ImGui::GetCursorScreenPos();
    const ImVec2 Max(Min.x + Size, Min.y + Size);

    const bool bPressed = ImGui::InvisibleButton(Id, ImVec2(Size, Size));
    const bool bHovered = ImGui::IsItemHovered();
    const bool bActive = ImGui::IsItemActive();

    ImDrawList* DrawList = ImGui::GetWindowDrawList();
    const ImU32 BgColor = bActive
                               ? IM_COL32(54, 68, 86, 255)
                               : (bHovered ? IM_COL32(45, 51, 60, 255) : IM_COL32(33, 36, 42, 255));
    const ImU32 BorderColor = bHovered ? IM_COL32(92, 132, 184, 255) : IM_COL32(58, 62, 70, 255);
    const ImU32 IconColor = bHovered ? IM_COL32(220, 228, 238, 255) : IM_COL32(155, 164, 176, 255);

    DrawList->AddRectFilled(Min, Max, BgColor, 3.0f);
    DrawList->AddRect(Min, Max, BorderColor, 3.0f);
    DrawOutlinerToolbarGlyph(DrawList, Icon, Min, Max, IconColor);

    if (bHovered)
    {
        ImGui::SetTooltip("%s", Tooltip);
    }

    return bPressed;
}

void DrawOutlinerEye(ImDrawList* DrawList, const ImVec2& Center, bool bVisible, ImU32 Color)
{
    DrawList->AddEllipse(Center, ImVec2(7.0f, 4.5f), Color, 0.0f, 12, 1.2f);
    if (bVisible)
    {
        DrawList->AddCircleFilled(Center, 2.0f, Color, 8);
    }
    else
    {
        DrawList->AddLine(
            ImVec2(Center.x - 7.0f, Center.y + 6.0f),
            ImVec2(Center.x + 7.0f, Center.y - 6.0f),
            Color,
            1.4f);
    }
}

void DrawCubeIcon(ImDrawList* DrawList, const ImVec2& Min, const ImVec2& Max, ImU32 Color)
{
    const float W = Max.x - Min.x;
    const float H = Max.y - Min.y;
    const ImVec2 FrontMin(Min.x + W * 0.22f, Min.y + H * 0.33f);
    const ImVec2 FrontMax(Min.x + W * 0.78f, Min.y + H * 0.83f);
    const ImVec2 BackMin(Min.x + W * 0.38f, Min.y + H * 0.17f);
    const ImVec2 BackMax(Min.x + W * 0.90f, Min.y + H * 0.62f);

    DrawList->AddRect(FrontMin, FrontMax, Color, 1.5f, 0, 1.2f);
    DrawList->AddRect(BackMin, BackMax, Color, 1.5f, 0, 1.2f);
    DrawList->AddLine(FrontMin, BackMin, Color, 1.2f);
    DrawList->AddLine(ImVec2(FrontMax.x, FrontMin.y), ImVec2(BackMax.x, BackMin.y), Color, 1.2f);
    DrawList->AddLine(FrontMax, BackMax, Color, 1.2f);
}

void DrawCameraIcon(ImDrawList* DrawList, const ImVec2& Min, const ImVec2& Max, ImU32 Color)
{
    const float W = Max.x - Min.x;
    const float H = Max.y - Min.y;
    const ImVec2 BodyMin(Min.x + W * 0.12f, Min.y + H * 0.30f);
    const ImVec2 BodyMax(Min.x + W * 0.62f, Min.y + H * 0.72f);
    const ImVec2 LensA(Min.x + W * 0.66f, Min.y + H * 0.40f);
    const ImVec2 LensB(Min.x + W * 0.90f, Min.y + H * 0.24f);
    const ImVec2 LensC(Min.x + W * 0.90f, Min.y + H * 0.78f);

    DrawList->AddRect(BodyMin, BodyMax, Color, 2.0f, 0, 1.3f);
    DrawList->AddTriangle(LensA, LensB, LensC, Color, 1.3f);
    DrawList->AddCircleFilled(ImVec2(Min.x + W * 0.30f, Min.y + H * 0.51f), 1.5f, Color, 8);
}

void DrawGenericActorIcon(ImDrawList* DrawList, const ImVec2& Min, const ImVec2& Max, ImU32 Color)
{
    const ImVec2 Center((Min.x + Max.x) * 0.5f, (Min.y + Max.y) * 0.5f);
    const float HalfW = (Max.x - Min.x) * 0.34f;
    const float HalfH = (Max.y - Min.y) * 0.34f;

    DrawList->AddQuad(
        ImVec2(Center.x, Center.y - HalfH),
        ImVec2(Center.x + HalfW, Center.y),
        ImVec2(Center.x, Center.y + HalfH),
        ImVec2(Center.x - HalfW, Center.y),
        Color,
        1.3f);
    DrawList->AddCircleFilled(Center, 1.7f, Color, 8);
}

void DrawUIIcon(ImDrawList* DrawList, const ImVec2& Min, const ImVec2& Max, ImU32 Color)
{
    const float W = Max.x - Min.x;
    const float H = Max.y - Min.y;
    const ImVec2 PanelMin(Min.x + W * 0.17f, Min.y + H * 0.27f);
    const ImVec2 PanelMax(Min.x + W * 0.83f, Min.y + H * 0.74f);

    DrawList->AddRect(PanelMin, PanelMax, Color, 2.0f, 0, 1.2f);
    DrawList->AddLine(
        ImVec2(PanelMin.x + W * 0.10f, PanelMin.y + H * 0.13f),
        ImVec2(PanelMax.x - W * 0.10f, PanelMin.y + H * 0.13f),
        Color,
        1.0f);
    DrawList->AddCircleFilled(ImVec2(PanelMin.x + W * 0.13f, PanelMax.y - H * 0.13f), 1.2f, Color, 8);
    DrawList->AddCircleFilled(ImVec2(PanelMin.x + W * 0.25f, PanelMax.y - H * 0.13f), 1.2f, Color, 8);
}

void DrawFolderIcon(ImDrawList* DrawList, const ImVec2& Min, const ImVec2& Max, bool bOpen, bool bMuted)
{
    const ImU32 FillColor = bMuted ? IM_COL32(84, 78, 55, 150) : IM_COL32(196, 148, 62, 235);
    const ImU32 LineColor = bMuted ? IM_COL32(134, 124, 90, 180) : IM_COL32(235, 195, 99, 255);
    const float W = Max.x - Min.x;
    const float H = Max.y - Min.y;
    const ImVec2 TabMin(Min.x + W * 0.10f, Min.y + H * 0.25f);
    const ImVec2 TabMax(Min.x + W * 0.46f, Min.y + H * 0.46f);
    const ImVec2 BodyMin(Min.x + W * 0.08f, Min.y + H * 0.38f);
    const ImVec2 BodyMax(Min.x + W * 0.92f, Min.y + H * 0.82f);

    DrawList->AddRectFilled(TabMin, TabMax, FillColor, 2.0f);
    DrawList->AddRectFilled(BodyMin, BodyMax, FillColor, 2.0f);
    DrawList->AddRect(BodyMin, BodyMax, LineColor, 2.0f, 0, 1.0f);
    if (bOpen)
    {
        DrawList->AddLine(ImVec2(BodyMin.x + 2.0f, BodyMax.y),
                          ImVec2(BodyMax.x - 2.0f, BodyMax.y),
                          IM_COL32(255, 218, 126, bMuted ? 120 : 230),
                          1.0f);
    }
}

void DrawEffectIcon(ImDrawList* DrawList, const ImVec2& Min, const ImVec2& Max, ImU32 Color)
{
    const ImVec2 Center((Min.x + Max.x) * 0.5f, (Min.y + Max.y) * 0.5f);
    const float Radius = (Max.x - Min.x) * 0.25f;

    DrawList->AddCircle(Center, Radius, Color, 12, 1.3f);
    DrawList->AddCircleFilled(Center, Radius * 0.45f, Color, 8);
    DrawList->AddLine(ImVec2(Center.x - Radius - 2.0f, Center.y), ImVec2(Center.x - Radius * 0.45f, Center.y), Color, 1.2f);
    DrawList->AddLine(ImVec2(Center.x + Radius * 0.45f, Center.y), ImVec2(Center.x + Radius + 2.0f, Center.y), Color, 1.2f);
    DrawList->AddLine(ImVec2(Center.x, Center.y - Radius - 2.0f), ImVec2(Center.x, Center.y - Radius * 0.45f), Color, 1.2f);
    DrawList->AddLine(ImVec2(Center.x, Center.y + Radius * 0.45f), ImVec2(Center.x, Center.y + Radius + 2.0f), Color, 1.2f);
}

ImU32 GetVectorIconColor(EOutlinerActorIcon IconType, bool bVisible)
{
    if (!bVisible)
    {
        return IM_COL32(112, 116, 124, 180);
    }

    switch (IconType)
    {
    case EOutlinerActorIcon::StaticMesh:
        return IM_COL32(112, 171, 236, 255);
    case EOutlinerActorIcon::Camera:
        return IM_COL32(170, 204, 230, 255);
    case EOutlinerActorIcon::Effect:
        return IM_COL32(255, 158, 88, 255);
    case EOutlinerActorIcon::UI:
        return IM_COL32(116, 210, 186, 255);
    default:
        return IM_COL32(170, 182, 198, 255);
    }
}

void DrawActorTypeIcon(ImDrawList* DrawList,
                       const ImVec2& Min,
                       const ImVec2& Max,
                       EOutlinerActorIcon IconType,
                       ID3D11ShaderResourceView* Texture,
                       bool bVisible)
{
    const ImU32 BgColor = bVisible ? IM_COL32(30, 34, 40, 185) : IM_COL32(28, 29, 32, 120);
    const ImU32 BorderColor = bVisible ? IM_COL32(74, 83, 96, 210) : IM_COL32(54, 56, 62, 150);

    DrawList->AddRectFilled(Min, Max, BgColor, 3.0f);
    DrawList->AddRect(Min, Max, BorderColor, 3.0f, 0, 1.0f);

    if (Texture)
    {
        const ImVec2 Padding(1.0f, 1.0f);
        DrawList->AddImage(
            reinterpret_cast<ImTextureID>(Texture),
            ImVec2(Min.x + Padding.x, Min.y + Padding.y),
            ImVec2(Max.x - Padding.x, Max.y - Padding.y),
            ImVec2(0.0f, 0.0f),
            ImVec2(1.0f, 1.0f),
            bVisible ? IM_COL32_WHITE : IM_COL32(255, 255, 255, 115));
        return;
    }

    const ImU32 IconColor = GetVectorIconColor(IconType, bVisible);
    switch (IconType)
    {
    case EOutlinerActorIcon::StaticMesh:
        DrawCubeIcon(DrawList, Min, Max, IconColor);
        break;
    case EOutlinerActorIcon::Camera:
        DrawCameraIcon(DrawList, Min, Max, IconColor);
        break;
    case EOutlinerActorIcon::Effect:
        DrawEffectIcon(DrawList, Min, Max, IconColor);
        break;
    case EOutlinerActorIcon::UI:
        DrawUIIcon(DrawList, Min, Max, IconColor);
        break;
    default:
        DrawGenericActorIcon(DrawList, Min, Max, IconColor);
        break;
    }
}

void DrawOutlinerLeafMarker(ImDrawList* DrawList, const ImVec2& Center, ImU32 Color)
{
    DrawList->AddLine(ImVec2(Center.x - 4.0f, Center.y), ImVec2(Center.x + 3.0f, Center.y), Color, 1.0f);
    DrawList->AddCircleFilled(ImVec2(Center.x + 5.0f, Center.y), 1.3f, Color, 6);
}

std::wstring ResolveOutlinerIconPath(const std::wstring& FileName)
{
    return FPaths::Combine(FPaths::EditorDir(), L"Icons/" + FileName);
}

void LoadOutlinerIcon(ID3D11Device* Device, const std::wstring& FileName, ID3D11ShaderResourceView** OutIcon)
{
    if (!Device || !OutIcon || *OutIcon)
    {
        return;
    }

    DirectX::CreateWICTextureFromFile(
        Device,
        ResolveOutlinerIconPath(FileName).c_str(),
        nullptr,
        OutIcon);
}

void ReleaseOutlinerIcon(ID3D11ShaderResourceView*& Icon)
{
    if (Icon)
    {
        Icon->Release();
        Icon = nullptr;
    }
}

constexpr const char* OutlinerActorDragPayload = "CRASH_OUTLINER_ACTOR";
} // namespace

void FEditorWorldOutlinerPanel::Initialize(UEditorEngine* InEditorEngine)
{
    FEditorPanel::Initialize(InEditorEngine);
    SearchBuffer[0] = '\0';
    RenameBuffer[0] = '\0';
    NewFolderNameBuffer[0] = '\0';
    FolderRenameBuffer[0] = '\0';
    bRenameFocusPending = false;
    bNewFolderFocusPending = false;
    bFolderRenameFocusPending = false;
    bCreateFolderPopupRequested = false;
    CreateFolderParentPath.clear();
    RenamingFolderPath.clear();
}

void FEditorWorldOutlinerPanel::Initialize(UEditorEngine* InEditorEngine, ID3D11Device* InDevice)
{
    Initialize(InEditorEngine);
    LoadIconTextures(InDevice);
}

void FEditorWorldOutlinerPanel::Release()
{
    ReleaseOutlinerIcon(DirectionalLightIcon);
    ReleaseOutlinerIcon(PointLightIcon);
    ReleaseOutlinerIcon(SpotLightIcon);
    ReleaseOutlinerIcon(AmbientLightIcon);
    ReleaseOutlinerIcon(DecalIcon);
    ReleaseOutlinerIcon(HeightFogIcon);
}

void FEditorWorldOutlinerPanel::Render(float DeltaTime)
{
    (void)DeltaTime;

    if (!EditorEngine)
    {
        return;
    }

    UWorld* World = EditorEngine->GetWorld();
    if (!World)
    {
        ImGui::SetNextWindowSize(ImVec2(420.0f, 520.0f), ImGuiCond_Once);
        ImGui::Begin("World Outliner");
        ImGui::TextDisabled("No active world.");
        ImGui::End();
        return;
    }

    const TArray<AActor*>& Actors = World->GetActors();
    TArray<AActor*> FilteredActors = BuildFilteredActors();
    const int32 TotalActorCount = static_cast<int32>(Actors.size());
    const int32 FilteredActorCount = static_cast<int32>(FilteredActors.size());
    const int32 SelectedActorCount = static_cast<int32>(EditorEngine->GetSelectionManager().GetSelectedActors().size());

    ActorToDuplicate = nullptr;
    ActorToDelete = nullptr;
    bool bDeleteSelectedRequested = false;

    ImGui::SetNextWindowSize(ImVec2(460.0f, 520.0f), ImGuiCond_Once);
    if (!ImGui::Begin("World Outliner"))
    {
        ImGui::End();
        return;
    }

    DrawToolbar(TotalActorCount, FilteredActorCount, SelectedActorCount);
    ImGui::Separator();
    DrawActorList(Actors, FilteredActors);
    DrawCreateFolderPopup();

    const ImGuiIO& IO = ImGui::GetIO();
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
        !IO.WantTextInput &&
        ImGui::IsKeyPressed(ImGuiKey_Delete))
    {
        bDeleteSelectedRequested = true;
    }

    ImGui::End();

    if (ActorToDuplicate)
    {
        DuplicateActor(ActorToDuplicate);
    }

    if (ActorToDelete)
    {
        DeleteActor(ActorToDelete);
    }

    if (bDeleteSelectedRequested)
    {
        DeleteSelectedActors();
    }
}

void FEditorWorldOutlinerPanel::DrawToolbar(int32 TotalActorCount, int32 FilteredActorCount, int32 SelectedActorCount)
{
    const ImGuiStyle& Style = ImGui::GetStyle();
    const float ButtonSize = ImGui::GetFrameHeight();
    const float AvailableWidth = ImGui::GetContentRegionAvail().x;
    const float SearchWidth = (std::max)(120.0f, AvailableWidth - (ButtonSize * 3.0f) - (Style.ItemSpacing.x * 3.0f));

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 5.0f));
    ImGui::SetNextItemWidth(SearchWidth);
    ImGui::InputTextWithHint("##WorldOutlinerSearch", "Search Actors", SearchBuffer, sizeof(SearchBuffer));
    ImGui::PopStyleVar();

    ImGui::SameLine(0.0f, Style.ItemSpacing.x);
    if (DrawOutlinerToolbarButton("##WorldOutlinerCreateFolderButton", EOutlinerToolbarIcon::Folder, "Create folder"))
    {
        OpenCreateFolderPopup();
    }

    ImGui::SameLine(0.0f, Style.ItemSpacing.x);
    if (DrawOutlinerToolbarButton("##WorldOutlinerFilterButton", EOutlinerToolbarIcon::Filter, "Filter options"))
    {
        ImGui::OpenPopup("WorldOutlinerFilterPopup");
    }
    if (ImGui::BeginPopup("WorldOutlinerFilterPopup"))
    {
        ImGui::TextDisabled("Search includes Actor and Class");
        ImGui::Separator();
        const bool bHasSearch = SearchBuffer[0] != '\0';
        if (!bHasSearch)
        {
            ImGui::BeginDisabled();
        }
        if (ImGui::MenuItem("Clear Search"))
        {
            SearchBuffer[0] = '\0';
        }
        if (!bHasSearch)
        {
            ImGui::EndDisabled();
        }
        ImGui::EndPopup();
    }

    ImGui::SameLine(0.0f, Style.ItemSpacing.x);
    if (DrawOutlinerToolbarButton("##WorldOutlinerSettingsButton", EOutlinerToolbarIcon::Settings, "Outliner settings"))
    {
        ImGui::OpenPopup("WorldOutlinerSettingsPopup");
    }
    if (ImGui::BeginPopup("WorldOutlinerSettingsPopup"))
    {
        ImGui::MenuItem("Actor Label", nullptr, true, false);
        ImGui::MenuItem("Class Hint", nullptr, true, false);
        ImGui::MenuItem("Visibility", nullptr, true, false);
        ImGui::EndPopup();
    }

    char StatusText[96];
    if (SearchBuffer[0])
    {
        std::snprintf(StatusText, sizeof(StatusText), "%d / %d actors   %d selected",
                      FilteredActorCount,
                      TotalActorCount,
                      SelectedActorCount);
    }
    else
    {
        std::snprintf(StatusText, sizeof(StatusText), "%d actors   %d selected",
                      TotalActorCount,
                      SelectedActorCount);
    }
    ImGui::TextDisabled("%s", StatusText);
}

void FEditorWorldOutlinerPanel::DrawActorList(const TArray<AActor*>& Actors, const TArray<AActor*>& FilteredActors)
{
    const ImGuiStyle& Style = ImGui::GetStyle();
    const ImVec2 HeaderMin = ImGui::GetCursorScreenPos();
    const float HeaderWidth = (std::max)(1.0f, ImGui::GetContentRegionAvail().x);
    const ImVec2 HeaderMax(HeaderMin.x + HeaderWidth, HeaderMin.y + OutlinerHeaderHeight);
    ImDrawList* DrawList = ImGui::GetWindowDrawList();

    DrawList->AddRectFilled(HeaderMin, HeaderMax, IM_COL32(29, 31, 35, 255));
    DrawList->AddLine(ImVec2(HeaderMin.x, HeaderMax.y), HeaderMax, IM_COL32(58, 62, 70, 255));
    DrawList->AddText(
        ImVec2(HeaderMin.x + OutlinerTreeGutterWidth + OutlinerIconSize + 9.0f, HeaderMin.y + 3.0f),
        IM_COL32(170, 170, 170, 255),
        "Label");
    DrawOutlinerEye(
        DrawList,
        ImVec2(HeaderMax.x - OutlinerVisibilityWidth * 0.5f, HeaderMin.y + OutlinerHeaderHeight * 0.5f),
        true,
        IM_COL32(150, 158, 168, 255));
    ImGui::Dummy(ImVec2(HeaderWidth, OutlinerHeaderHeight));

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(Style.ItemSpacing.x, 1.0f));
    if (!ImGui::BeginChild("WorldOutlinerRows", ImVec2(0.0f, 0.0f), false, ImGuiWindowFlags_AlwaysVerticalScrollbar))
    {
        ImGui::EndChild();
        ImGui::PopStyleVar();
        return;
    }

    const TArray<FString> VisibleFolders = BuildVisibleFolders(Actors, FilteredActors);
    const TArray<FString> RootFolders = BuildChildFolderPaths(FString(), VisibleFolders);
    const TArray<AActor*> VisibleRootActors = BuildVisibleRootActors(Actors, FilteredActors);
    TArray<AActor*> VisibleSelectionRange;
    for (const FString& FolderPath : RootFolders)
    {
        AppendVisibleActorsForFolderBranch(FolderPath, VisibleFolders, Actors, FilteredActors, VisibleSelectionRange);
    }
    VisibleSelectionRange.insert(VisibleSelectionRange.end(), VisibleRootActors.begin(), VisibleRootActors.end());

    if (RootFolders.empty() && VisibleRootActors.empty())
    {
        ImGui::TextDisabled(SearchBuffer[0] ? "No matching actors." : "No actors.");
        DrawRootDropTarget();
        DrawBackgroundContextMenu();
        ImGui::EndChild();
        ImGui::PopStyleVar();
        return;
    }

    for (const FString& FolderPath : RootFolders)
    {
        DrawFolderBranch(FolderPath, VisibleFolders, Actors, FilteredActors, VisibleSelectionRange);
    }

    for (AActor* Actor : VisibleRootActors)
    {
        DrawActorRow(Actor, VisibleSelectionRange);
    }

    DrawRootDropTarget();
    DrawBackgroundContextMenu();
    ImGui::EndChild();
    ImGui::PopStyleVar();
}

void FEditorWorldOutlinerPanel::DrawFolderBranch(const FString& FolderPath,
                                                 const TArray<FString>& VisibleFolders,
                                                 const TArray<AActor*>& Actors,
                                                 const TArray<AActor*>& FilteredActors,
                                                 const TArray<AActor*>& VisibleSelectionRange)
{
    const TArray<FString> ChildFolders = BuildChildFolderPaths(FolderPath, VisibleFolders);
    const TArray<AActor*> FolderActors = BuildVisibleActorsForFolder(FolderPath, Actors, FilteredActors);
    DrawFolderRow(FolderPath, FolderActors, !ChildFolders.empty());

    const bool bOpen = SearchBuffer[0] != '\0' || OpenFolderPaths.find(FolderPath) != OpenFolderPaths.end();
    if (!bOpen)
    {
        return;
    }

    for (const FString& ChildFolderPath : ChildFolders)
    {
        DrawFolderBranch(ChildFolderPath, VisibleFolders, Actors, FilteredActors, VisibleSelectionRange);
    }

    for (AActor* Actor : FolderActors)
    {
        DrawActorRow(Actor, VisibleSelectionRange);
    }
}

void FEditorWorldOutlinerPanel::DrawFolderRow(const FString& FolderPath,
                                              const TArray<AActor*>& FolderActors,
                                              bool bHasChildFolders)
{
    if (!EditorEngine)
    {
        return;
    }

    const bool bOpen = SearchBuffer[0] != '\0' || OpenFolderPaths.find(FolderPath) != OpenFolderPaths.end();
    const bool bRenaming = RenamingFolderPath == FolderPath;
    const bool bMuted = FolderActors.empty() && !bHasChildFolders;
    const float FolderIndent = static_cast<float>(GetFolderDepth(FolderPath)) * 18.0f;
    const FString FolderLabel = GetFolderLeafName(FolderPath);

    ImGui::PushID(FolderPath.c_str());
    const ImVec2 RowMin = ImGui::GetCursorScreenPos();
    const float RowWidth = (std::max)(1.0f, ImGui::GetContentRegionAvail().x);
    const ImVec2 RowMax(RowMin.x + RowWidth, RowMin.y + OutlinerRowHeight);

    ImGui::SetNextItemAllowOverlap();
    ImGui::InvisibleButton("##WorldOutlinerFolderRow", ImVec2(RowWidth, OutlinerRowHeight));
    const bool bClicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
    const bool bHovered = ImGui::IsItemHovered();
    const ImVec2 AfterRowCursor = ImGui::GetCursorScreenPos();

    if (bClicked && !bRenaming)
    {
        if (bOpen)
        {
            OpenFolderPaths.erase(FolderPath);
        }
        else
        {
            OpenFolderPaths.insert(FolderPath);
        }
    }

    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload(OutlinerActorDragPayload))
        {
            if (Payload->Data && Payload->DataSize == sizeof(AActor*))
            {
                AActor* DraggedActor = *static_cast<AActor**>(Payload->Data);
                MoveSelectedActorsToFolder(FolderPath, DraggedActor);
                OpenFolderPaths.insert(FolderPath);
            }
        }
        ImGui::EndDragDropTarget();
    }

    ImDrawList* DrawList = ImGui::GetWindowDrawList();
    if (bHovered)
    {
        DrawList->AddRectFilled(RowMin, RowMax, IM_COL32(255, 255, 255, 14));
    }

    if (ImGui::BeginPopupContextItem("WorldOutlinerFolderContext"))
    {
        if (ImGui::MenuItem("Rename"))
        {
            BeginRenameFolder(FolderPath);
        }

        const bool bHasSelectedActors = !EditorEngine->GetSelectionManager().GetSelectedActors().empty();
        if (!bHasSelectedActors)
        {
            ImGui::BeginDisabled();
        }
        if (ImGui::MenuItem("Move Selected Here"))
        {
            MoveSelectedActorsToFolder(FolderPath, nullptr);
            OpenFolderPaths.insert(FolderPath);
        }
        if (!bHasSelectedActors)
        {
            ImGui::EndDisabled();
        }

        ImGui::Separator();
        if (ImGui::MenuItem("Create Folder"))
        {
            OpenCreateFolderPopup(FolderPath);
        }
        if (ImGui::MenuItem("Remove Folder"))
        {
            RemoveFolder(FolderPath);
        }
        ImGui::EndPopup();
    }

    const ImU32 ArrowColor = bMuted ? IM_COL32(120, 120, 120, 180) : IM_COL32(190, 196, 206, 255);
    const ImVec2 ArrowCenter(RowMin.x + 8.0f + FolderIndent, RowMin.y + OutlinerRowHeight * 0.5f);
    if (bOpen)
    {
        DrawList->AddTriangleFilled(
            ImVec2(ArrowCenter.x - 4.0f, ArrowCenter.y - 2.5f),
            ImVec2(ArrowCenter.x + 4.0f, ArrowCenter.y - 2.5f),
            ImVec2(ArrowCenter.x, ArrowCenter.y + 3.5f),
            ArrowColor);
    }
    else
    {
        DrawList->AddTriangleFilled(
            ImVec2(ArrowCenter.x - 2.0f, ArrowCenter.y - 4.0f),
            ImVec2(ArrowCenter.x - 2.0f, ArrowCenter.y + 4.0f),
            ImVec2(ArrowCenter.x + 4.0f, ArrowCenter.y),
            ArrowColor);
    }

    const ImVec2 IconMin(RowMin.x + OutlinerTreeGutterWidth + FolderIndent, RowMin.y + (OutlinerRowHeight - OutlinerIconSize) * 0.5f);
    const ImVec2 IconMax(IconMin.x + OutlinerIconSize, IconMin.y + OutlinerIconSize);
    DrawFolderIcon(DrawList, IconMin, IconMax, bOpen, bMuted);

    const float NameX = IconMax.x + 7.0f;
    const float CountWidth = 42.0f;
    const float CountX = RowMax.x - OutlinerVisibilityWidth - CountWidth;
    const float TextY = RowMin.y + (OutlinerRowHeight - ImGui::GetTextLineHeight()) * 0.5f;
    const ImU32 TextColor = bMuted ? IM_COL32(145, 145, 145, 255) : IM_COL32(226, 226, 226, 255);

    if (bRenaming)
    {
        ImGui::SetCursorScreenPos(ImVec2(NameX, RowMin.y + 1.0f));
        ImGui::SetNextItemWidth((std::max)(80.0f, CountX - NameX - 8.0f));
        DrawFolderRenameInput(FolderPath);
        ImGui::SetCursorScreenPos(AfterRowCursor);
        ImGui::Dummy(ImVec2(0.0f, 0.0f));
    }
    else
    {
        DrawList->PushClipRect(ImVec2(NameX, RowMin.y), ImVec2(CountX - 8.0f, RowMax.y), true);
        DrawList->AddText(ImVec2(NameX, TextY), TextColor, FolderLabel.c_str());
        DrawList->PopClipRect();
    }

    if (bHovered && FolderLabel != FolderPath)
    {
        ImGui::SetTooltip("%s", FolderPath.c_str());
    }

    char CountText[32];
    std::snprintf(CountText, sizeof(CountText), "%d", static_cast<int32>(FolderActors.size()));
    DrawList->AddText(ImVec2(CountX, TextY), IM_COL32(135, 142, 152, 255), CountText);

    ImGui::PopID();
}

void FEditorWorldOutlinerPanel::DrawActorRow(AActor* Actor, const TArray<AActor*>& FilteredActors)
{
    if (!Actor || !EditorEngine)
    {
        return;
    }

    FSelectionManager& Selection = EditorEngine->GetSelectionManager();
    const FString ActorName = GetActorDisplayName(Actor);
    const FString ClassName = GetActorClassName(Actor);
    const bool bVisible = Actor->IsVisible();

    ImGui::PushID(Actor);
    const ImVec2 RowMin = ImGui::GetCursorScreenPos();
    const float RowWidth = (std::max)(1.0f, ImGui::GetContentRegionAvail().x);
    const ImVec2 RowMax(RowMin.x + RowWidth, RowMin.y + OutlinerRowHeight);
    const ImVec2 VisibilityMin(RowMax.x - OutlinerVisibilityWidth, RowMin.y);
    const ImVec2 VisibilityMax(RowMax.x, RowMax.y);
    const bool bMouseInVisibility = IsMouseInRect(VisibilityMin, VisibilityMax);

    ImGui::SetNextItemAllowOverlap();
    ImGui::InvisibleButton("##WorldOutlinerActorRow", ImVec2(RowWidth, OutlinerRowHeight));
    const bool bClicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
    const bool bRightClicked = ImGui::IsItemClicked(ImGuiMouseButton_Right);
    const bool bHovered = ImGui::IsItemHovered();
    const ImVec2 AfterRowCursor = ImGui::GetCursorScreenPos();

    if (bClicked && bMouseInVisibility)
    {
        Actor->SetVisible(!bVisible);
    }
    else if (bClicked && RenamingActor != Actor)
    {
        SelectActorFromRow(Actor, FilteredActors);
    }

    if (bRightClicked && !Selection.IsSelected(Actor))
    {
        Selection.Select(Actor);
    }

    const bool bSelected = Selection.IsSelected(Actor);
    ImDrawList* DrawList = ImGui::GetWindowDrawList();
    if (bSelected)
    {
        DrawList->AddRectFilled(RowMin, RowMax, IM_COL32(49, 92, 152, 185));
    }
    else if (bHovered)
    {
        DrawList->AddRectFilled(RowMin, RowMax, IM_COL32(255, 255, 255, 16));
    }

    if (ImGui::BeginPopupContextItem("WorldOutlinerActorContext"))
    {
        if (ImGui::MenuItem("Rename"))
        {
            BeginRename(Actor);
        }
        if (ImGui::MenuItem("Duplicate"))
        {
            ActorToDuplicate = Actor;
        }
        if (ImGui::MenuItem("Delete"))
        {
            ActorToDelete = Actor;
        }
        if (ImGui::BeginMenu("Move To Folder"))
        {
            if (ImGui::MenuItem("None"))
            {
                MoveSelectedActorsToFolder(FString(), Actor);
            }
            const TArray<FString> Folders = GetSortedFolderPaths();
            if (!Folders.empty())
            {
                ImGui::Separator();
            }
            for (const FString& FolderPath : Folders)
            {
                if (ImGui::MenuItem(FolderPath.c_str()))
                {
                    MoveSelectedActorsToFolder(FolderPath, Actor);
                    OpenFolderPaths.insert(FolderPath);
                }
            }
            ImGui::EndMenu();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Copy Name"))
        {
            ImGui::SetClipboardText(ActorName.c_str());
        }
        ImGui::EndPopup();
    }

    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
    {
        ImGui::SetDragDropPayload(OutlinerActorDragPayload, &Actor, sizeof(AActor*));
        ImGui::TextUnformatted(ActorName.c_str());
        ImGui::EndDragDropSource();
    }

    const float TextY = RowMin.y + (OutlinerRowHeight - ImGui::GetTextLineHeight()) * 0.5f;
    const float ChildIndent = Actor->GetEditorFolderPath().empty()
                                  ? 0.0f
                                  : static_cast<float>(GetFolderDepth(Actor->GetEditorFolderPath()) + 1) * 18.0f;
    const ImVec2 GuideTop(RowMin.x + 8.0f + ChildIndent, RowMin.y);
    const ImVec2 GuideBottom(RowMin.x + 8.0f + ChildIndent, RowMax.y);
    DrawList->AddLine(GuideTop, GuideBottom, IM_COL32(58, 61, 68, 130), 1.0f);
    DrawOutlinerLeafMarker(
        DrawList,
        ImVec2(RowMin.x + 8.0f + ChildIndent, RowMin.y + OutlinerRowHeight * 0.5f),
        IM_COL32(106, 112, 124, bVisible ? 210 : 120));

    const ImVec2 IconMin(RowMin.x + OutlinerTreeGutterWidth + ChildIndent, RowMin.y + (OutlinerRowHeight - OutlinerIconSize) * 0.5f);
    const ImVec2 IconMax(IconMin.x + OutlinerIconSize, IconMin.y + OutlinerIconSize);
    const ImU32 MainTextColor = bVisible ? IM_COL32(226, 226, 226, 255) : IM_COL32(130, 130, 130, 255);
    const ImU32 MutedTextColor = bVisible ? IM_COL32(138, 148, 160, 255) : IM_COL32(92, 92, 92, 255);

    const EOutlinerActorIcon IconType = ResolveActorIconType(Actor);
    ID3D11ShaderResourceView* IconTexture = nullptr;
    switch (IconType)
    {
    case EOutlinerActorIcon::DirectionalLight:
        IconTexture = DirectionalLightIcon;
        break;
    case EOutlinerActorIcon::PointLight:
        IconTexture = PointLightIcon;
        break;
    case EOutlinerActorIcon::SpotLight:
        IconTexture = SpotLightIcon;
        break;
    case EOutlinerActorIcon::AmbientLight:
        IconTexture = AmbientLightIcon;
        break;
    case EOutlinerActorIcon::Decal:
        IconTexture = DecalIcon;
        break;
    case EOutlinerActorIcon::HeightFog:
        IconTexture = HeightFogIcon;
        break;
    default:
        break;
    }
    DrawActorTypeIcon(DrawList, IconMin, IconMax, IconType, IconTexture, bVisible);

    const float NameX = IconMax.x + 7.0f;
    const float ContentMaxX = VisibilityMin.x - 8.0f;
    if (RenamingActor == Actor)
    {
        ImGui::SetCursorScreenPos(ImVec2(NameX, RowMin.y + 1.0f));
        ImGui::SetNextItemWidth((std::max)(80.0f, ContentMaxX - NameX));
        DrawRenameInput(Actor);
        ImGui::SetCursorScreenPos(AfterRowCursor);
        ImGui::Dummy(ImVec2(0.0f, 0.0f));
    }
    else
    {
        const float AvailableTextWidth = (std::max)(0.0f, ContentMaxX - NameX);
        const float ActorNameWidth = ImGui::CalcTextSize(ActorName.c_str()).x;
        const float ClassNameWidth = ImGui::CalcTextSize(ClassName.c_str()).x;
        float ClassX = ContentMaxX - ClassNameWidth;
        float NameClipMaxX = ContentMaxX;

        if (ClassNameWidth > 0.0f && AvailableTextWidth > 190.0f && ClassX > NameX + 86.0f)
        {
            NameClipMaxX = ClassX - 10.0f;
            DrawList->PushClipRect(ImVec2(ClassX, RowMin.y), ImVec2(ContentMaxX, RowMax.y), true);
            DrawList->AddText(ImVec2(ClassX, TextY), MutedTextColor, ClassName.c_str());
            DrawList->PopClipRect();
        }
        else if (ActorNameWidth + 10.0f + ClassNameWidth < AvailableTextWidth)
        {
            ClassX = NameX + ActorNameWidth + 10.0f;
            DrawList->AddText(ImVec2(ClassX, TextY), MutedTextColor, ClassName.c_str());
            NameClipMaxX = ClassX - 2.0f;
        }

        DrawList->PushClipRect(ImVec2(NameX, RowMin.y), ImVec2(NameClipMaxX, RowMax.y), true);
        DrawList->AddText(ImVec2(NameX, TextY), MainTextColor, ActorName.c_str());
        DrawList->PopClipRect();
    }

    if (bHovered && bMouseInVisibility)
    {
        ImGui::SetTooltip(bVisible ? "Hide actor" : "Show actor");
    }

    const ImVec2 EyeCenter(
        VisibilityMin.x + OutlinerVisibilityWidth * 0.5f,
        VisibilityMin.y + OutlinerRowHeight * 0.5f);
    const ImU32 EyeColor = bVisible ? IM_COL32(190, 204, 224, 255) : IM_COL32(95, 95, 95, 255);
    DrawOutlinerEye(DrawList, EyeCenter, bVisible, EyeColor);

    ImGui::PopID();
}

void FEditorWorldOutlinerPanel::DrawRootDropTarget()
{
    const ImVec2 Available = ImGui::GetContentRegionAvail();
    if (Available.y <= 2.0f)
    {
        return;
    }

    ImGui::InvisibleButton("##WorldOutlinerRootDropTarget", ImVec2((std::max)(1.0f, Available.x), Available.y));
    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload(OutlinerActorDragPayload))
        {
            if (Payload->Data && Payload->DataSize == sizeof(AActor*))
            {
                AActor* DraggedActor = *static_cast<AActor**>(Payload->Data);
                MoveSelectedActorsToFolder(FString(), DraggedActor);
            }
        }
        ImGui::EndDragDropTarget();
    }

    if (ImGui::BeginPopupContextItem("WorldOutlinerRootContext"))
    {
        if (ImGui::MenuItem("Create Folder"))
        {
            OpenCreateFolderPopup();
        }
        ImGui::EndPopup();
    }
}

void FEditorWorldOutlinerPanel::LoadIconTextures(ID3D11Device* InDevice)
{
    LoadOutlinerIcon(InDevice, L"DirectionalLight_64x.png", &DirectionalLightIcon);
    LoadOutlinerIcon(InDevice, L"PointLight_64x.png", &PointLightIcon);
    LoadOutlinerIcon(InDevice, L"SpotLight_64x.png", &SpotLightIcon);
    LoadOutlinerIcon(InDevice, L"SkyLightComponent_64x.png", &AmbientLightIcon);
    LoadOutlinerIcon(InDevice, L"S_DecalActorIcon.png", &DecalIcon);
    LoadOutlinerIcon(InDevice, L"S_ExpoHeightFog.png", &HeightFogIcon);
}

void FEditorWorldOutlinerPanel::DrawBackgroundContextMenu()
{
    if (!EditorEngine)
    {
        return;
    }

    if (ImGui::BeginPopupContextWindow("WorldOutlinerBackgroundContext",
                                       ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
    {
        if (ImGui::MenuItem("Create Folder"))
        {
            OpenCreateFolderPopup();
        }
        ImGui::EndPopup();
    }
}

void FEditorWorldOutlinerPanel::DrawCreateFolderPopup()
{
    if (bCreateFolderPopupRequested)
    {
        ImGui::OpenPopup("WorldOutlinerCreateFolderPopup");
        bCreateFolderPopupRequested = false;
    }

    if (ImGui::BeginPopup("WorldOutlinerCreateFolderPopup"))
    {
        if (bNewFolderFocusPending)
        {
            ImGui::SetKeyboardFocusHere();
            bNewFolderFocusPending = false;
        }

        ImGui::SetNextItemWidth(220.0f);
        const bool bSubmitted = ImGui::InputText("##WorldOutlinerNewFolderName",
                                                 NewFolderNameBuffer,
                                                 sizeof(NewFolderNameBuffer),
                                                 ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
        if (bSubmitted)
        {
            CreateFolderFromBuffer();
            ImGui::CloseCurrentPopup();
        }

        if (ImGui::Button("Create", ImVec2(86.0f, 0.0f)))
        {
            CreateFolderFromBuffer();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(86.0f, 0.0f)) || ImGui::IsKeyPressed(ImGuiKey_Escape))
        {
            CreateFolderParentPath.clear();
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void FEditorWorldOutlinerPanel::OpenCreateFolderPopup(const FString& ParentFolderPath)
{
    NewFolderNameBuffer[0] = '\0';
    bNewFolderFocusPending = true;
    bCreateFolderPopupRequested = true;
    CreateFolderParentPath = ParentFolderPath.empty() ? FString() : NormalizeFolderName(ParentFolderPath);
}

void FEditorWorldOutlinerPanel::CreateFolderFromBuffer()
{
    if (!EditorEngine)
    {
        return;
    }

    UWorld* World = EditorEngine->GetWorld();
    if (!World)
    {
        return;
    }

    FString BaseFolderPath = NewFolderNameBuffer[0] ? NewFolderNameBuffer : "New Folder";
    if (!CreateFolderParentPath.empty())
    {
        BaseFolderPath = CreateFolderParentPath + "/" + BaseFolderPath;
    }

    const FString FolderPath = MakeUniqueFolderName(BaseFolderPath);
    AddWorldFolderAndParents(World, FolderPath);
    OpenFolderAndParents(OpenFolderPaths, FolderPath);
    CreateFolderParentPath.clear();
}

void FEditorWorldOutlinerPanel::BeginRename(AActor* Actor)
{
    if (!Actor)
    {
        return;
    }

    RenamingActor = Actor;
    strncpy_s(RenameBuffer, sizeof(RenameBuffer), GetActorDisplayName(Actor).c_str(), _TRUNCATE);
    bRenameFocusPending = true;
}

void FEditorWorldOutlinerPanel::BeginRenameFolder(const FString& FolderPath)
{
    RenamingFolderPath = FolderPath;
    strncpy_s(FolderRenameBuffer, sizeof(FolderRenameBuffer), GetFolderLeafName(FolderPath).c_str(), _TRUNCATE);
    bFolderRenameFocusPending = true;
}

bool FEditorWorldOutlinerPanel::DrawFolderRenameInput(const FString& FolderPath)
{
    if (bFolderRenameFocusPending)
    {
        ImGui::SetKeyboardFocusHere();
        bFolderRenameFocusPending = false;
    }

    if (ImGui::InputText("##WorldOutlinerFolderRename",
                         FolderRenameBuffer,
                         sizeof(FolderRenameBuffer),
                         ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll))
    {
        RenameFolder(FolderPath, FolderRenameBuffer);
        RenamingFolderPath.clear();
        return true;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Escape))
    {
        RenamingFolderPath.clear();
        return false;
    }

    if (!ImGui::IsItemActive() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        RenamingFolderPath.clear();
    }

    return false;
}

bool FEditorWorldOutlinerPanel::DrawRenameInput(AActor* Actor)
{
    if (!Actor)
    {
        RenamingActor = nullptr;
        bRenameFocusPending = false;
        return false;
    }

    if (bRenameFocusPending)
    {
        ImGui::SetKeyboardFocusHere();
        bRenameFocusPending = false;
    }

    if (ImGui::InputText("##WorldOutlinerRename", RenameBuffer, sizeof(RenameBuffer),
                         ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll))
    {
        if (RenameBuffer[0] != '\0')
        {
            Actor->SetFName(FName(RenameBuffer));
        }
        RenamingActor = nullptr;
        return true;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Escape))
    {
        RenamingActor = nullptr;
        return false;
    }

    if (!ImGui::IsItemActive() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        RenamingActor = nullptr;
    }

    return false;
}

void FEditorWorldOutlinerPanel::SelectActorFromRow(AActor* Actor, const TArray<AActor*>& FilteredActors)
{
    if (!EditorEngine || !Actor)
    {
        return;
    }

    FSelectionManager& Selection = EditorEngine->GetSelectionManager();
    const ImGuiIO& IO = ImGui::GetIO();

    if (IO.KeyShift)
    {
        Selection.SelectRange(Actor, FilteredActors);
    }
    else if (IO.KeyCtrl)
    {
        Selection.ToggleSelect(Actor);
    }
    else
    {
        Selection.Select(Actor);
    }
}

void FEditorWorldOutlinerPanel::DuplicateActor(AActor* Actor)
{
    if (!Actor || !EditorEngine)
    {
        return;
    }

    AActor* Dup = Cast<AActor>(Actor->Duplicate(nullptr));
    if (!Dup)
    {
        return;
    }

    FString NewName = GetActorDisplayName(Actor) + " (Copy)";
    Dup->SetFName(FName(NewName));

    FSelectionManager& Selection = EditorEngine->GetSelectionManager();
    Selection.ClearSelection();
    Selection.Select(Dup);
}

void FEditorWorldOutlinerPanel::DeleteActor(AActor* Actor)
{
    if (!Actor || !EditorEngine)
    {
        return;
    }

    FSelectionManager& Selection = EditorEngine->GetSelectionManager();
    if (Selection.IsSelected(Actor))
    {
        DeleteSelectedActors();
        return;
    }

    DeleteActors(TArray<AActor*>{ Actor });
}

void FEditorWorldOutlinerPanel::DeleteSelectedActors()
{
    if (!EditorEngine)
    {
        return;
    }

    const TArray<AActor*> ActorsToDelete = EditorEngine->GetSelectionManager().GetSelectedActors();
    DeleteActors(ActorsToDelete);
}

void FEditorWorldOutlinerPanel::DeleteActors(const TArray<AActor*>& ActorsToDelete)
{
    if (!EditorEngine || ActorsToDelete.empty())
    {
        return;
    }

    UWorld* World = EditorEngine->GetWorld();
    if (!World)
    {
        return;
    }

    FSelectionManager& Selection = EditorEngine->GetSelectionManager();
    Selection.ClearSelection();

    World->BeginDeferredPickingBVHUpdate();
    TSet<AActor*> DestroyedActors;
    for (AActor* Actor : ActorsToDelete)
    {
        if (!Actor || Actor->GetWorld() != World || DestroyedActors.find(Actor) != DestroyedActors.end())
        {
            continue;
        }

        DestroyedActors.insert(Actor);
        if (RenamingActor == Actor)
        {
            RenamingActor = nullptr;
            bRenameFocusPending = false;
        }
        World->DestroyActor(Actor);
    }
    World->EndDeferredPickingBVHUpdate();
}

void FEditorWorldOutlinerPanel::RenameFolder(const FString& OldFolderPath, const FString& NewFolderName)
{
    if (!EditorEngine || OldFolderPath.empty())
    {
        return;
    }

    UWorld* World = EditorEngine->GetWorld();
    if (!World)
    {
        return;
    }

    const FString ParentPath = GetFolderParentPath(OldFolderPath);
    FString RequestedFolderPath = NewFolderName;
    if (!ParentPath.empty())
    {
        RequestedFolderPath = ParentPath + "/" + RequestedFolderPath;
    }

    const FString NewFolderPath = MakeUniqueFolderName(RequestedFolderPath, OldFolderPath);
    if (NewFolderPath.empty() || NewFolderPath == OldFolderPath)
    {
        return;
    }

    TArray<FString> UpdatedFolders;
    TSet<FString> SeenFolders;
    for (const FString& ExistingFolder : World->GetEditorActorFolders())
    {
        const FString UpdatedFolder = IsFolderPathWithin(ExistingFolder, OldFolderPath)
                                          ? ReplaceFolderPrefix(ExistingFolder, OldFolderPath, NewFolderPath)
                                          : ExistingFolder;
        AddFolderAndParents(SeenFolders, UpdatedFolders, UpdatedFolder);
    }

    for (AActor* Actor : World->GetActors())
    {
        if (Actor && IsFolderPathWithin(Actor->GetEditorFolderPath(), OldFolderPath))
        {
            const FString UpdatedActorFolder = ReplaceFolderPrefix(Actor->GetEditorFolderPath(), OldFolderPath, NewFolderPath);
            Actor->SetEditorFolderPath(UpdatedActorFolder);
            AddFolderAndParents(SeenFolders, UpdatedFolders, UpdatedActorFolder);
        }
    }

    AddFolderAndParents(SeenFolders, UpdatedFolders, NewFolderPath);
    World->SetEditorActorFolders(UpdatedFolders);

    TSet<FString> UpdatedOpenFolders;
    for (const FString& OpenFolderPath : OpenFolderPaths)
    {
        if (IsFolderPathWithin(OpenFolderPath, OldFolderPath))
        {
            UpdatedOpenFolders.insert(ReplaceFolderPrefix(OpenFolderPath, OldFolderPath, NewFolderPath));
        }
        else
        {
            UpdatedOpenFolders.insert(OpenFolderPath);
        }
    }
    OpenFolderPaths = UpdatedOpenFolders;
    OpenFolderAndParents(OpenFolderPaths, NewFolderPath);
}

void FEditorWorldOutlinerPanel::RemoveFolder(const FString& FolderPath)
{
    if (!EditorEngine || FolderPath.empty())
    {
        return;
    }

    UWorld* World = EditorEngine->GetWorld();
    if (!World)
    {
        return;
    }

    for (AActor* Actor : World->GetActors())
    {
        if (Actor && IsFolderPathWithin(Actor->GetEditorFolderPath(), FolderPath))
        {
            Actor->SetEditorFolderPath(FString());
        }
    }

    TArray<FString> UpdatedFolders;
    for (const FString& ExistingFolder : World->GetEditorActorFolders())
    {
        if (!IsFolderPathWithin(ExistingFolder, FolderPath))
        {
            UpdatedFolders.push_back(ExistingFolder);
        }
    }
    World->SetEditorActorFolders(UpdatedFolders);

    for (auto It = OpenFolderPaths.begin(); It != OpenFolderPaths.end();)
    {
        if (IsFolderPathWithin(*It, FolderPath))
        {
            It = OpenFolderPaths.erase(It);
        }
        else
        {
            ++It;
        }
    }
    if (IsFolderPathWithin(RenamingFolderPath, FolderPath))
    {
        RenamingFolderPath.clear();
        bFolderRenameFocusPending = false;
    }
}

void FEditorWorldOutlinerPanel::MoveActorToFolder(AActor* Actor, const FString& FolderPath)
{
    if (!Actor || !EditorEngine)
    {
        return;
    }

    UWorld* World = EditorEngine->GetWorld();
    if (!World)
    {
        return;
    }

    if (!FolderPath.empty())
    {
        AddWorldFolderAndParents(World, FolderPath);
        OpenFolderAndParents(OpenFolderPaths, FolderPath);
    }
    Actor->SetEditorFolderPath(FolderPath);
}

void FEditorWorldOutlinerPanel::MoveSelectedActorsToFolder(const FString& FolderPath, AActor* FallbackActor)
{
    if (!EditorEngine)
    {
        return;
    }

    FSelectionManager& Selection = EditorEngine->GetSelectionManager();
    const TArray<AActor*>& SelectedActors = Selection.GetSelectedActors();
    bool bMovedAny = false;

    if (FallbackActor && Selection.IsSelected(FallbackActor))
    {
        for (AActor* Actor : SelectedActors)
        {
            MoveActorToFolder(Actor, FolderPath);
            bMovedAny = true;
        }
    }
    else if (!SelectedActors.empty() && !FallbackActor)
    {
        for (AActor* Actor : SelectedActors)
        {
            MoveActorToFolder(Actor, FolderPath);
            bMovedAny = true;
        }
    }

    if (!bMovedAny && FallbackActor)
    {
        MoveActorToFolder(FallbackActor, FolderPath);
    }
}

TArray<AActor*> FEditorWorldOutlinerPanel::BuildFilteredActors() const
{
    TArray<AActor*> Result;
    if (!EditorEngine)
    {
        return Result;
    }

    UWorld* World = EditorEngine->GetWorld();
    if (!World)
    {
        return Result;
    }

    const TArray<AActor*>& Actors = World->GetActors();
    Result.reserve(Actors.size());
    for (AActor* Actor : Actors)
    {
        if (Actor && MatchesSearch(Actor))
        {
            Result.push_back(Actor);
        }
    }

    return Result;
}

TArray<FString> FEditorWorldOutlinerPanel::BuildVisibleFolders(const TArray<AActor*>& Actors, const TArray<AActor*>& FilteredActors) const
{
    TArray<FString> Result;
    TSet<FString> SeenFolders;
    TArray<FString> KnownFolders;
    TSet<FString> SeenKnownFolders;

    auto AddKnownFolder = [&](const FString& FolderPath)
    {
        if (FolderPath.empty())
        {
            return;
        }
        TArray<FString> FolderAndParents;
        TSet<FString> LocalSeenFolders;
        AddFolderAndParents(LocalSeenFolders, FolderAndParents, FolderPath);
        for (const FString& Path : FolderAndParents)
        {
            if (SeenKnownFolders.find(Path) == SeenKnownFolders.end())
            {
                SeenKnownFolders.insert(Path);
                KnownFolders.push_back(Path);
            }
        }
    };

    if (EditorEngine)
    {
        if (UWorld* World = EditorEngine->GetWorld())
        {
            for (const FString& FolderPath : World->GetEditorActorFolders())
            {
                AddKnownFolder(FolderPath);
            }
        }
    }

    for (AActor* Actor : Actors)
    {
        if (Actor)
        {
            AddKnownFolder(Actor->GetEditorFolderPath());
        }
    }

    if (SearchBuffer[0] == '\0')
    {
        for (const FString& FolderPath : KnownFolders)
        {
            AddFolderAndParents(SeenFolders, Result, FolderPath);
        }
    }
    else
    {
        for (const FString& FolderPath : KnownFolders)
        {
            if (FolderMatchesSearch(FolderPath))
            {
                AddFolderAndParents(SeenFolders, Result, FolderPath);
            }
        }

        for (AActor* Actor : Actors)
        {
            if (Actor && !Actor->GetEditorFolderPath().empty() && ActorIsInList(Actor, FilteredActors))
            {
                AddFolderAndParents(SeenFolders, Result, Actor->GetEditorFolderPath());
            }
        }
    }

    std::sort(Result.begin(), Result.end(), [](const FString& A, const FString& B)
    {
        return ToLowerAscii(A) < ToLowerAscii(B);
    });
    return Result;
}

TArray<AActor*> FEditorWorldOutlinerPanel::BuildVisibleActorsForFolder(const FString& FolderPath,
                                                                       const TArray<AActor*>& Actors,
                                                                       const TArray<AActor*>& FilteredActors) const
{
    TArray<AActor*> Result;
    const bool bFolderMatchedSearch = FolderMatchesSearch(FolderPath);
    for (AActor* Actor : Actors)
    {
        if (!Actor || Actor->GetEditorFolderPath() != FolderPath)
        {
            continue;
        }

        if (SearchBuffer[0] == '\0' || bFolderMatchedSearch || ActorIsInList(Actor, FilteredActors))
        {
            Result.push_back(Actor);
        }
    }
    return Result;
}

TArray<AActor*> FEditorWorldOutlinerPanel::BuildVisibleRootActors(const TArray<AActor*>& Actors, const TArray<AActor*>& FilteredActors) const
{
    TArray<AActor*> Result;
    for (AActor* Actor : Actors)
    {
        if (!Actor || !Actor->GetEditorFolderPath().empty())
        {
            continue;
        }

        if (SearchBuffer[0] == '\0' || ActorIsInList(Actor, FilteredActors))
        {
            Result.push_back(Actor);
        }
    }
    return Result;
}

TArray<FString> FEditorWorldOutlinerPanel::GetSortedFolderPaths() const
{
    TArray<FString> Result;
    if (!EditorEngine)
    {
        return Result;
    }

    UWorld* World = EditorEngine->GetWorld();
    if (!World)
    {
        return Result;
    }

    TSet<FString> SeenFolders;
    auto AddFolder = [&](const FString& FolderPath)
    {
        AddFolderAndParents(SeenFolders, Result, FolderPath);
    };

    for (const FString& FolderPath : World->GetEditorActorFolders())
    {
        AddFolder(FolderPath);
    }
    for (AActor* Actor : World->GetActors())
    {
        if (Actor)
        {
            AddFolder(Actor->GetEditorFolderPath());
        }
    }

    std::sort(Result.begin(), Result.end(), [](const FString& A, const FString& B)
    {
        return ToLowerAscii(A) < ToLowerAscii(B);
    });
    return Result;
}

void FEditorWorldOutlinerPanel::AppendVisibleActorsForFolderBranch(const FString& FolderPath,
                                                                   const TArray<FString>& VisibleFolders,
                                                                   const TArray<AActor*>& Actors,
                                                                   const TArray<AActor*>& FilteredActors,
                                                                   TArray<AActor*>& OutVisibleActors) const
{
    const bool bOpen = SearchBuffer[0] != '\0' || OpenFolderPaths.find(FolderPath) != OpenFolderPaths.end();
    if (!bOpen)
    {
        return;
    }

    const TArray<FString> ChildFolders = BuildChildFolderPaths(FolderPath, VisibleFolders);
    for (const FString& ChildFolderPath : ChildFolders)
    {
        AppendVisibleActorsForFolderBranch(ChildFolderPath, VisibleFolders, Actors, FilteredActors, OutVisibleActors);
    }

    const TArray<AActor*> FolderActors = BuildVisibleActorsForFolder(FolderPath, Actors, FilteredActors);
    OutVisibleActors.insert(OutVisibleActors.end(), FolderActors.begin(), FolderActors.end());
}

bool FEditorWorldOutlinerPanel::MatchesSearch(AActor* Actor) const
{
    if (!Actor || SearchBuffer[0] == '\0')
    {
        return true;
    }

    const FString Query = SearchBuffer;
    return ContainsCaseInsensitive(GetActorDisplayName(Actor), Query) ||
           ContainsCaseInsensitive(GetActorClassName(Actor), Query);
}

bool FEditorWorldOutlinerPanel::FolderMatchesSearch(const FString& FolderPath) const
{
    if (SearchBuffer[0] == '\0')
    {
        return true;
    }

    return ContainsCaseInsensitive(FolderPath, SearchBuffer);
}

bool FEditorWorldOutlinerPanel::ActorIsInList(AActor* Actor, const TArray<AActor*>& Actors) const
{
    return std::find(Actors.begin(), Actors.end(), Actor) != Actors.end();
}

bool FEditorWorldOutlinerPanel::FolderExists(const FString& FolderPath) const
{
    if (FolderPath.empty() || !EditorEngine)
    {
        return false;
    }

    UWorld* World = EditorEngine->GetWorld();
    if (!World)
    {
        return false;
    }

    for (const FString& ExistingFolder : World->GetEditorActorFolders())
    {
        if (ExistingFolder == FolderPath)
        {
            return true;
        }
    }

    for (AActor* Actor : World->GetActors())
    {
        if (Actor && Actor->GetEditorFolderPath() == FolderPath)
        {
            return true;
        }
    }

    return false;
}

FString FEditorWorldOutlinerPanel::NormalizeFolderName(const FString& FolderName) const
{
    const FString Input = TrimAscii(FolderName);
    TArray<FString> Segments;
    FString Segment;

    auto FlushSegment = [&]()
    {
        const FString TrimmedSegment = TrimAscii(Segment);
        if (!TrimmedSegment.empty())
        {
            Segments.push_back(TrimmedSegment);
        }
        Segment.clear();
    };

    for (char Ch : Input)
    {
        if (Ch == '/' || Ch == '\\')
        {
            FlushSegment();
        }
        else
        {
            Segment.push_back(Ch);
        }
    }
    FlushSegment();

    if (Segments.empty())
    {
        return "New Folder";
    }

    FString Result;
    for (const FString& PathSegment : Segments)
    {
        if (!Result.empty())
        {
            Result += "/";
        }
        Result += PathSegment;
    }
    return Result;
}

FString FEditorWorldOutlinerPanel::MakeUniqueFolderName(const FString& BaseName, const FString& IgnoredExistingFolder) const
{
    const FString NormalizedBase = NormalizeFolderName(BaseName);
    FString Candidate = NormalizedBase;
    int32 Suffix = 2;

    while (FolderExists(Candidate) && Candidate != IgnoredExistingFolder)
    {
        Candidate = NormalizedBase + " " + std::to_string(Suffix++);
    }

    return Candidate;
}

FString FEditorWorldOutlinerPanel::GetActorDisplayName(AActor* Actor) const
{
    if (!Actor)
    {
        return "None";
    }

    FString Name = Actor->GetFName().ToString();
    if (Name.empty())
    {
        Name = GetActorClassName(Actor);
    }
    return Name;
}

FString FEditorWorldOutlinerPanel::GetActorClassName(AActor* Actor) const
{
    if (!Actor || !Actor->GetClass())
    {
        return "Unknown";
    }
    return Actor->GetClass()->GetName();
}
