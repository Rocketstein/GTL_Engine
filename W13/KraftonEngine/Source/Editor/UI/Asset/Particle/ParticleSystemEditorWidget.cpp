#include "Editor/UI/Asset/Particle/ParticleSystemEditorWidget.h"
#include "Editor/UI/Panel/FPropertyTable.h"

#include "Asset/AssetRegistry.h"
#include "Component/Primitive/ParticleSystemComponent.h"
#include "Component/Shape/BoxComponent.h"
#include "Core/Logging/Log.h"
#include "Core/Types/CollisionTypes.h"
#include "Distributions/DistributionFloat.h"
#include "Distributions/DistributionVector.h"
#include "Particle/ParticleSystem.h"
#include "Particle/Asset/ParticleSystemManager.h"
#include "Particle/ParticleModuleCollision.h"
#include "Particle/ParticleModuleCollisionBase.h"
#include "Particle/ParticleModuleEvent.h"
#include "Editor/EditorEngine.h"
#include "Editor/UI/Util/EditorTextureManager.h"
#include "Component/Light/DirectionalLightComponent.h"
#include "Core/Property/ArrayProperty.h"
#include "Core/Property/ObjectProperty.h"
#include "Core/Property/StructProperty.h"
#include "Editor/Slate/SlateApplication.h"
#include "GameFramework/Actor/StaticMeshActor.h"
#include "GameFramework/Light/DirectionalLightActor.h"
#include "GameFramework/World.h"
#include "Math/Rotator.h"
#include "Materials/Material.h"
#include "Materials/MaterialManager.h"
#include "Object/Ptr/SoftObjectPtr.h"
#include "Object/Reflection/ObjectFactory.h"
#include "Object/Reflection/UClass.h"
#include "Render/Scene/FScene.h"
#include "Render/Types/MinimalViewInfo.h"
#include "Runtime/Engine.h"
#include "Viewport/Viewport.h"
#include "Platform/Paths.h"
#include <wincodec.h>
#include "ScreenGrab.h"

#include <algorithm>
#include <cfloat>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <unordered_map>
#include <imgui.h>

namespace
{
	FString MakeCascadeIconPath(const wchar_t* FileName)
	{
		return FPaths::ToUtf8(FPaths::Combine(FPaths::AssetDir(), L"Editor/Icons/Cascade/", FileName));
	}

	FString MakeToolIconPath(const wchar_t* FileName)
	{
		return FPaths::ToUtf8(FPaths::Combine(FPaths::AssetDir(), L"Editor/ToolIcons/", FileName));
	}

	bool ContainsParentDirectoryReference(const std::filesystem::path& Path)
	{
		for (const std::filesystem::path& Part : Path)
		{
			if (Part == L"..")
			{
				return true;
			}
		}

		return false;
	}

	std::filesystem::path MakeParticleThumbnailPath(const FString& AssetPath)
	{
		std::filesystem::path RelativePath(FPaths::ToWide(FPaths::MakeProjectRelative(AssetPath)));
		RelativePath = RelativePath.lexically_normal();
		if (RelativePath.is_absolute() || ContainsParentDirectoryReference(RelativePath))
		{
			RelativePath = std::filesystem::path(FPaths::ToWide(AssetPath)).filename();
		}
		RelativePath.replace_extension(L".png");
		return std::filesystem::path(FPaths::SaveDir()) / L"Thumbnails" / L"ParticleSystem" / RelativePath;
	}

	ID3D11ShaderResourceView* LoadIcon(const FString& Path)
	{
		return FEditorTextureManager::Get().GetOrLoadIcon(Path);
	}

	bool DrawIconTextButton(const char* Id, const FString& IconPath, const char* Label, const char* Tooltip = nullptr)
	{
		constexpr float ButtonHeight = 32.0f;
		constexpr float IconSize = 18.0f;
		constexpr float PadX = 8.0f;
		constexpr float Gap = 6.0f;

		const ImVec2 TextSize = Label && Label[0] ? ImGui::CalcTextSize(Label) : ImVec2(0.0f, 0.0f);
		const float ButtonWidth = PadX * 2.0f + IconSize + (TextSize.x > 0.0f ? Gap + TextSize.x : 0.0f);

		ImGui::PushID(Id);
		const ImVec2 Start = ImGui::GetCursorScreenPos();
		const bool bClicked = ImGui::InvisibleButton("##IconTextButton", ImVec2(ButtonWidth, ButtonHeight));
		const bool bHovered = ImGui::IsItemHovered();
		const bool bActive = ImGui::IsItemActive();

		ImDrawList* DrawList = ImGui::GetWindowDrawList();
		const ImU32 BgColor = bActive
			? IM_COL32(66, 72, 82, 255)
			: bHovered
			? IM_COL32(56, 60, 68, 255)
			: IM_COL32(42, 44, 48, 255);
		DrawList->AddRectFilled(Start, ImVec2(Start.x + ButtonWidth, Start.y + ButtonHeight), BgColor, 3.0f);

		const ImVec2 IconMin(Start.x + PadX, Start.y + (ButtonHeight - IconSize) * 0.5f);
		if (ID3D11ShaderResourceView* Icon = LoadIcon(IconPath))
		{
			DrawList->AddImage(
				reinterpret_cast<ImTextureID>(Icon),
				IconMin,
				ImVec2(IconMin.x + IconSize, IconMin.y + IconSize));
		}

		if (TextSize.x > 0.0f)
		{
			DrawList->AddText(
				ImVec2(IconMin.x + IconSize + Gap, Start.y + (ButtonHeight - TextSize.y) * 0.5f),
				ImGui::GetColorU32(ImGuiCol_Text),
				Label);
		}

		if (Tooltip && Tooltip[0] != '\0' && bHovered)
		{
			ImGui::SetTooltip("%s", Tooltip);
		}

		ImGui::PopID();
		return bClicked;
	}

	void SameLineToolbar()
	{
		ImGui::SameLine(0.0f, 4.0f);
	}

	void ToolbarSeparator(float Height)
	{
		SameLineToolbar();
		const ImVec2 Pos = ImGui::GetCursorScreenPos();
		ImGui::Dummy(ImVec2(1.0f, Height));
		ImGui::GetWindowDrawList()->AddLine(
			ImVec2(Pos.x, Pos.y + 4.0f),
			ImVec2(Pos.x, Pos.y + Height - 4.0f),
			IM_COL32(24, 24, 26, 255),
			1.0f);
		SameLineToolbar();
	}

	float ClampFloat(float Value, float MinValue, float MaxValue)
	{
		return (std::min)((std::max)(Value, MinValue), MaxValue);
	}

	bool ContainsTextInsensitive(const FString& Text, const char* Filter)
	{
		if (!Filter || Filter[0] == '\0')
		{
			return true;
		}

		FString LowerText = Text;
		FString LowerFilter = Filter;
		std::transform(LowerText.begin(), LowerText.end(), LowerText.begin(), [](unsigned char Ch) { return static_cast<char>(std::tolower(Ch)); });
		std::transform(LowerFilter.begin(), LowerFilter.end(), LowerFilter.begin(), [](unsigned char Ch) { return static_cast<char>(std::tolower(Ch)); });
		return LowerText.find(LowerFilter) != FString::npos;
	}

	FString GetAssetTypeMetadata(const FPropertyValue& Prop)
	{
		const TMap<FString, FString>& Metadata = Prop.GetMetadata();
		auto It = Metadata.find("assettype");
		if (It != Metadata.end())
		{
			return It->second;
		}

		It = Metadata.find("allowedclass");
		if (It != Metadata.end())
		{
			return It->second;
		}
		return {};
	}

	FString GetAssetPickerPreview(const FString& CurrentPath)
	{
		return (CurrentPath.empty() || CurrentPath == "None") ? FString("None") : CurrentPath;
	}

	bool DrawParticleAssetPathCombo(FSoftObjectPtr& Value, const FString& AssetType)
	{
		if (AssetType != "Material" && AssetType != "StaticMesh" && AssetType != "UVectorFieldAsset")
		{
			return false;
		}

		bool bChanged = false;
		FString CurrentPath = Value.ToString();
		const FString Preview = GetAssetPickerPreview(CurrentPath);
		if (ImGui::BeginCombo("##v", Preview.c_str()))
		{
			const bool bSelectedNone = CurrentPath.empty() || CurrentPath == "None";
			if (ImGui::Selectable("None", bSelectedNone))
			{
				Value.SetPath("None");
				CurrentPath = "None";
				bChanged = true;
			}
			if (bSelectedNone)
			{
				ImGui::SetItemDefaultFocus();
			}

			if (AssetType == "Material")
			{
				const TArray<FMaterialAssetListItem>& MaterialFiles = FMaterialManager::Get().GetAvailableMaterialFiles();
				for (const FMaterialAssetListItem& Item : MaterialFiles)
				{
					const bool bSelected = CurrentPath == Item.FullPath;
					if (ImGui::Selectable(Item.DisplayName.c_str(), bSelected))
					{
						Value.SetPath(Item.FullPath);
						CurrentPath = Item.FullPath;
						bChanged = true;
					}
					if (bSelected)
					{
						ImGui::SetItemDefaultFocus();
					}
				}
			}
			else
			{
				const char* RegistryTypeName = AssetType == "StaticMesh" ? "UStaticMesh" : AssetType.c_str();
				const TArray<FAssetListItem>& AssetFiles = FAssetRegistry::ListByTypeName(RegistryTypeName);
				for (const FAssetListItem& Item : AssetFiles)
				{
					const bool bSelected = CurrentPath == Item.FullPath;
					if (ImGui::Selectable(Item.DisplayName.c_str(), bSelected))
					{
						Value.SetPath(Item.FullPath);
						CurrentPath = Item.FullPath;
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

		return bChanged;
	}

	float GetSplitPaneSize(float TotalSize, float SplitterThickness, float Ratio, float MinFirst, float MinSecond)
	{
		const float UsableSize = (std::max)(1.0f, TotalSize - SplitterThickness);
		const float MinSize = (std::min)(MinFirst, UsableSize);
		const float MaxSize = (std::max)(MinSize, UsableSize - MinSecond);
		return ClampFloat(UsableSize * Ratio, MinSize, MaxSize);
	}

	TArray<UClass*> EnumerateParticleModuleClasses()
	{
		TArray<UClass*> Classes;
		UClass* BaseClass = UParticleModule::StaticClass();
		UClass* AbstractTypeDataBase = UParticleModuleTypeDataBase::StaticClass();
		UClass* AbstractBeamBase = UParticleModuleBeamBase::StaticClass();
		UClass* AbstractTrailBase = UParticleModuleTrailBase::StaticClass();
		UClass* AbstractCollisionBase = UParticleModuleCollisionBase::StaticClass();
		UClass* AbstractEventBase = UParticleModuleEventBase::StaticClass();
		UClass* AbstractEventReceiverBase = UParticleModuleEventReceiverBase::StaticClass();
		UClass* AbstractAccelerationBase = UParticleModuleAccelerationBase::StaticClass();
		UClass* AbstractSubUVBase = UParticleModuleSubUVBase::StaticClass();
		for (UClass* Class : UClass::GetAllClasses())
		{
			if (!Class
				|| Class == BaseClass
				|| Class == AbstractTypeDataBase
				|| Class == AbstractBeamBase
				|| Class == AbstractTrailBase
				|| Class == AbstractCollisionBase
				|| Class == AbstractEventBase
				|| Class == AbstractEventReceiverBase
				|| Class == AbstractAccelerationBase
				|| Class == AbstractSubUVBase)
			{
				continue;
			}
			if (Class->IsA(BaseClass))
			{
				Classes.push_back(Class);
			}
		}

		std::sort(Classes.begin(), Classes.end(), [](const UClass* A, const UClass* B)
		{
			return std::strcmp(A ? A->GetName() : "", B ? B->GetName() : "") < 0;
		});
		return Classes;
	}

	EParticleModuleType GetParticleModuleType(UClass* Class)
	{
		if (!Class)
		{
			return EParticleModuleType::General;
		}
		if (Class->IsA(UParticleModuleTypeDataBase::StaticClass()))
		{
			return EParticleModuleType::TypeData;
		}
		if (Class->IsA(UParticleModuleRequired::StaticClass()))
		{
			return EParticleModuleType::Required;
		}
		if (Class->IsA(UParticleModuleBeamBase::StaticClass()))
		{
			return EParticleModuleType::Beam;
		}
		if (Class->IsA(UParticleModuleTrailBase::StaticClass()))
		{
			return EParticleModuleType::Trail;
		}
		if (Class->IsA(UParticleModuleEventBase::StaticClass()))
		{
			return EParticleModuleType::Event;
		}
		if (Class->IsA(UParticleModuleSubUVBase::StaticClass()))
		{
			return EParticleModuleType::SubUV;
		}
		if (Class->IsA(UParticleModuleCollisionBase::StaticClass()))
		{
			return EParticleModuleType::Collision;
		}
		if (Class->IsA(UParticleModuleSpawn::StaticClass()) || Class->IsA(UParticleModuleSpawnPerUnit::StaticClass()))
		{
			return EParticleModuleType::Spawn;
		}
		return EParticleModuleType::General;
	}

	const char* GetParticleModuleCategoryName(EParticleModuleType ModuleType)
	{
		switch (ModuleType)
		{
		case EParticleModuleType::TypeData:
			return "Type Data";
		case EParticleModuleType::Beam:
			return "Beam";
		case EParticleModuleType::Trail:
			return "Trail";
		case EParticleModuleType::Spawn:
			return "Spawn";
		case EParticleModuleType::Required:
			return "Emitter";
		case EParticleModuleType::Event:
			return "Event";
		case EParticleModuleType::Collision:
			return "Collision";
		case EParticleModuleType::Light:
			return "Light";
		case EParticleModuleType::SubUV:
			return "SubUV";
		case EParticleModuleType::General:
		default:
			return "Module";
		}
	}

	const char* GetParticleModuleCategory(UClass* Class)
	{
		return GetParticleModuleCategoryName(GetParticleModuleType(Class));
	}

	const char* GetParticleEventTypeLabel(EParticleEventType Type)
	{
		switch (Type)
		{
		case EParticleEventType::Any:
			return "Any";
		case EParticleEventType::Spawn:
			return "Spawn";
		case EParticleEventType::Death:
			return "Death";
		case EParticleEventType::Collision:
			return "Collision";
		case EParticleEventType::Burst:
			return "Burst";
		case EParticleEventType::Kismet:
			return "Kismet";
		default:
			return "Unknown";
		}
	}

	std::string GetParticleModuleDisplayName(UClass* Class)
	{
		if (!Class || !Class->GetName())
		{
			return "Particle Module";
		}

		std::string Name = Class->GetName();
		const std::string Prefix = "UParticleModule";
		if (Name.rfind(Prefix, 0) == 0)
		{
			Name = Name.substr(Prefix.size());
		}
		if (Name == "SubUV")
		{
			return "SubImage Index";
		}
		if (Name.empty())
		{
			return "Particle Module";
		}

		std::string Out;
		for (size_t Index = 0; Index < Name.size(); ++Index)
		{
			const char C = Name[Index];
			if (Index > 0 && C >= 'A' && C <= 'Z' && Name[Index - 1] >= 'a' && Name[Index - 1] <= 'z')
			{
				Out.push_back(' ');
			}
			Out.push_back(C);
		}
		return Out;
	}

	std::string GetObjectClassDisplayName(UObject* Object)
	{
		return Object ? GetParticleModuleDisplayName(Object->GetClass()) : "";
	}

	void DrawPanelHeader(const char* Title)
	{
		const ImVec2 HeaderPos = ImGui::GetCursorScreenPos();
		const float HeaderWidth = ImGui::GetContentRegionAvail().x;
		constexpr float HeaderHeight = 24.0f;
		ImGui::GetWindowDrawList()->AddRectFilled(
			HeaderPos,
			ImVec2(HeaderPos.x + HeaderWidth, HeaderPos.y + HeaderHeight),
			IM_COL32(45, 47, 52, 255));
		ImGui::SetCursorScreenPos(ImVec2(HeaderPos.x + 8.0f, HeaderPos.y + 4.0f));
		ImGui::TextUnformatted(Title);
		ImGui::SetCursorScreenPos(ImVec2(HeaderPos.x, HeaderPos.y + HeaderHeight + 1.0f));
	}

	bool IsMouseInRect(const ImVec2& Min, const ImVec2& Max)
	{
		const ImVec2 MousePos = ImGui::GetMousePos();
		return MousePos.x >= Min.x && MousePos.x <= Max.x && MousePos.y >= Min.y && MousePos.y <= Max.y;
	}

	struct FParticleModuleDragPayload
	{
		UParticleLODLevel* SourceLODLevel = nullptr;
		UParticleModule* Module = nullptr;
	};

	struct FParticleEmitterDragPayload
	{
		int32 SourceIndex = -1;
		UParticleEmitter* Emitter = nullptr;
	};

	float GetDetailsCategoryLabelOffset()
	{
		return 16.0f;
	}

	void DrawCategoryArrow(ImDrawList* DrawList, const ImVec2& Pos, bool bOpen, ImU32 Color)
	{
		constexpr float Pad = 4.0f;
		constexpr float Size = 8.0f;
		const ImVec2 IconMin(Pos.x + Pad, Pos.y + Pad);
		if (bOpen)
		{
			DrawList->AddTriangleFilled(
				ImVec2(IconMin.x, IconMin.y + 1.0f),
				ImVec2(IconMin.x + Size, IconMin.y + 1.0f),
				ImVec2(IconMin.x + Size * 0.5f, IconMin.y + Size - 1.0f),
				Color);
		}
		else
		{
			DrawList->AddTriangleFilled(
				ImVec2(IconMin.x + 2.0f, IconMin.y + 1.0f),
				ImVec2(IconMin.x + 2.0f, IconMin.y + Size - 1.0f),
				ImVec2(IconMin.x + Size - 1.0f, IconMin.y + Size * 0.5f),
				Color);
		}
	}

	bool DrawDetailsCategoryHeader(const char* Label, bool bOpen)
	{
		constexpr float HeaderHeight = 22.0f;
		const float LabelOffset = GetDetailsCategoryLabelOffset();
		ImGui::Selectable("##CategoryHeaderHit", false, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0.0f, HeaderHeight));
		const bool bClicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);

		const ImVec2 Min = ImGui::GetItemRectMin();
		const ImVec2 Max = ImGui::GetItemRectMax();
		ImDrawList* DrawList = ImGui::GetWindowDrawList();
		DrawList->AddRectFilled(Min, Max, IM_COL32(68, 68, 68, 255));
		DrawCategoryArrow(DrawList, ImVec2(Min.x, Min.y + 3.0f), bOpen, IM_COL32(235, 235, 235, 255));
		DrawList->AddText(
			ImVec2(Min.x + LabelOffset, Min.y + (HeaderHeight - ImGui::GetTextLineHeight()) * 0.5f),
			ImGui::GetColorU32(ImGuiCol_Text),
			Label ? Label : "Category");

		return bClicked ? !bOpen : bOpen;
	}

	void ReservePopupWidth(float MinWidth)
	{
		const ImVec2 Cursor = ImGui::GetCursorPos();
		ImGui::Dummy(ImVec2(MinWidth, 0.0f));
		ImGui::SetCursorPos(Cursor);
	}

	void DrawFloatingModulePreview(const char* Label, ImU32 RowColor)
	{
		const ImVec2 MousePos = ImGui::GetMousePos();
		const ImVec2 Size(146.0f, 22.0f);
		const ImVec2 Min(MousePos.x + 12.0f, MousePos.y + 10.0f);
		const ImVec2 Max(Min.x + Size.x, Min.y + Size.y);
		ImDrawList* DrawList = ImGui::GetForegroundDrawList();
		DrawList->AddRectFilled(Min, Max, RowColor);
		DrawList->AddRect(Min, Max, IM_COL32(25, 25, 25, 160));
		DrawList->AddText(
			ImVec2(Min.x + 7.0f, Min.y + (Size.y - ImGui::GetTextLineHeight()) * 0.5f),
			ImGui::GetColorU32(ImGuiCol_Text),
			Label ? Label : "Module");
	}

	void DrawFloatingEmitterPreview(const char* Label)
	{
		const ImVec2 MousePos = ImGui::GetMousePos();
		const ImVec2 Size(150.0f, 58.0f);
		const ImVec2 Min(MousePos.x + 12.0f, MousePos.y + 10.0f);
		const ImVec2 Max(Min.x + Size.x, Min.y + Size.y);
		ImDrawList* DrawList = ImGui::GetForegroundDrawList();
		DrawList->AddRectFilled(Min, Max, IM_COL32(122, 92, 110, 240));
		DrawList->AddRect(Min, Max, IM_COL32(255, 180, 32, 255), 0.0f, 0, 1.5f);
		DrawList->AddRectFilled(
			ImVec2(Max.x - 48.0f, Min.y + 5.0f),
			ImVec2(Max.x - 6.0f, Max.y - 7.0f),
			IM_COL32(4, 4, 4, 255));
		DrawList->AddText(
			ImVec2(Min.x + 7.0f, Min.y + 6.0f),
			ImGui::GetColorU32(ImGuiCol_Text),
			Label ? Label : "Emitter");
	}

	void DrawModuleDropIndicator(const ImVec2& RowMin, float Width, bool bAfter)
	{
		const float Y = bAfter ? RowMin.y + 22.0f : RowMin.y;
		ImGui::GetWindowDrawList()->AddLine(
			ImVec2(RowMin.x, Y),
			ImVec2(RowMin.x + Width, Y),
			IM_COL32(255, 170, 32, 255),
			2.0f);
	}

	void DrawEmitterDropIndicator(const ImVec2& CardMin, const ImVec2& CardMax, bool bAfter)
	{
		const float X = bAfter ? CardMax.x : CardMin.x;
		ImGui::GetWindowDrawList()->AddLine(
			ImVec2(X, CardMin.y),
			ImVec2(X, CardMax.y),
			IM_COL32(255, 218, 61, 255),
			2.0f);
	}

	void DrawCurrentLODIndicator(int32 LODIndex)
	{
		constexpr float Width = 62.0f;
		constexpr float Height = 32.0f;
		const ImVec2 Min = ImGui::GetCursorScreenPos();
		const ImVec2 Max(Min.x + Width, Min.y + Height);
		ImGui::InvisibleButton("##CurrentLODIndicator", ImVec2(Width, Height));

		ImDrawList* DrawList = ImGui::GetWindowDrawList();
		DrawList->AddRectFilled(Min, Max, IM_COL32(34, 36, 40, 255));
		DrawList->AddText(
			ImVec2(Min.x + 5.0f, Min.y + (Height - ImGui::GetTextLineHeight()) * 0.5f),
			ImGui::GetColorU32(ImGuiCol_TextDisabled),
			"LOD:");

		const ImVec2 ValueMin(Min.x + 36.0f, Min.y + 5.0f);
		const ImVec2 ValueMax(Max.x - 4.0f, Max.y - 5.0f);
		DrawList->AddRectFilled(ValueMin, ValueMax, IM_COL32(18, 19, 21, 255), 2.0f);
		const FString ValueText = std::to_string(LODIndex);
		DrawList->AddText(
			ImVec2(ValueMin.x + 6.0f, Min.y + (Height - ImGui::GetTextLineHeight()) * 0.5f),
			ImGui::GetColorU32(ImGuiCol_Text),
			ValueText.c_str());
	}

	bool DrawSmallCheckbox(const char* Id, const ImVec2& Min, bool bChecked)
	{
		constexpr float CheckboxSize = 13.0f;
		const ImVec2 Max(Min.x + CheckboxSize, Min.y + CheckboxSize);
		ImDrawList* DrawList = ImGui::GetWindowDrawList();

		ImGui::SetCursorScreenPos(Min);
		ImGui::InvisibleButton(Id, ImVec2(CheckboxSize, CheckboxSize));
		const bool bHovered = ImGui::IsItemHovered();
		const bool bClicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);

		const ImU32 CheckboxBg = bChecked ? IM_COL32(64, 68, 70, 255) : IM_COL32(34, 34, 36, 255);
		const ImU32 CheckboxBorder = bHovered ? IM_COL32(210, 210, 210, 255) : IM_COL32(138, 142, 148, 255);
		DrawList->AddRectFilled(Min, Max, CheckboxBg, 1.0f);
		DrawList->AddRect(Min, Max, CheckboxBorder, 1.0f);
		if (bChecked)
		{
			const ImU32 CheckColor = IM_COL32(230, 230, 230, 255);
			DrawList->AddLine(ImVec2(Min.x + 3.0f, Min.y + 6.5f), ImVec2(Min.x + 5.5f, Min.y + 9.0f), CheckColor, 2.0f);
			DrawList->AddLine(ImVec2(Min.x + 5.5f, Min.y + 9.0f), ImVec2(Min.x + 10.5f, Min.y + 3.5f), CheckColor, 2.0f);
		}

		return bClicked;
	}

	bool DrawSmallCurveButton(const char* Id, const ImVec2& Min, bool bActive)
	{
		constexpr float ButtonSize = 13.0f;
		const ImVec2 Max(Min.x + ButtonSize, Min.y + ButtonSize);
		ImGui::SetCursorScreenPos(Min);
		ImGui::InvisibleButton(Id, ImVec2(ButtonSize, ButtonSize));
		const bool bHovered = ImGui::IsItemHovered();
		const bool bClicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);

		ImDrawList* DrawList = ImGui::GetWindowDrawList();
		const ImU32 Fill = bActive ? IM_COL32(72, 105, 56, 255) : IM_COL32(32, 35, 38, 255);
		const ImU32 Border = bHovered ? IM_COL32(225, 225, 225, 255) : IM_COL32(126, 150, 114, 255);
		DrawList->AddRectFilled(Min, Max, Fill);
		DrawList->AddRect(Min, Max, Border);
		if (ID3D11ShaderResourceView* Icon = LoadIcon(MakeCascadeIconPath(L"Icon_Curve_Editor_16x.png")))
		{
			DrawList->AddImage(
				reinterpret_cast<ImTextureID>(Icon),
				ImVec2(Min.x + 1.0f, Min.y + 1.0f),
				ImVec2(Max.x - 1.0f, Max.y - 1.0f));
		}
		return bClicked;
	}

