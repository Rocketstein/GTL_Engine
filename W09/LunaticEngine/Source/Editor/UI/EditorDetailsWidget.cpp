#include "Editor/UI/EditorPropertyWidget.h"

#include "Editor/EditorEngine.h"
#include "Editor/Settings/EditorSettings.h"
#include "Editor/UI/EditorAccentColor.h"
#include "Editor/UI/EditorPanelTitleUtils.h"

#include "Component/ActorComponent.h"
#include "Component/BillboardComponent.h"
#include "Component/CanvasRootComponent.h"
#include "Component/DecalComponent.h"
#include "Component/GizmoComponent.h"
#include "Component/HeightFogComponent.h"
#include "Component/UIButtonComponent.h"
#include "Component/UIBackgroundComponent.h"
#include "Component/UIImageComponent.h"
#include "Component/UIScreenTextComponent.h"
#include "Component/UWindowPanelComponent.h"
#include "Component/Light/LightComponentBase.h"
#include "Component/MeshComponent.h"
#include "Component/Movement/MovementComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Component/SceneComponent.h"
#include "Component/ScriptComponent.h"
#include "Component/SoundComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Component/TextRenderComponent.h"
#include "Core/ClassTypes.h"
#include "Core/AsciiUtils.h"
#include "Core/PropertyTypes.h"
#include "Engine/Runtime/Engine.h"
#include "Engine/Runtime/WindowsWindow.h"
#include "GameFramework/World.h"
#include "ImGui/imgui.h"
#include "Materials/Material.h"
#include "Mesh/ObjManager.h"
#include "Mesh/StaticMesh.h"
#include "Object/FName.h"
#include "Object/ObjectIterator.h"
#include "Platform/Paths.h"
#include "Platform/ScriptPaths.h"
#include "Resource/ResourceManager.h"
#include "Texture/Texture2D.h"


#include <Windows.h>
#include <algorithm>
#include <array>
#include <cfloat>
#include <commdlg.h>
#include <cstring>
#include <filesystem>
#include <functional>
#include <set>
#include <string>


#include "Materials/MaterialManager.h"

#define SEPARATOR()                                                                                                                                  \
    ;                                                                                                                                                \
    ImGui::Spacing();                                                                                                                                \
    ImGui::Spacing();                                                                                                                                \
    ImGui::Separator();                                                                                                                              \
    ImGui::Spacing();                                                                                                                                \
    ImGui::Spacing();

namespace
{
    constexpr ImVec4 PopupMenuItemColor = ImVec4(0.20f, 0.20f, 0.20f, 1.0f);
    constexpr ImVec4 PopupMenuItemHoverColor = EditorAccentColor::Value;
    constexpr ImVec4 PopupMenuItemActiveColor = EditorAccentColor::Value;
    constexpr ImVec4 DetailsHeaderButtonColor = ImVec4(0.20f, 0.20f, 0.20f, 1.0f);
    constexpr ImVec4 DetailsHeaderButtonHoveredColor = ImVec4(0.24f, 0.24f, 0.24f, 1.0f);
    constexpr ImVec4 DetailsHeaderButtonActiveColor = ImVec4(0.18f, 0.18f, 0.18f, 1.0f);
    constexpr ImVec4 DetailsHeaderButtonBorderColor = ImVec4(0.42f, 0.42f, 0.45f, 0.90f);

    namespace PopupPalette
    {
        constexpr ImVec4 PopupBg = ImVec4(42.0f / 255.0f, 42.0f / 255.0f, 42.0f / 255.0f, 0.98f);
        constexpr ImVec4 SurfaceBg = ImVec4(36.0f / 255.0f, 36.0f / 255.0f, 36.0f / 255.0f, 1.0f);
        constexpr ImVec4 FieldBg = ImVec4(26.0f / 255.0f, 26.0f / 255.0f, 26.0f / 255.0f, 1.0f);
        constexpr ImVec4 FieldHoverBg = ImVec4(33.0f / 255.0f, 33.0f / 255.0f, 33.0f / 255.0f, 1.0f);
        constexpr ImVec4 FieldActiveBg = ImVec4(43.0f / 255.0f, 43.0f / 255.0f, 43.0f / 255.0f, 1.0f);
        constexpr ImVec4 FieldBorder = ImVec4(58.0f / 255.0f, 58.0f / 255.0f, 58.0f / 255.0f, 1.0f);
    } // namespace PopupPalette

    FString GetEditorPathResource(const char *Key) { return FResourceManager::Get().ResolvePath(FName(Key)); }

    ID3D11ShaderResourceView *GetEditorIcon(const char *Key) { return FResourceManager::Get().FindLoadedTexture(GetEditorPathResource(Key)).Get(); }

    UTexture2D *GetTexturePreviewTexture(const FString &TexturePath)
    {
        if (TexturePath.empty() || TexturePath == "None")
        {
            return nullptr;
        }

        if (UTexture2D *CachedTexture = UTexture2D::LoadFromCached(TexturePath))
        {
            return CachedTexture;
        }

        if (!GEngine)
        {
            return nullptr;
        }

        ID3D11Device *Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
        if (!Device)
        {
            return nullptr;
        }

        return UTexture2D::LoadFromFile(TexturePath, Device);
    }

    FString MakeTextureFolderGroupLabel(const FString &FolderPath)
    {
        if (FolderPath.empty())
        {
            return "Other";
        }

        std::filesystem::path Folder(FPaths::ToWide(FolderPath));
        const FString FolderName = FPaths::ToUtf8(Folder.filename().wstring());
        return FolderName.empty() ? FolderPath : FolderName + "  (" + FolderPath + ")";
    }

    bool IsNineSliceStyleProperty(const UActorComponent* Component, const FPropertyDescriptor& Prop)
    {
        return Component
            && Component->IsA<UNineSlicePanelComponent>()
            && Prop.Type == EPropertyType::String
            && Prop.Name == "Style Json";
    }

    TArray<FString> CollectNineSliceStyleJsonPaths()
    {
        TArray<FString> Result;
        const std::filesystem::path ContentRoot = FPaths::ContentDir();
        std::error_code ErrorCode;
        if (!std::filesystem::exists(ContentRoot, ErrorCode))
        {
            return Result;
        }

        for (const auto& Entry : std::filesystem::recursive_directory_iterator(ContentRoot, ErrorCode))
        {
            if (ErrorCode || !Entry.is_regular_file())
            {
                continue;
            }

            FString FileName = FPaths::ToUtf8(Entry.path().filename().wstring());
            AsciiUtils::ToLowerInPlace(FileName);
            if (FileName != "nineslice.json")
            {
                continue;
            }

            Result.push_back(FPaths::ToUtf8(Entry.path().lexically_relative(FPaths::RootDir()).generic_wstring()));
        }

        std::sort(Result.begin(), Result.end());
        return Result;
    }

    TArray<FString> CollectLuaScriptPaths()
    {
        TArray<FString> Result;
        const std::filesystem::path ScriptsRoot = FPaths::ScriptsDir();
        std::error_code ErrorCode;
        if (!std::filesystem::exists(ScriptsRoot, ErrorCode))
        {
            return Result;
        }

        for (const auto& Entry : std::filesystem::recursive_directory_iterator(ScriptsRoot, ErrorCode))
        {
            if (ErrorCode || !Entry.is_regular_file())
            {
                continue;
            }

            FString Extension = FPaths::ToUtf8(Entry.path().extension().wstring());
            AsciiUtils::ToLowerInPlace(Extension);
            if (Extension != ".lua")
            {
                continue;
            }

            const FString RelativePath = FPaths::ToUtf8(
                Entry.path().lexically_relative(FPaths::RootDir()).generic_wstring());
            Result.push_back(FScriptPaths::NormalizeScriptPath(RelativePath));
        }

        std::sort(Result.begin(), Result.end());
        Result.erase(std::unique(Result.begin(), Result.end()), Result.end());
        return Result;
    }

