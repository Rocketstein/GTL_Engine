#include "SceneSaveManager.h"

#include <iostream>
#include <fstream>
#include <chrono>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>

#include "SimpleJSON/json.hpp"
#include "GameFramework/World.h"
#include "GameFramework/AActor.h"
#include "Component/SceneComponent.h"
#include "Component/ActorComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Component/BillboardComponent.h"
#include "Component/CameraComponent.h"
#include "Component/DecalComponent.h"
#include "Component/HeightFogComponent.h"
#include "Component/Light/LightComponentBase.h"
#include "GameFramework/StaticMeshActor.h"
#include "Materials/MaterialManager.h"
#include "Resource/ResourceManager.h"
#include "Object/Object.h"
#include "Object/ObjectFactory.h"
#include "Core/PropertyTypes.h"
#include "Object/FName.h"
#include "Profiling/PlatformTime.h"
#include "Engine/Serialization/WindowsArchive.h"
#include "Engine/Runtime/Engine.h"  // GIsEditor 플래그
// ---- JSON vector helpers ---------------------------------------------------

static void WriteVec3(json::JSON& Obj, const char* Key, const FVector& V)
{
	json::JSON arr = json::Array();
	arr.append(static_cast<double>(V.X));
	arr.append(static_cast<double>(V.Y));
	arr.append(static_cast<double>(V.Z));
	Obj[Key] = arr;
}

static FVector ReadVec3(json::JSON& Arr)
{
	FVector out(0, 0, 0);
	int i = 0;
	for (auto& e : Arr.ArrayRange()) {
		if (i == 0) out.X = static_cast<float>(e.ToFloat());
		else if (i == 1) out.Y = static_cast<float>(e.ToFloat());
		else if (i == 2) out.Z = static_cast<float>(e.ToFloat());
		++i;
	}
	return out;
}

static FString ReadAssetPathValue(json::JSON& Value)
{
	using JSONClass = json::JSON::Class;

	if (Value.JSONType() == JSONClass::String)
	{
		return Value.ToString();
	}

	if (Value.JSONType() == JSONClass::Object && Value.hasKey("Path"))
	{
		return Value["Path"].ToString();
	}

	return "";
}

static FString ResolveMaterialPath(const FString& MaterialValue)
{
	if (MaterialValue.empty() || MaterialValue == "None")
	{
		return MaterialValue;
	}

	if (const FMaterialResource* MaterialResource = FResourceManager::Get().FindMaterial(FName(MaterialValue)))
	{
		return MaterialResource->Path;
	}

	const FString ResolvedPath = FResourceManager::Get().ResolvePath(FName(MaterialValue));
	return ResolvedPath.empty() ? MaterialValue : ResolvedPath;
}

static FString ResolveTexturePath(const FString& TextureValue)
{
	if (TextureValue.empty() || TextureValue == "None")
	{
		return TextureValue;
	}

	if (const FTextureResource* TextureResource = FResourceManager::Get().FindTexture(FName(TextureValue)))
	{
		return TextureResource->Path;
	}

	const FString ResolvedPath = FResourceManager::Get().ResolvePath(FName(TextureValue));
	return ResolvedPath.empty() ? TextureValue : ResolvedPath;
}

static void ApplySingleMaterialOverride(UStaticMeshComponent* StaticMeshComponent, json::JSON& Value)
{
	if (!StaticMeshComponent)
	{
		return;
	}

	if (StaticMeshComponent->GetOverrideMaterials().size() != 1)
	{
		return;
	}

	const FString MaterialPath = ResolveMaterialPath(ReadAssetPathValue(Value));
	if (MaterialPath.empty() || MaterialPath == "None")
	{
		StaticMeshComponent->SetMaterial(0, nullptr);
		return;
	}

	if (UMaterial* LoadedMat = FMaterialManager::Get().GetOrCreateMaterial(MaterialPath))
	{
		StaticMeshComponent->SetMaterial(0, LoadedMat);
	}
}

static void ClearActorComponentsForDeserialization(AActor* Actor)
{
	if (!Actor)
	{
		return;
	}

	TArray<UActorComponent*> ComponentsToRemove = Actor->GetComponents();
	for (auto It = ComponentsToRemove.rbegin(); It != ComponentsToRemove.rend(); ++It)
	{
		UActorComponent* Component = *It;
		if (!Component)
		{
			continue;
		}

		Actor->RemoveComponent(Component);
	}
}

// ---------------------------------------------------------------------------

namespace SceneKeys
{
	static constexpr const char* Version = "Version";
	static constexpr const char* Name = "Name";
	static constexpr const char* ObjectName = "ObjectName";
	static constexpr const char* ClassName = "ClassName";
	static constexpr const char* WorldType = "WorldType";
	static constexpr const char* ContextName = "ContextName";
	static constexpr const char* ContextHandle = "ContextHandle";
	static constexpr const char* Actors = "Actors";
	static constexpr const char* UUID = "UUID";
	static constexpr const char* Visible = "bVisible";
	static constexpr const char* FolderPath = "FolderPath";
	static constexpr const char* RootComponent = "RootComponent";
	static constexpr const char* NonSceneComponents = "NonSceneComponents";
	static constexpr const char* Properties = "Properties";
	static constexpr const char* Children = "Children";
	static constexpr const char* HiddenInComponentTree = "bHiddenInComponentTree";
	static constexpr const char* GameModeClass = "GameModeClass";
	static constexpr const char* OutlinerFolders = "OutlinerFolders";
}

static void SerializeComponentEditorMetadata(json::JSON& Node, const UActorComponent* Comp)
{
	if (!Comp)
	{
		return;
	}

	if (Comp->IsHiddenInComponentTree())
	{
		Node[SceneKeys::HiddenInComponentTree] = true;
	}
}

static void DeserializeComponentEditorMetadata(UActorComponent* Comp, json::JSON& Node)
{
	if (!Comp)
	{
		return;
	}

	if (Node.hasKey(SceneKeys::HiddenInComponentTree))
	{
		Comp->SetHiddenInComponentTree(Node[SceneKeys::HiddenInComponentTree].ToBool());
	}
}