	void InitializeFlatCurve(FFloatCurve& Curve, float Value)
	{
		Curve.DefaultValue = Value;
		Curve.Keys.clear();
		Curve.AddKey(0.0f, Value);
		Curve.AddKey(1.0f, Value);
	}

	ImU32 GetParticleCurveColor(int32 Index)
	{
		static constexpr ImU32 Colors[] =
		{
			IM_COL32(255, 35, 142, 255),
			IM_COL32(70, 216, 70, 255),
			IM_COL32(45, 125, 255, 255),
			IM_COL32(255, 214, 45, 255),
			IM_COL32(230, 230, 230, 255),
			IM_COL32(255, 115, 40, 255),
			IM_COL32(35, 210, 214, 255),
		};
		return Colors[Index % IM_ARRAYSIZE(Colors)];
	}

	ImVec2 ParticleCurveToScreen(
		float Time, float Value, float MinTime, float MaxTime, float MinValue, float MaxValue,
		const ImVec2& Min, const ImVec2& Max)
	{
		const float TimeSpan = (std::max)(0.001f, MaxTime - MinTime);
		const float ValueSpan = (std::max)(0.001f, MaxValue - MinValue);
		return ImVec2(
			Min.x + (Time - MinTime) / TimeSpan * (Max.x - Min.x),
			Max.y - (Value - MinValue) / ValueSpan * (Max.y - Min.y));
	}

	void ParticleScreenToCurve(
		const ImVec2& Position, float MinTime, float MaxTime, float MinValue, float MaxValue,
		const ImVec2& Min, const ImVec2& Max, float& OutTime, float& OutValue)
	{
		const float Width = (std::max)(1.0f, Max.x - Min.x);
		const float Height = (std::max)(1.0f, Max.y - Min.y);
		OutTime = MinTime + (Position.x - Min.x) / Width * (MaxTime - MinTime);
		OutValue = MinValue + (Max.y - Position.y) / Height * (MaxValue - MinValue);
	}

	int32 FindModuleIndex(UParticleLODLevel* LODLevel, UParticleModule* Module)
	{
		if (!LODLevel || !Module)
		{
			return -1;
		}

		const TArray<UParticleModule*>& Modules = LODLevel->GetModules();
		for (int32 Index = 0; Index < static_cast<int32>(Modules.size()); ++Index)
		{
			if (Modules[Index] == Module)
			{
				return Index;
			}
		}
		return -1;
	}
}

static uint32 GNextParticleSystemEditorInstanceId = 0;

bool FParticleSystemEditorWidget::CanEdit(UObject* Object) const
{
	return Object && Object->IsA<UParticleSystem>();
}

FParticleSystemEditorWidget::FParticleSystemEditorWidget()
	: InstanceId(GNextParticleSystemEditorInstanceId++)
{
	const FString Id = std::to_string(InstanceId);
	PreviewWorldHandle = FName("ParticleSystemEditorPreview_" + Id);
	WindowIdSuffix = "###ParticleSystemEditor_" + Id;
}

FParticleSystemEditorWidget::~FParticleSystemEditorWidget()
{
	ReleasePreviewResources(true);
}

void FParticleSystemEditorWidget::Open(UObject* Object)
{
	FAssetEditorWidget::Open(Object);
	if (!IsOpen())
	{
		return;
	}

	UParticleSystem* ParticleSystem = Cast<UParticleSystem>(EditedObject);
	if (!ParticleSystem)
	{
		return;
	}
	if (ParticleSystem->GetEmitters().empty())
	{
		ParticleSystem->InitializeDefaultSpriteSystem();
		SelectedEmitterIndex = 0;
		MarkDirty();
	}
	if (ParticleSystem->SynchronizeEmitterLODLevels())
	{
		ParticleSystem->BumpVersion();
		MarkDirty();
	}
	SelectedLODIndex = 0;
	CurveTracks.clear();
	SelectedCurveTrackIndex = -1;
	SelectedCurveKeyIndex = -1;

	FWorldContext& WorldContext = GEngine->CreateWorldContext(EWorldType::EditorPreview, PreviewWorldHandle);
	WorldContext.World->SetWorldType(EWorldType::EditorPreview);
	WorldContext.World->InitWorld();

	AActor* PreviewActor = WorldContext.World->SpawnActor<AActor>();
	PreviewActor->SetActorLocation(FVector(0.0f, 0.0f, 0.0f));
	PreviewActor->bTickInEditor = true;

	UParticleSystemComponent* ParticleComponent = PreviewActor->AddComponent<UParticleSystemComponent>();
	PreviewActor->SetRootComponent(ParticleComponent);
	ParticleComponent->SetTemplate(ParticleSystem);
	ParticleComponent->SetComponentTickEnabled(false);
	PreviewParticleComponent = ParticleComponent;
	PreviewPlayback = {};
	
	ADirectionalLightActor* LightActor = WorldContext.World->SpawnActor<ADirectionalLightActor>();
	LightActor->InitDefaultComponents();
	LightActor->SetActorRotation(FVector(0.0f, 45.0f, -45.0f));
	if (UDirectionalLightComponent* LightComp = LightActor->GetComponentByClass<UDirectionalLightComponent>())
	{
		LightComp->SetShadowBias(0.002f);
		LightComp->PushToScene();
	}

	bShowPreviewFloor = true;
	PreviewFloorActor = WorldContext.World->SpawnActor<AStaticMeshActor>();
	PreviewFloorActor->InitDefaultComponents("Content/Data/BasicShape/Cube.OBJ");
	PreviewFloorActor->SetActorLocation(FVector(0.0f, 0.0f, -0.05f));
	PreviewFloorActor->SetActorScale(FVector(10.0f, 10.0f, 0.02f));
	PreviewFloorActor->SetVisible(bShowPreviewFloor);
	if (UBoxComponent* FloorCollision = PreviewFloorActor->AddComponent<UBoxComponent>())
	{
		FloorCollision->AttachToComponent(PreviewFloorActor->GetRootComponent());
		FloorCollision->SetBoxExtent(FVector(0.5f, 0.5f, 0.5f));
		FloorCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		FloorCollision->SetCollisionObjectType(ECollisionChannel::WorldStatic);
		FloorCollision->SetCollisionResponseToAllChannels(ECollisionResponse::Block);
	}

	WorldContext.World->BeginPlay();
	ParticleComponent->SetComponentTickEnabled(false);

	ViewportClient.Initialize(GEngine->GetRenderer().GetFD3DDevice().GetDevice(), 640, 360);
	ViewportClient.SetPreviewWorld(WorldContext.World);
	ViewportClient.SetPreviewActor(PreviewActor);
	ViewportClient.ResetCameraToPreviewBounds();

	WorldContext.World->SetEditorPOVProvider(&ViewportClient);
	FSlateApplication::Get().RegisterViewport(&ViewportClient);
}

void FParticleSystemEditorWidget::Close()
{
	CurveTracks.clear();
	SelectedCurveTrackIndex = -1;
	SelectedCurveKeyIndex = -1;
	FAssetEditorWidget::Close();
	ReleasePreviewResources(false);
}

void FParticleSystemEditorWidget::ReleasePreviewResources(bool bReleaseViewport)
{
	if (UWorld* PreviewWorld = ViewportClient.GetPreviewWorld())
	{
		FScene& PreviewScene = PreviewWorld->GetScene();
		GEngine->GetRenderer().GetResources().ReleaseShadowResourcesForScene(&PreviewScene);

		if (PreviewWorldHandle.IsValid())
		{
			GEngine->DestroyWorldContext(PreviewWorldHandle);
		}
	}

	FSlateApplication::Get().UnregisterViewport(&ViewportClient);
	ViewportClient.SetPreviewWorld(nullptr);
	ViewportClient.SetPreviewActor(nullptr);
	PreviewParticleComponent = nullptr;
	PreviewFloorActor = nullptr;

	if (bReleaseViewport)
	{
		ViewportClient.Release();
	}
}

void FParticleSystemEditorWidget::Tick(float DeltaTime)
{
	if (PreviewParticleComponent)
	{
		PreviewPlayback.Duration = CalculatePreviewDuration();
		if (!PreviewPlayback.bPaused && !PreviewPlayback.bComplete
			&& PreviewPlayback.Duration > 0.0f && PreviewPlayback.CurrentTime >= PreviewPlayback.Duration)
		{
			if (PreviewPlayback.bLooping)
			{
				PreviewParticleComponent->ResetSystem();
				PreviewPlayback.CurrentTime = 0.0f;
			}
			else
			{
				PreviewPlayback.bComplete = true;
			}
		}

		float SimulationDeltaTime = (!PreviewPlayback.bPaused && !PreviewPlayback.bComplete)
			? DeltaTime * ClampFloat(PreviewPlayback.PlayRate, 0.0f, 1.0f)
			: 0.0f;
		if (PreviewPlayback.Duration > 0.0f)
		{
			SimulationDeltaTime = (std::min)(
				SimulationDeltaTime,
				(std::max)(0.0f, PreviewPlayback.Duration - PreviewPlayback.CurrentTime));
		}

		PreviewParticleComponent->AdvanceSimulation(SimulationDeltaTime);
		if (SimulationDeltaTime > 0.0f)
		{
			PreviewPlayback.CurrentTime += SimulationDeltaTime;
			PreviewPlayback.AccumulatedTime += SimulationDeltaTime;
			if (!PreviewPlayback.bLooping && PreviewPlayback.Duration > 0.0f
				&& PreviewPlayback.CurrentTime >= PreviewPlayback.Duration)
			{
				PreviewPlayback.bComplete = true;
			}
		}
	}

	if (ViewportClient.IsRenderable())
	{
		ViewportClient.Tick(DeltaTime);
	}
}