    void PushDetailsHeaderButtonStyle(float FrameRounding = 6.0f)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, FrameRounding);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, DetailsHeaderButtonColor);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, DetailsHeaderButtonHoveredColor);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, DetailsHeaderButtonActiveColor);
        ImGui::PushStyleColor(ImGuiCol_Border, DetailsHeaderButtonBorderColor);
    }

    void PopDetailsHeaderButtonStyle()
    {
        ImGui::PopStyleColor(4);
        ImGui::PopStyleVar(2);
    }

    void PushPopupMenuStyle()
    {
        ImGui::PushStyleColor(ImGuiCol_PopupBg, PopupPalette::PopupBg);
        ImGui::PushStyleColor(ImGuiCol_Header, PopupMenuItemColor);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, PopupMenuItemHoverColor);
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, PopupMenuItemActiveColor);
    }

    const char *GetActorHeaderIconKey(const AActor *Actor)
    {
        if (!Actor)
        {
            return "Editor.Icon.Actor";
        }

        const FString ClassName = Actor->GetClass()->GetName();
        if (ClassName.find("Character") != FString::npos)
        {
            return "Editor.Icon.Character";
        }
        if (ClassName.find("Pawn") != FString::npos)
        {
            return "Editor.Icon.Pawn";
        }
        if (ClassName.find("SpotLight") != FString::npos)
        {
            return "Editor.Icon.SpotLight";
        }
        if (ClassName.find("PointLight") != FString::npos)
        {
            return "Editor.Icon.PointLight";
        }
        if (ClassName.find("DirectionalLight") != FString::npos)
        {
            return "Editor.Icon.DirectionalLight";
        }
        if (ClassName.find("AmbientLight") != FString::npos)
        {
            return "Editor.Icon.AmbientLight";
        }
        if (ClassName.find("Decal") != FString::npos)
        {
            return "Editor.Icon.Decal";
        }
        if (Actor->GetRootComponent() && Actor->GetRootComponent()->IsA<UStaticMeshComponent>())
        {
            return "Editor.Icon.StaticMeshActor";
        }
        return "Editor.Icon.Actor";
    }

    bool DrawAddHeaderButton(const char *Id, const ImVec2 &Size)
    {
        PushDetailsHeaderButtonStyle();
        const bool   bClicked = ImGui::Button(Id, Size);
        PopDetailsHeaderButtonStyle();
        const ImVec2 Min = ImGui::GetItemRectMin();
        const ImVec2 Max = ImGui::GetItemRectMax();
        ImDrawList  *DrawList = ImGui::GetWindowDrawList();
        const ImU32  PlusColor = IM_COL32(116, 201, 72, 255);
        const ImU32  TextColor = ImGui::GetColorU32(ImGuiCol_Text);
        const char  *PlusText = "+";
        const char  *LabelText = "Add";
        const ImVec2 PlusSize = ImGui::CalcTextSize(PlusText);
        const ImVec2 LabelSize = ImGui::CalcTextSize(LabelText);
        const float  TotalWidth = PlusSize.x + 5.0f + LabelSize.x;
        const float  StartX = Min.x + ((Max.x - Min.x) - TotalWidth) * 0.5f;
        const float  TextY = Min.y + ((Max.y - Min.y) - LabelSize.y) * 0.5f;
        DrawList->AddText(ImVec2(StartX, TextY), PlusColor, PlusText);
        DrawList->AddText(ImVec2(StartX + PlusSize.x + 5.0f, TextY), TextColor, LabelText);
        return bClicked;
    }

    bool DrawSearchInputWithIcon(const char *Id, const char *Hint, char *Buffer, size_t BufferSize, float Width)
    {
        ImGuiStyle &Style = ImGui::GetStyle();
        ImGui::SetNextItemWidth(Width);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 11.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(Style.FramePadding.x + 26.0f, Style.FramePadding.y));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.42f, 0.42f, 0.45f, 0.90f));
        const std::string PaddedHint = std::string("   ") + Hint;
        const bool        bChanged = ImGui::InputTextWithHint(Id, PaddedHint.c_str(), Buffer, BufferSize);
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(3);

        if (ID3D11ShaderResourceView *SearchIcon = GetEditorIcon("Editor.Icon.Search"))
        {
            const ImVec2 Min = ImGui::GetItemRectMin();
            const float  IconSize = ImGui::GetFrameHeight() - 12.0f;
            const float  IconY = Min.y + (ImGui::GetFrameHeight() - IconSize) * 0.5f;
            ImGui::GetWindowDrawList()->AddImage(reinterpret_cast<ImTextureID>(SearchIcon), ImVec2(Min.x + 7.0f, IconY),
                                                 ImVec2(Min.x + 7.0f + IconSize, IconY + IconSize), ImVec2(1.0f, 0.0f), ImVec2(0.0f, 1.0f),
                                                 IM_COL32(210, 210, 210, 255));
        }

        return bChanged;
    }

    bool DrawHeaderIconButton(const char *Id, const char *IconKey, const char *FallbackLabel, const char *Tooltip, const ImVec2 &Size,
                              ImU32 Tint = IM_COL32_WHITE)
    {
        PushDetailsHeaderButtonStyle();
        const bool   bClicked = ImGui::Button(Id, Size);
        PopDetailsHeaderButtonStyle();
        const ImVec2 Min = ImGui::GetItemRectMin();
        const ImVec2 Max = ImGui::GetItemRectMax();
        ImDrawList  *DrawList = ImGui::GetWindowDrawList();

        if (ID3D11ShaderResourceView *Icon = GetEditorIcon(IconKey))
        {
            const ImVec2 ItemSize = ImGui::GetItemRectSize();
            const float  IconSize = (std::min)(ItemSize.x, ItemSize.y) - 8.0f;
            const float  IconX = Min.x + ((Max.x - Min.x) - IconSize) * 0.5f;
            const float  IconY = Min.y + ((Max.y - Min.y) - IconSize) * 0.5f;
            DrawList->AddImage(reinterpret_cast<ImTextureID>(Icon), ImVec2(IconX, IconY), ImVec2(IconX + IconSize, IconY + IconSize),
                               ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), Tint);
        }
        else
        {
            const ImVec2 LabelSize = ImGui::CalcTextSize(FallbackLabel);
            DrawList->AddText(ImVec2(Min.x + ((Max.x - Min.x) - LabelSize.x) * 0.5f, Min.y + ((Max.y - Min.y) - LabelSize.y) * 0.5f),
                              ImGui::GetColorU32(ImGuiCol_Text), FallbackLabel);
        }

        if (Tooltip && ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("%s", Tooltip);
        }

        return bClicked;
    }

    bool DrawIconLabelButton(const char *Id, const char *IconKey, const char *FallbackLabel, const char *Tooltip, const ImVec2 &Size,
                             ImU32 Tint = IM_COL32_WHITE)
    {
        const float  Width = Size.x > 0.0f ? Size.x : 140.0f;
        const float  Height = Size.y > 0.0f ? Size.y : 24.0f;
        ImGui::InvisibleButton(Id, ImVec2(Width, Height));
        const bool   bClicked = ImGui::IsItemClicked();
        const ImVec2 Min = ImGui::GetItemRectMin();
        const ImVec2 Max = ImGui::GetItemRectMax();
        ImDrawList  *DrawList = ImGui::GetWindowDrawList();
        const bool   bHeld = ImGui::IsItemActive();
        const bool   bHovered = ImGui::IsItemHovered();
        ImU32        BackgroundColor = ImGui::GetColorU32(ImGuiCol_Button);
        if (bHeld)
        {
            BackgroundColor = ImGui::GetColorU32(ImGuiCol_ButtonActive);
        }
        else if (bHovered)
        {
            BackgroundColor = ImGui::GetColorU32(ImGuiCol_ButtonHovered);
        }
        DrawList->AddRectFilled(Min, Max, BackgroundColor, ImGui::GetStyle().FrameRounding);
        DrawList->AddRect(Min, Max, ImGui::GetColorU32(ImGuiCol_Border), ImGui::GetStyle().FrameRounding);
        float        CursorX = Min.x + 8.0f;
        const float  CenterY = Min.y + (Max.y - Min.y) * 0.5f;

        if (ID3D11ShaderResourceView *Icon = GetEditorIcon(IconKey))
        {
            const float IconSize = (std::min)(ImGui::GetItemRectSize().y - 8.0f, 14.0f);
            DrawList->AddImage(reinterpret_cast<ImTextureID>(Icon), ImVec2(CursorX, CenterY - IconSize * 0.5f),
                               ImVec2(CursorX + IconSize, CenterY + IconSize * 0.5f), ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), Tint);
            CursorX += IconSize + 8.0f;
        }

        const ImVec2 LabelSize = ImGui::CalcTextSize(FallbackLabel);
        DrawList->AddText(ImVec2(CursorX, CenterY - LabelSize.y * 0.5f), ImGui::GetColorU32(ImGuiCol_Text), FallbackLabel);

        if (Tooltip && ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("%s", Tooltip);
        }

        return bClicked;
    }

    FString TrimWhitespace(const FString& Value)
    {
        size_t Start = 0;
        while (Start < Value.size() && AsciiUtils::IsSpace(Value[Start]))
        {
            ++Start;
        }

        size_t End = Value.size();
        while (End > Start && AsciiUtils::IsSpace(Value[End - 1]))
        {
            --End;
        }

        return Value.substr(Start, End - Start);
    }

    bool IsSimpleLuaFunctionIdentifier(const FString& Name)
    {
        if (Name.empty())
        {
            return false;
        }

        const unsigned char First = static_cast<unsigned char>(Name[0]);
        if (!(AsciiUtils::IsAlpha(static_cast<char>(First)) || Name[0] == '_'))
        {
            return false;
        }

        for (char Character : Name)
        {
            const unsigned char Ch = static_cast<unsigned char>(Character);
            if (!(AsciiUtils::IsAlnum(static_cast<char>(Ch)) || Character == '_'))
            {
                return false;
            }
        }

        return true;
    }

    void AppendLuaFunctionsFromScript(const FString& ScriptPath, std::set<FString>& OutFunctionNames)
    {
        FString ScriptText;
        FString Error;
        if (!FScriptPaths::ReadScriptFile(ScriptPath, ScriptText, Error))
        {
            return;
        }

        size_t Cursor = 0;
        while (Cursor <= ScriptText.size())
        {
            const size_t LineEnd = ScriptText.find('\n', Cursor);
            const FString RawLine = ScriptText.substr(
                Cursor,
                LineEnd == FString::npos ? FString::npos : (LineEnd - Cursor));
            FString Line = TrimWhitespace(RawLine);
            if (!Line.empty() && Line.back() == '\r')
            {
                Line.pop_back();
            }

            if (!Line.empty() && Line.rfind("--", 0) != 0 && Line.rfind("function ", 0) == 0)
            {
                const size_t NameStart = 9;
                const size_t ParenPos = Line.find('(', NameStart);
                if (ParenPos != FString::npos)
                {
                    const FString FunctionName = TrimWhitespace(Line.substr(NameStart, ParenPos - NameStart));
                    if (IsSimpleLuaFunctionIdentifier(FunctionName))
                    {
                        OutFunctionNames.insert(FunctionName);
                    }
                }
            }

            if (LineEnd == FString::npos)
            {
                break;
            }

            Cursor = LineEnd + 1;
        }
    }

    bool IsButtonActionProperty(const UActorComponent* Component, const FString& PropertyName)
    {
        if (!Component || !Component->IsA<UIButtonComponent>())
        {
            return false;
        }

        return PropertyName == "On Click Action"
            || PropertyName == "On Press Action"
            || PropertyName == "On Release Action"
            || PropertyName == "On Hover Enter Action"
            || PropertyName == "On Hover Exit Action";
    }

    bool IsButtonBackgroundProperty(const UActorComponent* Component, const FString& PropertyName)
    {
        if (!Component || !Component->IsA<UIButtonComponent>())
        {
            return false;
        }

        return PropertyName == "Draw Background"
            || PropertyName == "Background Texture"
            || PropertyName == "Background Fit Mode"
            || PropertyName == "Background Content Alignment"
            || PropertyName == "Normal Fill"
            || PropertyName == "Hover Fill"
            || PropertyName == "Pressed Fill";
    }

    TArray<FString> CollectButtonActionFunctionNames(const AActor* OwnerActor)
    {
        TArray<FString> FunctionNames;
        if (!OwnerActor)
        {
            return FunctionNames;
        }

        std::set<FString> UniqueNames;
        for (UActorComponent* Component : OwnerActor->GetComponents())
        {
            const UScriptComponent* ScriptComponent = Cast<UScriptComponent>(Component);
            if (!ScriptComponent)
            {
                continue;
            }

            const FString& ScriptPath = ScriptComponent->GetScriptPath();
            if (ScriptPath.empty())
            {
                continue;
            }

            AppendLuaFunctionsFromScript(ScriptPath, UniqueNames);
        }

        FunctionNames.assign(UniqueNames.begin(), UniqueNames.end());
        return FunctionNames;
    }

    bool BeginDetailsSection(const char *SectionName)
    {
        const std::string HeaderId = std::string(SectionName) + "##DetailsSection";
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 6.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.76f, 0.76f, 0.78f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.20f, 0.20f, 0.20f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.24f, 0.24f, 0.24f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.18f, 0.18f, 0.18f, 1.0f));
        const bool bOpen = ImGui::CollapsingHeader(HeaderId.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth);
        ImGui::PopStyleColor(4);
        ImGui::PopStyleVar(2);
        return bOpen;
    }

    const char *GetComponentIconKey(const UActorComponent *Component)
    {
        if (Component && Component->IsA<UStaticMeshComponent>())
        {
            return "Editor.Icon.Component.StaticMesh";
        }
        return "Editor.Icon.Component";
    }

    void DrawLastTreeNodeIcon(const UActorComponent *Component)
    {
        if (ID3D11ShaderResourceView *Icon = GetEditorIcon(GetComponentIconKey(Component)))
        {
            const ImVec2 Min = ImGui::GetItemRectMin();
            const float  IconSize = 14.0f;
            const float  X = Min.x + ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.x + 1.0f;
            const float  Y = Min.y + (ImGui::GetItemRectSize().y - IconSize) * 0.5f;
            ImGui::GetWindowDrawList()->AddImage(reinterpret_cast<ImTextureID>(Icon), ImVec2(X, Y), ImVec2(X + IconSize, Y + IconSize));
        }
    }

    void DrawLastTreeNodeActorIcon(const AActor* Actor)
    {
        if (ID3D11ShaderResourceView* Icon = GetEditorIcon(GetActorHeaderIconKey(Actor)))
        {
            const ImVec2 Min = ImGui::GetItemRectMin();
            const float IconSize = 14.0f;
            const float X = Min.x + ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.x + 1.0f;
            const float Y = Min.y + (ImGui::GetItemRectSize().y - IconSize) * 0.5f;
            ImGui::GetWindowDrawList()->AddImage(reinterpret_cast<ImTextureID>(Icon), ImVec2(X, Y), ImVec2(X + IconSize, Y + IconSize));
        }
    }

    constexpr const char *ComponentTreeLabelPadding = "    ";

    FString ToLowerCopy(FString Value)
    {
        AsciiUtils::ToLowerInPlace(Value);
        return Value;
    }

    bool ContainsCaseInsensitive(const FString &Haystack, const FString &Needle)
    {
        if (Needle.empty())
        {
            return true;
        }

        return ToLowerCopy(Haystack).find(ToLowerCopy(Needle)) != FString::npos;
    }

    bool ShouldHideInComponentTree(const UActorComponent *Component, bool bShowEditorOnlyComponents)
    {
        if (!Component)
        {
            return true;
        }

        return Component->IsHiddenInComponentTree() && !(bShowEditorOnlyComponents && Component->IsEditorOnlyComponent());
    }

    bool IsComponentSelectableInDetails(const UActorComponent* Component)
    {
        return Component != nullptr;
    }

    void SyncDetailsComponentSelection(UEditorEngine* EditorEngine, UActorComponent* Component)
    {
        if (!EditorEngine || !Component)
        {
            return;
        }

        if (USceneComponent* SceneComponent = Cast<USceneComponent>(Component))
        {
            EditorEngine->GetSelectionManager().SelectComponent(SceneComponent);
            return;
        }

        // 비-씬 컴포넌트는 별도 전역 component selection이 없으므로
        // owning actor selection을 root로 맞춰 두고, details 패널이 로컬 선택을 유지하게 만든다.
        if (AActor* Owner = Component->GetOwner())
        {
            EditorEngine->GetSelectionManager().Select(Owner);
        }
    }

    FString MakeUniqueComponentName(AActor* Actor, const FString& DesiredName)
    {
        FString BaseName = DesiredName;
        if (BaseName.empty())
        {
            BaseName = "Component";
        }

        if (!Actor)
        {
            return BaseName;
        }

        std::set<FString> ExistingNames;
        for (UActorComponent* ExistingComponent : Actor->GetComponents())
        {
            if (!ExistingComponent)
            {
                continue;
            }

            FString ExistingName = ExistingComponent->GetFName().ToString();
            if (!ExistingName.empty())
            {
                ExistingNames.insert(ExistingName);
            }
        }

        if (!ExistingNames.count(BaseName))
        {
            return BaseName;
        }

        int32 Suffix = 1;
        while (true)
        {
            const FString CandidateName = BaseName + "_" + std::to_string(Suffix++);
            if (!ExistingNames.count(CandidateName))
            {
                return CandidateName;
            }
        }
    }

    FString BuildDuplicatedComponentName(AActor* Actor, UActorComponent* SourceComponent)
    {
        if (!SourceComponent)
        {
            return MakeUniqueComponentName(Actor, "Component_Copy");
        }

        FString SourceName = SourceComponent->GetFName().ToString();
        if (SourceName.empty())
        {
            SourceName = SourceComponent->GetClass() ? SourceComponent->GetClass()->GetName() : "Component";
        }

        return MakeUniqueComponentName(Actor, SourceName + "_Copy");
    }

    USceneComponent* DuplicateSceneComponentSubtree(AActor* Actor, USceneComponent* SourceComponent, USceneComponent* DuplicateParent)
    {
        if (!Actor || !SourceComponent)
        {
            return nullptr;
        }

        USceneComponent* DuplicatedComponent = Cast<USceneComponent>(SourceComponent->Duplicate(Actor));
        if (!DuplicatedComponent)
        {
            return nullptr;
        }

        DuplicatedComponent->SetOwner(Actor);
        DuplicatedComponent->SetFName(FName(BuildDuplicatedComponentName(Actor, SourceComponent)));
        if (DuplicateParent)
        {
            DuplicatedComponent->AttachToComponent(DuplicateParent);
        }

        Actor->RegisterComponent(DuplicatedComponent);

        for (USceneComponent* ChildComponent : SourceComponent->GetChildren())
        {
            DuplicateSceneComponentSubtree(Actor, ChildComponent, DuplicatedComponent);
        }

        return DuplicatedComponent;
    }

    UActorComponent* DuplicateComponentForActor(AActor* Actor, UActorComponent* SourceComponent)
    {
        if (!Actor || !SourceComponent)
        {
            return nullptr;
        }

        if (USceneComponent* SceneComponent = Cast<USceneComponent>(SourceComponent))
        {
            USceneComponent* DuplicateParent = SceneComponent->GetParent();
            if (!DuplicateParent)
            {
                DuplicateParent = Actor->GetRootComponent();
            }
            return DuplicateSceneComponentSubtree(Actor, SceneComponent, DuplicateParent);
        }

        UActorComponent* DuplicatedComponent = Cast<UActorComponent>(SourceComponent->Duplicate(Actor));
        if (!DuplicatedComponent)
        {
            return nullptr;
        }

        DuplicatedComponent->SetOwner(Actor);
        DuplicatedComponent->SetFName(FName(BuildDuplicatedComponentName(Actor, SourceComponent)));
        Actor->RegisterComponent(DuplicatedComponent);
        return DuplicatedComponent;
    }

    struct FComponentClassGroup
    {
        const char      *Label = nullptr;
        UClass          *AnchorClass = nullptr;
        TArray<UClass *> Classes;
    };

    constexpr ImVec4 AddComponentGroupHeaderTextColor = ImVec4(0.82f, 0.82f, 0.84f, 1.0f);
    constexpr ImVec4 DetailsVectorLabelColor = ImVec4(0.83f, 0.84f, 0.87f, 1.0f);
    constexpr ImVec4 DetailsVectorFieldBg = ImVec4(10.0f / 255.0f, 10.0f / 255.0f, 10.0f / 255.0f, 1.0f);
    constexpr ImVec4 DetailsVectorFieldHoverBg = ImVec4(15.0f / 255.0f, 15.0f / 255.0f, 15.0f / 255.0f, 1.0f);
    constexpr ImVec4 DetailsVectorFieldActiveBg = ImVec4(20.0f / 255.0f, 20.0f / 255.0f, 20.0f / 255.0f, 1.0f);
    constexpr ImVec4 DetailsVectorResetButtonColor = ImVec4(0.22f, 0.22f, 0.23f, 1.0f);
    constexpr ImVec4 DetailsVectorResetButtonHoveredColor = ImVec4(0.30f, 0.30f, 0.32f, 1.0f);
    constexpr ImVec4 DetailsVectorResetButtonActiveColor = ImVec4(0.36f, 0.36f, 0.38f, 1.0f);
    constexpr ImVec4 DetailsVectorResetButtonBorderColor = ImVec4(0.52f, 0.52f, 0.55f, 0.95f);
    constexpr float  DetailsVectorLabelWidth = 124.0f;
    constexpr float  DetailsPropertyLabelWidth = 124.0f;
    constexpr float  DetailsPropertyVerticalSpacing = 6.0f;
    constexpr float  DetailsVectorResetSpacing = 6.0f;

    void PushDetailsFieldStyle()
    {
        ImGui::PushStyleColor(ImGuiCol_FrameBg, PopupPalette::FieldBg);
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, PopupPalette::FieldHoverBg);
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, PopupPalette::FieldActiveBg);
        ImGui::PushStyleColor(ImGuiCol_Border, PopupPalette::FieldBorder);
    }

    void PopDetailsFieldStyle() { ImGui::PopStyleColor(4); }

    void PushDetailsVectorFieldStyle()
    {
        ImGui::PushStyleColor(ImGuiCol_FrameBg, DetailsVectorFieldBg);
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, DetailsVectorFieldHoverBg);
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, DetailsVectorFieldActiveBg);
        ImGui::PushStyleColor(ImGuiCol_Border, PopupPalette::FieldBorder);
    }

    void PopDetailsVectorFieldStyle() { ImGui::PopStyleColor(4); }

    void PushDetailsVectorResetButtonStyle()
    {
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, DetailsVectorResetButtonColor);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, DetailsVectorResetButtonHoveredColor);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, DetailsVectorResetButtonActiveColor);
        ImGui::PushStyleColor(ImGuiCol_Border, DetailsVectorResetButtonBorderColor);
    }

    void PopDetailsVectorResetButtonStyle()
    {
        ImGui::PopStyleColor(4);
        ImGui::PopStyleVar();
    }

    bool DrawLabeledField(const char *Label, const std::function<bool()> &DrawField)
    {
        const float RowStartX = ImGui::GetCursorPosX();
        const float TotalWidth = ImGui::GetContentRegionAvail().x;
        const float LabelTextWidth = ImGui::CalcTextSize(Label).x;
        const ImGuiStyle &Style = ImGui::GetStyle();
        const float DesiredLabelWidth = (std::max)(DetailsPropertyLabelWidth, LabelTextWidth + Style.ItemSpacing.x + Style.FramePadding.x * 2.0f);
        const float MaxLabelWidth = (std::max)(DetailsPropertyLabelWidth, TotalWidth * 0.48f);
        const float LabelColumnWidth = (std::min)(DesiredLabelWidth, MaxLabelWidth);

        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(Label);
        ImGui::SameLine(RowStartX + LabelColumnWidth);

        const float FieldWidth = TotalWidth - LabelColumnWidth;
        if (FieldWidth > 0.0f)
        {
            ImGui::SetNextItemWidth(FieldWidth);
        }

        return DrawField();
    }

    bool IsUIComponentForDetails(const UActorComponent *Component)
    {
        return Component
            && (Component->IsA<UCanvasRootComponent>()
                || Component->IsA<UUIImageComponent>()
                || Component->IsA<UIButtonComponent>()
                || Component->IsA<UUIScreenTextComponent>()
                || Component->IsA<UNineSlicePanelComponent>());
    }

    bool IsScreenCoordinateLayoutProperty(const FString& Name)
    {
        return Name == "ScreenPosition" || Name == "Screen Position";
    }

    bool IsAnchorLayoutProperty(const FString& Name)
    {
        return Name == "Anchor"
            || Name == "Alignment"
            || Name == "AnchorOffset"
            || Name == "Anchor Offset";
    }

    bool IsAnchoredLayoutEnabledForDetails(const UActorComponent* Component)
    {
        if (const UUIImageComponent* ImageComponent = dynamic_cast<const UUIImageComponent*>(Component))
        {
            return ImageComponent->IsAnchoredLayoutEnabled();
        }

        if (const UUIScreenTextComponent* TextComponent = dynamic_cast<const UUIScreenTextComponent*>(Component))
        {
            return TextComponent->IsAnchoredLayoutEnabled();
        }

        return false;
    }

    bool ShouldRenderDetailsProperty(const UActorComponent* Component, const FPropertyDescriptor& Prop)
    {
        if (!IsUIComponentForDetails(Component))
        {
            return true;
        }

        if (IsScreenCoordinateLayoutProperty(Prop.Name))
        {
            return !IsAnchoredLayoutEnabledForDetails(Component);
        }

        if (IsAnchorLayoutProperty(Prop.Name))
        {
            return IsAnchoredLayoutEnabledForDetails(Component);
        }

        if (const UUIImageComponent* ImageComponent = dynamic_cast<const UUIImageComponent*>(Component))
        {
            if (Prop.Name == "Shadow Offset"
                || Prop.Name == "Shadow Blur"
                || Prop.Name == "Shadow Tint"
                || Prop.Name == "Shadow Top Tint"
                || Prop.Name == "Shadow Bottom Tint")
            {
                return ImageComponent->IsShadowEnabled();
            }
        }

        return true;
    }

    int32 GetDetailsSectionPriority(const char *SectionName)
    {
        if (!SectionName)
        {
            return 100;
        }
        if (strcmp(SectionName, "Default") == 0)
        {
            return 0;
        }
        if (strcmp(SectionName, "Transform") == 0)
        {
            return 1;
        }
        if (strcmp(SectionName, "Visibility") == 0)
        {
            return 2;
        }
        if (strcmp(SectionName, "Appearance") == 0)
        {
            return 3;
        }
        if (strcmp(SectionName, "Background") == 0)
        {
            return 4;
        }
        if (strcmp(SectionName, "Shadow") == 0)
        {
            return 5;
        }
        if (strcmp(SectionName, "Content") == 0)
        {
            return 6;
        }
        if (strcmp(SectionName, "Behavior") == 0)
        {
            return 7;
        }
        if (strcmp(SectionName, "Layout") == 0)
        {
            return 8;
        }
        if (strcmp(SectionName, "Materials") == 0)
        {
            return 9;
        }
        if (strcmp(SectionName, "Static Mesh") == 0)
        {
            return 10;
        }
        return 50;
    }

    void SortDetailsSections(TArray<const char *> &Sections)
    {
        std::sort(Sections.begin(), Sections.end(), [](const char *A, const char *B)
        {
            const int32 PriorityA = GetDetailsSectionPriority(A);
            const int32 PriorityB = GetDetailsSectionPriority(B);
            if (PriorityA != PriorityB)
            {
                return PriorityA < PriorityB;
            }
            return strcmp(A, B) < 0;
        });
    }

    float GetAxisFieldWidth(int32 AxisCount, float AdditionalReservedWidth = 0.0f)
    {
        const float AvailableWidth = ImGui::GetContentRegionAvail().x - AdditionalReservedWidth;
        const float InterAxisSpacing = ImGui::GetStyle().ItemSpacing.x;
        const float BarWidth = 3.0f;
        const float BarSpacing = 3.0f;
        const float TotalReserved = (BarWidth + BarSpacing) * static_cast<float>(AxisCount);
        const float TotalSpacing = InterAxisSpacing * static_cast<float>((std::max)(0, AxisCount - 1));
        return (std::max)(22.0f, (AvailableWidth - TotalReserved - TotalSpacing) / static_cast<float>(AxisCount));
    }

    int32 GetUIComponentSortPriority(const UClass* ComponentClass)
    {
        if (ComponentClass == UCanvasRootComponent::StaticClass())
        {
            return 0;
        }
        if (ComponentClass == UUIBackgroundComponent::StaticClass())
        {
            return 1;
        }
        if (ComponentClass == UUIImageComponent::StaticClass())
        {
            return 2;
        }
        if (ComponentClass == UIButtonComponent::StaticClass())
        {
            return 3;
        }
        if (ComponentClass == UUIScreenTextComponent::StaticClass())
        {
            return 4;
        }
        if (ComponentClass == UNineSlicePanelComponent::StaticClass())
        {
            return 5;
        }
        return 100;
    }

    void AddComponentClassGroup(TArray<FComponentClassGroup> &Groups, const char *Label, UClass *AnchorClass)
    {
        FComponentClassGroup Group;
        Group.Label = Label;
        Group.AnchorClass = AnchorClass;
        Groups.push_back(Group);
    }

    UClass *FindComponentClassGroupAnchor(UClass *ComponentClass, const TArray<FComponentClassGroup> &Groups)
    {
        if (!ComponentClass)
        {
            return nullptr;
        }

        // UI 전용/밀접 컴포넌트는 상속 체계와 무관하게 한 섹션으로 모은다.
        if (ComponentClass->IsA(UCanvasRootComponent::StaticClass())
            || ComponentClass->IsA(UUIImageComponent::StaticClass())
            || ComponentClass->IsA(UUIScreenTextComponent::StaticClass()))
        {
            return UCanvasRootComponent::StaticClass();
        }

        // TextRender는 월드/스크린 겸용이라 Add Component에서는 OTHER에 둔다.
        if (ComponentClass == UTextRenderComponent::StaticClass())
        {
            return nullptr;
        }

        // Height Fog는 SceneComponent 기반이지만 Add Component에서는 Effects 쪽이 더 자연스럽다.
        if (ComponentClass == UHeightFogComponent::StaticClass())
        {
            return UBillboardComponent::StaticClass();
        }

        for (const FComponentClassGroup &Group : Groups)
        {
            if (Group.AnchorClass && ComponentClass->IsA(Group.AnchorClass))
            {
                return Group.AnchorClass;
            }
        }

        return nullptr;
    }

    const TArray<FComponentClassGroup> &GetCachedAddComponentClassGroups()
    {
        static TArray<FComponentClassGroup> CachedGroups;
        static size_t                       CachedClassCount = 0;

        const TArray<UClass *> &AllClasses = UClass::GetAllClasses();
        if (!CachedGroups.empty() && CachedClassCount == AllClasses.size())
        {
            return CachedGroups;
        }

        CachedGroups.clear();
        CachedClassCount = AllClasses.size();

        AddComponentClassGroup(CachedGroups, "PRIMITIVE", UPrimitiveComponent::StaticClass());
        AddComponentClassGroup(CachedGroups, "LIGHT", ULightComponentBase::StaticClass());
        AddComponentClassGroup(CachedGroups, "EFFECTS", UBillboardComponent::StaticClass());
        AddComponentClassGroup(CachedGroups, "MOVEMENT", UMovementComponent::StaticClass());
        AddComponentClassGroup(CachedGroups, "SOUND", USoundComponent::StaticClass());
        AddComponentClassGroup(CachedGroups, "SCENE", USceneComponent::StaticClass());
        AddComponentClassGroup(CachedGroups, "UI", UCanvasRootComponent::StaticClass());
        AddComponentClassGroup(CachedGroups, "OTHER", nullptr);

        for (UClass *Cls : AllClasses)
        {
            if (!Cls->IsA(UActorComponent::StaticClass()) || Cls->HasAnyClassFlags(CF_HiddenInComponentList))
            {
                continue;
            }

            UClass *AnchorClass = FindComponentClassGroupAnchor(Cls, CachedGroups);
            for (FComponentClassGroup &Group : CachedGroups)
            {
                if ((AnchorClass && Group.AnchorClass == AnchorClass) || (!AnchorClass && Group.AnchorClass == nullptr))
                {
                    Group.Classes.push_back(Cls);
                    break;
                }
            }
        }

        for (FComponentClassGroup &Group : CachedGroups)
        {
            std::sort(Group.Classes.begin(), Group.Classes.end(),
                      [AnchorClass = Group.AnchorClass](const UClass *A, const UClass *B)
                      {
                          if (AnchorClass == UCanvasRootComponent::StaticClass())
                          {
                              const int32 PriorityA = GetUIComponentSortPriority(A);
                              const int32 PriorityB = GetUIComponentSortPriority(B);
                              if (PriorityA != PriorityB)
                              {
                                  return PriorityA < PriorityB;
                              }
                          }
                          return strcmp(A->GetName(), B->GetName()) < 0;
                      });
        }

        return CachedGroups;
    }

    // ===========================================
    // Lua Script 관련 상세 패널 액션
    // ===========================================

    void RenderScriptComponentControls(UScriptComponent *ScriptComponent)
    {
        if (!ScriptComponent)
        {
            return;
        }

        // ScriptComponent는 단순 속성 표시만으로 끝나지 않고
        // 파일 생성/열기/reload 같은 명시적 액션이 필요해서 별도 버튼 묶음을 둔다.
        ImGui::Separator();
        ImGui::TextUnformatted("Lua Script");

        PushDetailsHeaderButtonStyle();
        if (ImGui::Button("Create Script"))
        {
            ScriptComponent->CreateScript();
        }

        ImGui::SameLine();
        if (ImGui::Button("Edit Script"))
        {
            ScriptComponent->OpenScript();
        }

        PopDetailsHeaderButtonStyle();
    }

    bool IsBehaviorPropertyName(const FString &Name) { return Name == "bTickEnable" || Name == "bEditorOnly"; }

    bool IsVisibilityPropertyName(const FString &Name)
    {
        return Name == "Visible" || Name == "Cast Shadow" || Name == "Two Sided Shadow" || Name == "Is Collidable" ||
               Name == "Generates Overlap Event";
    }
} // namespace

