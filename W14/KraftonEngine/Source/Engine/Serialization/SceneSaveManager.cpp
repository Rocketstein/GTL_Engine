#include "SceneSaveManager.h"

#include <algorithm>
#include <iostream>
#include <fstream>
#include <chrono>
#include <cstring>
#include "SimpleJSON/json.hpp"
#include "GameFramework/World.h"
#include "GameFramework/AActor.h"
#include "Component/SceneComponent.h"
#include "Component/ActorComponent.h"
#include "Render/Types/MinimalViewInfo.h"
#include "Component/Primitive/DecalComponent.h"
#include "Component/Primitive/HeightFogComponent.h"
#include "Component/Light/LightComponentBase.h"
#include "Object/Object.h"
#include "Object/GarbageCollection.h"
#include "Object/Reflection/ObjectFactory.h"
#include "Core/Types/PropertyTypes.h"
#include "Object/FName.h"
#include "Serialization/JsonArchive.h"
#include "Animation/AnimationManager.h"
#include "Animation/Montage/AnimMontage.h"
#include "Profiling/Time/PlatformTime.h"
#include "Core/Logging/Log.h"

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

static bool IsSceneSerializableObject(const UObject* Object)
{
	return IsValid(Object);
}

static bool IsSceneComponentReachableFromRootTree(const USceneComponent* Root, const USceneComponent* Target)
{
	if (!IsSceneSerializableObject(Root) || !IsSceneSerializableObject(Target))
	{
		return false;
	}

	if (Root == Target)
	{
		return true;
	}

	for (USceneComponent* Child : Root->GetChildren())
	{
		if (IsSceneComponentReachableFromRootTree(Child, Target))
		{
			return true;
		}
	}

	return false;
}

static bool WriteSceneAssetReference(const UObject* Object, json::JSON& Node)
{
	if (const UAnimMontage* Montage = Cast<UAnimMontage>(Object))
	{
		const FString& AssetPath = Montage->GetAssetPathFileName();
		if (!AssetPath.empty() && AssetPath != "None")
		{
			Node = json::Object();
			Node["ClassName"] = Montage->GetClass()->GetName();
			Node["AssetPath"] = AssetPath;
			return true;
		}
	}

	return false;
}

static UObject* LoadSceneAssetReference(const FString& ClassName, const FString& AssetPath)
{
	if (AssetPath.empty() || AssetPath == "None")
	{
		return nullptr;
	}

	if (ClassName.empty() || ClassName == UAnimMontage::StaticClass()->GetName())
	{
		return FAnimationManager::Get().LoadMontage(AssetPath);
	}

	return nullptr;
}

// ---------------------------------------------------------------------------

namespace SceneKeys
{
	static constexpr const char* Version = "Version";
	static constexpr const char* Name = "Name";
	static constexpr const char* ClassName = "ClassName";
	static constexpr const char* WorldType = "WorldType";
	static constexpr const char* ContextName = "ContextName";
	static constexpr const char* ContextHandle = "ContextHandle";
	static constexpr const char* WorldSettings = "WorldSettings";
	static constexpr const char* GameMode = "GameMode";  // legacy / WorldSettings 내부 키
	static constexpr const char* Gravity = "Gravity";
	static constexpr const char* Navigation = "Navigation";
	static constexpr const char* NavCellSize = "CellSize";
	static constexpr const char* NavMaxSearchNodes = "MaxSearchNodes";
	static constexpr const char* NavAgentRadius = "AgentRadius";
	static constexpr const char* NavAgentHeight = "AgentHeight";
	static constexpr const char* NavAgentStepHeight = "AgentStepHeight";
	static constexpr const char* NavAgentMaxClimbHeight = "AgentMaxClimbHeight";
	static constexpr const char* NavAgentMaxDropHeight = "AgentMaxDropHeight";
	static constexpr const char* NavAgentMaxSlopeDegrees = "AgentMaxSlopeDegrees";
	static constexpr const char* NavProjectionUp = "ProjectionUp";
	static constexpr const char* NavProjectionDown = "ProjectionDown";
	static constexpr const char* NavDirectPathSegmentLength = "DirectPathSegmentLength";
	static constexpr const char* NavObstaclePadding = "ObstaclePadding";
	static constexpr const char* NavUsePhysicsProjectionFallback = "bUsePhysicsProjectionFallback";
	static constexpr const char* NavUseNavigationData = "bUseNavigationData";
	static constexpr const char* NavAutoRebuildOnPathRequest = "bAutoRebuildOnPathRequest";
	static constexpr const char* NavAllowRuntimeFallback = "bAllowRuntimeFallback";
	static constexpr const char* NavEnableRuntimeAutoRebuild = "bEnableRuntimeAutoRebuild";
	static constexpr const char* NavRuntimeRebuildDelay = "RuntimeRebuildDelay";
	static constexpr const char* NavRuntimeRebuildMinInterval = "RuntimeRebuildMinInterval";
	static constexpr const char* NavDrawDebugOnBuild = "bDrawDebugOnBuild";
	static constexpr const char* NavDrawBlockedCells = "bDrawBlockedCells";
	static constexpr const char* NavDrawHeightColors = "bDrawHeightColors";
	static constexpr const char* NavDrawHeightContours = "bDrawHeightContours";
	static constexpr const char* NavDebugHeightContourInterval = "DebugHeightContourInterval";
	static constexpr const char* NavDebugDrawDuration = "DebugDrawDuration";
	static constexpr const char* NavDebugDrawMaxCells = "DebugDrawMaxCells";
	static constexpr const char* Actors = "Actors";
	static constexpr const char* RootComponent = "RootComponent";
	static constexpr const char* NonSceneComponents = "NonSceneComponents";
	static constexpr const char* Properties = "Properties";
	static constexpr const char* Children = "Children";
	static constexpr const char* HiddenInComponentTree = "bHiddenInComponentTree";
	static constexpr const char* ObjectId = "ObjectId";
}