void FParticleSystemEditorWidget::RestartPreviewSimulation()
{
	if (!PreviewParticleComponent)
	{
		return;
	}

	PreviewParticleComponent->ResetSystem();
	PreviewPlayback.bPaused = false;
	PreviewPlayback.bComplete = false;
	PreviewPlayback.CurrentTime = 0.0f;
	PreviewPlayback.AccumulatedTime = 0.0f;
}

void FParticleSystemEditorWidget::RestartLevelParticleSystems(UParticleSystem* ParticleSystem)
{
	RestartPreviewSimulation();

	UWorld* ActiveWorld = EditorEngine ? EditorEngine->GetWorld() : nullptr;
	if (!ActiveWorld || !ParticleSystem)
	{
		return;
	}

	for (AActor* Actor : ActiveWorld->GetActors())
	{
		if (!Actor)
		{
			continue;
		}

		for (UActorComponent* Component : Actor->GetComponents())
		{
			UParticleSystemComponent* ParticleComponent = Cast<UParticleSystemComponent>(Component);
			if (ParticleComponent && ParticleComponent->GetTemplate() == ParticleSystem)
			{
				ParticleComponent->ResetSystem();
				ParticleComponent->AdvanceSimulation(0.0f);
			}
		}
	}
}

bool FParticleSystemEditorWidget::CaptureThumbnail(UParticleSystem* ParticleSystem)
{
	if (!ParticleSystem || ParticleSystem->GetSourcePath().empty())
	{
		return false;
	}

	FViewport* Viewport = ViewportClient.GetViewport();
	ID3D11Texture2D* RenderTarget = Viewport ? Viewport->GetRTTexture() : nullptr;
	ID3D11DeviceContext* DeviceContext = GEngine
		? GEngine->GetRenderer().GetFD3DDevice().GetDeviceContext()
		: nullptr;
	if (!RenderTarget || !DeviceContext)
	{
		return false;
	}

	const std::filesystem::path ThumbnailPath = MakeParticleThumbnailPath(ParticleSystem->GetSourcePath());
	FPaths::CreateDir(ThumbnailPath.parent_path().wstring());

	const HRESULT Result = DirectX::SaveWICTextureToFile(
		DeviceContext,
		RenderTarget,
		GUID_ContainerFormatPng,
		ThumbnailPath.c_str());
	if (FAILED(Result))
	{
		UE_LOG("[ParticleSystemEditor] Failed to save thumbnail: %s (HRESULT=0x%08X)",
			FPaths::ToUtf8(ThumbnailPath.wstring()).c_str(),
			static_cast<uint32>(Result));
		return false;
	}

	if (EditorEngine)
	{
		EditorEngine->RefreshContentBrowser();
	}
	return true;
}

void FParticleSystemEditorWidget::CollectPreviewViewports(TArray<IEditorPreviewViewportClient*>& OutClients) const
{
	if (IsOpen())
	{
		OutClients.push_back(const_cast<FParticleEditorViewportClient*>(&ViewportClient));
	}
}

void FParticleSystemEditorWidget::Render(float DeltaTime)
{
	(void)DeltaTime;
	if (!IsOpen() || !EditedObject)
	{
		return;
	}

	UParticleSystem* ParticleSystem = static_cast<UParticleSystem*>(EditedObject);

	bool bWindowOpen = true;
	FString VisibleTitle = "Particle System Editor";
	if (!ParticleSystem->GetSourcePath().empty())
	{
		VisibleTitle += " - ";
		VisibleTitle += ParticleSystem->GetSourcePath();
	}
	if (IsDirty())
	{
		VisibleTitle += " *";
	}

	ImGuiWindowFlags WindowFlags = ImGuiWindowFlags_MenuBar;
	if (ViewportClient.IsMouseOverViewport())
	{
		WindowFlags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;
	}

	FString WindowTitle = VisibleTitle + WindowIdSuffix;
	if (ConsumeFocusRequest())
	{
		ImGui::SetNextWindowFocus();
	}

	ImGui::SetNextWindowSize(ImVec2(1600.0f, 900.0f), ImGuiCond_Once);
	if (!ImGui::Begin(WindowTitle.c_str(), &bWindowOpen, WindowFlags))
	{
		ImGui::End();
		if (!bWindowOpen)
		{
			Close();
		}
		return;
	}

	if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
	{
		FSlateApplication::Get().BringViewportToFront(&ViewportClient);
	}

	RenderMenuBar(ParticleSystem);
	RenderParticleAssetSearchPopup();
	RenderToolbar(ParticleSystem);

	const ImVec2 Available = ImGui::GetContentRegionAvail();
	constexpr float SplitterThickness = 6.0f;
	const float ContentWidth = (std::max)(1.0f, Available.x);
	const float ContentHeight = (std::max)(1.0f, Available.y);
	const float UsableWidth = (std::max)(1.0f, ContentWidth - SplitterThickness);
	const float UsableHeight = (std::max)(1.0f, ContentHeight - SplitterThickness);

	const float LeftWidth = GetSplitPaneSize(ContentWidth, SplitterThickness, ColumnSplitRatio, 260.0f, 260.0f);
	const float RightWidth = (std::max)(1.0f, UsableWidth - LeftWidth);
	ColumnSplitRatio = ClampFloat(LeftWidth / UsableWidth, 0.05f, 0.95f);

	const float LeftTopHeight = GetSplitPaneSize(ContentHeight, SplitterThickness, ViewportDetailsSplitRatio, 180.0f, 120.0f);
	const float LeftBottomHeight = (std::max)(1.0f, UsableHeight - LeftTopHeight);
	ViewportDetailsSplitRatio = ClampFloat(LeftTopHeight / UsableHeight, 0.05f, 0.95f);

	const float RightTopHeight = GetSplitPaneSize(ContentHeight, SplitterThickness, EmitterCurveSplitRatio, 180.0f, 120.0f);
	const float RightBottomHeight = (std::max)(1.0f, UsableHeight - RightTopHeight);
	EmitterCurveSplitRatio = ClampFloat(RightTopHeight / UsableHeight, 0.05f, 0.95f);

	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

	ImGui::BeginChild("ParticleEditorLeftColumn", ImVec2(LeftWidth, ContentHeight), false, ImGuiWindowFlags_NoScrollbar);
	RenderViewportPanel(ImVec2(LeftWidth, LeftTopHeight));
	RenderHorizontalSplitter("##ViewportDetailsSplitter", LeftWidth, ViewportDetailsSplitRatio, UsableHeight);
	RenderDetailsPanel(ParticleSystem, ImVec2(LeftWidth, LeftBottomHeight));
	ImGui::EndChild();

	ImGui::SameLine(0.0f, 0.0f);
	RenderVerticalSplitter("##ParticleColumnSplitter", ContentHeight, ColumnSplitRatio, UsableWidth);
	ImGui::SameLine(0.0f, 0.0f);

	ImGui::BeginChild("ParticleEditorRightColumn", ImVec2(RightWidth, ContentHeight), false, ImGuiWindowFlags_NoScrollbar);
	RenderEmitterPanel(ParticleSystem, ImVec2(RightWidth, RightTopHeight));
	RenderHorizontalSplitter("##EmitterCurveSplitter", RightWidth, EmitterCurveSplitRatio, UsableHeight);
	RenderCurveEditorPanel(ParticleSystem, ImVec2(RightWidth, RightBottomHeight));
	ImGui::EndChild();

	ImGui::PopStyleVar(2);

	ImGui::End();

	if (!bWindowOpen)
	{
		Close();
	}
}

void FParticleSystemEditorWidget::RenderMenuBar(UParticleSystem* ParticleSystem)
{
	if (!ImGui::BeginMenuBar())
	{
		return;
	}

	if (ImGui::BeginMenu("File"))
	{
		if (ImGui::MenuItem("Save", "Ctrl+S", false, ParticleSystem != nullptr))
		{
			if (ParticleSystem && FParticleSystemManager::Get().Save(ParticleSystem))
			{
				ClearDirty();
			}
		}
		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu("Edit"))
	{
		ImGui::MenuItem("No Actions", nullptr, false, false);
		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu("Asset"))
	{
		if (ImGui::MenuItem("Search Particle Asset..."))
		{
			bOpenParticleAssetSearchPopup = true;
			ParticleAssetSearchBuffer[0] = '\0';
		}
		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu("Windows"))
	{
		ImGui::MenuItem("Viewport", nullptr, false, false);
		ImGui::MenuItem("Emitter", nullptr, false, false);
		ImGui::MenuItem("Details", nullptr, false, false);
		ImGui::MenuItem("Curve Editor", nullptr, false, false);
		ImGui::EndMenu();
	}

	ImGui::EndMenuBar();
}

void FParticleSystemEditorWidget::RenderParticleAssetSearchPopup()
{
	constexpr const char* PopupId = "Particle Asset Search";
	if (bOpenParticleAssetSearchPopup)
	{
		ImGui::OpenPopup(PopupId);
		bOpenParticleAssetSearchPopup = false;
	}

	ImGui::SetNextWindowSize(ImVec2(520.0f, 360.0f), ImGuiCond_Appearing);
	if (!ImGui::BeginPopupModal(PopupId, nullptr, ImGuiWindowFlags_NoSavedSettings))
	{
		return;
	}

	ImGui::TextUnformatted("Search");
	ImGui::SetNextItemWidth(-1.0f);
	ImGui::InputText("##ParticleAssetSearchInput", ParticleAssetSearchBuffer, sizeof(ParticleAssetSearchBuffer));

	ImGui::Spacing();
	ImGui::BeginChild("##ParticleAssetSearchResults", ImVec2(0.0f, 250.0f), true);

	const TArray<FAssetListItem>& ParticleAssets = FAssetRegistry::ListByTypeName("UParticleSystem");
	int32 VisibleCount = 0;
	for (const FAssetListItem& Item : ParticleAssets)
	{
		if (!ContainsTextInsensitive(Item.DisplayName, ParticleAssetSearchBuffer)
			&& !ContainsTextInsensitive(Item.FullPath, ParticleAssetSearchBuffer))
		{
			continue;
		}

		++VisibleCount;
		const FString Label = Item.DisplayName + "##ParticleAssetSearch" + Item.FullPath;
		if (ImGui::Selectable(Label.c_str()))
		{
			if (EditorEngine)
			{
				if (UParticleSystem* ParticleSystem = FParticleSystemManager::Get().Load(Item.FullPath))
				{
					EditorEngine->OpenAssetEditorForObject(ParticleSystem);
				}
			}
			ImGui::CloseCurrentPopup();
		}
		ImGui::TextDisabled("%s", Item.FullPath.c_str());
		ImGui::Spacing();
	}

	if (VisibleCount == 0)
	{
		ImGui::TextDisabled("No particle assets found.");
	}

	ImGui::EndChild();

	if (ImGui::Button("Close", ImVec2(90.0f, 0.0f)))
	{
		ImGui::CloseCurrentPopup();
	}

	ImGui::EndPopup();
}

void FParticleSystemEditorWidget::RenderToolbar(UParticleSystem* ParticleSystem)
{
	constexpr float ToolbarHeight = 38.0f;
	const ImVec2 ToolbarPos = ImGui::GetCursorScreenPos();
	const float ToolbarWidth = ImGui::GetContentRegionAvail().x;
	ImGui::GetWindowDrawList()->AddRectFilled(
		ToolbarPos,
		ImVec2(ToolbarPos.x + ToolbarWidth, ToolbarPos.y + ToolbarHeight),
		IM_COL32(34, 36, 40, 255));

	ImGui::SetCursorScreenPos(ImVec2(ToolbarPos.x + 6.0f, ToolbarPos.y + 3.0f));

	if (DrawIconTextButton("Save", MakeToolIconPath(L"SavePreset.png"), "", "Save"))
	{
		if (ParticleSystem && FParticleSystemManager::Get().Save(ParticleSystem))
		{
			ClearDirty();
		}
	}
	SameLineToolbar();
	DrawIconTextButton("FindInContentBrowser", MakeCascadeIconPath(L"icon_toolbar_genericfinder_40px.png"), "", "Find in Content Browser");

	ToolbarSeparator(ToolbarHeight - 6.0f);

	if (DrawIconTextButton("RestartSim", MakeCascadeIconPath(L"icon_Cascade_RestartSim_40x.png"), "Restart Sim"))
	{
		RestartPreviewSimulation();
	}
	SameLineToolbar();
	if (DrawIconTextButton("RestartInLevel", MakeCascadeIconPath(L"icon_Cascade_RestartInLevel_40x.png"), "Restart Level"))
	{
		RestartLevelParticleSystems(ParticleSystem);
	}

	ToolbarSeparator(ToolbarHeight - 6.0f);

	if (DrawIconTextButton("Thumbnail", MakeCascadeIconPath(L"icon_Cascade_Thumbnail_40x.png"), "Thumbnail"))
	{
		CaptureThumbnail(ParticleSystem);
	}
	SameLineToolbar();
	if (DrawIconTextButton("Bounds", MakeCascadeIconPath(L"icon_Cascade_Bounds_40x.png"), "Bounds"))
	{
		FShowFlags& ShowFlags = ViewportClient.GetRenderOptions().ShowFlags;
		ShowFlags.bParticleEditorBounds = !ShowFlags.bParticleEditorBounds;
	}
	SameLineToolbar();
	if (DrawIconTextButton("Axis", MakeCascadeIconPath(L"icon_Cascade_Axis_40x.png"), "Origin Axis"))
	{
		FShowFlags& ShowFlags = ViewportClient.GetRenderOptions().ShowFlags;
		ShowFlags.bOriginAxisGizmo = !ShowFlags.bOriginAxisGizmo;
	}
	SameLineToolbar();
	if (DrawIconTextButton("Color", MakeCascadeIconPath(L"icon_Cascade_Color_40x.png"), "Background Color"))
	{
		bOpenBackgroundColorPopup = true;
	}

	if (bOpenBackgroundColorPopup)
	{
		ImGui::OpenPopup("##ParticleBackgroundColorPopup");
		bOpenBackgroundColorPopup = false;
	}
	ImGui::SetNextWindowSize(ImVec2(280.0f, 300.0f), ImGuiCond_Appearing);
	if (ImGui::BeginPopup("##ParticleBackgroundColorPopup"))
	{
		ImGui::TextUnformatted("Background Color");
		ImGui::Separator();
		ImGui::ColorPicker3(
			"##ParticleViewportBackgroundColor",
			ViewportClient.GetBackgroundColor(),
			ImGuiColorEditFlags_PickerHueWheel | ImGuiColorEditFlags_NoAlpha);
		ImGui::EndPopup();
	}

	ToolbarSeparator(ToolbarHeight - 6.0f);
	if (DrawIconTextButton("LowestLOD", MakeCascadeIconPath(L"icon_Cascade_LowestLOD_40x.png"), "Lowest LOD"))
	{
		SelectParticleLOD(ParticleSystem, 0);
	}
	SameLineToolbar();
	if (DrawIconTextButton("LowerLOD", MakeCascadeIconPath(L"icon_Cascade_LowerLOD_40x.png"), "Lower LOD"))
	{
		SelectParticleLOD(ParticleSystem, SelectedLODIndex - 1);
	}
	SameLineToolbar();
	ImGui::BeginDisabled(SelectedLODIndex <= 0);
	if (DrawIconTextButton("AddLOD", MakeCascadeIconPath(L"icon_Cascade_AddLOD1_40x.png"), "Add LOD"))
	{
		InsertParticleLOD(ParticleSystem, SelectedLODIndex);
	}
	ImGui::EndDisabled();
	SameLineToolbar();
	DrawCurrentLODIndicator(SelectedLODIndex);
	SameLineToolbar();
	if (DrawIconTextButton("AddLOD2", MakeCascadeIconPath(L"icon_Cascade_AddLOD2_40x.png"), "Add LOD"))
	{
		InsertParticleLOD(ParticleSystem, SelectedLODIndex + 1);
	}
	SameLineToolbar();
	if (DrawIconTextButton("HigherLOD", MakeCascadeIconPath(L"icon_Cascade_HigherLOD_40x.png"), "Higher LOD"))
	{
		SelectParticleLOD(ParticleSystem, SelectedLODIndex + 1);
	}
	SameLineToolbar();
	if (DrawIconTextButton("HighestLOD", MakeCascadeIconPath(L"icon_Cascade_HighestLOD_40x.png"), "Highest LOD"))
	{
		const int32 LastLODIndex = ParticleSystem ? static_cast<int32>(ParticleSystem->GetLODDistances().size()) - 1 : 0;
		SelectParticleLOD(ParticleSystem, LastLODIndex);
	}
	SameLineToolbar();
	ImGui::BeginDisabled(SelectedLODIndex <= 0);
	if (DrawIconTextButton("DeleteLOD", MakeCascadeIconPath(L"icon_Cascade_DeleteLOD_40x.png"), "Delete LOD"))
	{
		DeleteParticleLOD(ParticleSystem, SelectedLODIndex);
	}
	ImGui::EndDisabled();

	ImGui::SetCursorScreenPos(ImVec2(ToolbarPos.x, ToolbarPos.y + ToolbarHeight));
	ImGui::Dummy(ImVec2(ToolbarWidth, 1.0f));
}