static FString RemoveExtension(const FString &Path)
{
    size_t DotPos = Path.find_last_of('.');
    if (DotPos == FString::npos)
    {
        return Path;
    }
    return Path.substr(0, DotPos);
}

static FString GetStemFromPath(const FString &Path)
{
    size_t  SlashPos = Path.find_last_of("/\\");
    FString FileName = (SlashPos == FString::npos) ? Path : Path.substr(SlashPos + 1);
    return RemoveExtension(FileName);
}

static FString MakeAssetPreviewLabel(const FString &Path)
{
    if (Path.empty() || Path == "None")
    {
        return "None";
    }

    return GetStemFromPath(Path);
}

FString FEditorDetailsWidget::GetDisplayPropertyLabel(const FString &RawName)
{
    if (RawName == "bTickEnable")
        return "Tick Enabled";
    if (RawName == "bEditorOnly")
        return "Editor Only";
    if (RawName == "Is Collidable")
        return "Collision Enabled";
    if (RawName == "Generates Overlap Event")
        return "Generate Overlap Events";
    if (RawName == "On Click Action")
        return "OnClickAction";
    if (RawName == "On Press Action")
        return "OnPressAction";
    if (RawName == "On Release Action")
        return "OnReleaseAction";
    if (RawName == "On Hover Enter Action")
        return "OnHoverEnterAction";
    if (RawName == "On Hover Exit Action")
        return "OnHoverExitAction";

    FString Result;
    Result.reserve(RawName.size() + 8);
    size_t StartIndex = 0;
    if (RawName.size() > 1 && RawName[0] == 'b' && AsciiUtils::IsUpper(RawName[1]))
    {
        StartIndex = 1;
    }

    for (size_t i = StartIndex; i < RawName.size(); ++i)
    {
        const char C = RawName[i];
        if ((C == '_' || C == '-') && !Result.empty() && Result.back() != ' ')
        {
            Result.push_back(' ');
            continue;
        }

        const bool bIsUpper = AsciiUtils::IsUpper(C);
        const bool bNeedsSpace = i > StartIndex && bIsUpper && Result.back() != ' ' && !AsciiUtils::IsUpper(RawName[i - 1]);
        if (bNeedsSpace)
        {
            Result.push_back(' ');
        }
        Result.push_back(C);
    }

    return Result.empty() ? RawName : Result;
}

static FString GetDisplayClassLabel(const UClass *Class)
{
    if (!Class)
    {
        return "Unknown";
    }

    if (Class == UBackgroundSoundComponent::StaticClass())
    {
        return "BGM";
    }
    if (Class == UCanvasRootComponent::StaticClass())
    {
        return "Canvas";
    }
    if (Class == UUIBackgroundComponent::StaticClass())
    {
        return "Background";
    }
    if (Class == UUIImageComponent::StaticClass())
    {
        return "Image";
    }
    if (Class == UIButtonComponent::StaticClass())
    {
        return "Button";
    }
    if (Class == UUIScreenTextComponent::StaticClass())
    {
        return "Text";
    }

    FString RawName = Class->GetName();
    const bool bStartsWithUIAcronym = RawName.size() > 2
        && RawName[0] == 'U'
        && RawName[1] == 'I'
        && AsciiUtils::IsUpper(RawName[2]);
    if (!bStartsWithUIAcronym
        && RawName.size() > 1
        && (RawName[0] == 'U' || RawName[0] == 'A')
        && AsciiUtils::IsUpper(RawName[1]))
    {
        RawName.erase(RawName.begin());
    }

    constexpr const char *ComponentSuffix = "Component";
    const size_t SuffixLength = strlen(ComponentSuffix);
    if (RawName.size() > SuffixLength)
    {
        const size_t SuffixPos = RawName.size() - SuffixLength;
        if (RawName.compare(SuffixPos, SuffixLength, ComponentSuffix) == 0)
        {
            RawName.erase(SuffixPos);
        }
    }

    FString Result;
    Result.reserve(RawName.size() + 8);
    for (size_t i = 0; i < RawName.size(); ++i)
    {
        const char C = RawName[i];
        if ((C == '_' || C == '-') && !Result.empty() && Result.back() != ' ')
        {
            Result.push_back(' ');
            continue;
        }

        const bool bIsUpper = AsciiUtils::IsUpper(C);
        const bool bPrevIsUpper = i > 0 && AsciiUtils::IsUpper(RawName[i - 1]);
        const bool bPrevIsLowerOrDigit = i > 0
            && (AsciiUtils::IsLower(RawName[i - 1]) || AsciiUtils::IsDigit(RawName[i - 1]));
        const bool bNextIsLower = i + 1 < RawName.size() && AsciiUtils::IsLower(RawName[i + 1]);
        const bool bNeedsSpace = i > 0
            && bIsUpper
            && Result.back() != ' '
            && (bPrevIsLowerOrDigit || (bPrevIsUpper && bNextIsLower));
        if (bNeedsSpace)
        {
            Result.push_back(' ');
        }
        Result.push_back(C);
    }

    return Result.empty() ? RawName : Result;
}

FString FEditorDetailsWidget::GetPropertySectionName(const FPropertyDescriptor &Prop) const
{
    if (Prop.Name == "Location" || Prop.Name == "Rotation" || Prop.Name == "Scale")
    {
        return "Transform";
    }

    if (IsUIComponentForDetails(SelectedComponent))
    {
        if (Prop.Name == "ScreenPosition" || Prop.Name == "Screen Position"
            || Prop.Name == "ScreenSize" || Prop.Name == "Screen Size"
            || Prop.Name == "CanvasSize" || Prop.Name == "Canvas Size"
            || Prop.Name == "UseAnchoredLayout" || Prop.Name == "Use Anchored Layout"
            || Prop.Name == "Anchor"
            || Prop.Name == "Alignment"
            || Prop.Name == "AnchorOffset" || Prop.Name == "Anchor Offset"
            || Prop.Name == "LabelOffset" || Prop.Name == "Label Offset"
            || Prop.Name == "Slice"
            || Prop.Name == "Nine Slice Border"
            || Prop.Name == "ZOrder" || Prop.Name == "Z Order")
        {
            return "Layout";
        }

        if (Prop.Name == "Text" || Prop.Name == "Font"
            || Prop.Name == "FontSize" || Prop.Name == "Font Size"
            || Prop.Name == "Label"
            || Prop.Name == "LabelScale" || Prop.Name == "Label Scale")
        {
            return "Content";
        }

        if (IsButtonBackgroundProperty(SelectedComponent, Prop.Name))
        {
            return "Background";
        }

        if (Prop.Name == "Draw Shadow"
            || Prop.Name == "Shadow Offset"
            || Prop.Name == "Shadow Blur"
            || Prop.Name == "Shadow Tint"
            || Prop.Name == "Shadow Top Tint"
            || Prop.Name == "Shadow Bottom Tint")
        {
            return "Shadow";
        }

        if (Prop.Type == EPropertyType::TextureSlot
            || Prop.Name == "Tint"
            || Prop.Name == "Color"
            || Prop.Name == "Border Thickness"
            || Prop.Name == "Border Color"
            || Prop.Name == "Label Color"
            || Prop.Name == "Content Alignment"
            || Prop.Name == "Fit Mode"
            || Prop.Name == "NormalTint" || Prop.Name == "Normal Tint"
            || Prop.Name == "HoverTint" || Prop.Name == "Hover Tint"
            || Prop.Name == "PressedTint" || Prop.Name == "Pressed Tint"
            || Prop.Name == "DrawBorder" || Prop.Name == "Draw Border"
            || Prop.Name == "DrawCenter" || Prop.Name == "Draw Center")
        {
            return "Appearance";
        }
    }

    if (Prop.Type == EPropertyType::StaticMeshRef)
    {
        return "Static Mesh";
    }
    if (Prop.Type == EPropertyType::MaterialSlot)
    {
        return "Materials";
    }
    if (IsVisibilityPropertyName(Prop.Name))
    {
        return "Visibility";
    }
    if ((SelectedComponent && SelectedComponent->IsA<UIButtonComponent>() && Prop.Name == "Click Sound")
        || IsButtonActionProperty(SelectedComponent, Prop.Name))
    {
        return "Behavior";
    }
    if (IsBehaviorPropertyName(Prop.Name))
    {
        return "Behavior";
    }
    return "Default";
}

