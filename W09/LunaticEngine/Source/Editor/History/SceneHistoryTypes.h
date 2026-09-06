#pragma once

#include "Core/CoreTypes.h"
#include "Engine/Serialization/SceneSaveManager.h"
#include <unordered_map>

struct FTrackedSceneSnapshot
{
	std::unordered_map<uint32, FString> ActorStates;
	TArray<uint32> ActorOrderUUIDs;
	TArray<FString> OutlinerFolders;
	FPerspectiveCameraData CameraData;
	TArray<uint32> SelectedActorUUIDs;
};

struct FCreatedActorDelta
{
	uint32 ActorUUID = 0;
	FString SerializedActor;
};

struct FDeletedActorDelta
{
	uint32 ActorUUID = 0;
	FString SerializedActor;
};

struct FModifiedActorDelta
{
	uint32 ActorUUID = 0;
	FString BeforeSerializedActor;
	FString AfterSerializedActor;
};

struct FTrackedSceneChange
{
	TArray<FCreatedActorDelta> CreatedActors;
	TArray<FDeletedActorDelta> DeletedActors;
	TArray<FModifiedActorDelta> ModifiedActors;
	TArray<uint32> BeforeSelectedActorUUIDs;
	TArray<uint32> AfterSelectedActorUUIDs;
	TArray<uint32> BeforeActorOrderUUIDs;
	TArray<uint32> AfterActorOrderUUIDs;
	TArray<FString> BeforeOutlinerFolders;
	TArray<FString> AfterOutlinerFolders;
	FPerspectiveCameraData BeforeCameraData;
	FPerspectiveCameraData AfterCameraData;
};