class FSceneJsonSaveArchive : public FJsonArchive
{
public:
	FSceneJsonSaveArchive(json::JSON& Root, const FSceneSaveManager::FSceneSaveContext& InContext)
		: FJsonArchive(Root, /*bInIsSaving=*/true)
		, Context(InContext)
	{
	}

protected:
	uint32 ResolveJsonObjectId(const UObject* Object) const override
	{
		return Context.FindObjectId(Object);
	}

	bool SerializeJsonAssetReference(const UObject* Object, json::JSON& Node) const override
	{
		return WriteSceneAssetReference(Object, Node);
	}

private:
	const FSceneSaveManager::FSceneSaveContext& Context;
};

class FSceneJsonLoadArchive : public FJsonArchive
{
public:
	FSceneJsonLoadArchive(json::JSON& Root, const FSceneSaveManager::FSceneLoadContext& InContext)
		: FJsonArchive(Root, /*bInIsSaving=*/false)
		, Context(InContext)
	{
	}

protected:
	UObject* ResolveJsonObjectReference(uint32 ObjectId) const override
	{
		return ObjectId != 0 ? Context.FindObjectById(ObjectId) : nullptr;
	}

	UObject* ResolveJsonAssetReference(const FString& ClassName, const FString& AssetPath) const override
	{
		return LoadSceneAssetReference(ClassName, AssetPath);
	}

private:
	const FSceneSaveManager::FSceneLoadContext& Context;
};

uint32 FSceneSaveManager::FSceneSaveContext::RegisterSceneObject(const UObject* Object)
{
	if (!IsSceneSerializableObject(Object))
	{
		return 0;
	}

	auto It = ObjectToId.find(Object);
	if (It != ObjectToId.end())
	{
		return It->second;
	}

	const uint32 ObjectId = NextObjectId++;
	ObjectToId.emplace(Object, ObjectId);
	return ObjectId;
}

uint32 FSceneSaveManager::FSceneSaveContext::FindObjectId(const UObject* Object) const
{
	if (!IsSceneSerializableObject(Object))
	{
		return 0;
	}

	auto It = ObjectToId.find(Object);
	return It != ObjectToId.end() ? It->second : 0;
}

void FSceneSaveManager::FSceneLoadContext::RegisterLoadedObject(json::JSON& Node, UObject* Object)
{
	if (!IsSceneSerializableObject(Object) || !Node.hasKey(SceneKeys::ObjectId))
	{
		return;
	}

	const uint32 ObjectId = static_cast<uint32>(Node[SceneKeys::ObjectId].ToInt());
	if (ObjectId != 0)
	{
		ObjectById[ObjectId] = Object;
	}
}

UObject* FSceneSaveManager::FSceneLoadContext::FindObjectById(uint32 ObjectId) const
{
	auto It = ObjectById.find(ObjectId);
	return It != ObjectById.end() ? It->second : nullptr;
}