bool FEditorDetailsWidget::DrawColoredFloat3(const char *Label, float Values[3], float Speed, bool bShowReset, const float *ResetValues)
{
    ImGui::PushID(Label);
    ImGui::AlignTextToFramePadding();
    ImGui::PushStyleColor(ImGuiCol_Text, DetailsVectorLabelColor);
    ImGui::TextUnformatted(Label);
    ImGui::PopStyleColor();
    ImGui::SameLine(DetailsVectorLabelWidth);

    const float  ResetButtonWidth =
        bShowReset ? ImGui::CalcTextSize("RESET").x + ImGui::GetStyle().FramePadding.x * 2.0f + DetailsVectorResetSpacing : 0.0f;
    const float  Width = GetAxisFieldWidth(3, ResetButtonWidth);
    const ImVec4 AxisColors[3] = {ImVec4(0.85f, 0.22f, 0.22f, 1.0f), ImVec4(0.36f, 0.74f, 0.25f, 1.0f), ImVec4(0.23f, 0.54f, 0.92f, 1.0f)};

    bool bChanged = false;
    for (int32 Axis = 0; Axis < 3; ++Axis)
    {
        if (Axis > 0)
        {
            ImGui::SameLine();
        }
        const ImVec2 Start = ImGui::GetCursorScreenPos();
        const float  BarWidth = 3.0f;
        const float  Spacing = 3.0f;
        ImGui::GetWindowDrawList()->AddRectFilled(Start, ImVec2(Start.x + BarWidth, Start.y + ImGui::GetFrameHeight()),
                                                  ImGui::ColorConvertFloat4ToU32(AxisColors[Axis]), 2.0f);
        ImGui::SetCursorScreenPos(ImVec2(Start.x + BarWidth + Spacing, Start.y));
        PushDetailsVectorFieldStyle();
        ImGui::SetNextItemWidth((std::max)(18.0f, Width));
        bChanged |= ImGui::DragFloat(Axis == 0 ? "##X" : Axis == 1 ? "##Y" : "##Z", &Values[Axis], Speed, 0.0f, 0.0f, "%.3f");
        PopDetailsVectorFieldStyle();
    }
    if (bShowReset)
    {
        ImGui::SameLine(0.0f, DetailsVectorResetSpacing);
        PushDetailsVectorResetButtonStyle();
        if (ImGui::Button("RESET"))
        {
            Values[0] = ResetValues ? ResetValues[0] : 0.0f;
            Values[1] = ResetValues ? ResetValues[1] : 0.0f;
            Values[2] = ResetValues ? ResetValues[2] : 0.0f;
            bChanged = true;
        }
        PopDetailsVectorResetButtonStyle();
    }
    ImGui::PopID();
    return bChanged;
}

bool FEditorDetailsWidget::DrawColoredFloat2(const char *Label, float Values[3], float Speed, bool bShowReset, const float *ResetValues)
{
    ImGui::PushID(Label);
    ImGui::AlignTextToFramePadding();
    ImGui::PushStyleColor(ImGuiCol_Text, DetailsVectorLabelColor);
    ImGui::TextUnformatted(Label);
    ImGui::PopStyleColor();
    ImGui::SameLine(DetailsVectorLabelWidth);

    const float  ResetButtonWidth =
        bShowReset ? ImGui::CalcTextSize("RESET").x + ImGui::GetStyle().FramePadding.x * 2.0f + DetailsVectorResetSpacing : 0.0f;
    const float  Width = GetAxisFieldWidth(2, ResetButtonWidth);
    const ImVec4 AxisColors[2] = {ImVec4(0.85f, 0.22f, 0.22f, 1.0f), ImVec4(0.36f, 0.74f, 0.25f, 1.0f)};

    bool bChanged = false;
    for (int32 Axis = 0; Axis < 2; ++Axis)
    {
        if (Axis > 0)
        {
            ImGui::SameLine();
        }
        const ImVec2 Start = ImGui::GetCursorScreenPos();
        const float  BarWidth = 3.0f;
        const float  Spacing = 3.0f;
        ImGui::GetWindowDrawList()->AddRectFilled(Start, ImVec2(Start.x + BarWidth, Start.y + ImGui::GetFrameHeight()),
                                                  ImGui::ColorConvertFloat4ToU32(AxisColors[Axis]), 2.0f);
        ImGui::SetCursorScreenPos(ImVec2(Start.x + BarWidth + Spacing, Start.y));
        PushDetailsVectorFieldStyle();
        ImGui::SetNextItemWidth((std::max)(18.0f, Width));
        bChanged |= ImGui::DragFloat(Axis == 0 ? "##X" : "##Y", &Values[Axis], Speed, 0.0f, 0.0f, "%.3f");
        PopDetailsVectorFieldStyle();
    }
    if (bShowReset)
    {
        ImGui::SameLine(0.0f, DetailsVectorResetSpacing);
        PushDetailsVectorResetButtonStyle();
        if (ImGui::Button("RESET"))
        {
            Values[0] = ResetValues ? ResetValues[0] : 0.0f;
            Values[1] = ResetValues ? ResetValues[1] : 0.0f;
            Values[2] = ResetValues ? ResetValues[2] : 0.0f;
            bChanged = true;
        }
        PopDetailsVectorResetButtonStyle();
    }
    ImGui::PopID();
    return bChanged;
}

bool FEditorDetailsWidget::DrawColoredFloat4(const char *Label, float Values[4], float Speed, bool bShowReset)
{
    ImGui::PushID(Label);
    ImGui::AlignTextToFramePadding();
    ImGui::PushStyleColor(ImGuiCol_Text, DetailsVectorLabelColor);
    ImGui::TextUnformatted(Label);
    ImGui::PopStyleColor();
    ImGui::SameLine(DetailsVectorLabelWidth);

    const float  ResetButtonWidth =
        bShowReset ? ImGui::CalcTextSize("RESET").x + ImGui::GetStyle().FramePadding.x * 2.0f + DetailsVectorResetSpacing : 0.0f;
    const float  Width = GetAxisFieldWidth(4, ResetButtonWidth);
    const ImVec4 AxisColors[4] = {
        ImVec4(0.85f, 0.22f, 0.22f, 1.0f),
        ImVec4(0.36f, 0.74f, 0.25f, 1.0f),
        ImVec4(0.23f, 0.54f, 0.92f, 1.0f),
        ImVec4(0.72f, 0.72f, 0.72f, 1.0f)
    };

    bool bChanged = false;
    for (int32 Axis = 0; Axis < 4; ++Axis)
    {
        if (Axis > 0)
        {
            ImGui::SameLine();
        }

        const ImVec2 Start = ImGui::GetCursorScreenPos();
        const float  BarWidth = 3.0f;
        const float  Spacing = 3.0f;
        ImGui::GetWindowDrawList()->AddRectFilled(Start, ImVec2(Start.x + BarWidth, Start.y + ImGui::GetFrameHeight()),
                                                  ImGui::ColorConvertFloat4ToU32(AxisColors[Axis]), 2.0f);
        ImGui::SetCursorScreenPos(ImVec2(Start.x + BarWidth + Spacing, Start.y));
        PushDetailsVectorFieldStyle();
        ImGui::SetNextItemWidth((std::max)(16.0f, Width));
        bChanged |= ImGui::DragFloat(Axis == 0 ? "##X" : Axis == 1 ? "##Y" : Axis == 2 ? "##Z" : "##W", &Values[Axis], Speed, 0.0f, 0.0f, "%.3f");
        PopDetailsVectorFieldStyle();
    }
    if (bShowReset)
    {
        ImGui::SameLine(0.0f, DetailsVectorResetSpacing);
        PushDetailsVectorResetButtonStyle();
        if (ImGui::Button("RESET"))
        {
            Values[0] = 0.0f;
            Values[1] = 0.0f;
            Values[2] = 0.0f;
            Values[3] = 0.0f;
            bChanged = true;
        }
        PopDetailsVectorResetButtonStyle();
    }

    ImGui::PopID();
    return bChanged;
}

bool FEditorDetailsWidget::DrawNamedFloat4(const char *Label, float Values[4], float Speed, const char *AxisLabels[4], bool bShowReset)
{
    ImGui::PushID(Label);
    ImGui::AlignTextToFramePadding();
    ImGui::PushStyleColor(ImGuiCol_Text, DetailsVectorLabelColor);
    ImGui::TextUnformatted(Label);
    ImGui::PopStyleColor();
    ImGui::SameLine(DetailsVectorLabelWidth);

    const float ResetButtonWidth =
        bShowReset ? ImGui::CalcTextSize("RESET").x + ImGui::GetStyle().FramePadding.x * 2.0f + DetailsVectorResetSpacing : 0.0f;
    const float Width = (std::max)(52.0f, GetAxisFieldWidth(4, ResetButtonWidth));
    bool        bChanged = false;
    for (int32 Axis = 0; Axis < 4; ++Axis)
    {
        if (Axis > 0)
        {
            ImGui::SameLine();
        }

        const char *AxisLabel = (AxisLabels && AxisLabels[Axis]) ? AxisLabels[Axis] : "";
        const float AxisLabelWidth = AxisLabel[0] != '\0' ? ImGui::CalcTextSize(AxisLabel).x + 6.0f : 0.0f;
        const float FieldWidth = (std::max)(28.0f, Width - AxisLabelWidth);

        if (AxisLabel[0] != '\0')
        {
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(AxisLabel);
            ImGui::SameLine(0.0f, 6.0f);
        }

        PushDetailsVectorFieldStyle();
        ImGui::SetNextItemWidth(FieldWidth);
        bChanged |= ImGui::DragFloat(Axis == 0 ? "##Left" : Axis == 1 ? "##Top" : Axis == 2 ? "##Right" : "##Bottom", &Values[Axis], Speed, 0.0f, 0.0f, "%.3f");
        PopDetailsVectorFieldStyle();
    }
    if (bShowReset)
    {
        ImGui::SameLine(0.0f, DetailsVectorResetSpacing);
        PushDetailsVectorResetButtonStyle();
        if (ImGui::Button("RESET"))
        {
            Values[0] = 0.0f;
            Values[1] = 0.0f;
            Values[2] = 0.0f;
            Values[3] = 0.0f;
            bChanged = true;
        }
        PopDetailsVectorResetButtonStyle();
    }

    ImGui::PopID();
    return bChanged;
}

FString FEditorPropertyWidget::OpenObjFileDialog()
{
    wchar_t FilePath[MAX_PATH] = {};

    OPENFILENAMEW Ofn = {};
    Ofn.lStructSize = sizeof(Ofn);
    Ofn.hwndOwner = nullptr;
    Ofn.lpstrFilter = L"OBJ Files (*.obj)\0*.obj\0All Files (*.*)\0*.*\0";
    Ofn.lpstrFile = FilePath;
    Ofn.nMaxFile = MAX_PATH;
    Ofn.lpstrTitle = L"Import OBJ Mesh";
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

void FEditorPropertyWidget::Render(float DeltaTime)
{
    (void)DeltaTime;

    ImGui::SetNextWindowSize(ImVec2(350.0f, 500.0f), ImGuiCond_Once);

    FEditorSettings &Settings = FEditorSettings::Get();
    if (!Settings.UI.bProperty)
    {
        return;
    }

    constexpr const char *PanelIconKey = "Editor.Icon.Panel.Details";
    const std::string     WindowTitle = EditorPanelTitleUtils::MakeClosablePanelTitle("Details", PanelIconKey);
    ImGui::Begin(WindowTitle.c_str());
    EditorPanelTitleUtils::DrawPanelTitleIcon(PanelIconKey);
    EditorPanelTitleUtils::DrawSmallPanelCloseButton("    Details", Settings.UI.bProperty, "x##CloseDetails");
    EditorPanelTitleUtils::ApplyPanelContentTopInset();

    FSelectionManager &Selection = EditorEngine->GetSelectionManager();
    AActor            *PrimaryActor = bSelectionLocked ? LockedActor : Selection.GetPrimarySelection();
    if (!PrimaryActor)
    {
        if (bSelectionLocked)
        {
            bSelectionLocked = false;
            LockedActor = nullptr;
        }
        SelectedComponent = nullptr;
        LastSelectedActor = nullptr;
        bActorSelected = true;
        ImGui::Text("Select an object to view details.");
        ImGui::End();
        return;
    }

    // Actor 선택이 바뀌면 초기화
    if (PrimaryActor != LastSelectedActor)
    {
        SelectedComponent = PrimaryActor->GetRootComponent();
        LastSelectedActor = PrimaryActor;
        bActorSelected = (SelectedComponent == nullptr);
        bEditingActorName = false;
    }
    else if (!bSelectionLocked)
    {
        UActorComponent* SyncedSelectedComponent = Selection.GetSelectedComponent();
        if (SyncedSelectedComponent
            && (SyncedSelectedComponent->GetOwner() != PrimaryActor
                || !IsComponentSelectableInDetails(SyncedSelectedComponent)))
        {
            SyncedSelectedComponent = nullptr;
        }

        const bool bSelectedComponentIsNonScene =
            SelectedComponent != nullptr && Cast<USceneComponent>(SelectedComponent) == nullptr;
        const bool bKeepLocalNonSceneComponentSelection =
            bSelectedComponentIsNonScene
            && SelectedComponent->GetOwner() == PrimaryActor
            && (SyncedSelectedComponent == nullptr
                || SyncedSelectedComponent == PrimaryActor->GetRootComponent());
        const bool bKeepActorLevelDetailsSelected =
            bActorSelected && SyncedSelectedComponent == PrimaryActor->GetRootComponent();
        if (!bKeepActorLevelDetailsSelected
            && !bKeepLocalNonSceneComponentSelection
            && SyncedSelectedComponent != SelectedComponent)
        {
            SelectedComponent = SyncedSelectedComponent;
            bActorSelected = (SelectedComponent == nullptr);
        }
    }

    TArray<AActor *>        DisplayedActors;
    const TArray<AActor *> &SelectionActors = Selection.GetSelectedActors();
    const TArray<AActor *> *SelectedActorsPtr = &SelectionActors;
    if (bSelectionLocked)
    {
        DisplayedActors.push_back(PrimaryActor);
        SelectedActorsPtr = &DisplayedActors;
    }
    const TArray<AActor *> &SelectedActors = *SelectedActorsPtr;
    RenderHeader(PrimaryActor, SelectedActors);

    constexpr float ResizeHandleHeight = 8.0f;
    constexpr float MinTreeHeight = 80.0f;
    constexpr float MinDetailsHeight = 50.0f;
    const float     AvailableHeight = ImGui::GetContentRegionAvail().y;
    const float     MaxTreeHeight = (std::max)(MinTreeHeight, AvailableHeight - MinDetailsHeight - ResizeHandleHeight);
    ComponentTreeHeight = (std::clamp)(ComponentTreeHeight, MinTreeHeight, MaxTreeHeight);

    RenderComponentTree(PrimaryActor, ComponentTreeHeight);

    const ImVec2 HandleCursor = ImGui::GetCursorScreenPos();
    const float  HandleWidth = (std::max)(1.0f, ImGui::GetContentRegionAvail().x);
    ImGui::InvisibleButton("##ComponentTreeResizeHandle", ImVec2(HandleWidth, ResizeHandleHeight));
    const bool bHandleHovered = ImGui::IsItemHovered();
    const bool bHandleActive = ImGui::IsItemActive();
    if (bHandleHovered || bHandleActive)
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
    }
    if (bHandleActive)
    {
        ComponentTreeHeight = (std::clamp)(ComponentTreeHeight + ImGui::GetIO().MouseDelta.y, MinTreeHeight, MaxTreeHeight);
    }

    ImDrawList *DrawList = ImGui::GetWindowDrawList();
    const float LineY = HandleCursor.y + ResizeHandleHeight * 0.5f;
    const ImU32 LineColor = ImGui::GetColorU32(bHandleActive    ? EditorAccentColor::Value
                                               : bHandleHovered ? ImVec4(0.45f, 0.48f, 0.55f, 1.0f)
                                                                : ImVec4(0.26f, 0.28f, 0.32f, 1.0f));
    DrawList->AddLine(ImVec2(HandleCursor.x, LineY), ImVec2(HandleCursor.x + HandleWidth, LineY), LineColor, 2.0f);

    float ScrollHeight = ImGui::GetContentRegionAvail().y;
    if (ScrollHeight < MinDetailsHeight)
        ScrollHeight = MinDetailsHeight;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, PopupPalette::SurfaceBg);
    ImGui::BeginChild("##Details", ImVec2(0, ScrollHeight), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);
    {
        RenderDetails(PrimaryActor, SelectedActors);
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::End();
}