void FParticleSystemEditorWidget::RenderVerticalSplitter(const char* Id, float Height, float& InOutRatio, float UsableWidth)
{
	constexpr float SplitterThickness = 6.0f;
	ImGui::InvisibleButton(Id, ImVec2(SplitterThickness, Height));
	const bool bHovered = ImGui::IsItemHovered();
	const bool bActive = ImGui::IsItemActive();

	if (bActive && UsableWidth > 1.0f)
	{
		InOutRatio = ClampFloat(InOutRatio + ImGui::GetIO().MouseDelta.x / UsableWidth, 0.05f, 0.95f);
	}

	if (bHovered || bActive)
	{
		ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
	}

	const ImVec2 Min = ImGui::GetItemRectMin();
	const ImVec2 Max = ImGui::GetItemRectMax();
	const ImU32 Color = bActive
		? IM_COL32(92, 98, 110, 255)
		: bHovered
		? IM_COL32(70, 76, 86, 255)
		: IM_COL32(42, 44, 48, 255);
	ImGui::GetWindowDrawList()->AddRectFilled(Min, Max, Color);
}

void FParticleSystemEditorWidget::RenderHorizontalSplitter(const char* Id, float Width, float& InOutRatio, float UsableHeight)
{
	constexpr float SplitterThickness = 6.0f;
	ImGui::InvisibleButton(Id, ImVec2(Width, SplitterThickness));
	const bool bHovered = ImGui::IsItemHovered();
	const bool bActive = ImGui::IsItemActive();

	if (bActive && UsableHeight > 1.0f)
	{
		InOutRatio = ClampFloat(InOutRatio + ImGui::GetIO().MouseDelta.y / UsableHeight, 0.05f, 0.95f);
	}

	if (bHovered || bActive)
	{
		ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
	}

	const ImVec2 Min = ImGui::GetItemRectMin();
	const ImVec2 Max = ImGui::GetItemRectMax();
	const ImU32 Color = bActive
		? IM_COL32(92, 98, 110, 255)
		: bHovered
		? IM_COL32(70, 76, 86, 255)
		: IM_COL32(42, 44, 48, 255);
	ImGui::GetWindowDrawList()->AddRectFilled(Min, Max, Color);
}

void FParticleSystemEditorWidget::AddParticleEmitter(UParticleSystem* ParticleSystem)
{
	if (!ParticleSystem)
	{
		return;
	}

	UParticleEmitter* NewEmitter = ParticleSystem->AddEmitter();
	const TArray<UParticleEmitter*>& Emitters = ParticleSystem->GetEmitters();
	SelectedEmitterIndex = static_cast<int32>(Emitters.size()) - 1;
	SelectedModule = nullptr;
	MarkDirty();
}

void FParticleSystemEditorWidget::InsertParticleEmitter(UParticleSystem* ParticleSystem, int32 Index)
{
	if (!ParticleSystem)
	{
		return;
	}

	UParticleEmitter* NewEmitter = ParticleSystem->InsertEmitter(Index);
	const TArray<UParticleEmitter*>& Emitters = ParticleSystem->GetEmitters();
	SelectedEmitterIndex = Index < 0 ? 0 : Index;
	if (SelectedEmitterIndex >= static_cast<int32>(Emitters.size()))
	{
		SelectedEmitterIndex = static_cast<int32>(Emitters.size()) - 1;
	}
	SelectedModule = nullptr;
	MarkDirty();
}

void FParticleSystemEditorWidget::DeleteParticleEmitter(UParticleSystem* ParticleSystem, int32 Index)
{
	if (!ParticleSystem)
	{
		return;
	}

	const TArray<UParticleEmitter*>& Emitters = ParticleSystem->GetEmitters();
	if (Index < 0 || Index >= static_cast<int32>(Emitters.size()))
	{
		return;
	}

	UParticleEmitter* Emitter = Emitters[Index];
	if (!ParticleSystem->RemoveEmitter(Emitter))
	{
		return;
	}

	const int32 NewCount = static_cast<int32>(ParticleSystem->GetEmitters().size());
	if (NewCount <= 0)
	{
		SelectedEmitterIndex = -1;
		SelectedModule = nullptr;
	}
	else
	{
		SelectedEmitterIndex = (std::min)(Index, NewCount - 1);
		SelectedModule = nullptr;
	}

	MarkDirty();
}

void FParticleSystemEditorWidget::MoveParticleEmitter(UParticleSystem* ParticleSystem, int32 SourceIndex, int32 TargetIndex)
{
	if (!ParticleSystem)
	{
		return;
	}

	int32 NewSelectedIndex = TargetIndex;
	if (SourceIndex < NewSelectedIndex)
	{
		--NewSelectedIndex;
	}

	if (!ParticleSystem->MoveEmitter(SourceIndex, TargetIndex))
	{
		return;
	}

	const int32 Count = static_cast<int32>(ParticleSystem->GetEmitters().size());
	SelectedEmitterIndex = Count > 0 ? (std::min)((std::max)(NewSelectedIndex, 0), Count - 1) : -1;
	SelectedModule = nullptr;
	MarkDirty();
}

void FParticleSystemEditorWidget::InsertParticleLOD(UParticleSystem* ParticleSystem, int32 Index)
{
	if (!ParticleSystem || !ParticleSystem->InsertLODLevel(Index))
	{
		return;
	}

	SelectedLODIndex = Index;
	SelectedModule = nullptr;
	MarkDirty();
}

void FParticleSystemEditorWidget::DeleteParticleLOD(UParticleSystem* ParticleSystem, int32 Index)
{
	if (!ParticleSystem || !ParticleSystem->RemoveLODLevel(Index))
	{
		return;
	}

	SelectedLODIndex = (std::min)(Index, static_cast<int32>(ParticleSystem->GetLODDistances().size()) - 1);
	SelectedModule = nullptr;
	MarkDirty();
}

void FParticleSystemEditorWidget::SelectParticleLOD(UParticleSystem* ParticleSystem, int32 Index)
{
	if (!ParticleSystem || ParticleSystem->GetLODDistances().empty())
	{
		SelectedLODIndex = 0;
		SelectedModule = nullptr;
		return;
	}

	SelectedLODIndex = (std::min)(
		(std::max)(Index, 0),
		static_cast<int32>(ParticleSystem->GetLODDistances().size()) - 1);
	SelectedModule = nullptr;
}

void FParticleSystemEditorWidget::AddParticleModule(UParticleSystem* ParticleSystem, UParticleEmitter* Emitter, UClass* ModuleClass)
{
	if (!ParticleSystem || !Emitter || !ModuleClass)
	{
		return;
	}

	UParticleLODLevel* LODLevel = Emitter->GetLODLevel(SelectedLODIndex);
	if (!LODLevel)
	{
		LODLevel = Emitter->AddLODLevel();
	}
	if (!LODLevel)
	{
		return;
	}

	UObject* Created = FObjectFactory::Get().Create(ModuleClass->GetName(), LODLevel);
	UParticleModule* Module = Cast<UParticleModule>(Created);
	if (!Module)
	{
		UObjectManager::Get().DestroyObject(Created);
		return;
	}

	if (UParticleModuleSubUV* SubUVModule = Cast<UParticleModuleSubUV>(Module))
	{
		if (const UParticleModuleTypeDataSprite* SpriteTypeData = Cast<UParticleModuleTypeDataSprite>(LODLevel->GetTypeDataModule()))
		{
			if (SpriteTypeData->bUseSubUV)
			{
				SubUVModule->SubUVResourceName = SpriteTypeData->SubUVResourceName;
				SubUVModule->SubImagesX = SpriteTypeData->SubImagesX;
				SubUVModule->SubImagesY = SpriteTypeData->SubImagesY;
			}
		}
	}

	if (UParticleModuleRequired* RequiredModule = Cast<UParticleModuleRequired>(Module))
	{
		LODLevel->SetRequiredModule(RequiredModule);
		SelectedModule = RequiredModule;
	}
	else if (UParticleModuleTypeDataBase* TypeDataModule = Cast<UParticleModuleTypeDataBase>(Module))
	{
		LODLevel->SetTypeDataModule(TypeDataModule);
		SelectedModule = TypeDataModule;
	}
	else if (Cast<UParticleModuleSpawn>(Module))
	{
		for (UParticleModule* ExistingModule : LODLevel->GetModules())
		{
			if (UParticleModuleSpawn* ExistingSpawn = Cast<UParticleModuleSpawn>(ExistingModule))
			{
				UObjectManager::Get().DestroyObject(Module);
				SelectedModule = ExistingSpawn;
				ParticleSystem->CacheSystemModuleInfo();
				return;
			}
		}

		LODLevel->AddModule(Module);
		SelectedModule = Module;
	}
	else if (UParticleModuleSubUV* SubUVModule = Cast<UParticleModuleSubUV>(Module))
	{
		for (UParticleModule* ExistingModule : LODLevel->GetModules())
		{
			if (UParticleModuleSubUV* ExistingSubUV = Cast<UParticleModuleSubUV>(ExistingModule))
			{
				UObjectManager::Get().DestroyObject(SubUVModule);
				SelectedModule = ExistingSubUV;
				ParticleSystem->CacheSystemModuleInfo();
				return;
			}
		}

		LODLevel->AddModule(SubUVModule);
		SelectedModule = SubUVModule;
	}
	else
	{
		LODLevel->AddModule(Module);
		SelectedModule = Module;
	}

	ParticleSystem->CacheSystemModuleInfo();
	ParticleSystem->BumpVersion();
	MarkDirty();
}

void FParticleSystemEditorWidget::MoveParticleModule(UParticleSystem* ParticleSystem, UParticleLODLevel* SourceLODLevel, UParticleLODLevel* TargetLODLevel, UParticleModule* Module, int32 TargetIndex)
{
	if (!ParticleSystem || !SourceLODLevel || !TargetLODLevel || !Module || SourceLODLevel->IsFixedModule(Module))
	{
		return;
	}

	const int32 SourceIndex = FindModuleIndex(SourceLODLevel, Module);
	if (SourceIndex < 0)
	{
		return;
	}

	if (SourceLODLevel == TargetLODLevel && SourceIndex < TargetIndex)
	{
		--TargetIndex;
	}

	if (!SourceLODLevel->DetachModule(Module))
	{
		return;
	}

	TargetLODLevel->InsertModule(Module, TargetIndex);
	ParticleSystem->CacheSystemModuleInfo();
	ParticleSystem->BumpVersion();
	MarkDirty();
}

bool FParticleSystemEditorWidget::HasCurveEditableValues(UParticleModule* Module) const
{
	if (!Module || !Module->GetClass())
	{
		return false;
	}

	TArray<const FProperty*> Properties;
	Module->GetClass()->GetPropertyRefs(Properties, false);
	for (const FProperty* Property : Properties)
	{
		const FObjectProperty* ObjectProperty = Property ? Property->AsObjectProperty() : nullptr;
		if (!ObjectProperty)
		{
			continue;
		}

		UObject* Value = ObjectProperty->GetObjectValue(Module);
		if (Cast<UDistributionFloatConstant>(Value)
			|| Cast<UDistributionFloatUniform>(Value)
			|| Cast<UDistributionFloatConstantCurve>(Value)
			|| Cast<UDistributionFloatUniformCurve>(Value)
			|| Cast<UDistributionVectorConstant>(Value)
			|| Cast<UDistributionVectorUniform>(Value)
			|| Cast<UDistributionVectorConstantCurve>(Value)
			|| Cast<UDistributionVectorUniformCurve>(Value))
		{
			return true;
		}
	}
	return false;
}

void FParticleSystemEditorWidget::AddModuleCurvesToEditor(UParticleSystem* ParticleSystem, UParticleModule* Module)
{
	if (!ParticleSystem || !Module || !Module->GetClass())
	{
		return;
	}

	bool bConverted = false;
	bool bAddedTrack = false;
	TArray<const FProperty*> Properties;
	Module->GetClass()->GetPropertyRefs(Properties, false);

	auto AddTrack = [&](const FProperty* Property, const FString& Label, EParticleCurveChannel Channel)
	{
		for (const FParticleCurveTrack& Track : CurveTracks)
		{
			if (Track.Module == Module && Track.PropertyName == Property->Name && Track.Channel == Channel)
			{
				return;
			}
		}

		FParticleCurveTrack Track;
		Track.Module = Module;
		Track.PropertyName = Property->Name;
		Track.Label = Label;
		Track.Channel = Channel;
		Track.Color = GetParticleCurveColor(static_cast<int32>(CurveTracks.size()));
		CurveTracks.push_back(std::move(Track));
		bAddedTrack = true;
	};

	for (const FProperty* Property : Properties)
	{
		const FObjectProperty* ObjectProperty = Property ? Property->AsObjectProperty() : nullptr;
		if (!ObjectProperty)
		{
			continue;
		}

		const FString BaseLabel = Property->DisplayName && Property->DisplayName[0] ? Property->DisplayName : Property->Name;
		UObject* Current = ObjectProperty->GetObjectValue(Module);

		if (UDistributionFloatConstant* Constant = Cast<UDistributionFloatConstant>(Current))
		{
			UDistributionFloatConstantCurve* CurveDistribution = UObjectManager::Get().CreateObject<UDistributionFloatConstantCurve>(Module);
			InitializeFlatCurve(CurveDistribution->ConstantCurve, Constant->Constant);
			ObjectProperty->SetObjectValue(Module, CurveDistribution);
			UObjectManager::Get().DestroyObject(Current);
			Current = CurveDistribution;
			bConverted = true;
		}
		else if (UDistributionFloatUniform* Uniform = Cast<UDistributionFloatUniform>(Current))
		{
			UDistributionFloatUniformCurve* CurveDistribution = UObjectManager::Get().CreateObject<UDistributionFloatUniformCurve>(Module);
			InitializeFlatCurve(CurveDistribution->MinCurve, Uniform->Min);
			InitializeFlatCurve(CurveDistribution->MaxCurve, Uniform->Max);
			CurveDistribution->bUseExtremes = Uniform->bUseExtremes;
			ObjectProperty->SetObjectValue(Module, CurveDistribution);
			UObjectManager::Get().DestroyObject(Current);
			Current = CurveDistribution;
			bConverted = true;
		}
		else if (UDistributionVectorConstant* Constant = Cast<UDistributionVectorConstant>(Current))
		{
			UDistributionVectorConstantCurve* CurveDistribution = UObjectManager::Get().CreateObject<UDistributionVectorConstantCurve>(Module);
			InitializeFlatCurve(CurveDistribution->XCurve, Constant->Constant.X);
			InitializeFlatCurve(CurveDistribution->YCurve, Constant->Constant.Y);
			InitializeFlatCurve(CurveDistribution->ZCurve, Constant->Constant.Z);
			ObjectProperty->SetObjectValue(Module, CurveDistribution);
			UObjectManager::Get().DestroyObject(Current);
			Current = CurveDistribution;
			bConverted = true;
		}
		else if (UDistributionVectorUniform* Uniform = Cast<UDistributionVectorUniform>(Current))
		{
			UDistributionVectorUniformCurve* CurveDistribution = UObjectManager::Get().CreateObject<UDistributionVectorUniformCurve>(Module);
			InitializeFlatCurve(CurveDistribution->MinXCurve, Uniform->Min.X);
			InitializeFlatCurve(CurveDistribution->MinYCurve, Uniform->Min.Y);
			InitializeFlatCurve(CurveDistribution->MinZCurve, Uniform->Min.Z);
			InitializeFlatCurve(CurveDistribution->MaxXCurve, Uniform->Max.X);
			InitializeFlatCurve(CurveDistribution->MaxYCurve, Uniform->Max.Y);
			InitializeFlatCurve(CurveDistribution->MaxZCurve, Uniform->Max.Z);
			CurveDistribution->bUseExtremes = Uniform->bUseExtremes;
			CurveDistribution->LockedAxes = Uniform->LockedAxes;
			ObjectProperty->SetObjectValue(Module, CurveDistribution);
			UObjectManager::Get().DestroyObject(Current);
			Current = CurveDistribution;
			bConverted = true;
		}

		if (Cast<UDistributionFloatConstantCurve>(Current))
		{
			AddTrack(Property, BaseLabel, EParticleCurveChannel::FloatValue);
		}
		else if (Cast<UDistributionFloatUniformCurve>(Current))
		{
			AddTrack(Property, BaseLabel + " Min", EParticleCurveChannel::FloatMin);
			AddTrack(Property, BaseLabel + " Max", EParticleCurveChannel::FloatMax);
		}
		else if (Cast<UDistributionVectorConstantCurve>(Current))
		{
			AddTrack(Property, BaseLabel + " X", EParticleCurveChannel::VectorX);
			AddTrack(Property, BaseLabel + " Y", EParticleCurveChannel::VectorY);
			AddTrack(Property, BaseLabel + " Z", EParticleCurveChannel::VectorZ);
		}
		else if (Cast<UDistributionVectorUniformCurve>(Current))
		{
			AddTrack(Property, BaseLabel + " Min X", EParticleCurveChannel::VectorMinX);
			AddTrack(Property, BaseLabel + " Min Y", EParticleCurveChannel::VectorMinY);
			AddTrack(Property, BaseLabel + " Min Z", EParticleCurveChannel::VectorMinZ);
			AddTrack(Property, BaseLabel + " Max X", EParticleCurveChannel::VectorMaxX);
			AddTrack(Property, BaseLabel + " Max Y", EParticleCurveChannel::VectorMaxY);
			AddTrack(Property, BaseLabel + " Max Z", EParticleCurveChannel::VectorMaxZ);
		}
	}

	if (bAddedTrack)
	{
		SelectedCurveTrackIndex = static_cast<int32>(CurveTracks.size()) - 1;
		SelectedCurveKeyIndex = -1;
		FitCurveEditorView();
	}
	if (bConverted)
	{
		ParticleSystem->CacheSystemModuleInfo();
		ParticleSystem->BumpVersion();
		MarkDirty();
	}
}