static void EnsureEditorBillboardMetadata(UActorComponent* Comp)
{
	// Game/Shipping에서는 에디터 빌보드(아이콘 sprite)가 필요 없음 — 텍스처가 누락돼 로드 실패함.
	if (!GIsEditor) return;

	if (ULightComponentBase* LightComponent = Cast<ULightComponentBase>(Comp))
	{
		LightComponent->EnsureEditorBillboard();
	}
	else if (UDecalComponent* DecalComponent = Cast<UDecalComponent>(Comp))
	{
		DecalComponent->EnsureEditorBillboard();
	}
	else if (UHeightFogComponent* HeightFogComponent = Cast<UHeightFogComponent>(Comp))
	{
		HeightFogComponent->EnsureEditorBillboard();
	}
}

static const char* WorldTypeToString(EWorldType Type)
{
	switch (Type) {
	case EWorldType::Game: return "Game";
	case EWorldType::PIE:  return "PIE";
	default:               return "Editor";
	}
}

static EWorldType StringToWorldType(const string& Str)
{
	if (Str == "Game") return EWorldType::Game;
	if (Str == "PIE")  return EWorldType::PIE;
	return EWorldType::Editor;
}

static bool IsGameModeClassName(const string& ClassName)
{
	return ClassName == "AGameModeBase";
}

// ============================================================
// Save
// ============================================================

void FSceneSaveManager::SaveSceneAsJSON(const FString& InScenePathOrName, FWorldContext& WorldContext, UCameraComponent* PerspectiveCam)
{
	using namespace json;

	if (!WorldContext.World) return;

	const std::filesystem::path RequestedPath = FPaths::ToWide(InScenePathOrName);
	const bool bHasDirectory = RequestedPath.has_parent_path();
	const bool bHasExtension = RequestedPath.has_extension();

	std::filesystem::path FileDestination;
	string FinalName;
	if (InScenePathOrName.empty())
	{
		FinalName = "Save_" + GetCurrentTimeStamp();
		FileDestination = std::filesystem::path(GetSceneDirectory()) / (FPaths::ToWide(FinalName) + SceneExtension);
	}
	else if (bHasDirectory || bHasExtension)
	{
		FileDestination = RequestedPath;
		if (!bHasExtension)
		{
			FileDestination += SceneExtension;
		}
		FinalName = FPaths::ToUtf8(FileDestination.stem().wstring());
	}
	else
	{
		FinalName = InScenePathOrName;
		FileDestination = std::filesystem::path(GetSceneDirectory()) / (FPaths::ToWide(FinalName) + SceneExtension);
	}

	if (const std::filesystem::path ParentDir = FileDestination.parent_path(); !ParentDir.empty())
	{
		std::filesystem::create_directories(ParentDir);
	}

	JSON Root = SerializeWorld(WorldContext.World, WorldContext, PerspectiveCam);
	Root[SceneKeys::Version] = 2;
	Root[SceneKeys::Name] = FinalName;

	std::ofstream File(FileDestination);
	if (File.is_open()) {
		File << Root.dump();
		File.flush();
		File.close();
	}
}

string FSceneSaveManager::SerializeWorldToJSONString(FWorldContext& WorldContext, UCameraComponent* PerspectiveCam)
{
	if (!WorldContext.World)
	{
		return string();
	}

	json::JSON Root = SerializeWorld(WorldContext.World, WorldContext, PerspectiveCam);
	Root[SceneKeys::Version] = 2;
	return Root.dump();
}

string FSceneSaveManager::SerializeActorToJSONString(AActor* Actor)
{
	if (!Actor)
	{
		return string();
	}

	json::JSON Root = json::Object();
	Root["Actor"] = SerializeActor(Actor);
	return Root.dump();
}

json::JSON FSceneSaveManager::SerializeWorld(UWorld* World, const FWorldContext& Ctx, UCameraComponent* PerspectiveCam)
{
	using namespace json;
	JSON w = json::Object();
	w[SceneKeys::ClassName] = World->GetClass()->GetName();
	w[SceneKeys::WorldType] = WorldTypeToString(Ctx.WorldType);
	w[SceneKeys::ContextName] = Ctx.ContextName;
	w[SceneKeys::ContextHandle] = Ctx.ContextHandle.ToString();

	if (ULevel* PersistentLevel = World->GetPersistentLevel())
	{
		const FString& GMClass = PersistentLevel->GetGameModeClassName();
		if (!GMClass.empty())
		{
			w[SceneKeys::GameModeClass] = GMClass;
		}

		if (!PersistentLevel->GetOutlinerFolders().empty())
		{
			JSON FolderNames = json::Array();
			for (const FString& FolderName : PersistentLevel->GetOutlinerFolders())
			{
				if (!FolderName.empty())
				{
					FolderNames.append(FolderName);
				}
			}
			w[SceneKeys::OutlinerFolders] = FolderNames;
		}
	}

	// ---- Primitives: gather static mesh components into a top-level block
	JSON Primitives = json::Object();
	std::unordered_map<AActor*, string> ActorPrimitiveKey;

	for (AActor* Actor : World->GetActors()) {
		if (!Actor) continue;

		for (UActorComponent* Comp : Actor->GetComponents()) {
			if (!Comp) continue;
			if (!Comp->IsA<UStaticMeshComponent>()) continue;

			UStaticMeshComponent* S = static_cast<UStaticMeshComponent*>(Comp);

			JSON p = json::Object();

			const FMatrix& M = S->GetWorldMatrix();
			FVector loc = M.GetLocation();
			FVector rot = M.GetEuler();
			FVector scale = M.GetScale();

			p["ObjStaticMeshAsset"] = S->GetStaticMeshPath();

			JSON locArr = json::Array();
			locArr.append(static_cast<double>(loc.X));
			locArr.append(static_cast<double>(loc.Y));
			locArr.append(static_cast<double>(loc.Z));
			p["Location"] = locArr;

			JSON rotArr = json::Array();
			rotArr.append(static_cast<double>(rot.X));
			rotArr.append(static_cast<double>(rot.Y));
			rotArr.append(static_cast<double>(rot.Z));
			p["Rotation"] = rotArr;

			JSON scaleArr = json::Array();
			scaleArr.append(static_cast<double>(scale.X));
			scaleArr.append(static_cast<double>(scale.Y));
			scaleArr.append(static_cast<double>(scale.Z));
			p["Scale"] = scaleArr;

			p["Type"] = "StaticMeshComp";

			// Note: per design, material/UV overrides are stored on the Actor->RootComponent Properties
			// and are NOT duplicated inside the Primitives block to avoid redundancy.

			// Use the Actor's UUID as the primitive key to avoid positional/index coupling
			string key = std::to_string(Actor->GetUUID());
			Primitives[key] = p;
			ActorPrimitiveKey[Actor] = key;

			// only first static mesh component per actor is exported as primitive
			break;
		}
	}

	if (Primitives.size() > 0) {
		w["Primitives"] = Primitives;
	}

	// ---- Actors: serialize and attach PrimitiveKey when present ----
	JSON Actors = json::Array();
	for (AActor* Actor : World->GetActors()) {
		if (!Actor) continue;
		JSON a = SerializeActor(Actor);
		auto it = ActorPrimitiveKey.find(Actor);
		if (it != ActorPrimitiveKey.end()) {
			a["PrimitiveKey"] = it->second;
		}
		Actors.append(a);
	}
	w[SceneKeys::Actors] = Actors;

	// ---- Perspective camera ----
	JSON cam = SerializeCamera(PerspectiveCam);
	if (cam.size() > 0) {
		w["PerspectiveCamera"] = cam;
	}

	return w;
}