void FEditorDetailsWidget::RenderDetailsFilterBar(const TArray<const char *> &AvailableSections)
{
    const float SearchWidth = ImGui::GetContentRegionAvail().x;
    DrawSearchInputWithIcon("##DetailsSearch", "Search", DetailSearchBuffer, sizeof(DetailSearchBuffer), SearchWidth);
    ImGui::Spacing();

    TArray<const char *> SortedSections = AvailableSections;
    SortDetailsSections(SortedSections);

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12.0f, 5.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    const float ButtonSpacing = 4.0f;
    const float ContentMaxX = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
    bool bRenderedAnyButton = false;
    for (int32 SectionIndex = -1; SectionIndex < static_cast<int32>(SortedSections.size()); ++SectionIndex)
    {
        const char       *Label = SectionIndex < 0 ? "All" : SortedSections[SectionIndex];
        const std::string ButtonId = std::string(Label) + "##DetailsFilter";
        const bool        bActive = ActiveSectionFilter == Label;
        const float       ButtonWidth = ImGui::CalcTextSize(Label).x + ImGui::GetStyle().FramePadding.x * 2.0f;
        if (bRenderedAnyButton)
        {
            const float NextButtonRight = ImGui::GetItemRectMax().x + ButtonSpacing + ButtonWidth;
            if (NextButtonRight <= ContentMaxX)
            {
                ImGui::SameLine(0.0f, ButtonSpacing);
            }
        }

        ImGui::PushStyleColor(ImGuiCol_Button, bActive ? EditorAccentColor::WithAlpha(0.92f) : ImVec4(36.0f / 255.0f, 36.0f / 255.0f, 36.0f / 255.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, bActive ? EditorAccentColor::Value : ImVec4(44.0f / 255.0f, 44.0f / 255.0f, 44.0f / 255.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, bActive ? EditorAccentColor::Value : ImVec4(30.0f / 255.0f, 30.0f / 255.0f, 30.0f / 255.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(58.0f / 255.0f, 58.0f / 255.0f, 58.0f / 255.0f, 1.0f));
        if (ImGui::Button(ButtonId.c_str()))
        {
            ActiveSectionFilter = Label;
        }
        ImGui::PopStyleColor(4);
        bRenderedAnyButton = true;
    }
    ImGui::PopStyleVar(3);
}

bool FEditorDetailsWidget::SectionMatchesSearch(const char *SectionName, const TArray<FPropertyDescriptor> &Props, const TArray<int32> &Indices) const
{
    if (DetailSearchBuffer[0] == '\0')
    {
        return true;
    }

    const FString Query = DetailSearchBuffer;
    if (ContainsCaseInsensitive(SectionName, Query))
    {
        return true;
    }

    for (int32 PropIndex : Indices)
    {
        if (PropIndex < 0 || PropIndex >= static_cast<int32>(Props.size()))
        {
            continue;
        }

        if (!ShouldRenderDetailsProperty(SelectedComponent, Props[PropIndex]))
        {
            continue;
        }

        if (ContainsCaseInsensitive(GetDisplayPropertyLabel(Props[PropIndex].Name), Query) || ContainsCaseInsensitive(Props[PropIndex].Name, Query))
        {
            return true;
        }
    }

    return false;
}

bool FEditorDetailsWidget::ShouldDisplaySection(const char *SectionName, const TArray<FPropertyDescriptor> &Props, const TArray<int32> &Indices) const
{
    if (Indices.empty())
    {
        return false;
    }

    if (ActiveSectionFilter != "All" && ActiveSectionFilter != SectionName)
    {
        return false;
    }

    bool bHasVisibleProperty = false;
    for (int32 PropIndex : Indices)
    {
        if (PropIndex < 0 || PropIndex >= static_cast<int32>(Props.size()))
        {
            continue;
        }

        if (ShouldRenderDetailsProperty(SelectedComponent, Props[PropIndex]))
        {
            bHasVisibleProperty = true;
            break;
        }
    }

    if (!bHasVisibleProperty)
    {
        return false;
    }

    return SectionMatchesSearch(SectionName, Props, Indices);
}

void FEditorDetailsWidget::CommitActorNameEdit(AActor *Actor)
{
    if (!Actor)
    {
        return;
    }

    FString NewName = ActorNameBuffer;
    if (NewName.empty())
    {
        NewName = Actor->GetClass()->GetName();
    }

    EditorEngine->BeginTrackedSceneChange();
    Actor->SetFName(FName(NewName));
    EditorEngine->CommitTrackedSceneChange();
    bEditingActorName = false;
}

void FEditorDetailsWidget::RenderHeader(AActor *PrimaryActor, const TArray<AActor *> &SelectedActors)
{
    const int32 SelectionCount = static_cast<int32>(SelectedActors.size());

    if (!bEditingActorName)
    {
        strncpy_s(ActorNameBuffer, PrimaryActor->GetFName().ToString().c_str(), _TRUNCATE);
    }

    if (SelectionCount <= 1)
    {
        const float AvailableWidth = ImGui::GetContentRegionAvail().x;
        const float InputWidth = (std::clamp)(AvailableWidth - 84.0f, 120.0f, 240.0f);
        if (ID3D11ShaderResourceView *ActorIcon = GetEditorIcon(GetActorHeaderIconKey(PrimaryActor)))
        {
            ImGui::Image(ActorIcon, ImVec2(16.0f, 16.0f));
            ImGui::SameLine(0.0f, 6.0f);
        }
        if (bEditingActorName)
        {
            ImGui::SetWindowFontScale(1.18f);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.42f, 0.42f, 0.45f, 0.90f));
            ImGui::SetNextItemWidth(InputWidth);
            if (ImGui::InputText("##ActorNameEdit", ActorNameBuffer, sizeof(ActorNameBuffer), ImGuiInputTextFlags_EnterReturnsTrue))
            {
                CommitActorNameEdit(PrimaryActor);
            }
            if (ImGui::IsItemDeactivatedAfterEdit())
            {
                CommitActorNameEdit(PrimaryActor);
            }
            ImGui::PopStyleColor();
            ImGui::PopStyleVar();
            ImGui::SetWindowFontScale(1.0f);
        }
        else
        {
            ImGui::SetWindowFontScale(1.18f);
            ImGui::TextUnformatted(PrimaryActor->GetFName().ToString().c_str());
            ImGui::SetWindowFontScale(1.0f);
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                bEditingActorName = true;
            }
        }
    }
    else
    {
        ImGui::Text("%s (+%d)", PrimaryActor->GetFName().ToString().c_str(), SelectionCount - 1);
    }

    if (SelectionCount <= 1)
    {
        ImGui::SameLine();
        ImGui::TextDisabled("(%s)", PrimaryActor->GetClass()->GetName());
    }

    ImGui::SameLine((std::max)(0.0f, ImGui::GetWindowContentRegionMax().x - 130.0f));
    RenderAddComponentButton(PrimaryActor);
    ImGui::SameLine(0.0f, 6.0f);
    const bool  bLocked = bSelectionLocked && LockedActor == PrimaryActor;
    const char *LockIconKey = bLocked ? "Editor.Icon.Locked" : "Editor.Icon.Unlocked";
    const char *LockTooltip = bLocked ? "Unlock Details Panel" : "Lock Details Panel to Selection";
    const ImU32 LockTint = bLocked ? EditorAccentColor::ToU32() : IM_COL32(215, 215, 215, 255);
    if (DrawHeaderIconButton("##LockDetailsSelection", LockIconKey, bLocked ? "Unlock" : "Lock", LockTooltip, ImVec2(28.0f, 0.0f), LockTint))
    {
        if (bLocked)
        {
            bSelectionLocked = false;
            LockedActor = nullptr;
        }
        else
        {
            bSelectionLocked = true;
            LockedActor = PrimaryActor;
        }
    }
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0.0f, 8.0f));
}

void FEditorDetailsWidget::RenderAddComponentButton(AActor *Actor)
{
    if (!Actor)
    {
        return;
    }

    if (DrawAddHeaderButton("##AddComponentButton", ImVec2(70.0f, 0.0f)))
    {
        ImGui::OpenPopup("##AddComponentPopup");
    }

    PushPopupMenuStyle();
    if (!ImGui::BeginPopup("##AddComponentPopup"))
    {
        ImGui::PopStyleColor(4);
        return;
    }

    const TArray<FComponentClassGroup> &Groups = GetCachedAddComponentClassGroups();
    for (const FComponentClassGroup &Group : Groups)
    {
        if (Group.Classes.empty())
        {
            continue;
        }

        ImGui::PushStyleColor(ImGuiCol_Text, AddComponentGroupHeaderTextColor);
        ImGui::SeparatorText(Group.Label);
        ImGui::PopStyleColor();
        for (UClass *Cls : Group.Classes)
        {
            const FString DisplayClassName = GetDisplayClassLabel(Cls);
            if (!ImGui::MenuItem(DisplayClassName.c_str()))
            {
                continue;
            }

            EditorEngine->BeginTrackedSceneChange();
            UWorld *World = Actor->GetWorld();
            if (World)
            {
                World->BeginDeferredPickingBVHUpdate();
            }
            UActorComponent *Comp = Actor->AddComponentByClass(Cls);
            if (!Comp)
            {
                if (World)
                {
                    World->EndDeferredPickingBVHUpdate();
                }
                EditorEngine->CancelTrackedSceneChange();
                continue;
            }

            if (Cls->IsA(USceneComponent::StaticClass()))
            {
                USceneComponent *Root = Actor->GetRootComponent();
                if (SelectedComponent != nullptr && SelectedComponent->GetClass()->IsA(USceneComponent::StaticClass()))
                {
                    Cast<USceneComponent>(Comp)->AttachToComponent(Cast<USceneComponent>(SelectedComponent));
                }
                else if (Root)
                {
                    Cast<USceneComponent>(Comp)->AttachToComponent(Root);
                }

                if (Comp->IsA<ULightComponentBase>())
                {
                    Cast<ULightComponentBase>(Comp)->EnsureEditorBillboard();
                }
                else if (Comp->IsA<UDecalComponent>())
                {
                    Cast<UDecalComponent>(Comp)->EnsureEditorBillboard();
                }
                else if (Comp->IsA<UHeightFogComponent>())
                {
                    Cast<UHeightFogComponent>(Comp)->EnsureEditorBillboard();
                }
            }

            if (World)
            {
                World->EndDeferredPickingBVHUpdate();
            }

            SelectedComponent = Comp;
            bActorSelected = false;
            SyncDetailsComponentSelection(EditorEngine, Comp);

            EditorEngine->CommitTrackedSceneChange();
            ImGui::CloseCurrentPopup();
            break;
        }
    }

    ImGui::EndPopup();
    ImGui::PopStyleColor(4);
}

void FEditorPropertyWidget::RenderDetails(AActor *PrimaryActor, const TArray<AActor *> &SelectedActors)
{
    if (SelectedComponent && !IsComponentSelectableInDetails(SelectedComponent))
    {
        SelectedComponent = nullptr;
        bActorSelected = true;
    }

    if (bActorSelected)
    {
        RenderActorProperties(PrimaryActor, SelectedActors);
    }
    else if (SelectedComponent && SelectedActors.size() >= 2)
    {
        // 다중 선택 시 모든 액터의 타입이 동일한지 검증
        UClass *PrimaryClass = PrimaryActor->GetClass();
        bool    bAllSameType = true;
        for (const AActor *Actor : SelectedActors)
        {
            if (Actor && Actor->GetClass() != PrimaryClass)
            {
                bAllSameType = false;
                break;
            }
        }

        if (!bAllSameType)
        {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Multi-edit unavailable");
            ImGui::TextWrapped("Selected actors have different types. "
                               "Multi-component editing requires all selected actors to be the same type.");

            ImGui::Spacing();
            ImGui::TextDisabled("Primary: %s", PrimaryClass->GetName());
            for (const AActor *Actor : SelectedActors)
            {
                if (Actor && Actor->GetClass() != PrimaryClass)
                {
                    ImGui::TextDisabled("  Mismatch: %s (%s)", Actor->GetFName().ToString().c_str(), Actor->GetClass()->GetName());
                }
            }
        }
        else
        {
            RenderComponentProperties(PrimaryActor, SelectedActors);
        }
    }
    else if (SelectedComponent)
    {
        RenderComponentProperties(PrimaryActor, SelectedActors);
    }
    else
    {
        ImGui::TextDisabled("Select an actor or component to view details.");
    }
}

void FEditorPropertyWidget::RenderActorProperties(AActor *PrimaryActor, const TArray<AActor *> &SelectedActors)
{
    TArray<const char *> AvailableSections;
    if (PrimaryActor->GetRootComponent())
    {
        AvailableSections.push_back("Transform");
    }
    AvailableSections.push_back("Visibility");
    SortDetailsSections(AvailableSections);
    RenderDetailsFilterBar(AvailableSections);

    const FString SearchQuery = DetailSearchBuffer;
    const bool    bMatchesTransform = DetailSearchBuffer[0] == '\0' || ContainsCaseInsensitive("Transform", SearchQuery) ||
                                   ContainsCaseInsensitive("Location", SearchQuery) || ContainsCaseInsensitive("Rotation", SearchQuery) ||
                                   ContainsCaseInsensitive("Scale", SearchQuery);
    const bool bMatchesVisibility =
        DetailSearchBuffer[0] == '\0' || ContainsCaseInsensitive("Visibility", SearchQuery) || ContainsCaseInsensitive("Visible", SearchQuery);

    bool bRenderedAnySection = false;
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x, 3.0f));
    for (const char* SectionName : AvailableSections)
    {
        if (strcmp(SectionName, "Transform") == 0)
        {
            if (PrimaryActor->GetRootComponent() && (ActiveSectionFilter == "All" || ActiveSectionFilter == "Transform") && bMatchesTransform)
            {
                bRenderedAnySection = true;
                if (BeginDetailsSection("Transform"))
                {
                    FVector Pos = PrimaryActor->GetActorLocation();
                    float   PosArray[3] = {Pos.X, Pos.Y, Pos.Z};

                    USceneComponent *RootComp = PrimaryActor->GetRootComponent();

                    FVector Scale = PrimaryActor->GetActorScale();
                    float   ScaleArray[3] = {Scale.X, Scale.Y, Scale.Z};

                    static const float ZeroVectorReset[3] = { 0.0f, 0.0f, 0.0f };
                    static const float UnitScaleReset[3] = { 1.0f, 1.0f, 1.0f };

                    if (DrawColoredFloat3("Location", PosArray, 0.1f, true, ZeroVectorReset))
                    {
                        EditorEngine->BeginTrackedSceneChange();
                        FVector Delta = FVector(PosArray[0], PosArray[1], PosArray[2]) - Pos;
                        for (AActor *Actor : SelectedActors)
                        {
                            if (Actor)
                                Actor->AddActorWorldOffset(Delta);
                        }
                        EditorEngine->GetGizmo()->UpdateGizmoTransform();
                        EditorEngine->CommitTrackedSceneChange();
                    }
                    {
                        FRotator &CachedRot = RootComp->GetCachedEditRotator();
                        FRotator  PrevRot = CachedRot;
                        float     RotXYZ[3] = {CachedRot.Roll, CachedRot.Pitch, CachedRot.Yaw};

                        if (DrawColoredFloat3("Rotation", RotXYZ, 0.1f, true, ZeroVectorReset))
                        {
                            EditorEngine->BeginTrackedSceneChange();
                            CachedRot.Roll = RotXYZ[0];
                            CachedRot.Pitch = RotXYZ[1];
                            CachedRot.Yaw = RotXYZ[2];

                            if (SelectedActors.size() > 1)
                            {
                                FRotator Delta = CachedRot - PrevRot;
                                for (AActor *Actor : SelectedActors)
                                {
                                    if (!Actor || Actor == PrimaryActor)
                                        continue;
                                    USceneComponent *Root = Actor->GetRootComponent();
                                    if (Root)
                                    {
                                        FRotator Other = Root->GetCachedEditRotator();
                                        Root->SetRelativeRotation(Other + Delta);
                                    }
                                }
                            }
                            RootComp->ApplyCachedEditRotator();
                            EditorEngine->GetGizmo()->UpdateGizmoTransform();
                            EditorEngine->CommitTrackedSceneChange();
                        }
                    }
                    if (DrawColoredFloat3("Scale", ScaleArray, 0.1f, true, UnitScaleReset))
                    {
                        EditorEngine->BeginTrackedSceneChange();
                        FVector Delta = FVector(ScaleArray[0], ScaleArray[1], ScaleArray[2]) - Scale;
                        for (AActor *Actor : SelectedActors)
                        {
                            if (Actor)
                                Actor->SetActorScale(Actor->GetActorScale() + Delta);
                        }
                        EditorEngine->CommitTrackedSceneChange();
                    }
                }
            }
        }
        else if (strcmp(SectionName, "Visibility") == 0)
        {
            if ((ActiveSectionFilter == "All" || ActiveSectionFilter == "Visibility") && bMatchesVisibility)
            {
                bRenderedAnySection = true;
                if (BeginDetailsSection("Visibility"))
                {
                    bool bVisible = PrimaryActor->IsVisible();
                    if (ImGui::Checkbox("Visible", &bVisible))
                    {
                        EditorEngine->BeginTrackedSceneChange();
                        PrimaryActor->SetVisible(bVisible);
                        EditorEngine->CommitTrackedSceneChange();
                    }
                }
            }
        }
    }
    ImGui::PopStyleVar();

    if (!bRenderedAnySection)
    {
        ImGui::TextDisabled("No matching sections.");
    }
}