FFloatCurve* FParticleSystemEditorWidget::ResolveCurveTrack(int32 TrackIndex) const
{
	if (TrackIndex < 0 || TrackIndex >= static_cast<int32>(CurveTracks.size()))
	{
		return nullptr;
	}

	const FParticleCurveTrack& Track = CurveTracks[TrackIndex];
	if (!IsValid(Track.Module) || !Track.Module->GetClass())
	{
		return nullptr;
	}

	TArray<const FProperty*> Properties;
	Track.Module->GetClass()->GetPropertyRefs(Properties, false);
	for (const FProperty* Property : Properties)
	{
		if (!Property || !Property->Name || Track.PropertyName != Property->Name)
		{
			continue;
		}
		const FObjectProperty* ObjectProperty = Property->AsObjectProperty();
		UObject* Current = ObjectProperty ? ObjectProperty->GetObjectValue(Track.Module) : nullptr;
		switch (Track.Channel)
		{
		case EParticleCurveChannel::FloatValue:
			if (UDistributionFloatConstantCurve* Distribution = Cast<UDistributionFloatConstantCurve>(Current)) return &Distribution->ConstantCurve;
			break;
		case EParticleCurveChannel::FloatMin:
			if (UDistributionFloatUniformCurve* Distribution = Cast<UDistributionFloatUniformCurve>(Current)) return &Distribution->MinCurve;
			break;
		case EParticleCurveChannel::FloatMax:
			if (UDistributionFloatUniformCurve* Distribution = Cast<UDistributionFloatUniformCurve>(Current)) return &Distribution->MaxCurve;
			break;
		case EParticleCurveChannel::VectorX:
			if (UDistributionVectorConstantCurve* Distribution = Cast<UDistributionVectorConstantCurve>(Current)) return &Distribution->XCurve;
			break;
		case EParticleCurveChannel::VectorY:
			if (UDistributionVectorConstantCurve* Distribution = Cast<UDistributionVectorConstantCurve>(Current)) return &Distribution->YCurve;
			break;
		case EParticleCurveChannel::VectorZ:
			if (UDistributionVectorConstantCurve* Distribution = Cast<UDistributionVectorConstantCurve>(Current)) return &Distribution->ZCurve;
			break;
		case EParticleCurveChannel::VectorMinX:
			if (UDistributionVectorUniformCurve* Distribution = Cast<UDistributionVectorUniformCurve>(Current)) return &Distribution->MinXCurve;
			break;
		case EParticleCurveChannel::VectorMinY:
			if (UDistributionVectorUniformCurve* Distribution = Cast<UDistributionVectorUniformCurve>(Current)) return &Distribution->MinYCurve;
			break;
		case EParticleCurveChannel::VectorMinZ:
			if (UDistributionVectorUniformCurve* Distribution = Cast<UDistributionVectorUniformCurve>(Current)) return &Distribution->MinZCurve;
			break;
		case EParticleCurveChannel::VectorMaxX:
			if (UDistributionVectorUniformCurve* Distribution = Cast<UDistributionVectorUniformCurve>(Current)) return &Distribution->MaxXCurve;
			break;
		case EParticleCurveChannel::VectorMaxY:
			if (UDistributionVectorUniformCurve* Distribution = Cast<UDistributionVectorUniformCurve>(Current)) return &Distribution->MaxYCurve;
			break;
		case EParticleCurveChannel::VectorMaxZ:
			if (UDistributionVectorUniformCurve* Distribution = Cast<UDistributionVectorUniformCurve>(Current)) return &Distribution->MaxZCurve;
			break;
		}
	}
	return nullptr;
}

void FParticleSystemEditorWidget::RemoveInvalidCurveTracks()
{
	for (int32 Index = static_cast<int32>(CurveTracks.size()) - 1; Index >= 0; --Index)
	{
		if (!ResolveCurveTrack(Index))
		{
			CurveTracks.erase(CurveTracks.begin() + Index);
			if (SelectedCurveTrackIndex == Index)
			{
				SelectedCurveTrackIndex = -1;
				SelectedCurveKeyIndex = -1;
			}
			else if (SelectedCurveTrackIndex > Index)
			{
				--SelectedCurveTrackIndex;
			}
		}
	}
}

void FParticleSystemEditorWidget::FitCurveEditorView()
{
	CurveViewMinTime = 0.0f;
	CurveViewMaxTime = 1.0f;
	CurveViewMinValue = 0.0f;
	CurveViewMaxValue = 1.0f;
	bool bHasValue = false;

	for (int32 TrackIndex = 0; TrackIndex < static_cast<int32>(CurveTracks.size()); ++TrackIndex)
	{
		if (!CurveTracks[TrackIndex].bVisible)
		{
			continue;
		}

		FFloatCurve* Curve = ResolveCurveTrack(TrackIndex);
		if (!Curve)
		{
			continue;
		}
		if (Curve->Keys.empty())
		{
			CurveViewMinValue = bHasValue ? (std::min)(CurveViewMinValue, Curve->DefaultValue) : Curve->DefaultValue;
			CurveViewMaxValue = bHasValue ? (std::max)(CurveViewMaxValue, Curve->DefaultValue) : Curve->DefaultValue;
			bHasValue = true;
		}
		for (const FCurveKey& Key : Curve->Keys)
		{
			CurveViewMinTime = (std::min)(CurveViewMinTime, Key.Time);
			CurveViewMaxTime = (std::max)(CurveViewMaxTime, Key.Time);
			CurveViewMinValue = bHasValue ? (std::min)(CurveViewMinValue, Key.Value) : Key.Value;
			CurveViewMaxValue = bHasValue ? (std::max)(CurveViewMaxValue, Key.Value) : Key.Value;
			bHasValue = true;
		}
	}

	if (CurveViewMaxTime <= CurveViewMinTime + 0.001f)
	{
		CurveViewMaxTime = CurveViewMinTime + 1.0f;
	}
	if (!bHasValue)
	{
		CurveViewMinValue = -0.25f;
		CurveViewMaxValue = 1.25f;
	}
	else if (CurveViewMaxValue <= CurveViewMinValue + 0.001f)
	{
		CurveViewMinValue -= 0.5f;
		CurveViewMaxValue += 0.5f;
	}
	else
	{
		const float Pad = (CurveViewMaxValue - CurveViewMinValue) * 0.1f;
		CurveViewMinValue -= Pad;
		CurveViewMaxValue += Pad;
	}
}