json::JSON FSceneSaveManager::SerializeActor(AActor* Actor)
{
	using namespace json;
	JSON a = json::Object();
	a[SceneKeys::ClassName] = Actor->GetClass()->GetName();
	a[SceneKeys::ObjectName] = Actor->GetFName().ToString();
	a[SceneKeys::UUID] = static_cast<int32>(Actor->GetUUID());
	a[SceneKeys::Visible] = Actor->IsVisible();
	if (!Actor->GetFolderPath().empty())
	{
		a[SceneKeys::FolderPath] = Actor->GetFolderPath();
	}

	// RootComponent 트리 직렬화
	if (Actor->GetRootComponent()) {
		a[SceneKeys::RootComponent] = SerializeSceneComponentTree(Actor->GetRootComponent());
	}

	// Non-scene components
	JSON NonScene = json::Array();
	for (UActorComponent* Comp : Actor->GetComponents()) {
		if (!Comp) continue;
		if (Comp->IsA<USceneComponent>()) continue;

		JSON c = json::Object();
		c[SceneKeys::ClassName] = Comp->GetClass()->GetName();
		c[SceneKeys::ObjectName] = Comp->GetFName().ToString();
		c[SceneKeys::Properties] = SerializeProperties(Comp);
		SerializeComponentEditorMetadata(c, Comp);
		NonScene.append(c);
	}
	a[SceneKeys::NonSceneComponents] = NonScene;

	return a;
}

json::JSON FSceneSaveManager::SerializeSceneComponentTree(USceneComponent* Comp)
{
	using namespace json;
	JSON c = json::Object();
	c[SceneKeys::ClassName] = Comp->GetClass()->GetName();
	c[SceneKeys::ObjectName] = Comp->GetFName().ToString();
	c[SceneKeys::Properties] = SerializeProperties(Comp);
	SerializeComponentEditorMetadata(c, Comp);

	JSON Children = json::Array();
	for (USceneComponent* Child : Comp->GetChildren()) {
		if (!Child) continue;
		Children.append(SerializeSceneComponentTree(Child));
	}
	c[SceneKeys::Children] = Children;

	return c;
}

json::JSON FSceneSaveManager::SerializeProperties(UActorComponent* Comp)
{
	using namespace json;
	JSON props = json::Object();

	TArray<FPropertyDescriptor> Descriptors;
	Comp->GetEditableProperties(Descriptors);

	for (const auto& Prop : Descriptors) {
		//if (Prop.Name == "Static Mesh") continue; // Primitives 블록에 이미 저장됨
		props[Prop.Name] = SerializePropertyValue(Prop);
	}

	return props;
}

json::JSON FSceneSaveManager::SerializePropertyValue(const FPropertyDescriptor& Prop)
{
	using namespace json;

	switch (Prop.Type) {
	case EPropertyType::Bool:
		return JSON(*static_cast<bool*>(Prop.ValuePtr));

	case EPropertyType::Int:
		return JSON(*static_cast<int32*>(Prop.ValuePtr));

	case EPropertyType::Float:
		return JSON(static_cast<double>(*static_cast<float*>(Prop.ValuePtr)));

	case EPropertyType::Vec3: {
		float* v = static_cast<float*>(Prop.ValuePtr);
		JSON arr = json::Array();
		arr.append(static_cast<double>(v[0]));
		arr.append(static_cast<double>(v[1]));
		arr.append(static_cast<double>(v[2]));
		return arr;
	}
	case EPropertyType::Rotator: {
		float* v = static_cast<float*>(Prop.ValuePtr);
		JSON arr = json::Array();
		arr.append(static_cast<double>(v[0]));
		arr.append(static_cast<double>(v[1]));
		arr.append(static_cast<double>(v[2]));
		return arr;
	}
	case EPropertyType::Vec4:
	case EPropertyType::Color4: {
		float* v = static_cast<float*>(Prop.ValuePtr);
		JSON arr = json::Array();
		arr.append(static_cast<double>(v[0]));
		arr.append(static_cast<double>(v[1]));
		arr.append(static_cast<double>(v[2]));
		arr.append(static_cast<double>(v[3]));
		return arr;
	}
	case EPropertyType::String:
	case EPropertyType::SceneComponentRef:
	case EPropertyType::StaticMeshRef:
		return JSON(*static_cast<FString*>(Prop.ValuePtr));

	case EPropertyType::MaterialSlot: {
		const FMaterialSlot* Slot = static_cast<const FMaterialSlot*>(Prop.ValuePtr);
		JSON obj = json::Object();
		obj["Path"] = JSON(Slot->Path);
		return obj;
	}

	case EPropertyType::TextureSlot: {
		const FTextureSlot* Slot = static_cast<const FTextureSlot*>(Prop.ValuePtr);
		JSON obj = json::Object();
		obj["Path"] = JSON(Slot->Path);
		return obj;
	}

	case EPropertyType::ByteBool:
		return JSON(static_cast<bool>(*static_cast<uint8_t*>(Prop.ValuePtr) != 0));

	case EPropertyType::Name:
		return JSON(static_cast<FName*>(Prop.ValuePtr)->ToString());

	case EPropertyType::Enum:
		return JSON(*static_cast<int32*>(Prop.ValuePtr));

	case EPropertyType::Vec3Array: {
		const TArray<FVector>* Arr = static_cast<const TArray<FVector>*>(Prop.ValuePtr);
		JSON outer = json::Array();
		for (const FVector& v : *Arr) {
			JSON inner = json::Array();
			inner.append(static_cast<double>(v.X));
			inner.append(static_cast<double>(v.Y));
			inner.append(static_cast<double>(v.Z));
			outer.append(inner);
		}
		return outer;
	}

	default:
		return JSON();
	}
}

