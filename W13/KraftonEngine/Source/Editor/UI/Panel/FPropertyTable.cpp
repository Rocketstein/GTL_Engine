#include "Editor/UI/Panel/FPropertyTable.h"

#include "ImGui/imgui.h"

#include "Component/ActorComponent.h"
#include "Component/Primitive/ClothComponent.h"
#include "Component/Primitive/SkeletalMeshComponent.h"
#include "Component/SceneComponent.h"
#include "GameFramework/AActor.h"
#include "Core/Property/ClassProperty.h"
#include "Core/Property/ArrayProperty.h"
#include "Core/Property/NumericProperty.h"
#include "Core/Property/ObjectProperty.h"
#include "Core/Property/StructProperty.h"
#include "Core/Property/SoftObjectProperty.h"
#include "Object/FName.h"
#include "Object/Reflection/UClass.h"
#include "Object/Reflection/UStruct.h"
#include "Math/Transform.h"
#include "Core/Logging/Log.h"
#include "Asset/AssetRegistry.h"
#include "Materials/MaterialManager.h"
#include "Mesh/MeshManager.h"
#include "Mesh/Static/StaticMesh.h"
#include "Mesh/Skeletal/SkeletalMesh.h"
#include "Mesh/Importer/MeshImportOptions.h"
#include "Resource/ResourceManager.h"
#include "Lua/LuaScriptManager.h"
#include "Editor/UI/Asset/Mesh/MeshEditorWidget.h"
#include "Editor/UI/Dialog/FbxImportOptionsDialog.h"
#include "Editor/UI/ContentBrowser/ContentBrowserElement.h"
#include "Platform/Paths.h"
#include "Runtime/Engine.h"

#include <Windows.h>
#include <commdlg.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <utility>

// FTransform 회전 편집용 Euler 캐시. 회전을 FQuat 로 보관하는 Transform 은 매 프레임
// Quat→Euler 재추출 시 Pitch=±90°(짐벌 락)에서 Roll 이 0 으로 접혀 편집이 "틱 걸린다".
// 편집 중인 Euler(도, {Roll,Pitch,Yaw})를 FTransform 포인터별로 캐시해, quat 이 외부에서
// 바뀌지 않는 한 캐시값을 표시한다(라운드트립으로 인한 값 소실 방지).
static std::unordered_map<const void*, FVector> GTransformEulerCache;

// =====================================================================================
// 내부 헬퍼 — 원래 EditorPropertyWidget.cpp 익명 네임스페이스에 있던 순수 로직을 이관.
// =====================================================================================
namespace
{
	bool IsFbxFilePath(const FString& Path)
	{
		std::filesystem::path FilePath(FPaths::ToWide(Path));
		std::wstring Extension = FilePath.extension().wstring();
		std::transform(Extension.begin(), Extension.end(), Extension.begin(), ::towlower);
		return Extension == L".fbx";
	}

	FString RemoveExtension(const FString& Path)
	{
		size_t DotPos = Path.find_last_of('.');
		return DotPos == FString::npos ? Path : Path.substr(0, DotPos);
	}

	FString GetStemFromPath(const FString& Path)
	{
		size_t SlashPos = Path.find_last_of("/\\");
		FString FileName = (SlashPos == FString::npos) ? Path : Path.substr(SlashPos + 1);
		return RemoveExtension(FileName);
	}

	FString OpenStaticMeshFileDialog()
	{
		wchar_t FilePath[MAX_PATH] = {};
		OPENFILENAMEW Ofn = {};
		Ofn.lStructSize = sizeof(Ofn);
		Ofn.hwndOwner = nullptr;
		Ofn.lpstrFilter = L"Static Mesh Files (*.obj;*.fbx)\0*.obj;*.fbx\0OBJ Files (*.obj)\0*.obj\0FBX Files (*.fbx)\0*.fbx\0All Files (*.*)\0*.*\0";
		Ofn.lpstrFile = FilePath;
		Ofn.nMaxFile = MAX_PATH;
		Ofn.lpstrTitle = L"Import Static Mesh";
		Ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
		if (GetOpenFileNameW(&Ofn))
		{
			std::filesystem::path AbsPath = std::filesystem::path(FilePath).lexically_normal();
			std::filesystem::path RootPath = std::filesystem::path(FPaths::RootDir());
			std::filesystem::path RelPath = AbsPath.lexically_relative(RootPath);
			if (RelPath.empty() || RelPath.wstring().starts_with(L".."))
			{
				return FPaths::ToUtf8(AbsPath.generic_wstring());
			}
			return FPaths::ToUtf8(RelPath.generic_wstring());
		}
		return FString();
	}

	FString OpenFbxFileDialog()
	{
		wchar_t FilePath[MAX_PATH] = {};
		OPENFILENAMEW Ofn = {};
		Ofn.lStructSize = sizeof(Ofn);
		Ofn.hwndOwner = nullptr;
		Ofn.lpstrFilter = L"FBX Files (*.fbx)\0*.fbx\0All Files (*.*)\0*.*\0";
		Ofn.lpstrFile = FilePath;
		Ofn.nMaxFile = MAX_PATH;
		Ofn.lpstrTitle = L"Import FBX Mesh";
		Ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
		if (GetOpenFileNameW(&Ofn))
		{
			std::filesystem::path AbsPath = std::filesystem::path(FilePath).lexically_normal();
			std::filesystem::path RootPath = std::filesystem::path(FPaths::RootDir());
			std::filesystem::path RelPath = AbsPath.lexically_relative(RootPath);
			if (RelPath.empty() || RelPath.wstring().starts_with(L".."))
			{
				return FPaths::ToUtf8(AbsPath.generic_wstring());
			}
			return FPaths::ToUtf8(RelPath.generic_wstring());
		}
		return FString();
	}

	const char* GetPropertyDisplayName(const FPropertyValue& Prop)
	{
		return Prop.GetDisplayName();
	}

	const FString* FindPropertyMetadata(const FPropertyValue& Prop, const FString& Key)
	{
		const TMap<FString, FString>& Metadata = Prop.GetMetadata();
		auto It = Metadata.find(Key);
		return It != Metadata.end() ? &It->second : nullptr;
	}

	bool IsTruthyMetadataValue(const FString& Value)
	{
		return Value.empty() || Value == "true" || Value == "1" || Value == "yes";
	}

	bool HasTruthyPropertyMetadata(const FPropertyValue& Prop, const FString& Key)
	{
		if (const FString* Value = FindPropertyMetadata(Prop, Key))
		{
			return IsTruthyMetadataValue(*Value);
		}
		return false;
	}

	FString GetAssetTypeMetadata(const FPropertyValue& Prop)
	{
		if (const FString* AssetType = FindPropertyMetadata(Prop, "assettype"))
		{
			return *AssetType;
		}
		if (const FString* AllowedClass = FindPropertyMetadata(Prop, "allowedclass"))
		{
			return *AllowedClass;
		}
		return {};
	}

	UClass* GetAllowedClassMetadata(const FPropertyValue& Prop)
	{
		if (const FString* AllowedClass = FindPropertyMetadata(Prop, "allowedclass"))
		{
			return UClass::FindByName(AllowedClass->c_str());
		}
		return nullptr;
	}