void FParticleSystemEditorWidget::RenderEmitterPanel(UParticleSystem* ParticleSystem, const ImVec2& Size)
{
	ImGui::BeginChild("Emitter", Size, true, ImGuiWindowFlags_NoScrollbar);
	DrawPanelHeader("Emitter");

	const ImVec2 CanvasSize = ImGui::GetContentRegionAvail();
	const ImVec2 CanvasMin = ImGui::GetCursorScreenPos();
	ImGui::GetWindowDrawList()->AddRectFilled(
		CanvasMin,
		ImVec2(CanvasMin.x + CanvasSize.x, CanvasMin.y + CanvasSize.y),
		IM_COL32(0, 0, 0, 255));
	ImGui::BeginChild("##EmitterCanvas", CanvasSize, false, ImGuiWindowFlags_HorizontalScrollbar);

		bool bEmitterContextRequested = false;
		bool bClickedEmitterCard = false;
		int32 PendingInsertEmitterIndex = -1;
		int32 PendingDeleteEmitterIndex = -1;
		int32 PendingMoveEmitterSourceIndex = -1;
		int32 PendingMoveEmitterTargetIndex = -1;
		UParticleLODLevel* PendingMoveSourceLODLevel = nullptr;
		UParticleLODLevel* PendingMoveTargetLODLevel = nullptr;
		UParticleModule* PendingMoveModule = nullptr;
	int32 PendingMoveTargetIndex = -1;
	int32 PendingMoveTargetEmitterIndex = -1;
	UParticleLODLevel* PendingDeleteModuleLODLevel = nullptr;
	UParticleModule* PendingDeleteModule = nullptr;
	if (ParticleSystem)
	{
		const TArray<UParticleEmitter*>& Emitters = ParticleSystem->GetEmitters();
		SelectedLODIndex = (std::min)(
			(std::max)(SelectedLODIndex, 0),
			(std::max)(0, static_cast<int32>(ParticleSystem->GetLODDistances().size()) - 1));
		if (SelectedEmitterIndex >= static_cast<int32>(Emitters.size()))
		{
			SelectedEmitterIndex = static_cast<int32>(Emitters.size()) - 1;
			SelectedModule = nullptr;
		}

		constexpr float CardWidth = 180.0f;
		const float CardHeight = (std::max)(1.0f, ImGui::GetContentRegionAvail().y - 2.0f);
		constexpr float CardPad = 3.0f;
		const float CardInnerWidth = CardWidth - CardPad * 2.0f;
		for (int32 EmitterIndex = 0; EmitterIndex < static_cast<int32>(Emitters.size()); ++EmitterIndex)
		{
			UParticleEmitter* Emitter = Emitters[EmitterIndex];
			if (!Emitter)
			{
				continue;
			}

			ImGui::PushID(EmitterIndex);
			const ImVec2 CardMin = ImGui::GetCursorScreenPos();
			const ImVec2 CardMax = ImVec2(CardMin.x + CardWidth, CardMin.y + CardHeight);
			ImGui::GetWindowDrawList()->AddRectFilled(CardMin, CardMax, IM_COL32(52, 52, 52, 255));

			ImGui::BeginChild("##EmitterCard", ImVec2(CardWidth, CardHeight), false, ImGuiWindowFlags_NoScrollbar);

			const bool bSelectedEmitter = SelectedEmitterIndex == EmitterIndex;
			UParticleLODLevel* LODLevel = Emitter->GetLODLevel(SelectedLODIndex);
			const ImVec2 HeaderOuterMin = ImGui::GetCursorScreenPos();
			const ImVec2 HeaderMin(HeaderOuterMin.x + CardPad, HeaderOuterMin.y + CardPad);
			constexpr float HeaderHeight = 58.0f;
			ImDrawList* DrawList = ImGui::GetWindowDrawList();
			DrawList->AddRectFilled(
				HeaderMin,
				ImVec2(HeaderMin.x + CardInnerWidth, HeaderMin.y + HeaderHeight - CardPad),
				bSelectedEmitter ? IM_COL32(122, 92, 110, 255) : IM_COL32(178, 178, 178, 255));
			const ImVec2 MaterialPreviewMin(HeaderMin.x + CardInnerWidth - 56.0f, HeaderMin.y + 4.0f);
			const ImVec2 MaterialPreviewMax(HeaderMin.x + CardInnerWidth - 6.0f, HeaderMin.y + HeaderHeight - 7.0f);
			DrawList->AddRectFilled(MaterialPreviewMin, MaterialPreviewMax, IM_COL32(4, 4, 4, 255));
			if (LODLevel)
			{
				const UParticleModuleRequired* RequiredModule = LODLevel->GetRequiredModule();
				const FString MaterialPath = RequiredModule
					? RequiredModule->MaterialPath.ToString()
					: FString();
				if (!MaterialPath.empty())
				{
					if (UMaterial* Material = FMaterialManager::Get().GetOrCreateMaterial(MaterialPath))
					{
						const ID3D11ShaderResourceView* const* MaterialSRVs = Material->GetCachedSRVs();
						ID3D11ShaderResourceView* DiffuseSRV = MaterialSRVs
							? const_cast<ID3D11ShaderResourceView*>(MaterialSRVs[static_cast<int32>(EMaterialTextureSlot::Diffuse)])
							: nullptr;
						if (DiffuseSRV)
						{
							constexpr float MaterialPreviewPadding = 9.0f;
							DrawList->AddImage(
								reinterpret_cast<ImTextureID>(DiffuseSRV),
								ImVec2(MaterialPreviewMin.x + MaterialPreviewPadding, MaterialPreviewMin.y + MaterialPreviewPadding),
								ImVec2(MaterialPreviewMax.x - MaterialPreviewPadding, MaterialPreviewMax.y - MaterialPreviewPadding));
						}
					}
				}
			}

			ImGui::SetCursorScreenPos(ImVec2(HeaderMin.x + 6.0f, HeaderMin.y + 5.0f));
			FString EmitterName = Emitter->GetEmitterName().ToString();
			if (EmitterName.empty())
			{
				EmitterName = "Particle Emitter";
			}
			ImGui::TextUnformatted(EmitterName.c_str());
			constexpr float SmallCheckboxSize = 13.0f;
			const ImVec2 EmitterCheckboxMin(HeaderMin.x + 7.0f, HeaderMin.y + 33.0f);
			const ImVec2 EmitterCheckboxMax(EmitterCheckboxMin.x + SmallCheckboxSize, EmitterCheckboxMin.y + SmallCheckboxSize);
			if (DrawSmallCheckbox("##EmitterEnabled", EmitterCheckboxMin, Emitter->IsEnabled()))
			{
				bClickedEmitterCard = true;
				Emitter->SetEnabled(!Emitter->IsEnabled());
				ParticleSystem->CacheSystemModuleInfo();
				ParticleSystem->BumpVersion();
				MarkDirty();
			}

			ImGui::SetCursorScreenPos(HeaderOuterMin);
			ImGui::InvisibleButton("##EmitterHeaderHit", ImVec2(CardWidth, HeaderHeight));
			if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !IsMouseInRect(EmitterCheckboxMin, EmitterCheckboxMax))
			{
				bClickedEmitterCard = true;
				SelectedEmitterIndex = EmitterIndex;
				SelectedModule = nullptr;
			}
			if (!IsMouseInRect(EmitterCheckboxMin, EmitterCheckboxMax)
				&& ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceNoPreviewTooltip))
			{
				bClickedEmitterCard = true;
				SelectedEmitterIndex = EmitterIndex;
				SelectedModule = nullptr;

				FParticleEmitterDragPayload Payload;
				Payload.SourceIndex = EmitterIndex;
				Payload.Emitter = Emitter;
				ImGui::SetDragDropPayload("PARTICLE_EMITTER_CARD", &Payload, sizeof(Payload));
				DrawFloatingEmitterPreview(EmitterName.c_str());
				ImGui::EndDragDropSource();
			}

			if (IsMouseInRect(HeaderOuterMin, ImVec2(HeaderOuterMin.x + CardWidth, HeaderOuterMin.y + HeaderHeight)) &&
				ImGui::IsMouseClicked(ImGuiMouseButton_Right))
			{
				SelectedEmitterIndex = EmitterIndex;
				SelectedModule = nullptr;
				bEmitterContextRequested = true;
				ImGui::OpenPopup("##EmitterContext");
			}

			if (ImGui::BeginPopup("##EmitterContext"))
			{
				ReservePopupWidth(150.0f);
				if (ImGui::MenuItem("Insert Emitter Before"))
				{
					PendingInsertEmitterIndex = EmitterIndex;
				}
				if (ImGui::MenuItem("Insert Emitter After"))
				{
					PendingInsertEmitterIndex = EmitterIndex + 1;
				}
				if (ImGui::MenuItem("Delete Emitter"))
				{
					PendingDeleteEmitterIndex = EmitterIndex;
				}
				ImGui::Separator();

				TArray<UClass*> ModuleClasses = EnumerateParticleModuleClasses();
				const char* Categories[] = { "Emitter", "Type Data", "Beam", "Trail", "Spawn", "SubUV", "Event", "Collision", "Module" };
				for (const char* Category : Categories)
				{
					bool bHasCategory = false;
					for (UClass* Class : ModuleClasses)
					{
						if (std::strcmp(GetParticleModuleCategory(Class), Category) == 0)
						{
							bHasCategory = true;
							break;
						}
					}
					if (!bHasCategory || !ImGui::BeginMenu(Category))
					{
						continue;
					}

					for (UClass* Class : ModuleClasses)
					{
						if (std::strcmp(GetParticleModuleCategory(Class), Category) != 0)
						{
							continue;
						}
						const std::string Label = GetParticleModuleDisplayName(Class);
						if (ImGui::MenuItem(Label.c_str()))
						{
							AddParticleModule(ParticleSystem, Emitter, Class);
						}
					}
					ImGui::EndMenu();
				}
				ImGui::EndPopup();
			}

			const float ModuleX = HeaderOuterMin.x + CardPad;
			float ModuleY = HeaderOuterMin.y + HeaderHeight + 8.0f;
			auto RenderModuleRow = [&](UParticleModule* Module, const char* FallbackLabel, ImU32 RowColor, bool bCanDrag, int32 DropIndex)
			{
				if (!Module)
				{
					return;
				}

				const bool bSelectedModule = SelectedModule == Module && bSelectedEmitter;
				const bool bCanShowCurve = HasCurveEditableValues(Module);
				bool bCurveActive = false;
				for (const FParticleCurveTrack& Track : CurveTracks)
				{
					if (Track.Module == Module)
					{
						bCurveActive = true;
						break;
					}
				}
				ImGui::PushID(Module);
				const ImVec2 RowMin(ModuleX, ModuleY);
				constexpr float RowHeight = 22.0f;
				constexpr float CheckboxSize = 13.0f;
				constexpr float CheckboxRightPad = 5.0f;
				const ImVec2 RowMax(RowMin.x + CardInnerWidth, RowMin.y + RowHeight);
				DrawList->AddRectFilled(
					RowMin,
					RowMax,
					bSelectedModule ? IM_COL32(82, 92, 118, 255) : RowColor);

				std::string Label = GetObjectClassDisplayName(Module);
				if (Label.empty())
				{
					Label = FallbackLabel ? FallbackLabel : "Module";
				}
				const ImVec2 TextSize = ImGui::CalcTextSize(Label.c_str());
				DrawList->AddText(
					ImVec2(RowMin.x + 7.0f, RowMin.y + (RowHeight - TextSize.y) * 0.5f),
					ImGui::GetColorU32(ImGuiCol_Text),
					Label.c_str());

				const ImVec2 CurveMin(
					RowMin.x + CardInnerWidth - CheckboxSize - CheckboxRightPad,
					RowMin.y + (RowHeight - CheckboxSize) * 0.5f);
				const ImVec2 CheckboxMin(
					bCanShowCurve ? CurveMin.x - CheckboxSize - 4.0f : CurveMin.x,
					CurveMin.y);
				if (DrawSmallCheckbox("##ModuleEnabled", CheckboxMin, Module->IsEnabled()))
				{
					bClickedEmitterCard = true;
					Module->SetEnabled(!Module->IsEnabled());
					if (ParticleSystem)
					{
						ParticleSystem->CacheSystemModuleInfo();
						ParticleSystem->BumpVersion();
						MarkDirty();
					}
				}
				if (bCanShowCurve && DrawSmallCurveButton("##ModuleCurve", CurveMin, bCurveActive))
				{
					bClickedEmitterCard = true;
					AddModuleCurvesToEditor(ParticleSystem, Module);
				}

				ImGui::SetCursorScreenPos(RowMin);
				const float RowButtonWidth = bCanShowCurve
					? CardInnerWidth - CheckboxSize * 2.0f - CheckboxRightPad - 10.0f
					: CardInnerWidth - CheckboxSize - CheckboxRightPad * 2.0f;
				ImGui::InvisibleButton("##ModuleRow", ImVec2(RowButtonWidth, RowHeight));
				if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
				{
					bClickedEmitterCard = true;
					SelectedEmitterIndex = EmitterIndex;
					SelectedModule = Module;
				}
				if (IsMouseInRect(RowMin, RowMax) && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
				{
					bClickedEmitterCard = true;
					bEmitterContextRequested = true;
					SelectedEmitterIndex = EmitterIndex;
					SelectedModule = Module;
					if (bCanDrag)
					{
						ImGui::OpenPopup("##ModuleContext");
					}
				}
				if (bCanDrag && ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceNoPreviewTooltip))
				{
					FParticleModuleDragPayload Payload;
					Payload.SourceLODLevel = LODLevel;
					Payload.Module = Module;
					ImGui::SetDragDropPayload("PARTICLE_MODULE_ROW", &Payload, sizeof(Payload));
					DrawFloatingModulePreview(Label.c_str(), RowColor);
					ImGui::EndDragDropSource();
				}
				if (ImGui::BeginDragDropTarget())
				{
					const ImVec2 MousePos = ImGui::GetMousePos();
					const bool bDropAfter = MousePos.y > RowMin.y + RowHeight * 0.5f;
					DrawModuleDropIndicator(RowMin, CardInnerWidth, bDropAfter);
					if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload("PARTICLE_MODULE_ROW", ImGuiDragDropFlags_AcceptNoDrawDefaultRect))
					{
						if (Payload->DataSize == sizeof(FParticleModuleDragPayload))
						{
							const FParticleModuleDragPayload DragPayload = *static_cast<const FParticleModuleDragPayload*>(Payload->Data);
							PendingMoveSourceLODLevel = DragPayload.SourceLODLevel;
							PendingMoveTargetLODLevel = LODLevel;
							PendingMoveModule = DragPayload.Module;
							PendingMoveTargetIndex = DropIndex + (bCanDrag && bDropAfter ? 1 : 0);
							PendingMoveTargetEmitterIndex = EmitterIndex;
						}
					}
					ImGui::EndDragDropTarget();
				}
				if (bCanDrag && ImGui::BeginPopup("##ModuleContext"))
				{
					if (ImGui::MenuItem("Delete Module"))
					{
						PendingDeleteModuleLODLevel = LODLevel;
						PendingDeleteModule = Module;
						if (SelectedModule == Module)
						{
							SelectedModule = nullptr;
						}
					}
					ImGui::EndPopup();
				}
				ImGui::PopID();
				ModuleY += RowHeight;
			};

			if (LODLevel)
			{
				const int32 FirstMovableIndex = LODLevel->GetFirstMovableModuleIndex();
				UParticleModuleSpawn* SpawnModule = nullptr;
				const TArray<UParticleModule*>& Modules = LODLevel->GetModules();
				for (UParticleModule* Module : Modules)
				{
					if (UParticleModuleSpawn* Candidate = Cast<UParticleModuleSpawn>(Module))
					{
						SpawnModule = Candidate;
						break;
					}
				}

				RenderModuleRow(LODLevel->GetTypeDataModule(), "Type Data", IM_COL32(81, 117, 176, 255), false, FirstMovableIndex);
				RenderModuleRow(LODLevel->GetRequiredModule(), "Required", IM_COL32(63, 115, 130, 255), false, FirstMovableIndex);
				RenderModuleRow(SpawnModule, "Spawn", IM_COL32(119, 111, 156, 255), false, FirstMovableIndex);

				for (int32 ModuleIndex = 0; ModuleIndex < static_cast<int32>(Modules.size()); ++ModuleIndex)
				{
					UParticleModule* Module = Modules[ModuleIndex];
					if (!Module || Module == SpawnModule)
					{
						continue;
					}
					RenderModuleRow(Module, "Module", IM_COL32(45, 45, 56, 255), true, ModuleIndex);
				}
			}

			if (ModuleY < CardMax.y)
			{
				const ImVec2 EmptyMin(CardMin.x, ModuleY);
				const ImVec2 EmptyMax(CardMax.x, CardMax.y);
				ImGui::SetCursorScreenPos(ImVec2(CardMin.x, ModuleY));
				ImGui::InvisibleButton("##EmitterCardEmptyHit", ImVec2(CardWidth, CardMax.y - ModuleY));
				if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
				{
					bClickedEmitterCard = true;
					SelectedEmitterIndex = EmitterIndex;
					SelectedModule = nullptr;
				}
				if (IsMouseInRect(EmptyMin, EmptyMax) && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
				{
					bClickedEmitterCard = true;
					SelectedEmitterIndex = EmitterIndex;
					SelectedModule = nullptr;
					bEmitterContextRequested = true;
					ImGui::OpenPopup("##EmitterContext");
				}
				if (LODLevel && ImGui::BeginDragDropTarget())
				{
					ImGui::GetWindowDrawList()->AddLine(
						ImVec2(ModuleX, ModuleY),
						ImVec2(ModuleX + CardInnerWidth, ModuleY),
						IM_COL32(255, 170, 32, 255),
						2.0f);
					if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload("PARTICLE_MODULE_ROW", ImGuiDragDropFlags_AcceptNoDrawDefaultRect))
					{
						if (Payload->DataSize == sizeof(FParticleModuleDragPayload))
						{
							const FParticleModuleDragPayload DragPayload = *static_cast<const FParticleModuleDragPayload*>(Payload->Data);
							PendingMoveSourceLODLevel = DragPayload.SourceLODLevel;
							PendingMoveTargetLODLevel = LODLevel;
							PendingMoveModule = DragPayload.Module;
							PendingMoveTargetIndex = static_cast<int32>(LODLevel->GetModules().size());
							PendingMoveTargetEmitterIndex = EmitterIndex;
						}
					}
					ImGui::EndDragDropTarget();
				}
			}

			ImGui::EndChild();
			if (ImGui::BeginDragDropTarget())
			{
				const ImGuiPayload* ActivePayload = ImGui::GetDragDropPayload();
				if (ActivePayload && ActivePayload->IsDataType("PARTICLE_EMITTER_CARD"))
				{
					const bool bDropAfter = ImGui::GetMousePos().x > CardMin.x + CardWidth * 0.5f;
					DrawEmitterDropIndicator(CardMin, CardMax, bDropAfter);
					if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload("PARTICLE_EMITTER_CARD", ImGuiDragDropFlags_AcceptNoDrawDefaultRect))
					{
						if (Payload->DataSize == sizeof(FParticleEmitterDragPayload))
						{
							const FParticleEmitterDragPayload DragPayload = *static_cast<const FParticleEmitterDragPayload*>(Payload->Data);
							PendingMoveEmitterSourceIndex = DragPayload.SourceIndex;
							PendingMoveEmitterTargetIndex = EmitterIndex + (bDropAfter ? 1 : 0);
						}
					}
				}
				ImGui::EndDragDropTarget();
			}
			if (bSelectedEmitter)
			{
				ImGui::GetWindowDrawList()->AddRect(CardMin, CardMax, IM_COL32(255, 218, 61, 255), 0.0f, 0, 1.5f);
			}

			ImGui::PopID();
			if (EmitterIndex + 1 < static_cast<int32>(Emitters.size()))
			{
				ImGui::SameLine(0.0f, 1.0f);
			}
		}

		if (PendingDeleteModuleLODLevel && PendingDeleteModule)
		{
			if (PendingDeleteModuleLODLevel->RemoveModule(PendingDeleteModule))
			{
				ParticleSystem->CacheSystemModuleInfo();
				ParticleSystem->BumpVersion();
				MarkDirty();
			}
		}
		else if (PendingMoveEmitterSourceIndex >= 0 && PendingMoveEmitterTargetIndex >= 0)
		{
			MoveParticleEmitter(ParticleSystem, PendingMoveEmitterSourceIndex, PendingMoveEmitterTargetIndex);
		}
		else if (PendingMoveModule)
		{
			MoveParticleModule(ParticleSystem, PendingMoveSourceLODLevel, PendingMoveTargetLODLevel, PendingMoveModule, PendingMoveTargetIndex);
			if (PendingMoveTargetEmitterIndex >= 0)
			{
				SelectedEmitterIndex = PendingMoveTargetEmitterIndex;
				SelectedModule = PendingMoveModule;
			}
		}
		else if (PendingDeleteEmitterIndex >= 0)
		{
			DeleteParticleEmitter(ParticleSystem, PendingDeleteEmitterIndex);
		}
		else if (PendingInsertEmitterIndex >= 0)
		{
			InsertParticleEmitter(ParticleSystem, PendingInsertEmitterIndex);
		}
	}

	if (!bEmitterContextRequested && ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup) && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
	{
		ImGui::OpenPopup("##EmitterCanvasContext");
	}

	if (!bClickedEmitterCard && !bEmitterContextRequested && ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup) && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
	{
		SelectedEmitterIndex = -1;
		SelectedModule = nullptr;
	}

	if (ImGui::BeginPopup("##EmitterCanvasContext"))
	{
		if (ImGui::MenuItem("Create Particle Emitter"))
		{
			AddParticleEmitter(ParticleSystem);
		}
		ImGui::EndPopup();
	}

	ImGui::EndChild();
	ImGui::EndChild();
}

void FParticleSystemEditorWidget::RenderDetailsPanel(UParticleSystem* ParticleSystem, const ImVec2& Size)
{
	ImGui::BeginChild("Details", Size, true, ImGuiWindowFlags_NoScrollbar);
	DrawPanelHeader("Details");

	ImGui::BeginChild("##ParticleDetailsScroll", ImGui::GetContentRegionAvail(), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);
	ImGui::Indent(2.0f);

	bool bChanged = false;
	UParticleEmitter* SelectedEmitter = nullptr;
	if (ParticleSystem && SelectedEmitterIndex >= 0 && SelectedEmitterIndex < static_cast<int32>(ParticleSystem->GetEmitters().size()))
	{
		SelectedEmitter = ParticleSystem->GetEmitters()[SelectedEmitterIndex];
	}

	if (SelectedEmitter)
	{
		if (SelectedModule)
		{
			bChanged |= RenderObjectPropertiesInline(SelectedModule);
		}
		else
		{
			bChanged |= RenderObjectPropertiesInline(SelectedEmitter);
		}
	}
	else if (ParticleSystem)
	{
		bChanged |= RenderLODDistanceProperties(ParticleSystem);
	}

	if (bChanged && ParticleSystem)
	{
		ParticleSystem->CacheSystemModuleInfo();
		ParticleSystem->BumpVersion();
		MarkDirty();
	}

	ImGui::Unindent(2.0f);
	ImGui::EndChild();
	ImGui::EndChild();
}

bool FParticleSystemEditorWidget::RenderLODDistanceProperties(UParticleSystem* ParticleSystem)
{
	if (!ParticleSystem)
	{
		return false;
	}

	TArray<float>& LODDistances = ParticleSystem->GetLODDistances();
	static std::unordered_map<uint32, bool> OpenStates;
	const uint32 ObjectId = ParticleSystem->GetUUID();
	auto StateIt = OpenStates.find(ObjectId);
	if (StateIt == OpenStates.end())
	{
		StateIt = OpenStates.emplace(ObjectId, true).first;
	}

	bool bChanged = false;
	ImGui::PushID(ParticleSystem);
	if (ImGui::BeginTable("##ParticleLODDistances", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV))
	{
		ImGui::TableSetupColumn("##label", ImGuiTableColumnFlags_WidthFixed, 120.0f);
		ImGui::TableSetupColumn("##value", ImGuiTableColumnFlags_WidthStretch);

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		StateIt->second = DrawDetailsCategoryHeader("LODDistances", StateIt->second);
		ImGui::TableSetColumnIndex(1);
		ImGui::AlignTextToFramePadding();
		ImGui::Text("%zu Array elements", LODDistances.size());

		if (StateIt->second)
		{
			for (int32 Index = 0; Index < static_cast<int32>(LODDistances.size()); ++Index)
			{
				ImGui::PushID(Index);
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::AlignTextToFramePadding();
				ImGui::SetCursorPosX(ImGui::GetCursorPosX() + GetDetailsCategoryLabelOffset());
				ImGui::Text("Index [%d]", Index);
				ImGui::TableSetColumnIndex(1);
				ImGui::SetNextItemWidth(-FLT_MIN);

				const bool bFirstDistance = Index == 0;
				if (bFirstDistance)
				{
					ImGui::BeginDisabled();
				}
				if (ImGui::DragFloat("##Distance", &LODDistances[Index], 0.1f, 0.0f, FLT_MAX, "%.1f"))
				{
					bChanged = true;
				}
				if (bFirstDistance)
				{
					ImGui::EndDisabled();
				}
				ImGui::PopID();
			}
		}

		ImGui::EndTable();
	}
	ImGui::PopID();

	return bChanged;
}