// ---- Camera helpers ----

json::JSON FSceneSaveManager::SerializeCamera(UCameraComponent* Cam)
{
	using namespace json;
	JSON cam = json::Object();
	if (!Cam) return cam;

	const FMatrix& M = Cam->GetWorldMatrix();
	WriteVec3(cam, "Location", M.GetLocation());
	WriteVec3(cam, "Rotation", M.GetEuler());

	const FMinimalViewInfo& S = Cam->GetCameraState();
	cam["FOV"] = static_cast<double>(S.FOV);
	cam["NearClip"] = static_cast<double>(S.NearZ);
	cam["FarClip"] = static_cast<double>(S.FarZ);

	return cam;
}

void FSceneSaveManager::DeserializePrimitives(json::JSON& Primitives, UWorld* World, std::unordered_map<string, AActor*>& OutCreatedActors)
{
	using namespace json;

	// Octree 일괄 삽입을 위해 생성된 Actor를 모아둠
	TArray<AActor*> CreatedActors;
	CreatedActors.reserve(Primitives.size());

	for (auto& kv : Primitives.ObjectRange()) {
		const string& Key = kv.first;
		JSON& Entry = kv.second;

		if (!Entry.hasKey("Type")) continue;
		if (Entry["Type"].ToString() != "StaticMeshComp") continue;

		string MeshPath = Entry.hasKey("ObjStaticMeshAsset") ? Entry["ObjStaticMeshAsset"].ToString() : string("None");

		// Spawn a static mesh actor and initialize
		AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>();
		if (!Actor) continue;
		Actor->InitDefaultComponents(FString(MeshPath));
		OutCreatedActors[Key] = Actor;

		if (Entry.hasKey("Material"))
		{
			if (UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(Actor->GetRootComponent()))
			{
				ApplySingleMaterialOverride(StaticMeshComponent, Entry["Material"]);
			}
		}

		// Location / Rotation / Scale — 인덱스 직접 접근으로 iterator 순회 제거
		FVector Loc(0, 0, 0), Rot(0, 0, 0), Scale(1, 1, 1);

		if (Entry.hasKey("Location")) {
			JSON& arr = Entry["Location"];
			Loc.X = static_cast<float>(arr[0].ToFloat());
			Loc.Y = static_cast<float>(arr[1].ToFloat());
			Loc.Z = static_cast<float>(arr[2].ToFloat());
		}
		if (Entry.hasKey("Rotation")) {
			JSON& arr = Entry["Rotation"];
			Rot.X = static_cast<float>(arr[0].ToFloat());
			Rot.Y = static_cast<float>(arr[1].ToFloat());
			Rot.Z = static_cast<float>(arr[2].ToFloat());
		}
		if (Entry.hasKey("Scale")) {
			JSON& arr = Entry["Scale"];
			Scale.X = static_cast<float>(arr[0].ToFloat());
			Scale.Y = static_cast<float>(arr[1].ToFloat());
			Scale.Z = static_cast<float>(arr[2].ToFloat());
		}

		Actor->SetActorLocation(Loc);
		Actor->SetActorRotation(Rot);
		Actor->SetActorScale(Scale);
		World->RemoveActorToOctree(Actor);
		World->InsertActorToOctree(Actor);

		CreatedActors.push_back(Actor);
	}

	// Octree 일괄 삽입
	/*for (AActor* Actor : CreatedActors)
	{
		World->InsertActorToOctree(Actor);
	}*/
}

void FSceneSaveManager::DeserializeCamera(json::JSON& CameraJSON, FPerspectiveCameraData& OutCam)
{
	using namespace json;
	if (CameraJSON.JSONType() == JSON::Class::Null) return;

	if (CameraJSON.hasKey("Location")) OutCam.Location = ReadVec3(CameraJSON["Location"]);
	if (CameraJSON.hasKey("Rotation")) OutCam.Rotation = ReadVec3(CameraJSON["Rotation"]);
	if (CameraJSON.hasKey("FOV")) {
		auto& Val = CameraJSON["FOV"];
		float fov = static_cast<float>(Val.JSONType() == JSON::Class::Array ? Val[0].ToFloat() : Val.ToFloat());
		// 엔진 내부는 라디안 — π(~3.14)를 넘으면 degree로 간주하고 변환
		if (fov > 3.14159265f) fov *= (3.14159265f / 180.0f);
		OutCam.FOV = fov;
	}
	if (CameraJSON.hasKey("NearClip")) {
		auto& Val = CameraJSON["NearClip"];
		OutCam.NearClip = static_cast<float>(Val.JSONType() == JSON::Class::Array ? Val[0].ToFloat() : Val.ToFloat());
	}
	if (CameraJSON.hasKey("FarClip")) {
		auto& Val = CameraJSON["FarClip"];
		OutCam.FarClip = static_cast<float>(Val.JSONType() == JSON::Class::Array ? Val[0].ToFloat() : Val.ToFloat());
	}
	OutCam.bValid = true;
}