	FString MakePropertyPath(const FString& ParentPath, const char* PropertyName)
	{
		if (!PropertyName || PropertyName[0] == '\0')
		{
			return ParentPath;
		}
		if (ParentPath.empty())
		{
			return PropertyName;
		}
		return ParentPath + "." + PropertyName;
	}

	FString MakeArrayElementPath(const FString& ArrayPath, int32 ArrayIndex)
	{
		return ArrayPath + "[" + std::to_string(ArrayIndex) + "]";
	}

	AActor* GetPropertyOwnerActor(const FPropertyValue& Prop)
	{
		if (AActor* Actor = Cast<AActor>(Prop.Object))
		{
			return Actor;
		}
		if (UActorComponent* Component = Cast<UActorComponent>(Prop.Object))
		{
			return Component->GetOwner();
		}
		return nullptr;
	}

	TArray<UObject*> GetOwnerObjectReferenceChoices(const FPropertyValue& Prop, UClass* AllowedClass)
	{
		TArray<UObject*> Choices;
		if (!AllowedClass)
		{
			return Choices;
		}

		AActor* OwnerActor = GetPropertyOwnerActor(Prop);
		if (!OwnerActor)
		{
			return Choices;
		}

		if (OwnerActor->GetClass()->IsA(AllowedClass))
		{
			Choices.push_back(OwnerActor);
		}

		for (UActorComponent* Component : OwnerActor->GetComponents())
		{
			if (Component && Component->GetClass()->IsA(AllowedClass))
			{
				Choices.push_back(Component);
			}
		}

		return Choices;
	}

	USkeletalMeshComponent* FindSkeletalMeshComponentForBoneProperty(const FPropertyValue& Prop)
	{
		if (UClothComponent* ClothComponent = Cast<UClothComponent>(Prop.Object))
		{
			if (USkeletalMeshComponent* MasterPoseComponent = ClothComponent->GetMasterPoseComponent())
			{
				return MasterPoseComponent;
			}
		}

		if (USceneComponent* SceneComponent = Cast<USceneComponent>(Prop.Object))
		{
			for (USceneComponent* Parent = SceneComponent->GetParent(); Parent; Parent = Parent->GetParent())
			{
				if (USkeletalMeshComponent* ParentMesh = Cast<USkeletalMeshComponent>(Parent))
				{
					return ParentMesh;
				}
			}
		}

		AActor* OwnerActor = GetPropertyOwnerActor(Prop);
		if (!OwnerActor)
		{
			return nullptr;
		}

		for (UActorComponent* Component : OwnerActor->GetComponents())
		{
			if (USkeletalMeshComponent* MeshComponent = Cast<USkeletalMeshComponent>(Component))
			{
				return MeshComponent;
			}
		}

		return nullptr;
	}

	TArray<FString> GetBoneNameChoices(const FPropertyValue& Prop)
	{
		TArray<FString> Names;
		Names.push_back("None");

		USkeletalMeshComponent* MeshComponent = FindSkeletalMeshComponentForBoneProperty(Prop);
		USkeletalMesh* Mesh = MeshComponent ? MeshComponent->GetSkeletalMesh() : nullptr;
		FSkeletalMesh* MeshAsset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
		if (!MeshAsset)
		{
			return Names;
		}

		for (const FBone& Bone : MeshAsset->Bones)
		{
			if (!Bone.Name.empty())
			{
				Names.push_back(Bone.Name);
			}
		}

		return Names;
	}

	FString GetObjectReferenceChoiceLabel(const UObject* Object)
	{
		if (!Object)
		{
			return "None";
		}

		FString Label = Object->GetFName().ToString();
		if (Label.empty())
		{
			Label = Object->GetClass()->GetName();
		}
		return Label;
	}