bool FParticleSystemEditorWidget::RenderObjectPropertiesInline(UObject* Object)
{
	// 공용 FPropertyTable 로 렌더 — 파티클 고유 동작 3가지는 컨텍스트 훅으로 주입한다:
	//   (1) bEnabled 숨김(별도 토글로 노출), (2) Distribution(UDistributionFloat/Vector) 동적
	//   클래스 피커 + 재귀 렌더, (3) Events 배열 원소의 이벤트 타입 레이블.
	// Material/StaticMesh/UVectorFieldAsset SoftObject 피커는 FPropertyTable 내장으로 처리된다.
	FPropertyTable::FContext Ctx;

	Ctx.ShouldSkipProperty = [](const FProperty* Property) -> bool
	{
		return Property && Property->Name && std::strcmp(Property->Name, "bEnabled") == 0;
	};

	Ctx.RenderObjectRef = [this](FPropertyValue& Prop, bool& bHandled) -> bool
	{
		bHandled = true;   // 파티클 ObjectRef 는 항상 자체 처리(Distribution 피커 or 이름 표시)
		const FObjectProperty* ObjectValueProperty = Prop.Property ? Prop.Property->AsObjectProperty() : nullptr;
		if (!ObjectValueProperty)
		{
			ImGui::TextDisabled("None");
			return false;
		}

		UObject* Current = ObjectValueProperty->GetObjectValue(Prop.ContainerPtr);
		const FObjectPropertyBase* ObjectProperty = Prop.Property ? Prop.Property->AsObjectPropertyBase() : nullptr;
		UClass* AllowedClass = ObjectProperty ? ObjectProperty->GetAllowedClassType() : nullptr;
		const bool bDistributionObject = AllowedClass
			&& (AllowedClass->IsA(UDistributionFloat::StaticClass()) || AllowedClass->IsA(UDistributionVector::StaticClass()));

		bool bChanged = false;
		if (bDistributionObject)
		{
			FString Preview = Current ? Current->GetClass()->GetName() : FString("None");
			if (ImGui::BeginCombo("##v", Preview.c_str()))
			{
				for (UClass* CandidateClass : UClass::GetAllClasses())
				{
					if (!CandidateClass || !CandidateClass->IsA(AllowedClass) || CandidateClass == AllowedClass)
					{
						continue;
					}
					if (CandidateClass == UDistribution::StaticClass())
					{
						continue;
					}

					const bool bSelected = Current && Current->GetClass() == CandidateClass;
					if (ImGui::Selectable(CandidateClass->GetName(), bSelected))
					{
						UObject* NewObject = FObjectFactory::Get().Create(CandidateClass->GetName(), Prop.Object);
						if (NewObject)
						{
							if (Current && Current != NewObject)
							{
								UObjectManager::Get().DestroyObject(Current);
							}
							ObjectValueProperty->SetObjectValue(Prop.ContainerPtr, NewObject);
							Current = NewObject;
							bChanged = true;
						}
					}
					if (bSelected)
					{
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}

			if (Current)
			{
				ImGui::Indent(8.0f);
				if (RenderObjectPropertiesInline(Current))
				{
					bChanged = true;
				}
				ImGui::Unindent(8.0f);
			}
		}
		else
		{
			if (Current)
			{
				ImGui::TextUnformatted(Current->GetName().c_str());
			}
			else
			{
				ImGui::TextDisabled("None");
			}
		}
		return bChanged;
	};

	Ctx.ArrayElementLabel = [](const FPropertyValue& ArrayProp, int32 Index, void* ElementPtr) -> FString
	{
		const bool bIsEventList = ArrayProp.GetName() && std::strcmp(ArrayProp.GetName(), "Events") == 0;
		const FArrayProperty* ArrayProperty = ArrayProp.Property ? ArrayProp.Property->AsArrayProperty() : nullptr;
		const FProperty* InnerProperty = ArrayProperty ? ArrayProperty->GetInnerProperty() : nullptr;
		char Buffer[128];
		if (bIsEventList && InnerProperty && InnerProperty->GetType() == EPropertyType::Struct)
		{
			const FParticleEventGenerateInfo* EventInfo = static_cast<const FParticleEventGenerateInfo*>(ElementPtr);
			std::snprintf(Buffer, sizeof(Buffer), "Event %d (%s)", Index, GetParticleEventTypeLabel(EventInfo ? EventInfo->Type : EParticleEventType::Any));
		}
		else
		{
			std::snprintf(Buffer, sizeof(Buffer), "Element %d", Index);
		}
		return FString(Buffer);
	};

	return FPropertyTable::RenderObject(Object, Ctx);
}

void FParticleSystemEditorWidget::RenderPanel(const char* Title, const ImVec2& Size)
{
	ImGui::BeginChild(Title, Size, true, ImGuiWindowFlags_NoScrollbar);

	const ImVec2 HeaderPos = ImGui::GetCursorScreenPos();
	const float HeaderWidth = ImGui::GetContentRegionAvail().x;
	constexpr float HeaderHeight = 24.0f;
	ImGui::GetWindowDrawList()->AddRectFilled(
		HeaderPos,
		ImVec2(HeaderPos.x + HeaderWidth, HeaderPos.y + HeaderHeight),
		IM_COL32(45, 47, 52, 255));
	ImGui::SetCursorScreenPos(ImVec2(HeaderPos.x + 8.0f, HeaderPos.y + 4.0f));
	ImGui::TextUnformatted(Title);

	ImGui::SetCursorScreenPos(ImVec2(HeaderPos.x, HeaderPos.y + HeaderHeight + 1.0f));
	ImGui::Dummy(ImGui::GetContentRegionAvail());

	ImGui::EndChild();
}

void FParticleSystemEditorWidget::RenderCurveEditorPanel(UParticleSystem* ParticleSystem, const ImVec2& Size)
{
	RemoveInvalidCurveTracks();

	ImGui::BeginChild("Curve Editor", Size, true, ImGuiWindowFlags_NoScrollbar);
	DrawPanelHeader("Curve Editor");

	const ImVec2 ContentSize = ImGui::GetContentRegionAvail();
	constexpr float TrackListWidth = 192.0f;
	constexpr float TrackRowHeight = 25.0f;
	int32 PendingRemoveTrackIndex = -1;
	bool bPendingRemoveAllCurves = false;

	ImGui::BeginChild("##ParticleCurveTrackList", ImVec2(TrackListWidth, ContentSize.y), false, ImGuiWindowFlags_NoScrollbar);
	const ImVec2 ListMin = ImGui::GetCursorScreenPos();
	ImGui::GetWindowDrawList()->AddRectFilled(
		ListMin,
		ImVec2(ListMin.x + TrackListWidth, ListMin.y + ContentSize.y),
		IM_COL32(85, 85, 85, 255));

	for (int32 TrackIndex = 0; TrackIndex < static_cast<int32>(CurveTracks.size()); ++TrackIndex)
	{
		FParticleCurveTrack& Track = CurveTracks[TrackIndex];
		ImGui::PushID(TrackIndex);
		const ImVec2 RowMin = ImGui::GetCursorScreenPos();
		const ImVec2 RowMax(RowMin.x + TrackListWidth, RowMin.y + TrackRowHeight);
		const ImVec2 VisibilityCheckboxMin(RowMax.x - 18.0f, RowMin.y + (TrackRowHeight - 13.0f) * 0.5f);
		ImGui::InvisibleButton("##CurveTrack", ImVec2(TrackListWidth - 24.0f, TrackRowHeight));
		if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
		{
			SelectedCurveTrackIndex = TrackIndex;
			SelectedCurveKeyIndex = -1;
		}

		ImDrawList* DrawList = ImGui::GetWindowDrawList();
		DrawList->AddRectFilled(
			RowMin,
			RowMax,
			SelectedCurveTrackIndex == TrackIndex ? IM_COL32(164, 164, 164, 255) : IM_COL32(142, 142, 142, 255));
		DrawList->AddLine(ImVec2(RowMin.x, RowMax.y), RowMax, IM_COL32(114, 114, 114, 255));
		DrawList->AddText(
			ImVec2(RowMin.x + 6.0f, RowMin.y + 4.0f),
			IM_COL32(34, 34, 34, 255),
			Track.Label.c_str());
		DrawList->AddRectFilled(
			ImVec2(RowMin.x + 7.0f, RowMin.y + TrackRowHeight - 7.0f),
			ImVec2(RowMin.x + 14.0f, RowMin.y + TrackRowHeight - 2.0f),
			Track.Color);

		if (ImGui::BeginPopupContextItem("##ParticleCurveTrackContext"))
		{
			if (ImGui::MenuItem("Remove Curve"))
			{
				PendingRemoveTrackIndex = TrackIndex;
			}
			if (ImGui::MenuItem("Remove All Curves"))
			{
				bPendingRemoveAllCurves = true;
			}
			ImGui::EndPopup();
		}

		if (DrawSmallCheckbox("##CurveTrackVisible", VisibilityCheckboxMin, Track.bVisible))
		{
			Track.bVisible = !Track.bVisible;
			if (!Track.bVisible && SelectedCurveTrackIndex == TrackIndex)
			{
				SelectedCurveKeyIndex = -1;
				bCurveKeyDragging = false;
			}
			FitCurveEditorView();
		}
		ImGui::SetCursorScreenPos(ImVec2(RowMin.x, RowMax.y));
		ImGui::PopID();
	}
	ImGui::EndChild();

	if (bPendingRemoveAllCurves)
	{
		CurveTracks.clear();
		SelectedCurveTrackIndex = -1;
		SelectedCurveKeyIndex = -1;
		bCurveKeyDragging = false;
		bCurvePanning = false;
		FitCurveEditorView();
	}
	else if (PendingRemoveTrackIndex >= 0 && PendingRemoveTrackIndex < static_cast<int32>(CurveTracks.size()))
	{
		CurveTracks.erase(CurveTracks.begin() + PendingRemoveTrackIndex);
		if (SelectedCurveTrackIndex == PendingRemoveTrackIndex)
		{
			SelectedCurveTrackIndex = -1;
			SelectedCurveKeyIndex = -1;
		}
		else if (SelectedCurveTrackIndex > PendingRemoveTrackIndex)
		{
			--SelectedCurveTrackIndex;
		}
		FitCurveEditorView();
	}

	ImGui::SameLine(0.0f, 1.0f);
	ImGui::BeginChild("##ParticleCurveGraph", ImVec2(0.0f, ContentSize.y), false, ImGuiWindowFlags_NoScrollbar);
	const ImVec2 CanvasMin = ImGui::GetCursorScreenPos();
	const ImVec2 CanvasSize = ImGui::GetContentRegionAvail();
	const ImVec2 CanvasMax(CanvasMin.x + CanvasSize.x, CanvasMin.y + CanvasSize.y);
	ImGui::InvisibleButton("##ParticleCurveCanvas", CanvasSize);
	const bool bCanvasHovered = ImGui::IsItemHovered();
	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	const ImGuiIO& IO = ImGui::GetIO();
	DrawList->AddRectFilled(CanvasMin, CanvasMax, IM_COL32(55, 55, 55, 255));

	for (int32 GridIndex = 0; GridIndex <= 10; ++GridIndex)
	{
		const float Alpha = static_cast<float>(GridIndex) / 10.0f;
		const float GridX = CanvasMin.x + CanvasSize.x * Alpha;
		const float GridY = CanvasMin.y + CanvasSize.y * Alpha;
		const ImU32 Color = IM_COL32(154, 154, 154, 255);
		DrawList->AddLine(ImVec2(GridX, CanvasMin.y), ImVec2(GridX, CanvasMax.y), Color);
		DrawList->AddLine(ImVec2(CanvasMin.x, GridY), ImVec2(CanvasMax.x, GridY), Color);

		if (GridIndex < 10)
		{
			char TimeLabel[32];
			std::snprintf(TimeLabel, sizeof(TimeLabel), "%.2f", CurveViewMinTime + (CurveViewMaxTime - CurveViewMinTime) * Alpha);
			DrawList->AddText(ImVec2(GridX + 3.0f, CanvasMax.y - ImGui::GetTextLineHeight() - 3.0f), IM_COL32(235, 235, 235, 255), TimeLabel);
		}
		if (GridIndex % 2 == 0)
		{
			char ValueLabel[32];
			std::snprintf(ValueLabel, sizeof(ValueLabel), "%.2f", CurveViewMaxValue - (CurveViewMaxValue - CurveViewMinValue) * Alpha);
			DrawList->AddText(ImVec2(CanvasMin.x + 3.0f, GridY + 2.0f), IM_COL32(235, 235, 235, 255), ValueLabel);
		}
	}

	int32 HoveredTrackIndex = -1;
	int32 HoveredKeyIndex = -1;
	constexpr float KeyRadius = 6.0f;
	for (int32 TrackIndex = 0; TrackIndex < static_cast<int32>(CurveTracks.size()); ++TrackIndex)
	{
		if (!CurveTracks[TrackIndex].bVisible)
		{
			continue;
		}

		FFloatCurve* Curve = ResolveCurveTrack(TrackIndex);
		if (!Curve)
		{
			continue;
		}

		const ImU32 Color = CurveTracks[TrackIndex].Color;
		constexpr int32 SampleCount = 128;
		ImVec2 Previous = ParticleCurveToScreen(
			CurveViewMinTime,
			Curve->Evaluate(CurveViewMinTime),
			CurveViewMinTime, CurveViewMaxTime, CurveViewMinValue, CurveViewMaxValue,
			CanvasMin, CanvasMax);
		for (int32 SampleIndex = 1; SampleIndex < SampleCount; ++SampleIndex)
		{
			const float Alpha = static_cast<float>(SampleIndex) / static_cast<float>(SampleCount - 1);
			const float Time = CurveViewMinTime + (CurveViewMaxTime - CurveViewMinTime) * Alpha;
			const ImVec2 Current = ParticleCurveToScreen(
				Time, Curve->Evaluate(Time),
				CurveViewMinTime, CurveViewMaxTime, CurveViewMinValue, CurveViewMaxValue,
				CanvasMin, CanvasMax);
			DrawList->AddLine(Previous, Current, Color, TrackIndex == SelectedCurveTrackIndex ? 2.0f : 1.5f);
			Previous = Current;
		}

		for (int32 KeyIndex = 0; KeyIndex < static_cast<int32>(Curve->Keys.size()); ++KeyIndex)
		{
			const FCurveKey& Key = Curve->Keys[KeyIndex];
			const ImVec2 Point = ParticleCurveToScreen(
				Key.Time, Key.Value,
				CurveViewMinTime, CurveViewMaxTime, CurveViewMinValue, CurveViewMaxValue,
				CanvasMin, CanvasMax);
			const float DX = IO.MousePos.x - Point.x;
			const float DY = IO.MousePos.y - Point.y;
			if (bCanvasHovered && DX * DX + DY * DY <= KeyRadius * KeyRadius)
			{
				HoveredTrackIndex = TrackIndex;
				HoveredKeyIndex = KeyIndex;
			}
			const bool bSelected = TrackIndex == SelectedCurveTrackIndex && KeyIndex == SelectedCurveKeyIndex;
			DrawList->AddRectFilled(
				ImVec2(Point.x - 4.0f, Point.y - 4.0f),
				ImVec2(Point.x + 4.0f, Point.y + 4.0f),
				bSelected ? IM_COL32(255, 245, 120, 255) : Color);
		}
	}

	bool bChanged = false;
	if (bCanvasHovered && IO.MouseWheel != 0.0f)
	{
		float CursorTime = 0.0f;
		float CursorValue = 0.0f;
		ParticleScreenToCurve(IO.MousePos, CurveViewMinTime, CurveViewMaxTime, CurveViewMinValue, CurveViewMaxValue, CanvasMin, CanvasMax, CursorTime, CursorValue);
		const float Zoom = IO.MouseWheel > 0.0f ? 0.85f : 1.1764706f;
		CurveViewMinTime = CursorTime + (CurveViewMinTime - CursorTime) * Zoom;
		CurveViewMaxTime = CursorTime + (CurveViewMaxTime - CursorTime) * Zoom;
		CurveViewMinValue = CursorValue + (CurveViewMinValue - CursorValue) * Zoom;
		CurveViewMaxValue = CursorValue + (CurveViewMaxValue - CursorValue) * Zoom;
	}

	if (HoveredTrackIndex >= 0 && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
	{
		SelectedCurveTrackIndex = HoveredTrackIndex;
		SelectedCurveKeyIndex = HoveredKeyIndex;
		bCurveKeyDragging = true;
		bCurvePanning = false;
	}
	else if (bCanvasHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
	{
		SelectedCurveKeyIndex = -1;
		bCurvePanning = true;
	}

	const auto ResolveVisibleSelectedCurve = [&]() -> FFloatCurve*
	{
		if (SelectedCurveTrackIndex < 0 || SelectedCurveTrackIndex >= static_cast<int32>(CurveTracks.size())
			|| !CurveTracks[SelectedCurveTrackIndex].bVisible)
		{
			return nullptr;
		}
		return ResolveCurveTrack(SelectedCurveTrackIndex);
	};
	FFloatCurve* SelectedCurve = ResolveVisibleSelectedCurve();
	if (bCurveKeyDragging && SelectedCurve && SelectedCurveKeyIndex >= 0
		&& SelectedCurveKeyIndex < static_cast<int32>(SelectedCurve->Keys.size())
		&& ImGui::IsMouseDragging(ImGuiMouseButton_Left))
	{
		FCurveKey& Key = SelectedCurve->Keys[SelectedCurveKeyIndex];
		ParticleScreenToCurve(IO.MousePos, CurveViewMinTime, CurveViewMaxTime, CurveViewMinValue, CurveViewMaxValue, CanvasMin, CanvasMax, Key.Time, Key.Value);
		Key.Time = (std::max)(0.0f, Key.Time);
		if (SelectedCurveKeyIndex > 0)
		{
			Key.Time = (std::max)(Key.Time, SelectedCurve->Keys[SelectedCurveKeyIndex - 1].Time + 0.001f);
		}
		if (SelectedCurveKeyIndex + 1 < static_cast<int32>(SelectedCurve->Keys.size()))
		{
			Key.Time = (std::min)(Key.Time, SelectedCurve->Keys[SelectedCurveKeyIndex + 1].Time - 0.001f);
		}
		SelectedCurve->AutoSetTangents();
		bChanged = true;
	}
	if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
	{
		bCurveKeyDragging = false;
	}

	if (bCurvePanning && !bCurveKeyDragging
		&& ImGui::IsMouseDragging(ImGuiMouseButton_Left))
	{
		const float DeltaTime = IO.MouseDelta.x / (std::max)(1.0f, CanvasSize.x) * (CurveViewMaxTime - CurveViewMinTime);
		const float DeltaValue = IO.MouseDelta.y / (std::max)(1.0f, CanvasSize.y) * (CurveViewMaxValue - CurveViewMinValue);
		CurveViewMinTime -= DeltaTime;
		CurveViewMaxTime -= DeltaTime;
		CurveViewMinValue += DeltaValue;
		CurveViewMaxValue += DeltaValue;
	}
	if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
	{
		bCurvePanning = false;
	}

	if (HoveredTrackIndex >= 0 && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
	{
		SelectedCurveTrackIndex = HoveredTrackIndex;
		SelectedCurveKeyIndex = HoveredKeyIndex;
		ImGui::OpenPopup("##ParticleCurveKeyContext");
	}
	else if (bCanvasHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right) && SelectedCurve)
	{
		ParticleScreenToCurve(IO.MousePos, CurveViewMinTime, CurveViewMaxTime, CurveViewMinValue, CurveViewMaxValue, CanvasMin, CanvasMax, PendingCurveTime, PendingCurveValue);
		ImGui::OpenPopup("##ParticleCurveCanvasContext");
	}

	if (ImGui::BeginPopup("##ParticleCurveKeyContext"))
	{
		SelectedCurve = ResolveVisibleSelectedCurve();
		if (SelectedCurve && SelectedCurveKeyIndex >= 0 && SelectedCurveKeyIndex < static_cast<int32>(SelectedCurve->Keys.size()))
		{
			bool bDeletedKey = false;
			if (ImGui::MenuItem("Delete Key"))
			{
				SelectedCurve->Keys.erase(SelectedCurve->Keys.begin() + SelectedCurveKeyIndex);
				SelectedCurveKeyIndex = -1;
				bChanged = true;
				bDeletedKey = true;
			}
			if (!bDeletedKey)
			{
				ImGui::Separator();
				if (ImGui::MenuItem("Linear"))
				{
					SelectedCurve->Keys[SelectedCurveKeyIndex].InterpMode = ECurveInterpMode::Linear;
					bChanged = true;
				}
				if (ImGui::MenuItem("Cubic"))
				{
					SelectedCurve->Keys[SelectedCurveKeyIndex].InterpMode = ECurveInterpMode::Cubic;
					SelectedCurve->AutoSetTangents();
					bChanged = true;
				}
			}
		}
		ImGui::EndPopup();
	}

	if (ImGui::BeginPopup("##ParticleCurveCanvasContext"))
	{
		SelectedCurve = ResolveVisibleSelectedCurve();
		if (SelectedCurve && ImGui::MenuItem("Add Key"))
		{
			SelectedCurve->AddKey((std::max)(0.0f, PendingCurveTime), PendingCurveValue);
			SelectedCurve->SortKeys();
			SelectedCurve->AutoSetTangents();
			SelectedCurveKeyIndex = -1;
			bChanged = true;
		}
		if (ImGui::MenuItem("Fit To Curves"))
		{
			FitCurveEditorView();
		}
		ImGui::EndPopup();
	}

	if (HoveredTrackIndex >= 0)
	{
		FFloatCurve* HoverCurve = ResolveCurveTrack(HoveredTrackIndex);
		if (HoverCurve && HoveredKeyIndex >= 0 && HoveredKeyIndex < static_cast<int32>(HoverCurve->Keys.size()))
		{
			const FCurveKey& Key = HoverCurve->Keys[HoveredKeyIndex];
			ImGui::SetTooltip("%s\nTime %.3f  Value %.3f", CurveTracks[HoveredTrackIndex].Label.c_str(), Key.Time, Key.Value);
		}
	}

	if (bChanged && ParticleSystem)
	{
		ParticleSystem->CacheSystemModuleInfo();
		MarkDirty();
	}

	ImGui::EndChild();
	ImGui::EndChild();
}

void FParticleSystemEditorWidget::RenderViewportPanel(const ImVec2& Size)
{
	ImGui::BeginChild("Viewport", Size, true, ImGuiWindowFlags_NoScrollbar);

	const ImVec2 HeaderPos = ImGui::GetCursorScreenPos();
	const float HeaderWidth = ImGui::GetContentRegionAvail().x;
	constexpr float HeaderHeight = 24.0f;
	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	DrawList->AddRectFilled(
		HeaderPos,
		ImVec2(HeaderPos.x + HeaderWidth, HeaderPos.y + HeaderHeight),
		IM_COL32(45, 47, 52, 255));
	ImGui::SetCursorScreenPos(ImVec2(HeaderPos.x + 8.0f, HeaderPos.y + 4.0f));
	ImGui::TextUnformatted("Viewport");

	const ImVec2 ViewportPos(HeaderPos.x, HeaderPos.y + HeaderHeight + 1.0f);
	const ImVec2 ViewportSize(
		(std::max)(1.0f, ImGui::GetContentRegionAvail().x),
		(std::max)(1.0f, ImGui::GetContentRegionAvail().y - HeaderHeight - 1.0f));

	ImGui::SetCursorScreenPos(ViewportPos);
	ViewportClient.SetViewportRect(ViewportPos.x, ViewportPos.y, ViewportSize.x, ViewportSize.y);

	FViewport* VP = ViewportClient.GetViewport();
	if (VP && ViewportSize.x > 0.0f && ViewportSize.y > 0.0f)
	{
		VP->RequestResize(static_cast<uint32>(ViewportSize.x), static_cast<uint32>(ViewportSize.y));

		DrawList->AddRectFilled(
			ViewportPos,
			ImVec2(ViewportPos.x + ViewportSize.x, ViewportPos.y + ViewportSize.y),
			IM_COL32(0, 0, 0, 255));

		if (VP->GetSRV())
		{
			ImGui::Image(reinterpret_cast<ImTextureID>(VP->GetSRV()), ViewportSize);
		}
		else
		{
			ImGui::InvisibleButton("##ParticleViewportFallback", ViewportSize);
		}

		FSlateApplication::Get().SetViewportImGuiHovered(&ViewportClient, ImGui::IsItemHovered());
		DrawViewportAxisOverlay(DrawList, ViewportPos, ViewportSize);
		DrawViewportStatsOverlay(DrawList, ViewportPos, ViewportSize);

		ImGui::SetCursorScreenPos(ImVec2(ViewportPos.x + 8.0f, ViewportPos.y + 8.0f));
		RenderViewportMenus();
	}
	else
	{
		DrawList->AddRectFilled(
			ViewportPos,
			ImVec2(ViewportPos.x + ViewportSize.x, ViewportPos.y + ViewportSize.y),
			IM_COL32(0, 0, 0, 255));
		ImGui::Dummy(ViewportSize);
	}

	ImGui::EndChild();
}

void FParticleSystemEditorWidget::RenderViewportMenus()
{
	ImVec2 ViewPopupPos;
	ImVec2 TimePopupPos;
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 3.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);
	ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(48, 48, 50, 220));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(64, 64, 67, 245));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(72, 72, 74, 255));

	if (ImGui::Button("View"))
	{
		ImGui::OpenPopup("##ParticlePreviewViewOptions");
	}
	ViewPopupPos = ImVec2(ImGui::GetItemRectMin().x, ImGui::GetItemRectMax().y + 1.0f);
	ImGui::SameLine(0.0f, 6.0f);
	if (ImGui::Button("Time"))
	{
		ImGui::OpenPopup("##ParticlePreviewTimeOptions");
	}
	TimePopupPos = ImVec2(ImGui::GetItemRectMin().x, ImGui::GetItemRectMax().y + 1.0f);

	ImGui::PopStyleColor(3);
	ImGui::PopStyleVar(2);

	ImGui::SetNextWindowPos(ViewPopupPos, ImGuiCond_Always);
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x, 3.0f));
	if (ImGui::BeginPopup("##ParticlePreviewViewOptions"))
	{
		ImGui::Dummy(ImVec2(0.0f, 1.0f));

		ImGui::Checkbox("Particle Count", &bShowParticleCount);
		ImGui::Checkbox("Particle Time", &bShowParticleTime);
		ImGui::Checkbox("Particle Memory", &bShowParticleMemory);

		bool bShowGrid = ViewportClient.GetRenderOptions().ShowFlags.bGrid;
		if (ImGui::Checkbox("Grid", &bShowGrid))
		{
			ViewportClient.GetRenderOptions().ShowFlags.bGrid = bShowGrid;
		}
		if (ImGui::Checkbox("Floor", &bShowPreviewFloor) && PreviewFloorActor)
		{
			PreviewFloorActor->SetVisible(bShowPreviewFloor);
		}
		ImGui::Checkbox("Vector Field", &ViewportClient.GetRenderOptions().bParticleVectorFieldDebug);

		FViewportRenderOptions& RenderOptions = ViewportClient.GetRenderOptions();
		ImGui::Checkbox("Bloom", &RenderOptions.ShowFlags.bBloom);
		if (RenderOptions.ShowFlags.bBloom)
		{
			ImGui::SliderFloat("Bloom Threshold", &RenderOptions.BloomThreshold, 0.0f, 10.0f, "%.2f");
			ImGui::SliderFloat("Bloom Intensity", &RenderOptions.BloomIntensity, 0.0f, 3.0f, "%.2f");
			ImGui::SliderFloat("Bloom Radius", &RenderOptions.BloomBlurRadius, 0.25f, 4.0f, "%.2f");
			ImGui::SliderFloat("Bloom Soft Knee", &RenderOptions.BloomSoftKnee, 0.0f, 1.0f, "%.2f");
		}
		ImGui::Checkbox("Gamma Correction", &RenderOptions.ShowFlags.bGammaCorrection);
		if (RenderOptions.ShowFlags.bGammaCorrection)
		{
			ImGui::SliderFloat("Exposure", &RenderOptions.Exposure, 0.0f, 5.0f, "%.2f");
			ImGui::SliderFloat("Gamma", &RenderOptions.Gamma, 1.0f, 3.0f, "%.2f");
		}

		ImGui::Dummy(ImVec2(0.0f, 1.0f));
		ImGui::EndPopup();
	}
	ImGui::PopStyleVar();

	ImGui::SetNextWindowPos(TimePopupPos, ImGuiCond_Always);
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x, 3.0f));
	if (ImGui::BeginPopup("##ParticlePreviewTimeOptions"))
	{
		ImGui::Dummy(ImVec2(0.0f, 1.0f));

		ImGui::Checkbox("Pause", &PreviewPlayback.bPaused);
		if (ImGui::Checkbox("Loop", &PreviewPlayback.bLooping) && PreviewPlayback.bLooping && PreviewParticleComponent)
		{
			PreviewParticleComponent->ResetSystem();
			PreviewPlayback.CurrentTime = 0.0f;
			PreviewPlayback.bComplete = false;
		}
		ImGui::SetNextItemWidth(120.0f);
		ImGui::SliderFloat("Speed", &PreviewPlayback.PlayRate, 0.0f, 1.0f, "%.2fx");

		ImGui::Dummy(ImVec2(0.0f, 1.0f));
		ImGui::EndPopup();
	}
	ImGui::PopStyleVar();
}