AActor* FSceneSaveManager::DeserializeActorIntoWorld(UWorld* World, json::JSON& ActorJSON, AActor* ExistingActor)
{
	if (!World || !ActorJSON.hasKey(SceneKeys::ClassName))
	{
		return nullptr;
	}

	const string ActorClass = ActorJSON[SceneKeys::ClassName].ToString();
	AActor* Actor = ExistingActor;

	if (!Actor)
	{
		if (IsGameModeClassName(ActorClass))
		{
			Actor = reinterpret_cast<AActor*>(World->GetAuthorGameMode());
		}
		else
		{
			UObject* ActorObj = FObjectFactory::Get().Create(ActorClass, World);
			if (!ActorObj || !ActorObj->IsA<AActor>())
			{
				return nullptr;
			}

			Actor = static_cast<AActor*>(ActorObj);
			World->AddActor(Actor);
		}
	}

	if (!Actor)
	{
		return nullptr;
	}

	if (ActorJSON.hasKey(SceneKeys::UUID))
	{
		Actor->SetUUID(static_cast<uint32>(ActorJSON[SceneKeys::UUID].ToInt()));
	}
	if (ActorJSON.hasKey(SceneKeys::ObjectName))
	{
		Actor->SetFName(FName(ActorJSON[SceneKeys::ObjectName].ToString()));
	}

	if (ActorJSON.hasKey(SceneKeys::Visible)) {
		Actor->SetVisible(ActorJSON[SceneKeys::Visible].ToBool());
	}
	if (ActorJSON.hasKey(SceneKeys::FolderPath)) {
		Actor->SetFolderPath(ActorJSON[SceneKeys::FolderPath].ToString());
	}

	ClearActorComponentsForDeserialization(Actor);

	if (ActorJSON.hasKey(SceneKeys::RootComponent)) {
		json::JSON& RootJSON = ActorJSON[SceneKeys::RootComponent];
		USceneComponent* Root = DeserializeSceneComponentTree(RootJSON, Actor);
		if (Root) Actor->SetRootComponent(Root);
	}

	if (ActorJSON.hasKey(SceneKeys::NonSceneComponents)) {
		for (auto& CompJSON : ActorJSON[SceneKeys::NonSceneComponents].ArrayRange()) {
			string CompClass = CompJSON[SceneKeys::ClassName].ToString();
			UObject* CompObj = FObjectFactory::Get().Create(CompClass, Actor);
			if (!CompObj || !CompObj->IsA<UActorComponent>()) continue;

			UActorComponent* Comp = static_cast<UActorComponent*>(CompObj);
			Actor->RegisterComponent(Comp);
			if (CompJSON.hasKey(SceneKeys::ObjectName))
			{
				Comp->SetFName(FName(CompJSON[SceneKeys::ObjectName].ToString()));
			}

			if (CompJSON.hasKey(SceneKeys::Properties)) {
				json::JSON& PropsJSON = CompJSON[SceneKeys::Properties];
				DeserializeProperties(Comp, PropsJSON);
			}
			DeserializeComponentEditorMetadata(Comp, CompJSON);
		}
	}

	Actor->EnsureEditorBillboardForActor();

	World->RemoveActorToOctree(Actor);
	World->InsertActorToOctree(Actor);

	return Actor;
}

// ============================================================
// Load
// ============================================================

AActor* FSceneSaveManager::LoadActorFromJSONString(const string& ActorJson, UWorld* World)
{
	if (!World || ActorJson.empty())
	{
		return nullptr;
	}

	json::JSON Root = json::JSON::Load(ActorJson);
	if (!Root.hasKey("Actor"))
	{
		return nullptr;
	}

	json::JSON& ActorJSON = Root["Actor"];
	return DeserializeActorIntoWorld(World, ActorJSON);
}

bool FSceneSaveManager::ApplyActorFromJSONString(AActor* Actor, const string& ActorJson)
{
	if (!Actor || ActorJson.empty())
	{
		return false;
	}

	ULevel* OwningLevel = Cast<ULevel>(Actor->GetOuter());
	if (!OwningLevel)
	{
		return false;
	}

	UWorld* World = OwningLevel->GetWorld();
	if (!World)
	{
		return false;
	}

	json::JSON Root = json::JSON::Load(ActorJson);
	if (!Root.hasKey("Actor"))
	{
		return false;
	}

	json::JSON& ActorJSON = Root["Actor"];
	return DeserializeActorIntoWorld(World, ActorJSON, Actor) != nullptr;
}

void FSceneSaveManager::LoadSceneFromJSON(const string& filepath, FWorldContext& OutWorldContext, FPerspectiveCameraData& OutCam)
{
	std::ifstream File(std::filesystem::path(FPaths::ToWide(filepath)));
	if (!File.is_open()) {
		std::cerr << "Failed to open file at target destination" << std::endl;
		return;
	}

	string FileContent((std::istreambuf_iterator<char>(File)),
		std::istreambuf_iterator<char>());
	LoadSceneFromJSONString(FileContent, OutWorldContext, OutCam);
}