void FEditorPropertyWidget::RenderComponentTree(AActor *Actor, float Height)
{
    if (SelectedComponent
        && (ShouldHideInComponentTree(SelectedComponent, bShowEditorOnlyComponents)
            || !IsComponentSelectableInDetails(SelectedComponent)))
    {
        SelectedComponent = nullptr;
        bActorSelected = true;
    }

    USceneComponent *Root = Actor->GetRootComponent();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(2.0f, 4.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, 14.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(ImGui::GetStyle().FramePadding.x, ImGui::GetStyle().FramePadding.y + 2.0f));
    ImGui::BeginChild("##ComponentTreeBox", ImVec2(0.0f, Height), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);
    const bool bActorNodeSelected = bActorSelected || SelectedComponent == nullptr;
    ImGuiTreeNodeFlags ActorFlags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_OpenOnArrow;
    if (!Root && Actor->GetComponents().empty())
    {
        ActorFlags |= ImGuiTreeNodeFlags_Leaf;
    }
    if (bActorNodeSelected)
    {
        ActorFlags |= ImGuiTreeNodeFlags_Selected;
        ImGui::PushStyleColor(ImGuiCol_Header, EditorAccentColor::WithAlpha(0.95f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, EditorAccentColor::Value);
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, EditorAccentColor::Value);
    }

    FString ActorName = Actor->GetFName().ToString();
    if (ActorName.empty())
    {
        ActorName = GetDisplayClassLabel(Actor->GetClass());
    }

    const FString ActorClassName = GetDisplayClassLabel(Actor->GetClass());
    const bool bActorOpen = ImGui::TreeNodeEx(
        Actor,
        ActorFlags,
        "%s%s (%s)",
        ComponentTreeLabelPadding,
        ActorName.c_str(),
        ActorClassName.c_str());
    if (bActorNodeSelected)
    {
        ImGui::PopStyleColor(3);
    }
    DrawLastTreeNodeActorIcon(Actor);

    if (ImGui::IsItemClicked())
    {
        SelectedComponent = nullptr;
        bActorSelected = true;
    }

    if (bActorOpen)
    {
        if (Root)
        {
            RenderSceneComponentNode(Root);
        }

        for (UActorComponent *Comp : Actor->GetComponents())
        {
            if (!Comp)
                continue;
            if (Comp->IsA<USceneComponent>())
                continue;
            if (ShouldHideInComponentTree(Comp, bShowEditorOnlyComponents))
                continue;

            FString       Name = Comp->GetFName().ToString();
            const FString TypeName = GetDisplayClassLabel(Comp->GetClass());
            const FString DefaultNamePrefix = TypeName + "_";
            const bool    bUseTypeAsLabel = Name.empty() || Name == TypeName || Name.rfind(DefaultNamePrefix, 0) == 0;
            FString       LabelText = bUseTypeAsLabel ? TypeName : Name;

            ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
            const bool         bIsSelected = !bActorSelected && SelectedComponent == Comp;
            if (bIsSelected)
            {
                Flags |= ImGuiTreeNodeFlags_Selected;
                ImGui::PushStyleColor(ImGuiCol_Header, EditorAccentColor::WithAlpha(0.95f));
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, EditorAccentColor::Value);
                ImGui::PushStyleColor(ImGuiCol_HeaderActive, EditorAccentColor::Value);
            }

            ImGui::TreeNodeEx(Comp, Flags, "%s%s", ComponentTreeLabelPadding, LabelText.c_str());
            if (bIsSelected)
            {
                ImGui::PopStyleColor(3);
            }
            DrawLastTreeNodeIcon(Comp);
            if (ImGui::IsItemClicked() && IsComponentSelectableInDetails(Comp))
            {
                SelectedComponent = Comp;
                bActorSelected = false;
                SyncDetailsComponentSelection(EditorEngine, Comp);
            }
            RenderComponentContextMenu(Actor, Comp);
        }

        ImGui::TreePop();
    }
    ImGui::EndChild();
    ImGui::PopStyleVar(3);
}

void FEditorPropertyWidget::RenderSceneComponentNode(USceneComponent *Comp)
{
    if (!Comp)
        return;
    if (ShouldHideInComponentTree(Comp, bShowEditorOnlyComponents))
        return;

    FString Name = Comp->GetFName().ToString();
    if (Name.empty())
        Name = GetDisplayClassLabel(Comp->GetClass());

    const auto &Children = Comp->GetChildren();
    bool        bHasVisibleChildren = false;
    for (USceneComponent *Child : Children)
    {
        if (Child && !ShouldHideInComponentTree(Child, bShowEditorOnlyComponents))
        {
            bHasVisibleChildren = true;
            break;
        }
    }

    ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen;
    if (!bHasVisibleChildren)
        Flags |= ImGuiTreeNodeFlags_Leaf;
    const bool bIsSelected = !bActorSelected && SelectedComponent == Comp;
    if (bIsSelected)
    {
        Flags |= ImGuiTreeNodeFlags_Selected;
        ImGui::PushStyleColor(ImGuiCol_Header, EditorAccentColor::WithAlpha(0.95f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, EditorAccentColor::Value);
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, EditorAccentColor::Value);
    }

    bool    bIsRoot = (Comp->GetParent() == nullptr);
    FString LabelText;
    if (bIsRoot)
    {
        LabelText = "[Root] ";
    }
    LabelText += Name;
    const FString DisplayClassName = GetDisplayClassLabel(Comp->GetClass());
    bool bOpen = ImGui::TreeNodeEx(Comp, Flags, "%s%s (%s)", ComponentTreeLabelPadding, LabelText.c_str(), DisplayClassName.c_str());
    if (bIsSelected)
    {
        ImGui::PopStyleColor(3);
    }
    DrawLastTreeNodeIcon(Comp);

    if (ImGui::IsItemClicked() && IsComponentSelectableInDetails(Comp))
    {
        SelectedComponent = Comp;
        bActorSelected = false;
        EditorEngine->GetSelectionManager().SelectComponent(Comp);
    }
    RenderComponentContextMenu(Comp->GetOwner(), Comp);

    // 컴포넌트 트리에서 간단하게 드래그 앤 드랍으로 부모-자식 관계 변경 가능하도록 지원
    if (ImGui::BeginDragDropSource())
    {
        ImGui::SetDragDropPayload("SCENE_COMPONENT_REPARENT", &Comp, sizeof(USceneComponent *));
        ImGui::Text("Reparent %s", Name.c_str());
        ImGui::EndDragDropSource();
    }

    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("SCENE_COMPONENT_REPARENT"))
        {
            USceneComponent *DraggedComp = *(USceneComponent **)payload->Data;
            if (DraggedComp && DraggedComp != Comp)
            {
                // Circular dependency check: Ensure Comp is not a child of DraggedComp
                bool             bIsChildOfDragged = false;
                USceneComponent *Check = Comp;
                while (Check)
                {
                    if (Check == DraggedComp)
                    {
                        bIsChildOfDragged = true;
                        break;
                    }
                    Check = Check->GetParent();
                }

                if (!bIsChildOfDragged)
                {
                    DraggedComp->SetParent(Comp);
                    if (EditorEngine && EditorEngine->GetGizmo())
                    {
                        EditorEngine->GetGizmo()->UpdateGizmoTransform();
                    }
                }
            }
        }
        ImGui::EndDragDropTarget();
    }

    if (bOpen)
    {
        for (USceneComponent *Child : Children)
        {
            RenderSceneComponentNode(Child);
        }
        ImGui::TreePop();
    }
}

void FEditorPropertyWidget::RenderComponentContextMenu(AActor *Actor, UActorComponent *Component)
{
    if (!Actor || !Component)
    {
        return;
    }

    if (!ImGui::BeginPopupContextItem())
    {
        return;
    }

    if (IsComponentSelectableInDetails(Component))
    {
        SelectedComponent = Component;
        bActorSelected = false;
        SyncDetailsComponentSelection(EditorEngine, Component);
    }

    if (DrawIconLabelButton("##DuplicateComponentContext", "Editor.Icon.Component", "Duplicate", "Duplicate Component", ImVec2(120.0f, 0.0f)))
    {
        DuplicateSelectedComponent(Actor);
        ImGui::CloseCurrentPopup();
    }

    const bool bCanDelete = Component->CanDeleteFromDetails();
    if (!bCanDelete)
    {
        ImGui::BeginDisabled();
    }
    if (DrawIconLabelButton("##DeleteComponentContext", "Editor.Icon.Delete", "Delete", "Delete Component", ImVec2(120.0f, 0.0f),
                            IM_COL32(255, 225, 225, 255)) &&
        bCanDelete)
    {
        DeleteSelectedComponent(Actor);
        ImGui::CloseCurrentPopup();
    }
    if (!bCanDelete)
    {
        ImGui::EndDisabled();
    }

    ImGui::EndPopup();
}

void FEditorPropertyWidget::DeleteSelectedComponent(AActor *Actor)
{
    if (!Actor || !SelectedComponent || !SelectedComponent->CanDeleteFromDetails())
    {
        return;
    }

    EditorEngine->BeginTrackedSceneChange();
    Actor->RemoveComponent(SelectedComponent);
    SelectedComponent = nullptr;
    bActorSelected = true;
    EditorEngine->GetSelectionManager().Select(Actor);
    EditorEngine->CommitTrackedSceneChange();
}

void FEditorPropertyWidget::DuplicateSelectedComponent(AActor* Actor)
{
    if (!Actor || !SelectedComponent)
    {
        return;
    }

    EditorEngine->BeginTrackedSceneChange();
    UActorComponent* DuplicatedComponent = DuplicateComponentForActor(Actor, SelectedComponent);
    if (DuplicatedComponent)
    {
        SelectedComponent = DuplicatedComponent;
        bActorSelected = false;
        SyncDetailsComponentSelection(EditorEngine, DuplicatedComponent);
        if (EditorEngine->GetGizmo())
        {
            EditorEngine->GetGizmo()->UpdateGizmoTransform();
        }
    }
    EditorEngine->CommitTrackedSceneChange();
}

void FEditorPropertyWidget::RenderComponentProperties(AActor *Actor, const TArray<AActor *> &SelectedActors)
{
    // PropertyDescriptor 기반 자동 위젯 렌더링
    TArray<FPropertyDescriptor> Props;
    SelectedComponent->GetEditableProperties(Props);

    // ScriptComponent도 일반 PropertyDescriptor 목록은 제공하지만,
    // create/open/reload 버튼은 범용 descriptor 파이프라인 밖에서 따로 그린다.
    UScriptComponent *ScriptComponent = Cast<UScriptComponent>(SelectedComponent);
    bool              bAnyChanged = false;
    TArray<int32>     TransformIndices;
    TArray<int32>     StaticMeshIndices;
    TArray<int32>     MaterialIndices;
    TArray<int32>     LayoutIndices;
    TArray<int32>     ContentIndices;
    TArray<int32>     AppearanceIndices;
    TArray<int32>     BackgroundIndices;
    TArray<int32>     ShadowIndices;
    TArray<int32>     VisibilityIndices;
    TArray<int32>     BehaviorIndices;
    TArray<int32>     DefaultIndices;

    for (int32 i = 0; i < (int32)Props.size(); ++i)
    {
        const FString SectionName = GetPropertySectionName(Props[i]);
        if (SectionName == "Transform")
        {
            TransformIndices.push_back(i);
        }
        else if (SectionName == "Static Mesh")
        {
            StaticMeshIndices.push_back(i);
        }
        else if (SectionName == "Materials")
        {
            MaterialIndices.push_back(i);
        }
        else if (SectionName == "Layout")
        {
            LayoutIndices.push_back(i);
        }
        else if (SectionName == "Content")
        {
            ContentIndices.push_back(i);
        }
        else if (SectionName == "Appearance")
        {
            AppearanceIndices.push_back(i);
        }
        else if (SectionName == "Background")
        {
            BackgroundIndices.push_back(i);
        }
        else if (SectionName == "Shadow")
        {
            ShadowIndices.push_back(i);
        }
        else if (SectionName == "Visibility")
        {
            VisibilityIndices.push_back(i);
        }
        else if (SectionName == "Behavior")
        {
            BehaviorIndices.push_back(i);
        }
        else
        {
            DefaultIndices.push_back(i);
        }
    }

    TArray<const char *> AvailableSections;
    if (!TransformIndices.empty())
        AvailableSections.push_back("Transform");
    if (!StaticMeshIndices.empty())
        AvailableSections.push_back("Static Mesh");
    if (!MaterialIndices.empty() || (SelectedComponent && SelectedComponent->IsA<UStaticMeshComponent>()))
        AvailableSections.push_back("Materials");
    if (!LayoutIndices.empty())
        AvailableSections.push_back("Layout");
    if (!ContentIndices.empty())
        AvailableSections.push_back("Content");
    if (!AppearanceIndices.empty())
        AvailableSections.push_back("Appearance");
    if (!BackgroundIndices.empty())
        AvailableSections.push_back("Background");
    if (!ShadowIndices.empty())
        AvailableSections.push_back("Shadow");
    if (!VisibilityIndices.empty())
        AvailableSections.push_back("Visibility");
    if (!BehaviorIndices.empty())
        AvailableSections.push_back("Behavior");
    if (!DefaultIndices.empty())
        AvailableSections.push_back("Default");
    SortDetailsSections(AvailableSections);
    RenderDetailsFilterBar(AvailableSections);

    bool bRenderedAnySection = false;
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x, 3.0f));
    for (const char* SectionName : AvailableSections)
    {
        if (strcmp(SectionName, "Transform") == 0)
        {
            if (ShouldDisplaySection("Transform", Props, TransformIndices))
            {
                bRenderedAnySection = true;
                RenderPropertySection("Transform", Props, TransformIndices, SelectedActors, bAnyChanged);
            }
        }
        else if (strcmp(SectionName, "Static Mesh") == 0)
        {
            if (ShouldDisplaySection("Static Mesh", Props, StaticMeshIndices))
            {
                bRenderedAnySection = true;
                RenderPropertySection("Static Mesh", Props, StaticMeshIndices, SelectedActors, bAnyChanged);
            }
        }
        else if (strcmp(SectionName, "Materials") == 0)
        {
            if (ShouldDisplaySection("Materials", Props, MaterialIndices))
            {
                bRenderedAnySection = true;
                RenderPropertySection("Materials", Props, MaterialIndices, SelectedActors, bAnyChanged);
            }
            else if (MaterialIndices.empty() && SelectedComponent && SelectedComponent->IsA<UStaticMeshComponent>() &&
                     (ActiveSectionFilter == "All" || ActiveSectionFilter == "Materials") && SectionMatchesSearch("Materials", Props, MaterialIndices))
            {
                UStaticMeshComponent *StaticMeshComponent = static_cast<UStaticMeshComponent *>(SelectedComponent);
                StaticMeshComponent->EnsureMaterialSlotsForEditing();
                if (StaticMeshComponent->GetMaterialSlotCount() > 0)
                {
                    bRenderedAnySection = true;
                    TArray<FPropertyDescriptor> SyntheticProps;
                    SyntheticProps.reserve(StaticMeshComponent->GetMaterialSlotCount());

                    for (int32 SlotIndex = 0; SlotIndex < StaticMeshComponent->GetMaterialSlotCount(); ++SlotIndex)
                    {
                        if (FMaterialSlot *Slot = StaticMeshComponent->GetMaterialSlot(SlotIndex))
                        {
                            FPropertyDescriptor Desc;
                            Desc.Name = "Element " + std::to_string(SlotIndex);
                            Desc.Type = EPropertyType::MaterialSlot;
                            Desc.ValuePtr = Slot;
                            SyntheticProps.push_back(Desc);
                        }
                    }

                    TArray<int32> SyntheticIndices;
                    SyntheticIndices.reserve(SyntheticProps.size());
                    for (int32 Index = 0; Index < static_cast<int32>(SyntheticProps.size()); ++Index)
                    {
                        SyntheticIndices.push_back(Index);
                    }

                    RenderPropertySection("Materials", SyntheticProps, SyntheticIndices, SelectedActors, bAnyChanged);
                }
                else
                {
                    bRenderedAnySection = true;
                    if (BeginDetailsSection("Materials"))
                    {
                        ImGui::TextDisabled("No material slots.");
                    }
                }
            }
        }
        else if (strcmp(SectionName, "Layout") == 0)
        {
            if (ShouldDisplaySection("Layout", Props, LayoutIndices))
            {
                bRenderedAnySection = true;
                RenderPropertySection("Layout", Props, LayoutIndices, SelectedActors, bAnyChanged);
            }
        }
        else if (strcmp(SectionName, "Content") == 0)
        {
            if (ShouldDisplaySection("Content", Props, ContentIndices))
            {
                bRenderedAnySection = true;
                RenderPropertySection("Content", Props, ContentIndices, SelectedActors, bAnyChanged);
            }
        }
        else if (strcmp(SectionName, "Appearance") == 0)
        {
            if (ShouldDisplaySection("Appearance", Props, AppearanceIndices))
            {
                bRenderedAnySection = true;
                RenderPropertySection("Appearance", Props, AppearanceIndices, SelectedActors, bAnyChanged);
            }
        }
        else if (strcmp(SectionName, "Background") == 0)
        {
            if (ShouldDisplaySection("Background", Props, BackgroundIndices))
            {
                bRenderedAnySection = true;
                RenderPropertySection("Background", Props, BackgroundIndices, SelectedActors, bAnyChanged);
            }
        }
        else if (strcmp(SectionName, "Shadow") == 0)
        {
            if (ShouldDisplaySection("Shadow", Props, ShadowIndices))
            {
                bRenderedAnySection = true;
                RenderPropertySection("Shadow", Props, ShadowIndices, SelectedActors, bAnyChanged);
            }
        }
        else if (strcmp(SectionName, "Visibility") == 0)
        {
            if (ShouldDisplaySection("Visibility", Props, VisibilityIndices))
            {
                bRenderedAnySection = true;
                RenderPropertySection("Visibility", Props, VisibilityIndices, SelectedActors, bAnyChanged);
            }
        }
        else if (strcmp(SectionName, "Behavior") == 0)
        {
            if (ShouldDisplaySection("Behavior", Props, BehaviorIndices))
            {
                bRenderedAnySection = true;
                RenderPropertySection("Behavior", Props, BehaviorIndices, SelectedActors, bAnyChanged);
            }
        }
        else if (strcmp(SectionName, "Default") == 0)
        {
            if (ShouldDisplaySection("Default", Props, DefaultIndices))
            {
                bRenderedAnySection = true;
                RenderPropertySection("Default", Props, DefaultIndices, SelectedActors, bAnyChanged);
            }
        }
    }
    ImGui::PopStyleVar();

    if (!bRenderedAnySection)
    {
        ImGui::TextDisabled("No matching sections.");
    }

    // 경로 필드를 먼저 보여준 뒤 스크립트 액션 버튼을 배치해야
    // 사용자가 현재 어떤 파일을 대상으로 동작하는지 바로 확인할 수 있다.
    RenderScriptComponentControls(ScriptComponent);

    // 실제 변경이 있었을 때만 Transform dirty 마킹
    if (bAnyChanged && SelectedComponent->IsA<USceneComponent>())
    {
        static_cast<USceneComponent *>(SelectedComponent)->MarkTransformDirty();
    }
}