float FParticleSystemEditorWidget::CalculatePreviewDuration() const
{
	const UParticleSystem* ParticleSystem = Cast<UParticleSystem>(EditedObject);
	if (!ParticleSystem)
	{
		return 0.0f;
	}

	float Duration = 0.0f;
	for (const UParticleEmitter* Emitter : ParticleSystem->GetEmitters())
	{
		if (!Emitter || !Emitter->IsEnabled())
		{
			continue;
		}

		const UParticleLODLevel* LODLevel = Emitter->GetLODLevel(0);
		const UParticleModuleRequired* Required = LODLevel ? LODLevel->GetRequiredModule() : nullptr;
		if (Required)
		{
			Duration = (std::max)(Duration, Required->EmitterDuration);
		}
	}
	return Duration;
}

void FParticleSystemEditorWidget::DrawViewportStatsOverlay(ImDrawList* DrawList, const ImVec2& ViewportPos, const ImVec2& ViewportSize) const
{
	if (!DrawList || !PreviewParticleComponent || (!bShowParticleCount && !bShowParticleTime && !bShowParticleMemory))
	{
		return;
	}

	const ImU32 NameColor = IM_COL32(245, 245, 245, 255);
	const ImU32 CountColor = IM_COL32(255, 32, 194, 255);
	const ImU32 EventColor = IM_COL32(255, 190, 70, 255);
	const ImU32 TimeColor = IM_COL32(60, 215, 255, 255);
	const float LineHeight = ImGui::GetTextLineHeight();
	int32 LineCount = bShowParticleMemory ? 1 : 0;
	if (bShowParticleCount || bShowParticleTime)
	{
		LineCount += static_cast<int32>(PreviewParticleComponent->GetEmitterInstances().size());
	}
	if (bShowParticleCount)
	{
		++LineCount;
	}
	float CurrentY = ViewportPos.y + ViewportSize.y - 10.0f - LineHeight * static_cast<float>(LineCount);

	if (bShowParticleCount)
	{
		char EventBuffer[96] = {};
		std::snprintf(EventBuffer, sizeof(EventBuffer), "Events this frame: %zu", PreviewParticleComponent->GetParticleEvents().size());
		const ImVec2 TextSize = ImGui::CalcTextSize(EventBuffer);
		const ImVec2 TextPos(ViewportPos.x + ViewportSize.x - TextSize.x - 10.0f, CurrentY);
		DrawList->AddText(TextPos, EventColor, EventBuffer);
		CurrentY += LineHeight;
	}

	if (bShowParticleCount || bShowParticleTime)
	{
		for (const FParticleEmitterInstance* Instance : PreviewParticleComponent->GetEmitterInstances())
		{
			if (!Instance)
			{
				continue;
			}

			char NameBuffer[128] = {};
			char CountBuffer[64] = {};
			char EventBuffer[96] = {};
			char TimeBuffer[128] = {};
			const FString Name = Instance->GetTemplateName().empty() ? FString("Emitter") : Instance->GetTemplateName();
			std::snprintf(NameBuffer, sizeof(NameBuffer), "%s: ", Name.c_str());
			if (bShowParticleCount)
			{
				std::snprintf(CountBuffer, sizeof(CountBuffer), "%d / %d",
					Instance->GetActiveParticleCount(), Instance->GetMaxActiveParticleCount());
				if (Instance->GetLastProcessedParticleEventCount() > 0
					|| Instance->GetLastEventReceiverSpawnRequestCount() > 0)
				{
					std::snprintf(EventBuffer, sizeof(EventBuffer), " / recv %d, spawned %d/%d",
						Instance->GetLastProcessedParticleEventCount(),
						Instance->GetLastEventReceiverSpawnedCount(),
						Instance->GetLastEventReceiverSpawnRequestCount());
				}
			}
			if (bShowParticleTime)
			{
				std::snprintf(TimeBuffer, sizeof(TimeBuffer), "%.4f s / %.4f s",
					Instance->GetEmitterTime(), PreviewPlayback.AccumulatedTime);
			}

			const char* Separator = bShowParticleCount && bShowParticleTime ? " / " : "";
			const float LineWidth =
				ImGui::CalcTextSize(NameBuffer).x
				+ ImGui::CalcTextSize(CountBuffer).x
				+ ImGui::CalcTextSize(EventBuffer).x
				+ ImGui::CalcTextSize(Separator).x
				+ ImGui::CalcTextSize(TimeBuffer).x;
			ImVec2 TextPos(ViewportPos.x + ViewportSize.x - LineWidth - 10.0f, CurrentY);
			DrawList->AddText(TextPos, NameColor, NameBuffer);
			TextPos.x += ImGui::CalcTextSize(NameBuffer).x;
			if (bShowParticleCount)
			{
				DrawList->AddText(TextPos, CountColor, CountBuffer);
				TextPos.x += ImGui::CalcTextSize(CountBuffer).x;
				if (EventBuffer[0] != '\0')
				{
					DrawList->AddText(TextPos, EventColor, EventBuffer);
					TextPos.x += ImGui::CalcTextSize(EventBuffer).x;
				}
			}
			if (Separator[0] != '\0')
			{
				DrawList->AddText(TextPos, NameColor, Separator);
				TextPos.x += ImGui::CalcTextSize(Separator).x;
			}
			if (bShowParticleTime)
			{
				DrawList->AddText(TextPos, TimeColor, TimeBuffer);
			}
			CurrentY += LineHeight;
		}
	}

	if (bShowParticleMemory)
	{
		char Buffer[256] = {};
		const double TemplateKBytes = static_cast<double>(PreviewParticleComponent->GetTemplateMemoryBytes()) / 1024.0;
		const double InstanceKBytes = static_cast<double>(PreviewParticleComponent->GetInstanceMemoryBytes()) / 1024.0;
		std::snprintf(Buffer, sizeof(Buffer), "Template: %.2f KByte / Instance: %.2f KByte", TemplateKBytes, InstanceKBytes);
		const ImVec2 TextSize = ImGui::CalcTextSize(Buffer);
		const ImVec2 TextPos(ViewportPos.x + ViewportSize.x - TextSize.x - 10.0f, CurrentY);
		DrawList->AddText(TextPos, NameColor, Buffer);
	}
}

void FParticleSystemEditorWidget::DrawViewportAxisOverlay(ImDrawList* DrawList, const ImVec2& ViewportPos, const ImVec2& ViewportSize) const
{
	if (!DrawList)
	{
		return;
	}

	FMinimalViewInfo POV;
	if (!ViewportClient.GetCameraView(POV))
	{
		return;
	}

	FRotator AxisRotation = POV.Rotation;
	AxisRotation.Yaw = 180.0f - AxisRotation.Yaw;
	const FVector CameraRight = AxisRotation.GetRightVector();
	const FVector CameraUp = AxisRotation.GetUpVector();

	const ImVec2 Origin(
		ViewportPos.x + 42.0f,
		ViewportPos.y + ViewportSize.y - 42.0f);
	const float AxisLength = 28.0f;

	struct FAxisSpec
	{
		FVector WorldAxis;
		ImU32 Color;
		const char* Label;
	};

	const FAxisSpec Axes[] = {
		{ FVector(-1.0f, 0.0f, 0.0f), IM_COL32(255, 45, 30, 255), "X" },
		{ FVector(0.0f, 1.0f, 0.0f), IM_COL32(60, 220, 45, 255), "Y" },
		{ FVector(0.0f, 0.0f, 1.0f), IM_COL32(60, 145, 255, 255), "Z" },
	};

	for (const FAxisSpec& Axis : Axes)
	{
		const float ScreenX = -Axis.WorldAxis.Dot(CameraRight);
		const float ScreenY = -Axis.WorldAxis.Dot(CameraUp);
		ImVec2 End(Origin.x + ScreenX * AxisLength, Origin.y + ScreenY * AxisLength);

		DrawList->AddLine(Origin, End, Axis.Color, 2.0f);
		DrawList->AddCircleFilled(End, 2.5f, Axis.Color);
		DrawList->AddText(ImVec2(End.x + 3.0f, End.y - 7.0f), Axis.Color, Axis.Label);
	}
}