void FSceneSaveManager::LoadSceneFromJSONString(const string& SceneJson, FWorldContext& OutWorldContext, FPerspectiveCameraData& OutCam)
{
	using json::JSON;

	JSON root = JSON::Load(SceneJson);

	string ClassName = root[SceneKeys::ClassName].ToString();
	ClassName = ClassName.empty() ? "UWorld" : ClassName;
	UObject* WorldObj = FObjectFactory::Get().Create(ClassName);
	if (!WorldObj || !WorldObj->IsA<UWorld>()) return;

	UWorld* World = static_cast<UWorld*>(WorldObj);

	EWorldType WorldType = root.hasKey(SceneKeys::WorldType)
		? StringToWorldType(root[SceneKeys::WorldType].ToString())
		: EWorldType::Editor;
	FString ContextName = root.hasKey(SceneKeys::ContextName)
		? root[SceneKeys::ContextName].ToString()
		: "Loaded Scene";
	FString ContextHandle = root.hasKey(SceneKeys::ContextHandle)
		? root[SceneKeys::ContextHandle].ToString()
		: ContextName;

	// GameMode 클래스를 InitWorld 전에 PersistentLevel에 주입할 수 없으므로
	// 일단 World를 만든 뒤 PersistentLevel에 메타데이터로 설정한다.
	World->InitWorld();
	if (root.hasKey(SceneKeys::GameModeClass))
	{
		if (ULevel* PersistentLevel = World->GetPersistentLevel())
		{
			PersistentLevel->SetGameModeClassName(root[SceneKeys::GameModeClass].ToString());
		}
	}

	if (root.hasKey(SceneKeys::OutlinerFolders))
	{
		if (ULevel* PersistentLevel = World->GetPersistentLevel())
		{
			TArray<FString> FolderNames;
			for (auto& FolderJSON : root[SceneKeys::OutlinerFolders].ArrayRange())
			{
				const FString FolderName = FolderJSON.ToString();
				if (!FolderName.empty())
				{
					FolderNames.push_back(FolderName);
				}
			}
			PersistentLevel->SetOutlinerFolders(FolderNames);
		}
	}

	std::unordered_map<string, AActor*> CreatedFromPrimitives;
	if (root.hasKey("Primitives")) {
		JSON& Prims = root["Primitives"];
		DeserializePrimitives(Prims, World, CreatedFromPrimitives);
	}

	const char* CamKey = root.hasKey("PerspectiveCamera") ? "PerspectiveCamera"
		: root.hasKey("Camera") ? "Camera"
		: nullptr;
	if (CamKey) {
		JSON& Cam = root[CamKey];
		DeserializeCamera(Cam, OutCam);
	}

	if (root.hasKey(SceneKeys::Actors))
	{
		for (auto& ActorJSON : root[SceneKeys::Actors].ArrayRange()) {
			AActor* Actor = nullptr;
			if (ActorJSON.hasKey("PrimitiveKey")) {
				string pk = ActorJSON["PrimitiveKey"].ToString();
				auto it = CreatedFromPrimitives.find(pk);
				if (it != CreatedFromPrimitives.end()) {
					Actor = it->second;
				}
			}

			DeserializeActorIntoWorld(World, ActorJSON, Actor);
		}
	}

	OutWorldContext.WorldType = WorldType;
	OutWorldContext.World = World;
	OutWorldContext.ContextName = ContextName;
	OutWorldContext.ContextHandle = FName(ContextHandle);
}

void FSceneSaveManager::SaveWorldToBinary(const FString& FilePath, UWorld* World)
{
	if (!World)
		return;
	FWindowsBinWriter Ar(FilePath);
	if (Ar.IsValid())
	{
		World->Serialize(Ar);
	}
	else
	{
		std::cerr << "Failed to open file for writing: " << FilePath << std::endl;
	}
}

void FSceneSaveManager::LoadWorldFromBinary(const FString& FilePath, UWorld* World)
{
	if (!World)
		return;
	FWindowsBinReader Ar(FilePath);
	if (Ar.IsValid())
	{
		//World data Initialize
		World->EndPlay();
		World->InitWorld();
		//load world data
		World->Serialize(Ar);

		if (World->GetWorldType()== EWorldType::Game|| World->GetWorldType() == EWorldType::PIE)
		{
			World->BeginPlay();
		}
	}
	else
	{
		std::cerr << "Failed to open file for reading: " << FilePath << std::endl;
	}
}

// ============================================================
// Cooking — Editor용 .Scene(JSON)을 Shipping용 .umap(바이너리)로 변환.
// 핵심:
//   JSON 로드해서 임시 UWorld 생성
//   모든 Actor의 EditorOnly 컴포넌트 제거 
//   WorldType을 Game으로 강제
//   바이너리로 저장
// ============================================================
namespace
{
	// 액터에서 editor-only 컴포넌트들을 모두 떼어낸다.
	// AActor::RemoveComponent는 OwnedComponents에서만 제거하고 scene tree(parent.Children)는 끊지 않으므로
	// SerializeSceneComponentTree가 부모를 따라 다시 직렬화할 수 있다. → 명시적으로 detach까지 수행.
	void StripEditorOnlyComponentsFromActor(AActor* Actor)
	{
		if (!Actor) return;

		const TArray<UActorComponent*> Snapshot = Actor->GetComponents();
		for (auto It = Snapshot.rbegin(); It != Snapshot.rend(); ++It)
		{
			UActorComponent* Comp = *It;
			if (!Comp) continue;
			if (!Comp->IsEditorOnlyComponent()) continue;

			// Scene tree에서 부모 분리 (SerializeSceneComponentTree가 못 보게)
			if (USceneComponent* SceneComp = Cast<USceneComponent>(Comp))
			{
				SceneComp->SetParent(nullptr);
			}
			Actor->RemoveComponent(Comp);
		}
	}
}

bool FSceneSaveManager::CookSceneToBinary(const FString& InSceneJsonPath, const FString& OutUmapPath)
{
	std::filesystem::path InPath(FPaths::ToWide(InSceneJsonPath));
	if (!std::filesystem::exists(InPath))
	{
		std::cerr << "[Cook] Source scene not found: " << InSceneJsonPath << std::endl;
		return false;
	}

	// JSON을 임시 컨텍스트에 로드 — 자체 World가 새로 생성됨
	FWorldContext TempCtx;
	TempCtx.WorldType = EWorldType::Game;
	TempCtx.ContextHandle = FName("CookTarget");
	TempCtx.ContextName = "CookTarget";
	FPerspectiveCameraData DummyCam;
	LoadSceneFromJSON(InSceneJsonPath, TempCtx, DummyCam);

	UWorld* World = TempCtx.World;
	if (!World)
	{
		std::cerr << "[Cook] Failed to deserialize scene: " << InSceneJsonPath << std::endl;
		return false;
	}

	// 모든 액터의 editor-only 컴포넌트 제거
	for (AActor* Actor : World->GetActors())
	{
		if (!Actor) continue;
		StripEditorOnlyComponentsFromActor(Actor);
	}

	// 런타임용으로 WorldType 강제
	World->SetWorldType(EWorldType::Game);

	// 다시 JSON으로 직렬화 — 바이너리 경로(AActor::Serialize)가 컴포넌트를 저장하지 않으므로
	// "stripped JSON"을 .umap에 쓴다. 런타임은 .umap을 만나면 JSON 파서로 처리한다.
	TempCtx.WorldType = EWorldType::Game;
	const std::string CookedJson = SerializeWorldToJSONString(TempCtx, /*PerspectiveCam=*/nullptr);

	std::filesystem::path OutPath(FPaths::ToWide(OutUmapPath));
	if (OutPath.has_parent_path())
	{
		std::filesystem::create_directories(OutPath.parent_path());
	}

	std::ofstream Out(OutPath, std::ios::binary);
	if (!Out.is_open())
	{
		std::cerr << "[Cook] Failed to open output: " << OutUmapPath << std::endl;
		return false;
	}
	Out << CookedJson;
	Out.close();

	const bool bOk = std::filesystem::exists(OutPath);
	if (bOk)
	{
		std::cerr << "[Cook] OK: " << InSceneJsonPath << " -> " << OutUmapPath << std::endl;
	}
	else
	{
		std::cerr << "[Cook] FAILED to write: " << OutUmapPath << std::endl;
	}

	// 임시 World는 의도적으로 누수 — destroy 시 외부 참조(Render proxy 등)와 충돌 위험
	return bOk;
}