void FEditorDetailsWidget::RenderPropertySection(const char *SectionName, TArray<FPropertyDescriptor> &Props, const TArray<int32> &Indices,
                                                 const TArray<AActor *> &SelectedActors, bool &bAnyChanged)
{
    if (Indices.empty())
    {
        return;
    }

    if (!BeginDetailsSection(SectionName))
    {
        return;
    }

    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.05f, 0.05f, 0.06f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.08f, 0.08f, 0.09f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.10f, 0.10f, 0.11f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.05f, 0.05f, 0.06f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.10f, 0.10f, 0.12f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.15f, 0.15f, 0.18f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.18f, 0.18f, 0.20f, 1.0f));
    const FString Query = DetailSearchBuffer;

    for (int32 PropIndex : Indices)
    {
        if (!ShouldRenderDetailsProperty(SelectedComponent, Props[PropIndex]))
        {
            continue;
        }

        if (DetailSearchBuffer[0] != '\0' && !ContainsCaseInsensitive(GetDisplayPropertyLabel(Props[PropIndex].Name), Query) &&
            !ContainsCaseInsensitive(Props[PropIndex].Name, Query))
        {
            continue;
        }

        int32 MutableIndex = PropIndex;
        EditorEngine->BeginTrackedSceneChange();
        const bool bChanged = RenderPropertyWidget(Props, MutableIndex);
        if (bChanged)
        {
            bAnyChanged = true;
            PropagatePropertyChange(Props[PropIndex].Name, SelectedActors);
            EditorEngine->CommitTrackedSceneChange();
        }
        else
        {
            EditorEngine->CancelTrackedSceneChange();
        }

        ImGui::Dummy(ImVec2(0.0f, DetailsPropertyVerticalSpacing));
    }

    ImGui::PopStyleColor(7);
}

void FEditorPropertyWidget::PropagatePropertyChange(const FString &PropName, const TArray<AActor *> &SelectedActors)
{
    if (!SelectedComponent || SelectedActors.size() < 2)
        return;

    UClass *CompClass = SelectedComponent->GetClass();
    AActor *PrimaryActor = SelectedActors[0];

    // Primary 컴포넌트에서 변경된 프로퍼티의 값 포인터 찾기
    TArray<FPropertyDescriptor> SrcProps;
    SelectedComponent->GetEditableProperties(SrcProps);

    const FPropertyDescriptor *SrcProp = nullptr;
    for (const auto &P : SrcProps)
    {
        if (P.Name == PropName)
        {
            SrcProp = &P;
            break;
        }
    }
    if (!SrcProp)
        return;

    for (AActor *Actor : SelectedActors)
    {
        if (!Actor || Actor == PrimaryActor)
            continue;

        for (UActorComponent *Comp : Actor->GetComponents())
        {
            if (!Comp || Comp->GetClass() != CompClass)
                continue;

            TArray<FPropertyDescriptor> DstProps;
            Comp->GetEditableProperties(DstProps);

            for (const auto &DstProp : DstProps)
            {
                if (DstProp.Name != PropName || DstProp.Type != SrcProp->Type)
                    continue;

                size_t Size = 0;
                switch (DstProp.Type)
                {
                case EPropertyType::Bool:
                    Size = sizeof(bool);
                    break;
                case EPropertyType::ByteBool:
                    Size = sizeof(uint8);
                    break;
                case EPropertyType::Int:
                    Size = sizeof(int32);
                    break;
                case EPropertyType::Float:
                    Size = sizeof(float);
                    break;
                case EPropertyType::Vec3:
                case EPropertyType::Rotator:
                    Size = sizeof(float) * 3;
                    break;
                case EPropertyType::Vec4:
                case EPropertyType::Color4:
                    Size = sizeof(float) * 4;
                    break;
                case EPropertyType::String:
                case EPropertyType::SceneComponentRef:
                case EPropertyType::StaticMeshRef:
                    *static_cast<FString *>(DstProp.ValuePtr) = *static_cast<FString *>(SrcProp->ValuePtr);
                    break;
                case EPropertyType::Name:
                    *static_cast<FName *>(DstProp.ValuePtr) = *static_cast<FName *>(SrcProp->ValuePtr);
                    break;
                case EPropertyType::MaterialSlot:
                    *static_cast<FMaterialSlot *>(DstProp.ValuePtr) = *static_cast<FMaterialSlot *>(SrcProp->ValuePtr);
                    break;
                case EPropertyType::TextureSlot:
                    *static_cast<FTextureSlot *>(DstProp.ValuePtr) = *static_cast<FTextureSlot *>(SrcProp->ValuePtr);
                    break;
                case EPropertyType::Enum:
                    Size = sizeof(int32);
                    break;
                case EPropertyType::Vec3Array:
                    *static_cast<TArray<FVector> *>(DstProp.ValuePtr) = *static_cast<TArray<FVector> *>(SrcProp->ValuePtr);
                    break;
                }
                if (Size > 0)
                    memcpy(DstProp.ValuePtr, SrcProp->ValuePtr, Size);

                Comp->PostEditProperty(PropName.c_str());
                break;
            }
            break; // 같은 타입의 첫 번째 컴포넌트에만 전파
        }
    }
}

