#pragma once

#include <string>
#include <filesystem>
#include "Core/CoreTypes.h"
#include "Platform/Paths.h"
#include "GameFramework/WorldContext.h"
#include "Math/Vector.h"

// Forward declarations
class UObject;
class UWorld;
class AActor;
class UActorComponent;
class USceneComponent;
class UCameraComponent;

namespace json
{
	class JSON;
}

#include "Core/PropertyTypes.h"

using std::string;

// Perspective 뷰포트 카메라의 씬 스냅샷 — 씬 저장/로드 시 주고받는 순수 데이터
struct FPerspectiveCameraData
{
	FVector Location = FVector(0, 0, 0);
	FVector Rotation = FVector(0, 0, 0); // Euler (Roll, Pitch, Yaw) in degrees
	float   FOV      = 3.14159265f / 3.0f;
	float   NearClip = 0.1f;
	float   FarClip  = 1000.0f;
	bool    bValid   = false;
};

class FSceneSaveManager
{
public:
	static constexpr const wchar_t* SceneExtension = L".Scene";

	static std::wstring GetSceneDirectory() { return FPaths::SceneDir(); }

	static void SaveSceneAsJSON(const FString& ScenePathOrName, FWorldContext& WorldContext, UCameraComponent* PerspectiveCam = nullptr);
	static string SerializeWorldToJSONString(FWorldContext& WorldContext, UCameraComponent* PerspectiveCam = nullptr);
	static string SerializeActorToJSONString(AActor* Actor);
	static void LoadSceneFromJSON(const string& filepath, FWorldContext& OutWorldContext, FPerspectiveCameraData& OutCam);
	static void SaveWorldToBinary(const FString& FilePath, UWorld* World);
	static void LoadWorldFromBinary(const FString& FilePath, UWorld* World);
	static void LoadSceneFromJSONString(const string& SceneJson, FWorldContext& OutWorldContext, FPerspectiveCameraData& OutCam);
	static AActor* LoadActorFromJSONString(const string& ActorJson, UWorld* World);
	static bool ApplyActorFromJSONString(AActor* Actor, const string& ActorJson);

	static TArray<FString> GetSceneFileList();

	// Cooking (Editor -> Shipping 변환) 
	// .Scene을 로드해 editor-only 컴포넌트 제거 + WorldType=Game 강제 후 .umap 바이너리로 저장.
	//  성공 시 true, 입력 파일 누락이거나 파싱 실패 시 false.
	static bool CookSceneToBinary(const FString& InSceneJsonPath, const FString& OutUmapPath);

	// 모든 .Scene 파일을 일괄 쿠킹. 출력 루트가 비어 있으면 같은 디렉터리에 .umap 파일을 생성한다.
	static int32 CookAllScenes(const FString& OutputSceneRoot = "");

	static bool IsJsonFile(const FString& FilePath);

private:
	// ---- Serialization ----
	static json::JSON SerializeWorld(UWorld* World, const FWorldContext& Ctx, UCameraComponent* PerspectiveCam);
	static json::JSON SerializeActor(AActor* Actor);
	static json::JSON SerializeSceneComponentTree(USceneComponent* Comp);
	static json::JSON SerializeProperties(UActorComponent* Comp);
	static json::JSON SerializePropertyValue(const FPropertyDescriptor& Prop);

	// ---- Camera ----
	static json::JSON SerializeCamera(UCameraComponent* Cam);
	static void DeserializeCamera(json::JSON& CamJSON, FPerspectiveCameraData& OutCam);

	// ---- Primitives ----
	static void DeserializePrimitives(json::JSON& Primitives, UWorld* World, std::unordered_map<string, AActor*>& OutCreatedActors);

	// ---- Deserialization helpers ----
	static void DeserializeSceneComponentIntoExisting(USceneComponent* Existing, json::JSON& Node, AActor* Owner);
	static USceneComponent* DeserializeSceneComponentTree(json::JSON& Node, AActor* Owner);
	static void DeserializeProperties(UActorComponent* Comp, json::JSON& PropsJSON);
	static void DeserializePropertyValue(FPropertyDescriptor& Prop, json::JSON& Value);
	static AActor* DeserializeActorIntoWorld(UWorld* World, json::JSON& ActorJSON, AActor* ExistingActor = nullptr);

	static string GetCurrentTimeStamp();
};