	bool RenderClassPropertyWidget(FPropertyValue& Prop)
	{
		const FClassProperty* ClassProperty = Prop.Property ? Prop.Property->AsClassProperty() : nullptr;
		if (!ClassProperty || !Prop.GetValuePtr())
		{
			return false;
		}

		UClass* AllowedClass = GetAllowedClassMetadata(Prop);
		UClass* CurrentClass = ClassProperty->GetClassValue(Prop.ContainerPtr);
		FString Preview = CurrentClass ? CurrentClass->GetName() : FString("None");
		bool bChanged = false;

		if (ImGui::BeginCombo("##Value", Preview.c_str()))
		{
			const bool bSelectedNone = CurrentClass == nullptr;
			if (ImGui::Selectable("None", bSelectedNone))
			{
				ClassProperty->SetClassValue(Prop.ContainerPtr, nullptr);
				bChanged = true;
			}
			if (bSelectedNone)
			{
				ImGui::SetItemDefaultFocus();
			}

			TArray<UClass*>& Classes = UClass::GetAllClasses();
			for (UClass* Candidate : Classes)
			{
				if (!Candidate)
				{
					continue;
				}
				if (AllowedClass && !Candidate->IsA(AllowedClass))
				{
					continue;
				}

				const bool bSelected = Candidate == CurrentClass;
				if (ImGui::Selectable(Candidate->GetName(), bSelected))
				{
					ClassProperty->SetClassValue(Prop.ContainerPtr, Candidate);
					bChanged = true;
				}
				if (bSelected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}

			ImGui::EndCombo();
		}

		return bChanged;
	}

	bool RenderEnumPropertyWidget(FPropertyValue& Prop)
	{
		const FEnum* EnumType = Prop.GetEnumType();
		if (!EnumType || !EnumType->GetNames() || EnumType->GetCount() == 0 || !Prop.GetValuePtr())
		{
			return false;
		}

		bool bChanged = false;
		const char** EnumNames = EnumType->GetNames();
		const uint32 EnumCount = EnumType->GetCount();
		const uint32 EnumSize = EnumType->GetSize();
		int32 Val = 0;
		memcpy(&Val, Prop.GetValuePtr(), EnumSize);
		const char* Preview = ((uint32)Val < EnumCount) ? EnumNames[Val] : "Unknown";
		if (ImGui::BeginCombo("##Value", Preview))
		{
			for (uint32 i = 0; i < EnumCount; ++i)
			{
				bool bSelected = (Val == (int32)i);
				if (ImGui::Selectable(EnumNames[i], bSelected))
				{
					int32 NewVal = (int32)i;
					memcpy(Prop.GetValuePtr(), &NewVal, EnumSize);
					bChanged = true;
				}
				if (bSelected) ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		return bChanged;
	}
}

// =====================================================================================
// 공개 API
// =====================================================================================
void FPropertyTable::DispatchPostEditChange(
	const FPropertyValue& Prop,
	EPropertyChangeType ChangeType,
	int32 ArrayIndex,
	const FString& PropertyPath,
	const char* OverridePropertyName,
	const char* OverrideDisplayName)
{
	if (!Prop.Object)
	{
		return;
	}

	FPropertyChangedEvent Event;
	Event.Object = Prop.Object;
	Event.Property = Prop.Property;
	Event.PropertyName = OverridePropertyName ? OverridePropertyName : Prop.GetName();
	Event.DisplayName = OverrideDisplayName ? OverrideDisplayName : GetPropertyDisplayName(Prop);
	Event.PropertyPath = PropertyPath.empty() ? Prop.GetName() : PropertyPath;
	Event.Type = Prop.GetType();
	Event.ChangeType = ChangeType;
	Event.ArrayIndex = ArrayIndex;
	Prop.Object->PostEditChangeProperty(Event);
}

namespace
{
	// SoftObjectRef 에셋 피커 — 모든 호출자(액터/파티클/애니/물리)가 공유하는 내장 구현.
	// assettype 메타데이터로 분기: Material / SkeletalMesh / StaticMesh / UVectorFieldAsset /
	// UAnimSequence / UAnimGraphAsset / UParticleSystem / LuaAnimScript / Script. 임포트 대화상자
	// 상태는 함수-로컬 static 으로 보관(위젯 멤버 의존 제거).
	bool RenderSoftObjectWidget(FPropertyValue& Prop)
	{
		void* ValuePtr = Prop.GetValuePtr();
		if (!ValuePtr)
		{
			return false;
		}

		bool bChanged = false;
		const FSoftObjectProperty* SoftProperty = Prop.Property ? Prop.Property->AsSoftObjectProperty() : nullptr;
		FString AssetType = SoftProperty ? SoftProperty->GetAssetType() : GetAssetTypeMetadata(Prop);
		FString* Val = SoftProperty ? nullptr : static_cast<FString*>(ValuePtr);
		FString CurrentPath = SoftProperty ? SoftProperty->GetPath(Prop.ContainerPtr) : *Val;
		auto SetPath = [&](const FString& NewPath)
		{
			if (SoftProperty) { SoftProperty->SetPath(Prop.ContainerPtr, NewPath); }
			else              { *Val = NewPath; }
			CurrentPath = NewPath;
		};

		// 에셋 레지스트리 타입명으로 콤보를 그리는 공용 헬퍼.
		auto RegistryCombo = [&](const char* ComboId, const char* TypeName)
		{
			FString Preview = (CurrentPath.empty() || CurrentPath == "None") ? "None" : GetStemFromPath(CurrentPath);
			if (ImGui::BeginCombo(ComboId, Preview.c_str()))
			{
				const bool bSelectedNone = (CurrentPath.empty() || CurrentPath == "None");
				if (ImGui::Selectable("None", bSelectedNone)) { SetPath("None"); bChanged = true; }
				if (bSelectedNone) ImGui::SetItemDefaultFocus();
				const TArray<FAssetListItem>& Items = FAssetRegistry::ListByTypeName(TypeName);
				for (const FAssetListItem& Item : Items)
				{
					const bool bSelected = (CurrentPath == Item.FullPath);
					if (ImGui::Selectable(Item.DisplayName.c_str(), bSelected)) { SetPath(Item.FullPath); bChanged = true; }
					if (bSelected) ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
		};

		if (AssetType == "Material")
		{
			FString Preview = (CurrentPath.empty() || CurrentPath == "None") ? "None" : CurrentPath;
			if (ImGui::BeginCombo("##Material", Preview.c_str()))
			{
				const bool bSelectedNone = (CurrentPath == "None" || CurrentPath.empty());
				if (ImGui::Selectable("None", bSelectedNone)) { SetPath("None"); bChanged = true; }
				if (bSelectedNone) ImGui::SetItemDefaultFocus();
				const TArray<FMaterialAssetListItem>& MatFiles = FMaterialManager::Get().GetAvailableMaterialFiles();
				for (const FMaterialAssetListItem& Item : MatFiles)
				{
					const bool bSelected = (CurrentPath == Item.FullPath);
					if (ImGui::Selectable(Item.DisplayName.c_str(), bSelected)) { SetPath(Item.FullPath); bChanged = true; }
					if (bSelected) ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("MaterialContentItem"))
				{
					FContentItem ContentItem = *reinterpret_cast<const FContentItem*>(payload->Data);
					SetPath(FPaths::ToUtf8(ContentItem.Path.lexically_relative(FPaths::RootDir()).generic_wstring()));
					bChanged = true;
				}
				ImGui::EndDragDropTarget();
			}
			return bChanged;
		}

		if (AssetType == "Script")
		{
			char Buf[256];
			strncpy_s(Buf, sizeof(Buf), CurrentPath.c_str(), _TRUNCATE);
			if (ImGui::InputText("##Value", Buf, sizeof(Buf))) { SetPath(Buf); bChanged = true; }
			if (ImGui::Button("Edit Script"))
			{
				if (!FLuaScriptManager::OpenOrCreateScript(CurrentPath))
				{
					UE_LOG("Failed to open script file: %s", CurrentPath.c_str());
				}
			}
			return bChanged;
		}

		if (AssetType == "SkeletalMesh")
		{
			static FFbxSceneImportDialogState s_SkelFbxDialog;
			FString Preview = (CurrentPath.empty() || CurrentPath == "None") ? "None" : GetStemFromPath(CurrentPath);

			float ButtonWidth = ImGui::CalcTextSize("Import FBX").x + ImGui::GetStyle().FramePadding.x * 2.0f;
			float Spacing = ImGui::GetStyle().ItemSpacing.x;
			ImGui::SetNextItemWidth(-(ButtonWidth + Spacing));
			if (ImGui::BeginCombo("##SkeletalMesh", Preview.c_str()))
			{
				const bool bSelectedNone = (CurrentPath == "None" || CurrentPath.empty());
				if (ImGui::Selectable("None", bSelectedNone)) { SetPath("None"); bChanged = true; }
				if (bSelectedNone) ImGui::SetItemDefaultFocus();
				const TArray<FAssetListItem>& MeshFiles = FMeshManager::GetAvailableSkeletalMeshFiles();
				for (const FAssetListItem& Item : MeshFiles)
				{
					const bool bSelected = (CurrentPath == Item.FullPath);
					if (ImGui::Selectable(Item.DisplayName.c_str(), bSelected)) { SetPath(Item.FullPath); bChanged = true; }
					if (bSelected) ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}

			ImGui::SameLine();
			ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMax().x - ButtonWidth);
			if (ImGui::Button("Import FBX"))
			{
				FString FbxPath = OpenFbxFileDialog();
				if (!FbxPath.empty()) { FFbxImportOptionsDialog::BeginSceneImport(s_SkelFbxDialog, FbxPath); }
			}

			FFbxSceneImportRequest       Request;
			const EFbxImportDialogResult DialogResult = FFbxImportOptionsDialog::RenderSceneImportPopup(
				"Skeletal FBX Import Options", s_SkelFbxDialog, Request);
			if (DialogResult == EFbxImportDialogResult::Submitted)
			{
				FFbxSceneImportResult Result;
				if (FMeshManager::ImportFbxScene(Request, GEngine->GetRenderer().GetFD3DDevice().GetDevice(), Result))
				{
					if (Result.SkeletalMesh) { SetPath(Result.SkeletalMesh->GetAssetPathFileName()); bChanged = true; }
					FMeshManager::ScanMeshAssets();
					FFbxImportOptionsDialog::RequestClose(s_SkelFbxDialog);
				}
				else
				{
					s_SkelFbxDialog.Error = "FBX import failed. See the engine log for details.";
				}
			}
			return bChanged;
		}

		if (AssetType == "UAnimSequence")    { RegistryCombo("##AnimSequence",  "UAnimSequence");    return bChanged; }
		if (AssetType == "UAnimGraphAsset")  { RegistryCombo("##AnimGraphAsset", "UAnimGraphAsset");  return bChanged; }
		if (AssetType == "UParticleSystem")  { RegistryCombo("##ParticleSystem", "UParticleSystem");  return bChanged; }
		if (AssetType == "UVectorFieldAsset"){ RegistryCombo("##VectorField",    "UVectorFieldAsset"); return bChanged; }
		if (AssetType == "ClothAsset" || AssetType == "UClothAsset") { RegistryCombo("##ClothAsset", "UClothAsset"); return bChanged; }

		if (AssetType == "LuaAnimScript")
		{
			FString Preview = (CurrentPath.empty() || CurrentPath == "None") ? "None" : GetStemFromPath(CurrentPath);
			float ButtonWidth = ImGui::CalcTextSize("Edit Script").x + ImGui::GetStyle().FramePadding.x * 2.0f;
			float Spacing = ImGui::GetStyle().ItemSpacing.x;
			ImGui::SetNextItemWidth(-(ButtonWidth + Spacing));
			if (ImGui::BeginCombo("##LuaAnimScript", Preview.c_str()))
			{
				const bool bSelectedNone = (CurrentPath == "None" || CurrentPath.empty());
				if (ImGui::Selectable("None", bSelectedNone)) { SetPath("None"); bChanged = true; }
				if (bSelectedNone) ImGui::SetItemDefaultFocus();
				const TArray<FAssetListItem>& LuaFiles = FAssetRegistry::ListByTypeName("LuaAnimScript");
				for (const FAssetListItem& Item : LuaFiles)
				{
					const bool bSelected = (CurrentPath == Item.FullPath);
					if (ImGui::Selectable(Item.DisplayName.c_str(), bSelected)) { SetPath(Item.FullPath); bChanged = true; }
					if (bSelected) ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
			ImGui::SameLine();
			if (ImGui::Button("Edit Script"))
			{
				if (!FLuaScriptManager::OpenOrCreateScript(CurrentPath))
				{
					UE_LOG("Failed to open script file: %s", CurrentPath.c_str());
				}
			}
			return bChanged;
		}

		// 기본: StaticMesh 콤보 + FBX/OBJ 임포트(스킨드 메시 처리 모달 포함).
		static FString s_PendingSrcPath;
		static int32   s_PendingPolicy = 0;
		{
			FString Preview = (CurrentPath.empty() || CurrentPath == "None") ? "None" : GetStemFromPath(CurrentPath);
			float ButtonWidth = ImGui::CalcTextSize("Import").x + ImGui::GetStyle().FramePadding.x * 2.0f;
			float Spacing = ImGui::GetStyle().ItemSpacing.x;
			ImGui::SetNextItemWidth(-(ButtonWidth + Spacing));
			if (ImGui::BeginCombo("##Mesh", Preview.c_str()))
			{
				const bool bSelectedNone = (CurrentPath == "None");
				if (ImGui::Selectable("None", bSelectedNone)) { SetPath("None"); bChanged = true; }
				if (bSelectedNone) ImGui::SetItemDefaultFocus();
				const TArray<FAssetListItem>& MeshFiles = FMeshManager::GetAvailableStaticMeshFiles();
				for (const FAssetListItem& Item : MeshFiles)
				{
					const bool bSelected = (CurrentPath == Item.FullPath);
					if (ImGui::Selectable(Item.DisplayName.c_str(), bSelected)) { SetPath(Item.FullPath); bChanged = true; }
					if (bSelected) ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}

			ImGui::SameLine();
			ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMax().x - ButtonWidth);
			if (ImGui::Button("Import"))
			{
				FString MeshPath = OpenStaticMeshFileDialog();
				if (!MeshPath.empty())
				{
					if (IsFbxFilePath(MeshPath))
					{
						s_PendingSrcPath = MeshPath;
						s_PendingPolicy = FImportOptions::Default().StaticFbxSkinnedMeshPolicy == EStaticFbxSkinnedMeshPolicy::ImportBindPoseAsStatic ? 1 : 0;
						ImGui::OpenPopup("Static FBX Import Options");
					}
					else
					{
						ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
						if (FMeshManager::LoadStaticMesh(MeshPath, Device))
						{
							SetPath(FMeshManager::GetStaticMeshBinaryFilePath(MeshPath));
							bChanged = true;
						}
					}
				}
			}

			if (ImGui::BeginPopupModal("Static FBX Import Options", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
			{
				ImGui::TextUnformatted("Skinned mesh handling");
				ImGui::RadioButton("Skip skinned meshes", &s_PendingPolicy, 0);
				ImGui::RadioButton("Import bind pose as static mesh", &s_PendingPolicy, 1);
				if (ImGui::Button("Import"))
				{
					FImportOptions Options = FImportOptions::Default();
					Options.StaticFbxSkinnedMeshPolicy = s_PendingPolicy == 1
						? EStaticFbxSkinnedMeshPolicy::ImportBindPoseAsStatic
						: EStaticFbxSkinnedMeshPolicy::Skip;
					ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
					if (FMeshManager::LoadStaticMesh(s_PendingSrcPath, Options, Device))
					{
						SetPath(FMeshManager::GetStaticMeshBinaryFilePath(s_PendingSrcPath));
						bChanged = true;
					}
					s_PendingSrcPath.clear();
					ImGui::CloseCurrentPopup();
				}
				ImGui::SameLine();
				if (ImGui::Button("Cancel")) { s_PendingSrcPath.clear(); ImGui::CloseCurrentPopup(); }
				ImGui::EndPopup();
			}
		}
		return bChanged;
	}

	bool RenderStructPropertyWidget(FPropertyValue& Prop, const FPropertyTable::FContext& Ctx, bool bDispatchChange, const FString& PropertyPath)
	{
		const FStructProperty* StructProperty = Prop.Property ? Prop.Property->AsStructProperty() : nullptr;
		if (!StructProperty || !StructProperty->GetStructType() || !Prop.GetValuePtr())
		{
			return false;
		}

		bool bChanged = false;
		ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_DefaultOpen |
			ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding;

		bool bOpen = ImGui::TreeNodeEx("##StructValue", Flags, "");
		if (bOpen)
		{
			TArray<FPropertyValue> ChildProps;
			Prop.GetStructChildren(ChildProps);

			ImGui::Indent(8.0f);

			for (int32 ci = 0; ci < (int32)ChildProps.size(); ++ci)
			{
				ImGui::PushID(ci);

				FPropertyValue& ChildProp = ChildProps[ci];
				ImGui::AlignTextToFramePadding();
				ImGui::TextUnformatted(GetPropertyDisplayName(ChildProp));
				ImGui::SameLine(120.0f);
				ImGui::SetNextItemWidth(-1);

				const FString ChildPath = MakePropertyPath(PropertyPath, ChildProp.GetName());
				int32 ChildIdx = ci;
				if (FPropertyTable::RenderValue(ChildProps, ChildIdx, Ctx, bDispatchChange, ChildPath))
				{
					bChanged = true;
				}
				ImGui::PopID();
			}

			ImGui::Unindent(8.0f);
			ImGui::TreePop();
		}

		return bChanged;
	}

	bool RenderArrayPropertyWidget(FPropertyValue& Prop, const FPropertyTable::FContext& Ctx, bool bDispatchChange, const FString& PropertyPath)
	{
		const FArrayProperty* ArrayProperty = Prop.Property ? Prop.Property->AsArrayProperty() : nullptr;
		void* ArrayPtr = Prop.GetValuePtr();
		if (!ArrayProperty || !ArrayPtr || !ArrayProperty->GetArrayOps() || !ArrayProperty->GetInnerProperty())
		{
			return false;
		}

		const FArrayProperty::FArrayOps* Ops = ArrayProperty->GetArrayOps();
		const FProperty* InnerProperty = ArrayProperty->GetInnerProperty();
		if (!Ops->GetNum || !Ops->GetElementPtr)
		{
			return false;
		}

		bool bChanged = false;
		size_t Num = Ops->GetNum(ArrayPtr);
		const bool bEditFixedSize = HasTruthyPropertyMetadata(Prop, "editfixedsize") || HasTruthyPropertyMetadata(Prop, "fixedsize");

		if (!bEditFixedSize && Ops->InsertDefault && ImGui::Button("+"))
		{
			Ops->InsertDefault(ArrayPtr, Num);
			bChanged = true;
			if (bDispatchChange)
			{
				FPropertyTable::DispatchPostEditChange(Prop, EPropertyChangeType::ArrayAdd, static_cast<int32>(Num), MakeArrayElementPath(PropertyPath, static_cast<int32>(Num)));
			}
			Num = Ops->GetNum(ArrayPtr);
		}

		for (int32 ElemIdx = 0; ElemIdx < static_cast<int32>(Num); ++ElemIdx)
		{
			void* ElementPtr = Ops->GetElementPtr(ArrayPtr, static_cast<size_t>(ElemIdx));
			if (!ElementPtr)
			{
				continue;
			}

			ImGui::PushID(ElemIdx);

			FString ElementName = Ctx.ArrayElementLabel
				? Ctx.ArrayElementLabel(Prop, ElemIdx, ElementPtr)
				: ("Element " + std::to_string(ElemIdx));
			const FString ElementPath = MakeArrayElementPath(PropertyPath, ElemIdx);

			if (!bEditFixedSize && Ops->RemoveAt && ImGui::Button("-"))
			{
				Ops->RemoveAt(ArrayPtr, static_cast<size_t>(ElemIdx));
				bChanged = true;
				if (bDispatchChange)
				{
					FPropertyTable::DispatchPostEditChange(Prop, EPropertyChangeType::ArrayRemove, ElemIdx, ElementPath, ElementName.c_str(), ElementName.c_str());
				}
				ImGui::PopID();
				break;
			}

			if (!bEditFixedSize && Ops->RemoveAt)
			{
				ImGui::SameLine();
			}
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted(ElementName.c_str());
			ImGui::SameLine(120.0f);
			ImGui::SetNextItemWidth(-1);

			FPropertyValue ElementValue;
			ElementValue.Object = Prop.Object;
			ElementValue.Property = InnerProperty;
			ElementValue.ContainerPtr = ElementPtr;

			TArray<FPropertyValue> ElementProps;
			ElementProps.push_back(ElementValue);
			int32 ElementPropIndex = 0;
			if (FPropertyTable::RenderValue(ElementProps, ElementPropIndex, Ctx, false, ElementPath))
			{
				bChanged = true;
				if (bDispatchChange)
				{
					FPropertyTable::DispatchPostEditChange(Prop, EPropertyChangeType::ValueSet, ElemIdx, ElementPath, ElementName.c_str(), ElementName.c_str());
				}
			}

			ImGui::PopID();
		}

		return bChanged;
	}
}

bool FPropertyTable::RenderValue(TArray<FPropertyValue>& Props, int32& Index, const FContext& Ctx, bool bDispatchChange, const FString& PropertyPath)
{
	ImGui::PushID(Index);
	FPropertyValue& Prop = Props[Index];
	bool bChanged = false;
	const FString EffectivePropertyPath = PropertyPath.empty() ? FString(Prop.GetName()) : PropertyPath;
	const bool bReadOnly = Prop.Property && (Prop.Property->Flags & PF_ReadOnly) != 0;
	if (bReadOnly)
	{
		ImGui::BeginDisabled();
	}

	switch (Prop.GetType())
	{
	case EPropertyType::Bool:
	{
		bool* Val = static_cast<bool*>(Prop.GetValuePtr());
		if (!Val)
		{
			break;
		}

		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(1.0f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.07f, 0.07f, 0.07f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.18f, 0.18f, 0.18f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(0.055f, 0.525f, 1.0f, 1.0f));

		bChanged = ImGui::Checkbox("##Value", Val);

		ImGui::PopStyleColor(3);
		ImGui::PopStyleVar();
		break;
	}
	case EPropertyType::ByteBool:
	{
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(1.0f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.07f, 0.07f, 0.07f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.18f, 0.18f, 0.18f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(0.055f, 0.525f, 1.0f, 1.0f));

		uint8* Val = static_cast<uint8*>(Prop.GetValuePtr());
		bool bVal = (*Val != 0);
		if (ImGui::Checkbox("##Value", &bVal))
		{
			*Val = bVal ? 1 : 0;
			bChanged = true;
		}

		ImGui::PopStyleColor(3);
		ImGui::PopStyleVar();
		break;
	}
	case EPropertyType::Int:
	{
		const FNumericProperty* NumericProperty = Prop.Property ? Prop.Property->AsNumericProperty() : nullptr;
		int32* Val = static_cast<int32*>(Prop.GetValuePtr());
		const float Min = NumericProperty ? NumericProperty->GetMin() : Prop.GetMin();
		const float Max = NumericProperty ? NumericProperty->GetMax() : Prop.GetMax();
		const float Speed = NumericProperty ? NumericProperty->GetSpeed() : Prop.GetSpeed();
		if (Min != 0.0f || Max != 0.0f)
			bChanged = ImGui::DragInt("##Value", Val, Speed, (int32)Min, (int32)Max);
		else
			bChanged = ImGui::DragInt("##Value", Val, Speed);
		break;
	}
	case EPropertyType::Float:
	{
		const FNumericProperty* NumericProperty = Prop.Property ? Prop.Property->AsNumericProperty() : nullptr;
		float* Val = static_cast<float*>(Prop.GetValuePtr());
		const float Min = NumericProperty ? NumericProperty->GetMin() : Prop.GetMin();
		const float Max = NumericProperty ? NumericProperty->GetMax() : Prop.GetMax();
		const float Speed = NumericProperty ? NumericProperty->GetSpeed() : Prop.GetSpeed();
		if (Min != 0.0f || Max != 0.0f)
			bChanged = ImGui::DragFloat("##Value", Val, Speed, Min, Max, "%.4f");
		else
			bChanged = ImGui::DragFloat("##Value", Val, Speed);
		break;
	}
	case EPropertyType::Vec3:
	{
		float* Val = static_cast<float*>(Prop.GetValuePtr());
		bChanged = ImGui::DragFloat3("##Value", Val, Prop.GetSpeed());
		break;
	}
	case EPropertyType::Rotator:
	{
		// FRotator 메모리 레이아웃 [Pitch,Yaw,Roll] → UI X=Roll(X축), Y=Pitch(Y축), Z=Yaw(Z축)
		FRotator* Rot = static_cast<FRotator*>(Prop.GetValuePtr());
		float RotXYZ[3] = { Rot->Roll, Rot->Pitch, Rot->Yaw };
		bChanged = ImGui::DragFloat3("##Value", RotXYZ, Prop.GetSpeed());
		if (bChanged)
		{
			Rot->Roll = RotXYZ[0];
			Rot->Pitch = RotXYZ[1];
			Rot->Yaw = RotXYZ[2];
			if (Ctx.OnRotatorEdited)
			{
				Ctx.OnRotatorEdited();
			}
		}
		break;
	}
	case EPropertyType::Transform:
	{
		// FTransform { Location, FQuat Rotation, Scale } 을 Location / Rotation(Euler) / Scale
		// 3행으로 렌더. 회전은 FRotator 변환으로 편집(Rotator 케이스와 동일한 X=Roll,Y=Pitch,Z=Yaw).
		FTransform* Xf = static_cast<FTransform*>(Prop.GetValuePtr());
		if (!Xf)
		{
			break;
		}
		const float Speed = Prop.GetSpeed();

		float Loc[3] = { Xf->Location.X, Xf->Location.Y, Xf->Location.Z };
		ImGui::TextUnformatted("L"); ImGui::SameLine();
		if (ImGui::DragFloat3("##XfLoc", Loc, Speed))
		{
			Xf->Location.X = Loc[0]; Xf->Location.Y = Loc[1]; Xf->Location.Z = Loc[2];
			bChanged = true;
		}

		// 짐벌 락 회피: 매 프레임 Quat→Euler 재추출 대신, 편집 중인 Euler 를 캐시해서
		// quat 이 외부(기즈모 등)에서 바뀌지 않은 한 캐시값을 표시한다. Pitch=±90° 에서도
		// Roll/Yaw 입력이 다음 프레임에 사라지지 않는다.
		const FRotator CurRot = Xf->GetRotator();
		FVector DispEuler(CurRot.Roll, CurRot.Pitch, CurRot.Yaw);
		const auto CacheIt = GTransformEulerCache.find(Xf);
		if (CacheIt != GTransformEulerCache.end())
		{
			FRotator Cached;
			Cached.Roll = CacheIt->second.X; Cached.Pitch = CacheIt->second.Y; Cached.Yaw = CacheIt->second.Z;
			const FQuat CQ = Cached.ToQuaternion();
			const FQuat& Q = Xf->Rotation;
			// quat 동일성(이중커버 q≡-q 고려): |dot| ≈ 1 이면 외부 변경 없음 → 캐시 Euler 유지.
			const float Dot = CQ.X * Q.X + CQ.Y * Q.Y + CQ.Z * Q.Z + CQ.W * Q.W;
			if (std::fabs(Dot) > 0.9999f)
			{
				DispEuler = CacheIt->second;
			}
		}

		float RotXYZ[3] = { DispEuler.X, DispEuler.Y, DispEuler.Z };
		ImGui::TextUnformatted("R"); ImGui::SameLine();
		if (ImGui::DragFloat3("##XfRot", RotXYZ, Speed))
		{
			FRotator NewRot;
			NewRot.Roll = RotXYZ[0]; NewRot.Pitch = RotXYZ[1]; NewRot.Yaw = RotXYZ[2];
			Xf->SetRotation(NewRot);
			bChanged = true;
		}
		// 캐시를 현재 표시값으로 동기화(편집분 포함).
		GTransformEulerCache[Xf] = FVector(RotXYZ[0], RotXYZ[1], RotXYZ[2]);

		float Scl[3] = { Xf->Scale.X, Xf->Scale.Y, Xf->Scale.Z };
		ImGui::TextUnformatted("S"); ImGui::SameLine();
		if (ImGui::DragFloat3("##XfScl", Scl, Speed))
		{
			Xf->Scale.X = Scl[0]; Xf->Scale.Y = Scl[1]; Xf->Scale.Z = Scl[2];
			bChanged = true;
		}
		break;
	}
	case EPropertyType::Vec4:
	{
		float* Val = static_cast<float*>(Prop.GetValuePtr());
		bChanged = ImGui::DragFloat4("##Value", Val, Prop.GetSpeed());
		break;
	}
	case EPropertyType::Color4:
	{
		float* Val = static_cast<float*>(Prop.GetValuePtr());
		bChanged = ImGui::ColorEdit4("##Value", Val);
		break;
	}
	case EPropertyType::String:
	{
		FString* Val = static_cast<FString*>(Prop.GetValuePtr());
		if (!Val)
		{
			break;
		}

		char Buf[256];
		strncpy_s(Buf, sizeof(Buf), Val->c_str(), _TRUNCATE);
		if (ImGui::InputText("##Value", Buf, sizeof(Buf)))
		{
			*Val = Buf;
			bChanged = true;
		}
		break;
	}
	case EPropertyType::ClassRef:
	{
		bChanged = RenderClassPropertyWidget(Prop);
		break;
	}
	case EPropertyType::ObjectRef:
	{
		// 호출자별 ObjectRef 커스텀(예: 파티클 Distribution 동적 클래스 피커)을 우선 적용.
		if (Ctx.RenderObjectRef)
		{
			bool bHandled = false;
			const bool bRefChanged = Ctx.RenderObjectRef(Prop, bHandled);
			if (bHandled) { bChanged = bRefChanged; break; }
		}

		const FObjectProperty* ObjectValueProperty = Prop.Property ? Prop.Property->AsObjectProperty() : nullptr;
		if (!ObjectValueProperty)
		{
			break;
		}

		auto SetObjectValue = [&](UObject* Object)
		{
			ObjectValueProperty->SetObjectValue(Prop.ContainerPtr, Object);
			bChanged = true;
		};

		UObject* Current = ObjectValueProperty->GetObjectValue(Prop.ContainerPtr);
		FString Preview = Current ? Current->GetName() : FString("None");

		const FObjectPropertyBase* ObjectProperty = Prop.Property ? Prop.Property->AsObjectPropertyBase() : nullptr;
		UClass* AllowedClass = ObjectProperty ? ObjectProperty->GetAllowedClassType() : nullptr;

		if (AllowedClass == UStaticMesh::StaticClass())
		{
			UStaticMesh* CurrentMesh = Cast<UStaticMesh>(Current);
			Preview = CurrentMesh && CurrentMesh->GetAssetPathFileName() != "None"
				? GetStemFromPath(CurrentMesh->GetAssetPathFileName())
				: FString("None");

			float ButtonWidth = ImGui::CalcTextSize("Import").x + ImGui::GetStyle().FramePadding.x * 2.0f;
			float Spacing = ImGui::GetStyle().ItemSpacing.x;
			ImGui::SetNextItemWidth(-(ButtonWidth + Spacing));

			if (ImGui::BeginCombo("##StaticMeshObject", Preview.c_str()))
			{
				const bool bSelectedNone = CurrentMesh == nullptr;
				if (ImGui::Selectable("None", bSelectedNone))
				{
					SetObjectValue(nullptr);
				}
				if (bSelectedNone)
				{
					ImGui::SetItemDefaultFocus();
				}

				const TArray<FAssetListItem>& MeshFiles = FMeshManager::GetAvailableStaticMeshFiles();
				for (const FAssetListItem& Item : MeshFiles)
				{
					const bool bSelected = CurrentMesh && CurrentMesh->GetAssetPathFileName() == Item.FullPath;
					if (ImGui::Selectable(Item.DisplayName.c_str(), bSelected))
					{
						ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
						UStaticMesh* Loaded = FMeshManager::LoadStaticMesh(Item.FullPath, Device);
						if (Loaded)
						{
							SetObjectValue(Loaded);
						}
					}
					if (bSelected)
					{
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}

			ImGui::SameLine();
			ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMax().x - ButtonWidth);
			if (ImGui::Button("Import"))
			{
				FString MeshPath = OpenStaticMeshFileDialog();
				if (!MeshPath.empty())
				{
					if (IsFbxFilePath(MeshPath))
					{
						ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
						UStaticMesh* Loaded = FMeshManager::LoadStaticMesh(MeshPath, FImportOptions::Default(), Device);
						if (Loaded)
						{
							SetObjectValue(Loaded);
						}
					}
					else
					{
						ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
						UStaticMesh* Loaded = FMeshManager::LoadStaticMesh(MeshPath, Device);
						if (Loaded)
						{
							SetObjectValue(Loaded);
						}
					}
				}
			}
			break;
		}

		if (AllowedClass == USkeletalMesh::StaticClass())
		{
			USkeletalMesh* CurrentMesh = Cast<USkeletalMesh>(Current);
			Preview = CurrentMesh && CurrentMesh->GetAssetPathFileName() != "None"
				? GetStemFromPath(CurrentMesh->GetAssetPathFileName())
				: FString("None");

			static FFbxSceneImportDialogState s_SkelFbxObjDialog;

			float ButtonWidth = ImGui::CalcTextSize("Import FBX").x + ImGui::GetStyle().FramePadding.x * 2.0f;
			float Spacing = ImGui::GetStyle().ItemSpacing.x;
			ImGui::SetNextItemWidth(-(ButtonWidth + Spacing));

			if (ImGui::BeginCombo("##SkeletalMeshObject", Preview.c_str()))
			{
				const bool bSelectedNone = CurrentMesh == nullptr;
				if (ImGui::Selectable("None", bSelectedNone))
				{
					SetObjectValue(nullptr);
				}
				if (bSelectedNone)
				{
					ImGui::SetItemDefaultFocus();
				}

				const TArray<FAssetListItem>& MeshFiles = FMeshManager::GetAvailableSkeletalMeshFiles();
				for (const FAssetListItem& Item : MeshFiles)
				{
					const bool bSelected = CurrentMesh && CurrentMesh->GetAssetPathFileName() == Item.FullPath;
					if (ImGui::Selectable(Item.DisplayName.c_str(), bSelected))
					{
						ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
						USkeletalMesh* Loaded = FMeshManager::LoadSkeletalMesh(Item.FullPath, Device);
						if (Loaded)
						{
							SetObjectValue(Loaded);
						}
					}
					if (bSelected)
					{
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}

			ImGui::SameLine();
			ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMax().x - ButtonWidth);
			if (ImGui::Button("Import FBX"))
			{
				FString FbxPath = OpenFbxFileDialog();
				if (!FbxPath.empty())
				{
					FFbxImportOptionsDialog::BeginSceneImport(s_SkelFbxObjDialog, FbxPath);
				}
			}

			FFbxSceneImportRequest       Request;
			const EFbxImportDialogResult DialogResult = FFbxImportOptionsDialog::RenderSceneImportPopup(
				"Object Skeletal FBX Import Options",
				s_SkelFbxObjDialog,
				Request
			);
			if (DialogResult == EFbxImportDialogResult::Submitted)
			{
				FFbxSceneImportResult Result;
				const auto ImportStart = std::chrono::steady_clock::now();
				if (FMeshManager::ImportFbxScene(Request, GEngine->GetRenderer().GetFD3DDevice().GetDevice(), Result))
				{
					if (Result.SkeletalMesh)
					{
						const std::chrono::duration<double> Elapsed = std::chrono::steady_clock::now() - ImportStart;
						FMeshEditorWidget::RecordImportDurationForAsset(
							Result.SkeletalMesh->GetAssetPathFileName(),
							Elapsed.count()
						);
						SetObjectValue(Result.SkeletalMesh);
					}
					FMeshManager::ScanMeshAssets();
					FFbxImportOptionsDialog::RequestClose(s_SkelFbxObjDialog);
				}
				else
				{
					s_SkelFbxObjDialog.Error = "FBX import failed. See the engine log for details.";
				}
			}

			break;
		}

		if (AllowedClass && AllowedClass->IsA(UActorComponent::StaticClass()))
		{
			Preview = GetObjectReferenceChoiceLabel(Current);

			if (ImGui::BeginCombo("##OwnerObjectRef", Preview.c_str()))
			{
				const bool bSelectedNone = Current == nullptr;
				if (ImGui::Selectable("None", bSelectedNone))
				{
					SetObjectValue(nullptr);
				}
				if (bSelectedNone)
				{
					ImGui::SetItemDefaultFocus();
				}

				for (UObject* Candidate : GetOwnerObjectReferenceChoices(Prop, AllowedClass))
				{
					const FString CandidateName = GetObjectReferenceChoiceLabel(Candidate);
					const bool bSelected = Current == Candidate;
					if (ImGui::Selectable(CandidateName.c_str(), bSelected))
					{
						SetObjectValue(Candidate);
					}
					if (bSelected)
					{
						ImGui::SetItemDefaultFocus();
					}
				}

				ImGui::EndCombo();
			}
			break;
		}

		if (ImGui::BeginCombo("##Value", Preview.c_str()))
		{
			const bool bSelectedNone = Current == nullptr;
			if (ImGui::Selectable("None", bSelectedNone))
			{
				SetObjectValue(nullptr);
			}
			if (bSelectedNone)
			{
				ImGui::SetItemDefaultFocus();
			}

			for (UObject* Candidate : GUObjectArray)
			{
				if (!IsValid(Candidate))
				{
					continue;
				}

				if (AllowedClass && !Candidate->GetClass()->IsA(AllowedClass))
				{
					continue;
				}

				FString CandidateName = Candidate->GetName();
				if (CandidateName.empty())
				{
					CandidateName = Candidate->GetClass()->GetName();
				}

				const bool bSelected = Current == Candidate;
				if (ImGui::Selectable(CandidateName.c_str(), bSelected))
				{
					SetObjectValue(Candidate);
				}
				if (bSelected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}

			ImGui::EndCombo();
		}
		break;
	}
	case EPropertyType::SoftObjectRef:
	{
		bChanged = RenderSoftObjectWidget(Prop);
		break;
	}
	case EPropertyType::Array:
	{
		bChanged = RenderArrayPropertyWidget(Prop, Ctx, bDispatchChange, EffectivePropertyPath);
		bDispatchChange = false;
		break;
	}
	case EPropertyType::Name:
	{
		FName* Val = static_cast<FName*>(Prop.GetValuePtr());
		FString Current = Val->ToString();

		// 리소스 키와 매칭되는 프로퍼티면 콤보 박스로 렌더링
		TArray<FString> Names;
		FString AssetType = GetAssetTypeMetadata(Prop);
		if (AssetType.empty())
		{
			AssetType = Prop.GetName();
		}

		if (AssetType == "Font")
			Names = FResourceManager::Get().GetFontNames();
		else if (AssetType == "SubUVResource")
			Names = FResourceManager::Get().GetSubUVResourceNames();
		else if (AssetType == "Texture")
			Names = FResourceManager::Get().GetTextureNames();
		else if (AssetType == "BoneName")
			Names = GetBoneNameChoices(Prop);

		if (!Names.empty())
		{
			if (Current.empty())
			{
				Current = "None";
			}

			if (ImGui::BeginCombo("##Value", Current.c_str()))
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
			if (ImGui::InputText("##Value", Buf, sizeof(Buf)))
			{
				*Val = FName(Buf);
				bChanged = true;
			}
		}
		break;
	}
	case EPropertyType::Enum:
	{
		bChanged = RenderEnumPropertyWidget(Prop);
		break;
	}
	case EPropertyType::Struct:
	{
		bChanged = RenderStructPropertyWidget(Prop, Ctx, bDispatchChange, EffectivePropertyPath);
		bDispatchChange = false;
		break;
	}
	default:
		break;
	}

	if (bReadOnly)
	{
		ImGui::EndDisabled();
		bChanged = false;
	}

	if (bDispatchChange && bChanged)
	{
		DispatchPostEditChange(Prop, EPropertyChangeType::ValueSet, -1, EffectivePropertyPath);
	}

	ImGui::PopID();
	return bChanged;
}

namespace
{
	// Props 를 카테고리 구분 2열 테이블로 렌더. 한 프레임에 하나의 변경만 처리하고 즉시
	// 빠져나온다(배열 +/- 등으로 후속 prop 의 ContainerPtr 가 무효화되는 dangling 방지).
	bool RenderPropsTable(const char* TableId, TArray<FPropertyValue>& Props, const FPropertyTable::FContext& Ctx)
	{
		// 카테고리 등장 순서 수집
		TArray<std::string> CategoryOrder;
		for (const auto& P : Props)
		{
			const char* Category = P.GetCategory();
			bool bFound = false;
			for (const auto& C : CategoryOrder)
			{
				if (C == Category) { bFound = true; break; }
			}
			if (!bFound) CategoryOrder.push_back(Category);
		}

		bool bAnyChanged = false;
		for (const auto& Cat : CategoryOrder)
		{
			if (!Cat.empty())
			{
				ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.22f, 0.22f, 0.22f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.27f, 0.27f, 0.27f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.30f, 0.30f, 0.30f, 1.0f));
				ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(5.0f, 3.0f));
				bool bOpen = ImGui::CollapsingHeader(Cat.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
				ImGui::PopStyleVar();
				ImGui::PopStyleColor(3);
				if (!bOpen) continue;
			}

			if (ImGui::BeginTable(TableId, 2,
				ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_PadOuterX | ImGuiTableFlags_RowBg))
			{
				ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 150.0f);
				ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

				ImGui::PushStyleColor(ImGuiCol_TableRowBg, ImVec4(0.13f, 0.13f, 0.13f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_TableRowBgAlt, ImVec4(0.145f, 0.145f, 0.145f, 1.0f));

				for (int32 i = 0; i < (int32)Props.size(); ++i)
				{
					if (Cat != Props[i].GetCategory())
						continue;

					ImGui::TableNextRow();
					ImGui::PushID(i);

					ImGui::TableSetColumnIndex(0);
					ImGui::SetWindowFontScale(0.92f);
					ImGui::AlignTextToFramePadding();
					ImGui::TextUnformatted(GetPropertyDisplayName(Props[i]));
					ImGui::SetWindowFontScale(1.0f);

					ImGui::TableSetColumnIndex(1);
					ImGui::SetNextItemWidth(-1);

					const bool bRowChanged = FPropertyTable::RenderValue(Props, i, Ctx);
					ImGui::PopID();
					if (bRowChanged)
					{
						bAnyChanged = true;
						// 배열 +/- 등 "구조 변경"만 후속 prop 의 ContainerPtr 를 무효화할 수 있으므로
						// 그 경우에만 즉시 빠져나와 다음 프레임에 재수집한다. 스칼라/구조체/회전/Enum 같은
						// "값 변경"은 패널을 끝까지 마저 그린다 — 중간에 return 하면 콘텐츠가 잘려
						// (뷰포트보다 짧아지면) ImGui 가 스크롤을 맨 위로 클램프하는 문제를 막기 위함.
						if (Props[i].GetType() == EPropertyType::Array)
						{
							ImGui::EndTable();
							ImGui::PopStyleColor(2);
							return true;
						}
					}
				}

				ImGui::EndTable();
				ImGui::PopStyleColor(2);
			}
		}

		return bAnyChanged;
	}
}

bool FPropertyTable::RenderObject(UObject* Object, const FContext& Ctx)
{
	if (!Object)
	{
		ImGui::TextDisabled("(no object)");
		return false;
	}

	TArray<FPropertyValue> Props;
	Object->GetEditableProperties(Props);

	// 호출자별 제외 술어(예: 파티클 bEnabled) 적용.
	if (Ctx.ShouldSkipProperty)
	{
		TArray<FPropertyValue> Filtered;
		for (const FPropertyValue& P : Props)
		{
			if (!Ctx.ShouldSkipProperty(P.Property)) { Filtered.push_back(P); }
		}
		Props = std::move(Filtered);
	}

	if (Props.empty())
	{
		ImGui::TextDisabled("(no editable properties)");
		return false;
	}
	return RenderPropsTable("##ObjectProperties", Props, Ctx);
}

bool FPropertyTable::RenderStruct(UStruct* StructType, void* Value, UObject* Owner, const FContext& Ctx)
{
	if (!StructType || !Value)
	{
		ImGui::TextDisabled("(no struct)");
		return false;
	}

	// 구조체 자식 프로퍼티를 FPropertyValue 로 합성 — Owner 를 디스패치 대상으로 둔다.
	TArray<const FProperty*> ChildProperties;
	StructType->GetPropertyRefs(ChildProperties);

	TArray<FPropertyValue> Props;
	for (const FProperty* ChildProperty : ChildProperties)
	{
		if (!ChildProperty || (ChildProperty->Flags & PF_Edit) == 0)
		{
			continue;
		}
		if (Ctx.ShouldSkipProperty && Ctx.ShouldSkipProperty(ChildProperty))
		{
			continue;
		}
		if (!ChildProperty->GetValuePtrFor(Value))
		{
			continue;
		}
		Props.push_back(ChildProperty->ToValue(Value, Owner));
	}
	if (Props.empty())
	{
		ImGui::TextDisabled("(no editable properties)");
		return false;
	}
	return RenderPropsTable("##StructProperties", Props, Ctx);
}