bool FEditorPropertyWidget::RenderPropertyWidget(TArray<FPropertyDescriptor> &Props, int32 &Index)
{
    ImGui::PushID(Index);
    FPropertyDescriptor &Prop = Props[Index];
    const FString        DisplayName = GetDisplayPropertyLabel(Prop.Name);
    const char          *Label = DisplayName.c_str();
    bool                 bChanged = false;

    switch (Prop.Type)
    {
    case EPropertyType::Bool:
    {
        bool *Val = static_cast<bool *>(Prop.ValuePtr);
        bChanged = ImGui::Checkbox(Label, Val);
        break;
    }
    case EPropertyType::ByteBool:
    {
        uint8 *Val = static_cast<uint8 *>(Prop.ValuePtr);
        bool   bVal = (*Val != 0);
        if (ImGui::Checkbox(Label, &bVal))
        {
            *Val = bVal ? 1 : 0;
            bChanged = true;
        }
        break;
    }
    case EPropertyType::Int:
    {
        int32 *Val = static_cast<int32 *>(Prop.ValuePtr);
        bChanged = DrawLabeledField(Label, [&]()
                                    {
                                        PushDetailsFieldStyle();
                                        bool bLocalChanged = false;
                                        if (Prop.Min != 0.0f || Prop.Max != 0.0f)
                                            bLocalChanged = ImGui::DragInt("##Value", Val, Prop.Speed, (int32)Prop.Min, (int32)Prop.Max);
                                        else
                                            bLocalChanged = ImGui::DragInt("##Value", Val, Prop.Speed);
                                        PopDetailsFieldStyle();
                                        return bLocalChanged;
                                    });
        break;
    }
    case EPropertyType::Float:
    {
        float *Val = static_cast<float *>(Prop.ValuePtr);
        bChanged = DrawLabeledField(Label, [&]()
                                    {
                                        PushDetailsFieldStyle();
                                        bool bLocalChanged = false;
                                        if (Prop.Min != 0.0f || Prop.Max != 0.0f)
                                            bLocalChanged = ImGui::DragFloat("##Value", Val, Prop.Speed, Prop.Min, Prop.Max, "%.4f");
                                        else
                                            bLocalChanged = ImGui::DragFloat("##Value", Val, Prop.Speed);
                                        PopDetailsFieldStyle();
                                        return bLocalChanged;
                                    });
        break;
    }
    case EPropertyType::Vec3:
    {
        float *Val = static_cast<float *>(Prop.ValuePtr);
        const bool bIsScreenSizeProperty = Prop.Name == "ScreenSize" || Prop.Name == "Screen Size";
        const bool bIsShadowOffsetProperty = Prop.Name == "Shadow Offset";
        const bool bIsCanvasSizeProperty = SelectedComponent
            && SelectedComponent->IsA<UCanvasRootComponent>()
            && (Prop.Name == "CanvasSize" || Prop.Name == "Canvas Size");
        static const float ZeroVectorReset[3] = { 0.0f, 0.0f, 0.0f };
        static const float UnitScaleReset[3] = { 1.0f, 1.0f, 1.0f };
        static const float DefaultCanvasSizeReset[3] = { 1920.0f, 1080.0f, 0.0f };
        const float *ResetValues = ZeroVectorReset;
        if (Prop.Name == "Scale")
        {
            ResetValues = UnitScaleReset;
        }
        else if (bIsCanvasSizeProperty)
        {
            ResetValues = DefaultCanvasSizeReset;
        }
        bChanged = (bIsScreenSizeProperty || bIsShadowOffsetProperty)
            ? DrawColoredFloat2(Label, Val, Prop.Speed, true, ResetValues)
            : DrawColoredFloat3(Label, Val, Prop.Speed, true, ResetValues);
        if (bIsCanvasSizeProperty)
        {
            ImGui::SameLine();
            PushDetailsVectorResetButtonStyle();
            if (ImGui::Button("FULL SCREEN") && GEngine)
            {
                if (FWindowsWindow *Window = GEngine->GetWindow())
                {
                    Val[0] = (std::max)(1.0f, Window->GetWidth());
                    Val[1] = (std::max)(1.0f, Window->GetHeight());
                    bChanged = true;
                }
            }
            PopDetailsVectorResetButtonStyle();
        }
        break;
    }
    case EPropertyType::Rotator:
    {
        // FRotator 메모리 레이아웃 [Pitch,Yaw,Roll] → UI X=Roll(X축), Y=Pitch(Y축), Z=Yaw(Z축)
        FRotator *Rot = static_cast<FRotator *>(Prop.ValuePtr);
        float     RotXYZ[3] = {Rot->Roll, Rot->Pitch, Rot->Yaw};
        static const float ZeroVectorReset[3] = { 0.0f, 0.0f, 0.0f };
        bChanged = DrawColoredFloat3(Label, RotXYZ, Prop.Speed, true, ZeroVectorReset);
        if (bChanged)
        {
            Rot->Roll = RotXYZ[0];
            Rot->Pitch = RotXYZ[1];
            Rot->Yaw = RotXYZ[2];
            if (SelectedComponent && SelectedComponent->IsA<USceneComponent>())
            {
                static_cast<USceneComponent *>(SelectedComponent)->ApplyCachedEditRotator();
            }
        }
        break;
    }
    case EPropertyType::Vec4:
    {
        float *Val = static_cast<float *>(Prop.ValuePtr);
        PushDetailsFieldStyle();
        if (Prop.Name == "Slice" || Prop.Name == "Nine Slice Border")
        {
            const char *AxisLabels[4] = {"Left", "Top", "Right", "Bottom"};
            bChanged = DrawNamedFloat4(Label, Val, Prop.Speed, AxisLabels, true);
        }
        else
        {
            bChanged = DrawColoredFloat4(Label, Val, Prop.Speed, true);
        }
        PopDetailsFieldStyle();
        break;
    }
    case EPropertyType::Color4:
    {
        float *Val = static_cast<float *>(Prop.ValuePtr);
        bChanged = DrawLabeledField(Label, [&]()
                                    { return ImGui::ColorEdit4("##Value", Val, ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreviewHalf); });
        break;
    }
    case EPropertyType::String:
    {
        FString *Val = static_cast<FString *>(Prop.ValuePtr);
        PushDetailsFieldStyle();
        const bool bIsScriptPath = (Prop.Name == "ScriptPath") && Cast<UScriptComponent>(SelectedComponent);
        const bool bIsButtonAction = IsButtonActionProperty(SelectedComponent, Prop.Name);
        const bool bIsNineSliceStyle = IsNineSliceStyleProperty(SelectedComponent, Prop);
        if (bIsScriptPath)
        {
            const TArray<FString> ScriptPaths = CollectLuaScriptPaths();
            const FString Preview = Val->empty() ? FString("None") : MakeAssetPreviewLabel(*Val);
            bChanged = DrawLabeledField(Label, [&]()
            {
                bool bLocalChanged = false;
                if (ImGui::BeginCombo("##Value", Preview.c_str()))
                {
                    const bool bSelectedNone = Val->empty();
                    if (ImGui::Selectable("None", bSelectedNone))
                    {
                        if (!Val->empty())
                        {
                            Val->clear();
                            bLocalChanged = true;
                        }
                    }
                    if (bSelectedNone)
                    {
                        ImGui::SetItemDefaultFocus();
                    }

                    for (const FString& ScriptPath : ScriptPaths)
                    {
                        const bool bSelected = (*Val == ScriptPath);
                        if (ImGui::Selectable(ScriptPath.c_str(), bSelected))
                        {
                            if (!bSelected)
                            {
                                *Val = ScriptPath;
                                bLocalChanged = true;
                            }
                        }
                        if (bSelected)
                        {
                            ImGui::SetItemDefaultFocus();
                        }
                    }

                    if (!Val->empty()
                        && std::find(ScriptPaths.begin(), ScriptPaths.end(), *Val) == ScriptPaths.end())
                    {
                        ImGui::Separator();
                        const FString MissingLabel = *Val + "  (missing)";
                        ImGui::Selectable(MissingLabel.c_str(), true, ImGuiSelectableFlags_Disabled);
                    }

                    ImGui::EndCombo();
                }
                return bLocalChanged;
            });
        }
        else if (bIsNineSliceStyle)
        {
            const TArray<FString> StylePaths = CollectNineSliceStyleJsonPaths();
            const FString Preview = Val->empty() ? FString("None") : MakeAssetPreviewLabel(*Val);

            bChanged = DrawLabeledField(Label, [&]()
            {
                bool bLocalChanged = false;
                if (ImGui::BeginCombo("##Value", Preview.c_str()))
                {
                    const bool bSelectedNone = Val->empty() || *Val == "None";
                    if (ImGui::Selectable("None", bSelectedNone))
                    {
                        Val->clear();
                        bLocalChanged = true;
                    }
                    if (bSelectedNone)
                    {
                        ImGui::SetItemDefaultFocus();
                    }

                    if (!StylePaths.empty())
                    {
                        ImGui::Separator();
                    }

                    for (const FString& StylePath : StylePaths)
                    {
                        const bool bSelected = (*Val == StylePath);
                        if (ImGui::Selectable(StylePath.c_str(), bSelected))
                        {
                            *Val = StylePath;
                            bLocalChanged = true;
                        }
                        if (bSelected)
                        {
                            ImGui::SetItemDefaultFocus();
                        }
                    }

                    if (StylePaths.empty())
                    {
                        ImGui::Separator();
                        ImGui::TextDisabled("No nineslice.json found under Asset/Content");
                    }

                    ImGui::EndCombo();
                }
                return bLocalChanged;
            });
        }
        else if (bIsButtonAction)
        {
            const AActor* OwnerActor = SelectedComponent ? SelectedComponent->GetOwner() : nullptr;
            const TArray<FString> FunctionNames = CollectButtonActionFunctionNames(OwnerActor);
            const FString Preview = Val->empty() ? FString("None") : *Val;

            bChanged = DrawLabeledField(Label, [&]()
            {
                bool bLocalChanged = false;
                if (ImGui::BeginCombo("##Value", Preview.c_str()))
                {
                    const bool bSelectedNone = Val->empty();
                    if (ImGui::Selectable("None", bSelectedNone))
                    {
                        Val->clear();
                        bLocalChanged = true;
                    }
                    if (bSelectedNone)
                    {
                        ImGui::SetItemDefaultFocus();
                    }

                    for (const FString& FunctionName : FunctionNames)
                    {
                        const bool bSelected = (*Val == FunctionName);
                        if (ImGui::Selectable(FunctionName.c_str(), bSelected))
                        {
                            *Val = FunctionName;
                            bLocalChanged = true;
                        }
                        if (bSelected)
                        {
                            ImGui::SetItemDefaultFocus();
                        }
                    }

                    if (FunctionNames.empty())
                    {
                        ImGui::Separator();
                        ImGui::TextDisabled("No callable Lua functions found");
                    }

                    ImGui::EndCombo();
                }
                return bLocalChanged;
            });
        }
        else
        {
            char Buf[256];
            strncpy_s(Buf, sizeof(Buf), Val->c_str(), _TRUNCATE);
            bChanged = DrawLabeledField(Label, [&]()
                                        {
                                            if (ImGui::InputText("##Value", Buf, sizeof(Buf)))
                                            {
                                                *Val = Buf;
                                                return true;
                                            }
                                            return false;
                                        });
        }
        PopDetailsFieldStyle();
        break;
    }
    case EPropertyType::SceneComponentRef:
    {
        FString            *Val = static_cast<FString *>(Prop.ValuePtr);
        UMovementComponent *MovementComp = SelectedComponent ? Cast<UMovementComponent>(SelectedComponent) : nullptr;
        FString             Preview = MovementComp ? MovementComp->GetUpdatedComponentDisplayName() : FString("None");

        bChanged = DrawLabeledField(Label, [&]()
        {
            bool bLocalChanged = false;
            if (ImGui::BeginCombo("##Value", Preview.c_str()))
            {
                bool bSelectedAuto = Val->empty();
                if (ImGui::Selectable("Auto (Root)", bSelectedAuto))
                {
                    Val->clear();
                    bLocalChanged = true;
                }
                if (bSelectedAuto)
                {
                    ImGui::SetItemDefaultFocus();
                }

                if (MovementComp)
                {
                    for (USceneComponent *Candidate : MovementComp->GetOwnerSceneComponents())
                    {
                        if (!Candidate)
                        {
                            continue;
                        }

                        FString CandidatePath = MovementComp->BuildUpdatedComponentPath(Candidate);
                        FString CandidateName = Candidate->GetFName().ToString();
                        if (CandidateName.empty())
                        {
                            CandidateName = Candidate->GetClass()->GetName();
                        }
                        if (!CandidatePath.empty())
                        {
                            CandidateName += " (" + CandidatePath + ")";
                        }

                        bool bSelected = (*Val == CandidatePath);
                        if (ImGui::Selectable(CandidateName.c_str(), bSelected))
                        {
                            *Val = CandidatePath;
                            bLocalChanged = true;
                        }
                        if (bSelected)
                        {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                }

                ImGui::EndCombo();
            }
            return bLocalChanged;
        });
        break;
    }
    case EPropertyType::StaticMeshRef:
    {
        FString *Val = static_cast<FString *>(Prop.ValuePtr);

        FString Preview = Val->empty() ? "None" : GetStemFromPath(*Val);
        if (*Val == "None")
            Preview = "None";

        ImGui::Text("%s", Label);
        ImGui::SameLine(120);

        float ButtonWidth = ImGui::CalcTextSize("Import OBJ").x + ImGui::GetStyle().FramePadding.x * 2.0f;
        float Spacing = ImGui::GetStyle().ItemSpacing.x;
        ImGui::SetNextItemWidth(-(ButtonWidth + Spacing));

        if (ImGui::BeginCombo("##Mesh", Preview.c_str()))
        {
            bool bSelectedNone = (*Val == "None");
            if (ImGui::Selectable("None", bSelectedNone))
            {
                *Val = "None";
                bChanged = true;
            }
            if (bSelectedNone)
                ImGui::SetItemDefaultFocus();

            const TArray<FMeshAssetListItem> &MeshFiles = FObjManager::GetAvailableMeshFiles();
            for (const FMeshAssetListItem &Item : MeshFiles)
            {
                bool bSelected = (*Val == Item.FullPath);
                if (ImGui::Selectable(Item.DisplayName.c_str(), bSelected))
                {
                    *Val = Item.FullPath;
                    bChanged = true;
                }
                if (bSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        // .obj 임포트 버튼
        ImGui::SameLine();

        ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMax().x - ButtonWidth);
        if (ImGui::Button("Import OBJ"))
        {
            FString ObjPath = OpenObjFileDialog();
            if (!ObjPath.empty())
            {
                ID3D11Device *Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
                UStaticMesh  *Loaded = FObjManager::LoadObjStaticMesh(ObjPath, Device);
                if (Loaded)
                {
                    *Val = FObjManager::GetBinaryFilePath(ObjPath);
                    bChanged = true;
                }
            }
        }
        break;
    }
    case EPropertyType::MaterialSlot:
    {
        FMaterialSlot *Slot = static_cast<FMaterialSlot *>(Prop.ValuePtr);
        int32          ElemIdx = (strncmp(Prop.Name.c_str(), "Element ", 8) == 0) ? atoi(&Prop.Name[8]) : -1;

        FString SlotName = "None";
        if (ElemIdx != -1 && SelectedComponent && SelectedComponent->IsA<UStaticMeshComponent>())
        {
            UStaticMeshComponent *SMC = static_cast<UStaticMeshComponent *>(SelectedComponent);
            if (SMC->GetStaticMesh() && ElemIdx < (int32)SMC->GetStaticMesh()->GetStaticMaterials().size())
                SlotName = SMC->GetStaticMesh()->GetStaticMaterials()[ElemIdx].MaterialSlotName;
        }

        // 좌측: Element 인덱스 + 슬롯 이름
        ImGui::BeginGroup();
        ImGui::Text("Element %d", ElemIdx);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
        ImGui::TextUnformatted(SlotName.c_str());
        ImGui::PopStyleColor();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", SlotName.c_str());
        ImGui::EndGroup();

        ImGui::SameLine(120);

        // 우측: Material 콤보
        ImGui::BeginGroup();
        ImGui::PushID(Prop.Name.c_str());
        constexpr float PreviewImageSize = 26.0f;
        constexpr float PreviewSpacing = 6.0f;
        UMaterial *CurrentMaterial = (Slot->Path.empty() || Slot->Path == "None") ? nullptr : FMaterialManager::Get().GetOrCreateMaterial(Slot->Path);
        UTexture2D *CurrentPreviewTexture = FMaterialManager::Get().GetMaterialPreviewTexture(CurrentMaterial);
        const float ReservedPreviewWidth = (CurrentPreviewTexture && CurrentPreviewTexture->GetSRV()) ? (PreviewImageSize + PreviewSpacing) : 0.0f;
        float       ComboWidth = ImGui::GetContentRegionAvail().x - ReservedPreviewWidth;
        if (ComboWidth < 120.0f)
        {
            ComboWidth = 120.0f;
        }
        ImGui::SetNextItemWidth(ComboWidth);

        FString Preview = MakeAssetPreviewLabel(Slot->Path);
        if (ImGui::BeginCombo("##MatCombo", Preview.c_str()))
        {
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
            const TArray<FMaterialAssetListItem> &MatFiles = FMaterialManager::Get().GetAvailableMaterialFiles();
            for (const FMaterialAssetListItem &Item : MatFiles)
            {
                bool        bSelected = (Slot->Path == Item.FullPath);
                UTexture2D *PreviewTexture = FMaterialManager::Get().GetMaterialPreviewTexture(Item.FullPath);
                if (PreviewTexture && PreviewTexture->GetSRV())
                {
                    ImGui::Image(PreviewTexture->GetSRV(), ImVec2(24.0f, 24.0f));
                    ImGui::SameLine();
                }
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

        if (CurrentPreviewTexture && CurrentPreviewTexture->GetSRV())
        {
            ImGui::SameLine(0.0f, PreviewSpacing);
            ImGui::Image(CurrentPreviewTexture->GetSRV(), ImVec2(PreviewImageSize, PreviewImageSize));
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("%s", Preview.c_str());
            }
        }

        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("MaterialContentItem"))
            {
                FContentItem ContentItem = *reinterpret_cast<const FContentItem *>(payload->Data);
                Slot->Path = FPaths::ToUtf8(ContentItem.Path.lexically_relative(FPaths::RootDir()).generic_wstring());
                bChanged = true;
            }
            ImGui::EndDragDropTarget();
        }

        ImGui::PopID();
        ImGui::EndGroup();
        break;
    }
    case EPropertyType::TextureSlot:
    {
        FTextureSlot *Slot = static_cast<FTextureSlot *>(Prop.ValuePtr);
        FString       Preview = MakeAssetPreviewLabel(Slot->Path);

        bChanged = DrawLabeledField(Label, [&]()
        {
            bool bLocalChanged = false;
            if (ImGui::BeginCombo("##Value", Preview.c_str()))
            {
                bool bSelectedNone = (Slot->Path == "None" || Slot->Path.empty());
                if (ImGui::Selectable("None", bSelectedNone))
                {
                    Slot->Path = "None";
                    bLocalChanged = true;
                }
                if (bSelectedNone)
                {
                    ImGui::SetItemDefaultFocus();
                }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                const TArray<FTextureAssetListItem> &TextureFiles = UTexture2D::GetAvailableTextureFiles();
                for (size_t TextureIndex = 0; TextureIndex < TextureFiles.size();)
                {
                    const FString &CurrentSourceFolder = TextureFiles[TextureIndex].SourceFolder;
                    size_t GroupEndIndex = TextureIndex + 1;
                    while (GroupEndIndex < TextureFiles.size() && TextureFiles[GroupEndIndex].SourceFolder == CurrentSourceFolder)
                    {
                        ++GroupEndIndex;
                    }

                    const bool bGroupHasSelection =
                        !Slot->Path.empty() &&
                        Slot->Path != "None" &&
                        Slot->Path.rfind(CurrentSourceFolder, 0) == 0;

                    ImGui::Dummy(ImVec2(0.0f, 4.0f));
                    ImGuiTreeNodeFlags HeaderFlags = ImGuiTreeNodeFlags_SpanAvailWidth;
                    if (bGroupHasSelection)
                    {
                        HeaderFlags |= ImGuiTreeNodeFlags_DefaultOpen;
                    }

                    const bool bGroupOpen = ImGui::CollapsingHeader(
                        MakeTextureFolderGroupLabel(CurrentSourceFolder).c_str(),
                        HeaderFlags);

                    if (bGroupOpen)
                    {
                        for (size_t GroupIndex = TextureIndex; GroupIndex < GroupEndIndex; ++GroupIndex)
                        {
                            const FTextureAssetListItem &Item = TextureFiles[GroupIndex];
                            bool                         bSelected = (Slot->Path == Item.FullPath);
                            UTexture2D                  *PreviewTexture = GetTexturePreviewTexture(Item.FullPath);
                            if (PreviewTexture && PreviewTexture->GetSRV())
                            {
                                ImGui::Image(PreviewTexture->GetSRV(), ImVec2(24.0f, 24.0f));
                                ImGui::SameLine();
                            }

                            if (ImGui::Selectable(Item.DisplayName.c_str(), bSelected))
                            {
                                Slot->Path = Item.FullPath;
                                bLocalChanged = true;
                            }
                            if (bSelected)
                            {
                                ImGui::SetItemDefaultFocus();
                            }
                        }
                    }

                    ImGui::Dummy(ImVec2(0.0f, 6.0f));
                    TextureIndex = GroupEndIndex;
                }
                ImGui::EndCombo();
            }
            return bLocalChanged;
        });

        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("PNGElement"))
            {
                FContentItem ContentItem = *reinterpret_cast<const FContentItem *>(payload->Data);
                Slot->Path = FPaths::ToUtf8(ContentItem.Path.lexically_relative(FPaths::RootDir()).generic_wstring());
                bChanged = true;
            }
            ImGui::EndDragDropTarget();
        }
        break;
    }
    case EPropertyType::Name:
    {
        FName  *Val = static_cast<FName *>(Prop.ValuePtr);
        FString Current = Val->ToString();

        // 리소스 키와 매칭되는 프로퍼티면 콤보 박스로 렌더링
        TArray<FString> Names;
        if (strcmp(Prop.Name.c_str(), "Font") == 0)
            Names = FResourceManager::Get().GetFontNames();
        else if (strcmp(Prop.Name.c_str(), "Particle") == 0)
            Names = FResourceManager::Get().GetParticleNames();
        else if (strcmp(Prop.Name.c_str(), "FilePath") == 0)
        {
            if (USoundComponent *SoundComponent = Cast<USoundComponent>(SelectedComponent))
                Names = FResourceManager::Get().GetSoundNames(SoundComponent->GetCategory());
            else
                Names = FResourceManager::Get().GetSoundNames();
        }
        else if (strcmp(Prop.Name.c_str(), "Click Sound") == 0)
            Names = FResourceManager::Get().GetSoundNames(ESoundCategory::SFX);
        else if (strcmp(Prop.Name.c_str(), "Texture") == 0)
            Names = FResourceManager::Get().GetTextureNames(false);

        if (!Names.empty())
        {
            bChanged = DrawLabeledField(Label, [&]()
            {
                bool bLocalChanged = false;
                if (ImGui::BeginCombo("##Value", Current.c_str()))
                {
                    for (const auto &Name : Names)
                    {
                        bool bSelected = (Current == Name);
                        if (ImGui::Selectable(Name.c_str(), bSelected))
                        {
                            *Val = FName(Name);
                            bLocalChanged = true;
                        }
                        if (bSelected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
                return bLocalChanged;
            });
        }
        else
        {
            char Buf[256];
            strncpy_s(Buf, sizeof(Buf), Current.c_str(), _TRUNCATE);
            bChanged = DrawLabeledField(Label, [&]()
                                        {
                                            if (ImGui::InputText("##Value", Buf, sizeof(Buf)))
                                            {
                                                *Val = FName(Buf);
                                                return true;
                                            }
                                            return false;
                                        });
        }
        break;
    }
    case EPropertyType::Enum:
    {
        if (!Prop.EnumNames || Prop.EnumCount == 0)
            break;
        int32      *Val = static_cast<int32 *>(Prop.ValuePtr);
        const char *Preview = ((uint32)*Val < Prop.EnumCount) ? Prop.EnumNames[*Val] : "Unknown";
        bChanged = DrawLabeledField(Label, [&]()
        {
            bool bLocalChanged = false;
            if (ImGui::BeginCombo("##Value", Preview))
            {
                for (uint32 i = 0; i < Prop.EnumCount; ++i)
                {
                    bool bSelected = (*Val == (int32)i);
                    if (ImGui::Selectable(Prop.EnumNames[i], bSelected))
                    {
                        *Val = (int32)i;
                        bLocalChanged = true;
                    }
                    if (bSelected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            return bLocalChanged;
        });
        break;
    }
    case EPropertyType::Vec3Array:
    {
        TArray<FVector> *Arr = static_cast<TArray<FVector> *>(Prop.ValuePtr);

        ImGui::TextUnformatted(Label);

        int32 RemoveIdx = -1;
        for (int32 i = 0; i < (int32)Arr->size(); ++i)
        {
            ImGui::PushID(i);
            char PointLabel[16];
            snprintf(PointLabel, sizeof(PointLabel), "[%d]", i);
            float Point[3] = {(*Arr)[i].X, (*Arr)[i].Y, (*Arr)[i].Z};
            if (DrawColoredFloat3(PointLabel, Point, 1.0f))
            {
                (*Arr)[i].X = Point[0];
                (*Arr)[i].Y = Point[1];
                (*Arr)[i].Z = Point[2];
                bChanged = true;
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("x"))
                RemoveIdx = i;
            ImGui::PopID();
        }
        if (RemoveIdx >= 0)
        {
            Arr->erase(Arr->begin() + RemoveIdx);
            bChanged = true;
        }
        if (ImGui::Button("+ Add Point"))
        {
            Arr->push_back(FVector(0.0f, 0.0f, 0.0f));
            bChanged = true;
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