void FSceneSaveManager::FSceneLoadContext::QueueProperties(UObject* Object, json::JSON& Properties)
{
	if (!IsSceneSerializableObject(Object))
	{
		return;
	}

	PendingProperties.push_back({ Object, &Properties });
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

// ============================================================
// Save
// ============================================================

void FSceneSaveManager::SaveSceneAsJSON(const string& InSceneName, FWorldContext& WorldContext, const FMinimalViewInfo* PerspectivePOV)
{
	FScopedGarbageCollectionBlocker GCBlocker;

	if (!IsSceneSerializableObject(WorldContext.World)) return;

	string FinalName = InSceneName.empty()
		? "Save_" + GetCurrentTimeStamp()
		: InSceneName;

	std::wstring SceneDir = GetSceneDirectory();
	std::filesystem::path FileDestination = std::filesystem::path(SceneDir) / (FPaths::ToWide(FinalName) + SceneExtension);
	SaveSceneToFile(FileDestination, FinalName, WorldContext, PerspectivePOV);
}

void FSceneSaveManager::SaveSceneToJSON(const string& InFilePath, FWorldContext& WorldContext, const FMinimalViewInfo* PerspectivePOV)
{
	FScopedGarbageCollectionBlocker GCBlocker;

	if (!IsSceneSerializableObject(WorldContext.World) || InFilePath.empty()) return;

	std::filesystem::path FileDestination(FPaths::ToWide(InFilePath));
	if (!FileDestination.has_filename())
	{
		return;
	}

	if (FileDestination.extension().empty())
	{
		FileDestination += SceneExtension;
	}

	const string SceneName = FPaths::ToUtf8(FileDestination.stem().wstring());
	SaveSceneToFile(FileDestination, SceneName, WorldContext, PerspectivePOV);
}

void FSceneSaveManager::SaveSceneToFile(const std::filesystem::path& FileDestination, const string& SceneName, FWorldContext& WorldContext, const FMinimalViewInfo* PerspectivePOV)
{
	using namespace json;

	if (!IsSceneSerializableObject(WorldContext.World)) return;

	if (std::filesystem::path ParentPath = FileDestination.parent_path(); !ParentPath.empty())
	{
		std::filesystem::create_directories(ParentPath);
	}

	FSceneSaveContext SaveContext;
	CollectWorldObjectIds(WorldContext.World, SaveContext);

	JSON Root = SerializeWorld(WorldContext.World, WorldContext, PerspectivePOV, SaveContext);
	Root[SceneKeys::Version] = 2;
	Root[SceneKeys::Name] = SceneName;

	std::ofstream File(FileDestination);
	if (File.is_open()) {
		File << Root.dump();
		File.flush();
		File.close();
	}
}

void FSceneSaveManager::CollectWorldObjectIds(UWorld* World, FSceneSaveContext& Context)
{
	if (!IsSceneSerializableObject(World))
	{
		return;
	}

	Context.RegisterSceneObject(World);
	for (AActor* Actor : World->GetActors())
	{
		CollectActorObjectIds(Actor, Context);
	}
}

void FSceneSaveManager::CollectActorObjectIds(AActor* Actor, FSceneSaveContext& Context)
{
	if (!IsSceneSerializableObject(Actor))
	{
		return;
	}

	Context.RegisterSceneObject(Actor);
	if (IsSceneSerializableObject(Actor->GetRootComponent()))
	{
		CollectSceneComponentObjectIds(Actor->GetRootComponent(), Context);
	}

	for (UActorComponent* Comp : Actor->GetComponents())
	{
		if (!IsSceneSerializableObject(Comp))
		{
			continue;
		}

		if (Comp->IsA<USceneComponent>())
		{
			continue;
		}

		Context.RegisterSceneObject(Comp);
	}
}

void FSceneSaveManager::CollectSceneComponentObjectIds(USceneComponent* Comp, FSceneSaveContext& Context)
{
	if (!IsSceneSerializableObject(Comp))
	{
		return;
	}

	Context.RegisterSceneObject(Comp);
	for (USceneComponent* Child : Comp->GetChildren())
	{
		if (!IsSceneSerializableObject(Child))
		{
			continue;
		}
		CollectSceneComponentObjectIds(Child, Context);
	}
}

json::JSON FSceneSaveManager::SerializeWorld(UWorld* World, const FWorldContext& Ctx, const FMinimalViewInfo* PerspectivePOV, FSceneSaveContext& Context)
{
	using namespace json;
	JSON w = json::Object();
	w[SceneKeys::ClassName] = World->GetClass()->GetName();
	w[SceneKeys::ObjectId] = static_cast<int>(Context.RegisterSceneObject(World));
	w[SceneKeys::WorldType] = WorldTypeToString(Ctx.WorldType);
	w[SceneKeys::ContextName] = Ctx.ContextName;
	w[SceneKeys::ContextHandle] = Ctx.ContextHandle.ToString();

	// ---- WorldSettings (씬 단위 게임 설정) ----
	{
		const FWorldSettings& WS = World->GetWorldSettings();
		JSON WSObj = json::Object();
		WSObj[SceneKeys::GameMode] = WS.GameModeClassName;
		WriteVec3(WSObj, SceneKeys::Gravity, WS.Gravity);

		JSON NavObj = json::Object();
		NavObj[SceneKeys::NavCellSize] = WS.Navigation.CellSize;
		NavObj[SceneKeys::NavMaxSearchNodes] = WS.Navigation.MaxSearchNodes;
		NavObj[SceneKeys::NavAgentRadius] = WS.Navigation.AgentRadius;
		NavObj[SceneKeys::NavAgentHeight] = WS.Navigation.AgentHeight;
		NavObj[SceneKeys::NavAgentStepHeight] = WS.Navigation.AgentStepHeight;
		NavObj[SceneKeys::NavAgentMaxClimbHeight] = WS.Navigation.AgentMaxClimbHeight;
		NavObj[SceneKeys::NavAgentMaxDropHeight] = WS.Navigation.AgentMaxDropHeight;
		NavObj[SceneKeys::NavAgentMaxSlopeDegrees] = WS.Navigation.AgentMaxSlopeDegrees;
		NavObj[SceneKeys::NavProjectionUp] = WS.Navigation.ProjectionUp;
		NavObj[SceneKeys::NavProjectionDown] = WS.Navigation.ProjectionDown;
		NavObj[SceneKeys::NavDirectPathSegmentLength] = WS.Navigation.DirectPathSegmentLength;
		NavObj[SceneKeys::NavObstaclePadding] = WS.Navigation.ObstaclePadding;
		NavObj[SceneKeys::NavUsePhysicsProjectionFallback] = WS.Navigation.bUsePhysicsProjectionFallback;
		NavObj[SceneKeys::NavUseNavigationData] = WS.Navigation.bUseNavigationData;
		NavObj[SceneKeys::NavAutoRebuildOnPathRequest] = WS.Navigation.bAutoRebuildOnPathRequest;
		NavObj[SceneKeys::NavAllowRuntimeFallback] = WS.Navigation.bAllowRuntimeFallback;
		NavObj[SceneKeys::NavEnableRuntimeAutoRebuild] = WS.Navigation.bEnableRuntimeAutoRebuild;
		NavObj[SceneKeys::NavRuntimeRebuildDelay] = WS.Navigation.RuntimeRebuildDelay;
		NavObj[SceneKeys::NavRuntimeRebuildMinInterval] = WS.Navigation.RuntimeRebuildMinInterval;
		NavObj[SceneKeys::NavDrawDebugOnBuild] = WS.Navigation.bDrawDebugOnBuild;
		NavObj[SceneKeys::NavDrawBlockedCells] = WS.Navigation.bDrawBlockedCells;
		NavObj[SceneKeys::NavDrawHeightColors] = WS.Navigation.bDrawHeightColors;
		NavObj[SceneKeys::NavDrawHeightContours] = WS.Navigation.bDrawHeightContours;
		NavObj[SceneKeys::NavDebugHeightContourInterval] = WS.Navigation.DebugHeightContourInterval;
		NavObj[SceneKeys::NavDebugDrawDuration] = WS.Navigation.DebugDrawDuration;
		NavObj[SceneKeys::NavDebugDrawMaxCells] = WS.Navigation.DebugDrawMaxCells;
		WSObj[SceneKeys::Navigation] = NavObj;
		w[SceneKeys::WorldSettings] = WSObj;
	}

	// ---- Actors ----
	JSON Actors = json::Array();
	for (AActor* Actor : World->GetActors()) {
		if (!IsSceneSerializableObject(Actor)) continue;
		Actors.append(SerializeActor(Actor, Context));
	}
	w[SceneKeys::Actors] = Actors;

	// ---- Perspective camera ----
	JSON cam = SerializeCamera(PerspectivePOV);
	if (cam.size() > 0) {
		w["PerspectiveCamera"] = cam;
	}

	return w;
}

json::JSON FSceneSaveManager::SerializeActor(AActor* Actor, FSceneSaveContext& Context)
{
	using namespace json;
	JSON a = json::Object();
	a[SceneKeys::ClassName] = Actor->GetClass()->GetName();
	a[SceneKeys::ObjectId] = static_cast<int>(Context.RegisterSceneObject(Actor));
	a[SceneKeys::Name] = Actor->GetFName().ToString();
	a[SceneKeys::Properties] = SerializeProperties(Actor, Context);

	// RootComponent 트리 직렬화
	if (IsSceneSerializableObject(Actor->GetRootComponent())) {
		a[SceneKeys::RootComponent] = SerializeSceneComponentTree(Actor->GetRootComponent(), Context);
	}

	// Non-scene components
	JSON NonScene = json::Array();
	for (UActorComponent* Comp : Actor->GetComponents()) {
		if (!IsSceneSerializableObject(Comp)) continue;
		if (Comp->IsA<USceneComponent>())
		{
			USceneComponent* SceneComp = static_cast<USceneComponent*>(Comp);
			if (!IsSceneComponentReachableFromRootTree(Actor->GetRootComponent(), SceneComp))
			{
				UE_LOG("[SceneSave] Skipping detached SceneComponent not reachable from RootComponent tree. Actor=%s Component=%s Class=%s",
					Actor->GetName().c_str(),
					SceneComp->GetName().c_str(),
					SceneComp->GetClass()->GetName());
			}
			continue;
		}

		JSON c = json::Object();
		c[SceneKeys::ClassName] = Comp->GetClass()->GetName();
		c[SceneKeys::ObjectId] = static_cast<int>(Context.RegisterSceneObject(Comp));
		c[SceneKeys::Properties] = SerializeProperties(Comp, Context);
		SerializeComponentEditorMetadata(c, Comp);
		NonScene.append(c);
	}
	a[SceneKeys::NonSceneComponents] = NonScene;

	return a;
}

json::JSON FSceneSaveManager::SerializeSceneComponentTree(USceneComponent* Comp, FSceneSaveContext& Context)
{
	using namespace json;
	JSON c = json::Object();
	if (!IsSceneSerializableObject(Comp))
	{
		return c;
	}
	c[SceneKeys::ClassName] = Comp->GetClass()->GetName();
	c[SceneKeys::ObjectId] = static_cast<int>(Context.RegisterSceneObject(Comp));
	c[SceneKeys::Properties] = SerializeProperties(Comp, Context);
	SerializeComponentEditorMetadata(c, Comp);

	JSON Children = json::Array();
	for (USceneComponent* Child : Comp->GetChildren()) {
		if (!IsSceneSerializableObject(Child)) continue;
		Children.append(SerializeSceneComponentTree(Child, Context));
	}
	c[SceneKeys::Children] = Children;

	return c;
}

json::JSON FSceneSaveManager::SerializeProperties(UObject* Obj, FSceneSaveContext& Context)
{
	using namespace json;
	JSON Props = json::Object();
	if (!IsSceneSerializableObject(Obj)) return Props;

	FSceneJsonSaveArchive Ar(Props, Context);
	Obj->PreSaveForArchive(Ar);
	Obj->SerializeProperties(Ar, PF_Save);
	return Props;
}

// ---- Camera helpers ----

json::JSON FSceneSaveManager::SerializeCamera(const FMinimalViewInfo* POV)
{
	using namespace json;
	JSON cam = json::Object();
	if (!POV) return cam;

	WriteVec3(cam, "Location", POV->Location);
	// FRotator(Pitch, Yaw, Roll) → 직렬화 컨벤션 FVector(Roll, Pitch, Yaw)
	WriteVec3(cam, "Rotation", FVector(POV->Rotation.Roll, POV->Rotation.Pitch, POV->Rotation.Yaw));

	cam["FOV"] = static_cast<double>(POV->FOV);
	cam["NearClip"] = static_cast<double>(POV->NearClip);
	cam["FarClip"] = static_cast<double>(POV->FarClip);

	return cam;
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

// ============================================================
// Load
// ============================================================

void FSceneSaveManager::LoadSceneFromJSON(const string& filepath, FWorldContext& OutWorldContext, FPerspectiveCameraData& OutCam, const EWorldType* OverrideWorldType)
{
	using json::JSON;
	FScopedGarbageCollectionBlocker GCBlocker;
	std::ifstream File(std::filesystem::path(FPaths::ToWide(filepath)));
	if (!File.is_open()) {
		std::cerr << "Failed to open file at target destination" << std::endl;
		return;
	}

	string FileContent((std::istreambuf_iterator<char>(File)),
		std::istreambuf_iterator<char>());

	JSON root = JSON::Load(FileContent);

	string ClassName = root[SceneKeys::ClassName].ToString();
	ClassName = ClassName.empty() ? "UWorld" : ClassName; // Default to "World" if ClassName is missing
	UObject* WorldObj = FObjectFactory::Get().Create(ClassName);
	if (!WorldObj || !WorldObj->IsA<UWorld>()) return;

	UWorld* World = static_cast<UWorld*>(WorldObj);
	FSceneLoadContext LoadContextState;
	LoadContextState.RegisterLoadedObject(root, World);

	EWorldType WorldType = OverrideWorldType
		? *OverrideWorldType
		: (root.hasKey(SceneKeys::WorldType)
			? StringToWorldType(root[SceneKeys::WorldType].ToString())
			: EWorldType::Editor);

	// World 의 WorldType 을 actor deserialize 전에 적용. Default 가 Editor 라 actor 추가
	// 시 CreateRenderState 의 "EditorOnly && WorldType != Editor" 체크가 잘못 통과돼 Game
	// 빌드에서도 editor billboard SceneProxy 가 만들어지는 버그를 막기 위해.
	World->SetWorldType(WorldType);
	FString ContextName = root.hasKey(SceneKeys::ContextName)
		? root[SceneKeys::ContextName].ToString()
		: "Loaded Scene";
	FString ContextHandle = root.hasKey(SceneKeys::ContextHandle)
		? root[SceneKeys::ContextHandle].ToString()
		: ContextName;

	// WorldSettings — scene 단위 게임 설정. 신규 포맷은 root["WorldSettings"] 객체.
	// 구버전 호환: root["GameMode"] (top-level) 도 fallback 으로 읽음.
	FWorldSettings WorldSettings;
	if (root.hasKey(SceneKeys::WorldSettings))
	{
		JSON& WSObj = root[SceneKeys::WorldSettings];
		if (WSObj.hasKey(SceneKeys::GameMode))
		{
			WorldSettings.GameModeClassName = WSObj[SceneKeys::GameMode].ToString();
		}
		if (WSObj.hasKey(SceneKeys::Gravity) &&
			WSObj[SceneKeys::Gravity].JSONType() == JSON::Class::Array)
		{
			WorldSettings.Gravity = ReadVec3(WSObj[SceneKeys::Gravity]);
		}
		if (WSObj.hasKey(SceneKeys::Navigation) &&
			WSObj[SceneKeys::Navigation].JSONType() == JSON::Class::Object)
		{
			JSON& NavObj = WSObj[SceneKeys::Navigation];
			if (NavObj.hasKey(SceneKeys::NavCellSize)) WorldSettings.Navigation.CellSize = static_cast<float>(NavObj[SceneKeys::NavCellSize].ToFloat());
			if (NavObj.hasKey(SceneKeys::NavMaxSearchNodes)) WorldSettings.Navigation.MaxSearchNodes = static_cast<int32>(NavObj[SceneKeys::NavMaxSearchNodes].ToInt());
			if (NavObj.hasKey(SceneKeys::NavAgentRadius)) WorldSettings.Navigation.AgentRadius = static_cast<float>(NavObj[SceneKeys::NavAgentRadius].ToFloat());
			if (NavObj.hasKey(SceneKeys::NavAgentHeight)) WorldSettings.Navigation.AgentHeight = static_cast<float>(NavObj[SceneKeys::NavAgentHeight].ToFloat());
			if (NavObj.hasKey(SceneKeys::NavAgentStepHeight)) WorldSettings.Navigation.AgentStepHeight = static_cast<float>(NavObj[SceneKeys::NavAgentStepHeight].ToFloat());
			if (NavObj.hasKey(SceneKeys::NavAgentMaxClimbHeight))
			{
				WorldSettings.Navigation.AgentMaxClimbHeight = static_cast<float>(NavObj[SceneKeys::NavAgentMaxClimbHeight].ToFloat());
			}
			else if (NavObj.hasKey(SceneKeys::NavAgentStepHeight))
			{
				WorldSettings.Navigation.AgentMaxClimbHeight = WorldSettings.Navigation.AgentStepHeight;
			}
			if (NavObj.hasKey(SceneKeys::NavAgentMaxDropHeight))
			{
				WorldSettings.Navigation.AgentMaxDropHeight = static_cast<float>(NavObj[SceneKeys::NavAgentMaxDropHeight].ToFloat());
			}
			else if (NavObj.hasKey(SceneKeys::NavAgentStepHeight))
			{
				WorldSettings.Navigation.AgentMaxDropHeight = WorldSettings.Navigation.AgentStepHeight;
			}
			if (NavObj.hasKey(SceneKeys::NavAgentMaxSlopeDegrees)) WorldSettings.Navigation.AgentMaxSlopeDegrees = static_cast<float>(NavObj[SceneKeys::NavAgentMaxSlopeDegrees].ToFloat());
			if (NavObj.hasKey(SceneKeys::NavProjectionUp)) WorldSettings.Navigation.ProjectionUp = static_cast<float>(NavObj[SceneKeys::NavProjectionUp].ToFloat());
			if (NavObj.hasKey(SceneKeys::NavProjectionDown)) WorldSettings.Navigation.ProjectionDown = static_cast<float>(NavObj[SceneKeys::NavProjectionDown].ToFloat());
			if (NavObj.hasKey(SceneKeys::NavDirectPathSegmentLength)) WorldSettings.Navigation.DirectPathSegmentLength = static_cast<float>(NavObj[SceneKeys::NavDirectPathSegmentLength].ToFloat());
			if (NavObj.hasKey(SceneKeys::NavObstaclePadding)) WorldSettings.Navigation.ObstaclePadding = static_cast<float>(NavObj[SceneKeys::NavObstaclePadding].ToFloat());
			if (NavObj.hasKey(SceneKeys::NavUsePhysicsProjectionFallback)) WorldSettings.Navigation.bUsePhysicsProjectionFallback = NavObj[SceneKeys::NavUsePhysicsProjectionFallback].ToBool();
			if (NavObj.hasKey(SceneKeys::NavUseNavigationData)) WorldSettings.Navigation.bUseNavigationData = NavObj[SceneKeys::NavUseNavigationData].ToBool();
			if (NavObj.hasKey(SceneKeys::NavAutoRebuildOnPathRequest)) WorldSettings.Navigation.bAutoRebuildOnPathRequest = NavObj[SceneKeys::NavAutoRebuildOnPathRequest].ToBool();
			if (NavObj.hasKey(SceneKeys::NavAllowRuntimeFallback)) WorldSettings.Navigation.bAllowRuntimeFallback = NavObj[SceneKeys::NavAllowRuntimeFallback].ToBool();
			if (NavObj.hasKey(SceneKeys::NavEnableRuntimeAutoRebuild)) WorldSettings.Navigation.bEnableRuntimeAutoRebuild = NavObj[SceneKeys::NavEnableRuntimeAutoRebuild].ToBool();
			if (NavObj.hasKey(SceneKeys::NavRuntimeRebuildDelay)) WorldSettings.Navigation.RuntimeRebuildDelay = static_cast<float>(NavObj[SceneKeys::NavRuntimeRebuildDelay].ToFloat());
			if (NavObj.hasKey(SceneKeys::NavRuntimeRebuildMinInterval)) WorldSettings.Navigation.RuntimeRebuildMinInterval = static_cast<float>(NavObj[SceneKeys::NavRuntimeRebuildMinInterval].ToFloat());
			if (NavObj.hasKey(SceneKeys::NavDrawDebugOnBuild)) WorldSettings.Navigation.bDrawDebugOnBuild = NavObj[SceneKeys::NavDrawDebugOnBuild].ToBool();
			if (NavObj.hasKey(SceneKeys::NavDrawBlockedCells)) WorldSettings.Navigation.bDrawBlockedCells = NavObj[SceneKeys::NavDrawBlockedCells].ToBool();
			if (NavObj.hasKey(SceneKeys::NavDrawHeightColors)) WorldSettings.Navigation.bDrawHeightColors = NavObj[SceneKeys::NavDrawHeightColors].ToBool();
			if (NavObj.hasKey(SceneKeys::NavDrawHeightContours)) WorldSettings.Navigation.bDrawHeightContours = NavObj[SceneKeys::NavDrawHeightContours].ToBool();
			if (NavObj.hasKey(SceneKeys::NavDebugHeightContourInterval)) WorldSettings.Navigation.DebugHeightContourInterval = static_cast<float>(NavObj[SceneKeys::NavDebugHeightContourInterval].ToFloat());
			if (NavObj.hasKey(SceneKeys::NavDebugDrawDuration)) WorldSettings.Navigation.DebugDrawDuration = static_cast<float>(NavObj[SceneKeys::NavDebugDrawDuration].ToFloat());
			if (NavObj.hasKey(SceneKeys::NavDebugDrawMaxCells)) WorldSettings.Navigation.DebugDrawMaxCells = static_cast<int32>(NavObj[SceneKeys::NavDebugDrawMaxCells].ToInt());
		}
	}
	else if (root.hasKey(SceneKeys::GameMode))
	{
		WorldSettings.GameModeClassName = root[SceneKeys::GameMode].ToString();
	}
	World->GetWorldSettings() = WorldSettings;

	World->InitWorld();

	// "PerspectiveCamera" 우선, 구버전 "Camera" 키도 지원
	const char* CamKey = root.hasKey("PerspectiveCamera") ? "PerspectiveCamera"
		: root.hasKey("Camera") ? "Camera"
		: nullptr;
	if (CamKey) {
		JSON& Cam = root[CamKey];
		DeserializeCamera(Cam, OutCam);
	}

	// Deserialize Actors
	if (root.hasKey(SceneKeys::Actors))
	{
		for (auto& ActorJSON : root[SceneKeys::Actors].ArrayRange()) {
			string ActorClass = ActorJSON[SceneKeys::ClassName].ToString();

			UObject* ActorObj = FObjectFactory::Get().Create(ActorClass, World);
			if (!ActorObj || !ActorObj->IsA<AActor>()) continue;
			AActor* Actor = static_cast<AActor*>(ActorObj);
			LoadContextState.RegisterLoadedObject(ActorJSON, Actor);
			World->AddActor(Actor);

			if (ActorJSON.hasKey(SceneKeys::Name)) {
				Actor->SetFName(FName(ActorJSON[SceneKeys::Name].ToString()));
			}

			// RootComponent 트리 복원
			if (ActorJSON.hasKey(SceneKeys::RootComponent)) {
				JSON& RootJSON = ActorJSON[SceneKeys::RootComponent];
				USceneComponent* Root = DeserializeSceneComponentTree(RootJSON, Actor, LoadContextState);
				if (Root) Actor->SetRootComponent(Root);
			}

			// Actor 프로퍼티(Location/Rotation/Scale/Visible 및 서브클래스 추가 항목)
			// 복원 — RootComponent 복원 뒤여야 SetActorLocation 등이 적용됨.
			if (ActorJSON.hasKey(SceneKeys::Properties)) {
				LoadContextState.QueueProperties(Actor, ActorJSON[SceneKeys::Properties]);
			}

			// Non-scene components 복원
			if (ActorJSON.hasKey(SceneKeys::NonSceneComponents)) {
				for (auto& CompJSON : ActorJSON[SceneKeys::NonSceneComponents].ArrayRange()) {
					string CompClass = CompJSON[SceneKeys::ClassName].ToString();
					UObject* CompObj = FObjectFactory::Get().Create(CompClass, Actor);
					if (!CompObj || !CompObj->IsA<UActorComponent>()) continue;

					UActorComponent* Comp = static_cast<UActorComponent*>(CompObj);
					LoadContextState.RegisterLoadedObject(CompJSON, Comp);
					Actor->RegisterComponent(Comp);

					if (CompJSON.hasKey(SceneKeys::Properties)) {
						JSON& PropsJSON = CompJSON[SceneKeys::Properties];
						LoadContextState.QueueProperties(Comp, PropsJSON);
					}
					DeserializeComponentEditorMetadata(Comp, CompJSON);
				}
			}
		}
	}

	for (FPendingPropertyLoad& Pending : LoadContextState.PendingProperties)
	{
		if (IsSceneSerializableObject(Pending.Object) && Pending.Properties)
		{
			DeserializeProperties(Pending.Object, *Pending.Properties, LoadContextState);
		}
	}

	// Components are registered while the object graph is being created so object-id
	// references can be resolved. Some render resources, however, depend on properties
	// that are only applied in the deferred pass above. Rebuild render state once after
	// all reflected properties/PostLoad fixups are complete.
	for (AActor* Actor : World->GetActors())
	{
		if (!IsValid(Actor))
		{
			continue;
		}

		for (UActorComponent* Component : Actor->GetComponents())
		{
			if (!IsValid(Component))
			{
				continue;
			}

			Component->DestroyRenderState();
			if (USceneComponent* SceneComponent = Cast<USceneComponent>(Component))
			{
				SceneComponent->MarkTransformDirty();
			}
			Component->CreateRenderState();
		}
	}

	for (AActor* Actor : World->GetActors())
	{
		if (!IsSceneSerializableObject(Actor))
		{
			continue;
		}

		World->RemoveActorToOctree(Actor);
		World->InsertActorToOctree(Actor);
	}

	OutWorldContext.WorldType = WorldType;
	OutWorldContext.World = World;
	OutWorldContext.ContextName = ContextName;
	OutWorldContext.ContextHandle = FName(ContextHandle);
}

USceneComponent* FSceneSaveManager::DeserializeSceneComponentTree(json::JSON& Node, AActor* Owner, FSceneLoadContext& Context)
{
	if (!IsSceneSerializableObject(Owner))
	{
		return nullptr;
	}

	string ClassName = Node[SceneKeys::ClassName].ToString();
	UObject* Obj = FObjectFactory::Get().Create(ClassName, Owner);
	if (!IsSceneSerializableObject(Obj) || !Obj->IsA<USceneComponent>()) return nullptr;

	USceneComponent* Comp = static_cast<USceneComponent*>(Obj);
	Context.RegisterLoadedObject(Node, Comp);
	Owner->RegisterComponent(Comp);

	// Restore properties
	if (Node.hasKey(SceneKeys::Properties)) {
		json::JSON& PropsJSON = Node[SceneKeys::Properties];
		Context.QueueProperties(Comp, PropsJSON);
	}
	DeserializeComponentEditorMetadata(Comp, Node);
	Comp->MarkTransformDirty();

	// Restore children recursively
	if (Node.hasKey(SceneKeys::Children)) {
		for (auto& ChildJSON : Node[SceneKeys::Children].ArrayRange()) {
			USceneComponent* Child = DeserializeSceneComponentTree(ChildJSON, Owner, Context);
			if (Child) {
				Child->AttachToComponent(Comp);
			}
		}
	}

	EnsureEditorBillboardMetadata(Comp);

	return Comp;
}

void FSceneSaveManager::DeserializeProperties(UObject* Obj, json::JSON& PropsJSON, FSceneLoadContext& Context)
{
	if (!IsSceneSerializableObject(Obj)) return;

	TArray<const FProperty*> Properties;
	Obj->GetClass()->GetPropertyRefs(Properties);
	for (const FProperty* Property : Properties)
	{
		if(!Property || (Property->Flags & PF_Save) == 0)
		{
			continue;
		}

		const char* PropertyKey = Property->Name;
		if (!PropsJSON.hasKey(PropertyKey) && Property->DisplayName && PropsJSON.hasKey(Property->DisplayName))
		{
			PropertyKey = Property->DisplayName;
		}

		if (!PropsJSON.hasKey(PropertyKey))
		{
			continue;
		}

		if (PropertyKey != Property->Name)
		{
			PropsJSON[Property->Name] = PropsJSON[PropertyKey];
		}
	}

	for (const FProperty* Property : Properties)
	{
		if(!Property || (Property->Flags & PF_Save) == 0)
		{
			continue;
		}

		const char* PropertyKey = Property->Name;
		if (!PropsJSON.hasKey(PropertyKey))
		{
			continue;
		}

		if(!Property->GetValuePtrFor(Obj))
		{
			continue;
		}

		FSceneJsonLoadArchive Ar(PropsJSON[PropertyKey], Context);
		Property->Serialize(Obj, Ar);

		FPropertyChangedEvent Event;
		Event.Object = Obj;
		Event.Property = Property;
		Event.PropertyName = Property->Name;
		Event.DisplayName = Property->DisplayName ? Property->DisplayName : Property->Name;
		Event.PropertyPath = Property->Name;
		Event.Type = Property->GetType();
		Event.ChangeType = EPropertyChangeType::Load;
		Obj->PostEditChangeProperty(Event);
	}

	FSceneJsonLoadArchive PostLoadArchive(PropsJSON, Context);
	Obj->PostLoadFromArchive(PostLoadArchive);
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

	std::error_code Error;
	for (std::filesystem::recursive_directory_iterator It(
			SceneDir,
			std::filesystem::directory_options::skip_permission_denied,
			Error),
		End;
		!Error && It != End;
		It.increment(Error))
	{
		const std::filesystem::directory_entry& Entry = *It;
		if (Entry.is_regular_file() && Entry.path().extension() == SceneExtension)
		{
			std::filesystem::path RelativePath = Entry.path().lexically_relative(SceneDir);
			RelativePath.replace_extension();
			Result.push_back(FPaths::ToUtf8(RelativePath.generic_wstring()));
		}
	}
	std::sort(Result.begin(), Result.end());
	return Result;
}