int32 FSceneSaveManager::CookAllScenes(const FString& OutputSceneRoot)
{
	int32 Cooked = 0;
	const std::wstring SceneDir = GetSceneDirectory();
	if (!std::filesystem::exists(SceneDir))
	{
		return 0;
	}

	const std::filesystem::path OutputRoot = OutputSceneRoot.empty()
		? std::filesystem::path()
		: std::filesystem::path(FPaths::ToWide(OutputSceneRoot));

	for (auto& Entry : std::filesystem::recursive_directory_iterator(SceneDir))
	{
		if (!Entry.is_regular_file()) continue;
		const std::wstring Ext = Entry.path().extension().wstring();
		if (Ext != SceneExtension) continue;

		const std::filesystem::path InPath = Entry.path();
		std::filesystem::path OutPath;
		if (OutputRoot.empty())
		{
			OutPath = InPath;
			OutPath.replace_extension(L".umap");
		}
		else
		{
			OutPath = OutputRoot / std::filesystem::relative(InPath, SceneDir);
			OutPath.replace_extension(L".umap");
		}

		const FString InUtf8 = FPaths::ToUtf8(InPath.wstring());
		const FString OutUtf8 = FPaths::ToUtf8(OutPath.wstring());
		if (CookSceneToBinary(InUtf8, OutUtf8))
		{
			++Cooked;
		}
	}

	return Cooked;
}

	bool FSceneSaveManager::IsJsonFile(const FString& FilePath)
	{
	std::ifstream File(std::filesystem::path(FPaths::ToWide(FilePath)), std::ios::binary);
	if (!File.is_open()) return false;
	char Ch = 0;
	while (File.get(Ch))
	{
		// Skip whitespace and BOM
		if (Ch == ' ' || Ch == '\t' || Ch == '\r' || Ch == '\n' || Ch == '\xEF' || Ch == '\xBB' || Ch == '\xBF') continue;
		return Ch == '{';
	}
	return false;
	}

	USceneComponent* FSceneSaveManager::DeserializeSceneComponentTree(json::JSON& Node, AActor* Owner)
{
	string ClassName = Node[SceneKeys::ClassName].ToString();
	UObject* Obj = FObjectFactory::Get().Create(ClassName, Owner);
	if (!Obj || !Obj->IsA<USceneComponent>()) return nullptr;

	USceneComponent* Comp = static_cast<USceneComponent*>(Obj);
	Owner->RegisterComponent(Comp);
	if (Node.hasKey(SceneKeys::ObjectName))
	{
		Comp->SetFName(FName(Node[SceneKeys::ObjectName].ToString()));
	}

	// Restore properties
	if (Node.hasKey(SceneKeys::Properties)) {
		json::JSON& PropsJSON = Node[SceneKeys::Properties];
		DeserializeProperties(Comp, PropsJSON);
	}
	DeserializeComponentEditorMetadata(Comp, Node);
	Comp->MarkTransformDirty();

	// Restore children recursively
	if (Node.hasKey(SceneKeys::Children)) {
		for (auto& ChildJSON : Node[SceneKeys::Children].ArrayRange()) {
			USceneComponent* Child = DeserializeSceneComponentTree(ChildJSON, Owner);
			if (Child) {
				Child->AttachToComponent(Comp);
			}
		}
	}

	EnsureEditorBillboardMetadata(Comp);

	return Comp;
}

void FSceneSaveManager::DeserializeSceneComponentIntoExisting(USceneComponent* Existing, json::JSON& Node, AActor* Owner)
{
	using namespace json;
	if (!Existing) return;

	if (Node.hasKey(SceneKeys::Properties)) {
		JSON& PropsJSON = Node[SceneKeys::Properties];
		DeserializeProperties(Existing, PropsJSON);
	}
	DeserializeComponentEditorMetadata(Existing, Node);

	// Children: merge into existing children by order; create new children if missing
	if (Node.hasKey(SceneKeys::Children)) {
		auto& ChildrenJSON = Node[SceneKeys::Children];
		auto ExistingChildren = Existing->GetChildren();

		size_t idx = 0;
		for (auto& ChildJSON : ChildrenJSON.ArrayRange()) {
			if (idx < ExistingChildren.size()) {
				DeserializeSceneComponentIntoExisting(ExistingChildren[idx], const_cast<json::JSON&>(ChildJSON), Owner);
			}
			else {
				USceneComponent* NewChild = DeserializeSceneComponentTree(const_cast<json::JSON&>(ChildJSON), Owner);
				if (NewChild) NewChild->AttachToComponent(Existing);
			}
			idx++;
		}
	}

	EnsureEditorBillboardMetadata(Existing);
}

void FSceneSaveManager::DeserializeProperties(UActorComponent* Comp, json::JSON& PropsJSON)
{
	TArray<FPropertyDescriptor> Descriptors;
	Comp->GetEditableProperties(Descriptors);

	for (auto& Prop : Descriptors) {
		const char* PropertyKey = Prop.Name.c_str();
		if (!PropsJSON.hasKey(PropertyKey))
		{
			if (Prop.Name == "Nine Slice Border" && PropsJSON.hasKey("Slice"))
			{
				PropertyKey = "Slice";
			}
			else
			{
				continue;
			}
		}
		json::JSON& Value = PropsJSON[PropertyKey];
		DeserializePropertyValue(Prop, Value);
		Comp->PostEditProperty(Prop.Name.c_str());
	}

	// 2nd pass: PostEditProperty가 새 프로퍼티를 추가할 수 있음
	// (예: SetStaticMesh → MaterialSlots 생성 → "Element N" 디스크립터 추가)
	TArray<FPropertyDescriptor> Descriptors2;
	Comp->GetEditableProperties(Descriptors2);

	for (size_t i = Descriptors.size(); i < Descriptors2.size(); ++i) {
		auto& Prop = Descriptors2[i];
		if (!PropsJSON.hasKey(Prop.Name.c_str())) continue;
		json::JSON& Value = PropsJSON[Prop.Name.c_str()];
		DeserializePropertyValue(Prop, Value);
		Comp->PostEditProperty(Prop.Name.c_str());
	}

	if (UBillboardComponent* BillboardComponent = Cast<UBillboardComponent>(Comp))
	{
		if (PropsJSON.hasKey("Material") && !PropsJSON.hasKey("Texture"))
		{
			TArray<FPropertyDescriptor> BillboardProps;
			BillboardComponent->GetEditableProperties(BillboardProps);
			for (FPropertyDescriptor& Prop : BillboardProps)
			{
				if (Prop.Name == "Texture" && Prop.Type == EPropertyType::TextureSlot)
				{
					json::JSON& Value = PropsJSON["Material"];
					DeserializePropertyValue(Prop, Value);
					BillboardComponent->PostEditProperty("Texture");
					break;
				}
			}
		}
	}

	if (UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(Comp))
	{
		if (PropsJSON.hasKey("Material") && !PropsJSON.hasKey("Element 0"))
		{
			ApplySingleMaterialOverride(StaticMeshComponent, PropsJSON["Material"]);
		}
	}

}

void FSceneSaveManager::DeserializePropertyValue(FPropertyDescriptor& Prop, json::JSON& Value)
{
	switch (Prop.Type) {
	case EPropertyType::Bool:
		*static_cast<bool*>(Prop.ValuePtr) = Value.ToBool();
		break;

	case EPropertyType::ByteBool:
		*static_cast<uint8_t*>(Prop.ValuePtr) = Value.ToBool() ? 1 : 0;
		break;

	case EPropertyType::Int:
		*static_cast<int32*>(Prop.ValuePtr) = Value.ToInt();
		break;

	case EPropertyType::Float:
		*static_cast<float*>(Prop.ValuePtr) = static_cast<float>(Value.ToFloat());
		break;

	case EPropertyType::Vec3: {
		float* v = static_cast<float*>(Prop.ValuePtr);
		int i = 0;
		for (auto& elem : Value.ArrayRange()) {
			if (i < 3) v[i] = static_cast<float>(elem.ToFloat());
			i++;
		}
		break;
	}
	case EPropertyType::Rotator: {
		float* v = static_cast<float*>(Prop.ValuePtr);
		int i = 0;
		for (auto& elem : Value.ArrayRange()) {
			if (i < 3) v[i] = static_cast<float>(elem.ToFloat());
			i++;
		}
		break;
	}
	case EPropertyType::Vec4:
	case EPropertyType::Color4: {
		float* v = static_cast<float*>(Prop.ValuePtr);
		int i = 0;
		for (auto& elem : Value.ArrayRange()) {
			if (i < 4) v[i] = static_cast<float>(elem.ToFloat());
			i++;
		}
		break;
	}
	case EPropertyType::String:
	case EPropertyType::SceneComponentRef:
	case EPropertyType::StaticMeshRef:
		*static_cast<FString*>(Prop.ValuePtr) = Value.ToString();
		break;

	case EPropertyType::MaterialSlot: {
		FMaterialSlot* Slot = static_cast<FMaterialSlot*>(Prop.ValuePtr);
		Slot->Path = ResolveMaterialPath(ReadAssetPathValue(Value));
		break;
	}

	case EPropertyType::TextureSlot: {
		FTextureSlot* Slot = static_cast<FTextureSlot*>(Prop.ValuePtr);
		Slot->Path = ResolveTexturePath(ReadAssetPathValue(Value));
		break;
	}

	case EPropertyType::Name:
		*static_cast<FName*>(Prop.ValuePtr) = FName(Value.ToString());
		break;

	case EPropertyType::Enum:
		*static_cast<int32*>(Prop.ValuePtr) = Value.ToInt();
		break;

	case EPropertyType::Vec3Array: {
		TArray<FVector>* Arr = static_cast<TArray<FVector>*>(Prop.ValuePtr);
		Arr->clear();
		for (auto& elem : Value.ArrayRange()) {
			FVector v(0, 0, 0);
			int i = 0;
			for (auto& c : elem.ArrayRange()) {
				if (i == 0) v.X = static_cast<float>(c.ToFloat());
				else if (i == 1) v.Y = static_cast<float>(c.ToFloat());
				else if (i == 2) v.Z = static_cast<float>(c.ToFloat());
				++i;
			}
			Arr->push_back(v);
		}
		break;
	}

	default:
		break;
	}
}

// ============================================================
// Utility
// ============================================================

string FSceneSaveManager::GetCurrentTimeStamp()
{
	std::time_t t = std::time(nullptr);
	std::tm tm{};
	localtime_s(&tm, &t);

	char buf[20];
	std::strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", &tm);
	return buf;
}

TArray<FString> FSceneSaveManager::GetSceneFileList()
{
	TArray<FString> Result;
	std::wstring SceneDir = GetSceneDirectory();
	if (!std::filesystem::exists(SceneDir))
	{
		return Result;
	}

	const std::filesystem::path SceneRoot(SceneDir);
	std::unordered_set<FString> Seen;
	for (auto& Entry : std::filesystem::recursive_directory_iterator(SceneRoot))
	{
		if (!Entry.is_regular_file()) continue;

		auto Ext = Entry.path().extension().wstring();
		if (Ext != SceneExtension && Ext != L".umap" && Ext != L".UMAP") continue;

		std::filesystem::path Rel = std::filesystem::relative(Entry.path(), SceneRoot);
		Rel.replace_extension();
		std::wstring RelW = Rel.generic_wstring();
		FString RelUtf8 = FPaths::ToUtf8(RelW);
		if (Seen.insert(RelUtf8).second)
		{
			Result.push_back(std::move(RelUtf8));
		}
	}
	std::sort(Result.begin(), Result.end());
	return Result;
}
