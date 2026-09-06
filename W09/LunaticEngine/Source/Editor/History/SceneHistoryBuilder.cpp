#include "Editor/History/SceneHistoryBuilder.h"

#include "Editor/EditorEngine.h"
#include "Component/CameraComponent.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Object/Object.h"

#include <set>

namespace
{
	bool IsGameModeActor(const AActor* Actor)
	{
		return Actor && Actor->GetClass() && Actor->GetClass()->GetName() == "AGameModeBase";
	}
}

FTrackedSceneSnapshot FSceneHistoryBuilder::CaptureSnapshot(const UEditorEngine& EditorEngine)
{
	FTrackedSceneSnapshot Snapshot;

	const UWorld* World = EditorEngine.GetWorld();
	if (!World)
	{
		return Snapshot;
	}

	if (UCameraComponent* Camera = EditorEngine.GetCamera())
	{
		Snapshot.CameraData.Location = Camera->GetWorldLocation();
		const FRotator Rotation = Camera->GetRelativeRotation();
		Snapshot.CameraData.Rotation = FVector(Rotation.Roll, Rotation.Pitch, Rotation.Yaw);
		const FMinimalViewInfo CameraState = Camera->GetCameraState();
		Snapshot.CameraData.FOV = CameraState.FOV;
		Snapshot.CameraData.NearClip = CameraState.NearZ;
		Snapshot.CameraData.FarClip = CameraState.FarZ;
		Snapshot.CameraData.bValid = true;
	}

	for (AActor* Actor : World->GetActors())
	{
		if (!Actor || IsGameModeActor(Actor))
		{
			continue;
		}

		Snapshot.ActorOrderUUIDs.push_back(Actor->GetUUID());
		Snapshot.ActorStates[Actor->GetUUID()] = FSceneSaveManager::SerializeActorToJSONString(Actor);
	}

	for (AActor* Actor : EditorEngine.GetSelectionManager().GetSelectedActors())
	{
		if (!Actor)
		{
			continue;
		}

		Snapshot.SelectedActorUUIDs.push_back(Actor->GetUUID());
	}

	if (const ULevel* PersistentLevel = World->GetPersistentLevel())
	{
		Snapshot.OutlinerFolders = PersistentLevel->GetOutlinerFolders();
	}

	return Snapshot;
}

bool FSceneHistoryBuilder::HasMeaningfulDelta(const FTrackedSceneSnapshot& Before, const FTrackedSceneSnapshot& After)
{
	if (Before.ActorOrderUUIDs != After.ActorOrderUUIDs)
	{
		return true;
	}

	if (Before.OutlinerFolders != After.OutlinerFolders)
	{
		return true;
	}

	if (Before.ActorStates.size() != After.ActorStates.size())
	{
		return true;
	}

	for (const auto& BeforePair : Before.ActorStates)
	{
		const auto AfterIt = After.ActorStates.find(BeforePair.first);
		if (AfterIt == After.ActorStates.end() || AfterIt->second != BeforePair.second)
		{
			return true;
		}
	}

	return false;
}

FTrackedSceneChange FSceneHistoryBuilder::BuildChange(const FTrackedSceneSnapshot& Before, const FTrackedSceneSnapshot& After)
{
	FTrackedSceneChange Change;
	Change.BeforeSelectedActorUUIDs = Before.SelectedActorUUIDs;
	Change.AfterSelectedActorUUIDs = After.SelectedActorUUIDs;
	Change.BeforeActorOrderUUIDs = Before.ActorOrderUUIDs;
	Change.AfterActorOrderUUIDs = After.ActorOrderUUIDs;
	Change.BeforeOutlinerFolders = Before.OutlinerFolders;
	Change.AfterOutlinerFolders = After.OutlinerFolders;
	Change.BeforeCameraData = Before.CameraData;
	Change.AfterCameraData = After.CameraData;

	std::set<uint32> AllUUIDs;
	for (const auto& BeforePair : Before.ActorStates)
	{
		AllUUIDs.insert(BeforePair.first);
	}
	for (const auto& AfterPair : After.ActorStates)
	{
		AllUUIDs.insert(AfterPair.first);
	}

	for (uint32 UUID : AllUUIDs)
	{
		const auto BeforeIt = Before.ActorStates.find(UUID);
		const auto AfterIt = After.ActorStates.find(UUID);
		const FString BeforeState = BeforeIt != Before.ActorStates.end() ? BeforeIt->second : FString();
		const FString AfterState = AfterIt != After.ActorStates.end() ? AfterIt->second : FString();
		if (BeforeState == AfterState)
		{
			continue;
		}

		if (BeforeState.empty())
		{
			Change.CreatedActors.push_back({ UUID, AfterState });
		}
		else if (AfterState.empty())
		{
			Change.DeletedActors.push_back({ UUID, BeforeState });
		}
		else
		{
			Change.ModifiedActors.push_back({ UUID, BeforeState, AfterState });
		}
	}

	return Change;
}

TArray<uint32> FSceneHistoryBuilder::GetChangedActorUUIDs(const FTrackedSceneChange& Change, bool bSelectAfterChange)
{
	const TArray<uint32>& PreferredSelection = bSelectAfterChange ? Change.AfterSelectedActorUUIDs : Change.BeforeSelectedActorUUIDs;
	TArray<uint32> ChangedUUIDs;
	ChangedUUIDs.reserve(Change.CreatedActors.size() + Change.DeletedActors.size() + Change.ModifiedActors.size());
	std::set<uint32> ChangedUUIDSet;

	if (bSelectAfterChange)
	{
		for (const FCreatedActorDelta& Delta : Change.CreatedActors)
		{
			ChangedUUIDSet.insert(Delta.ActorUUID);
		}
		for (const FModifiedActorDelta& Delta : Change.ModifiedActors)
		{
			ChangedUUIDSet.insert(Delta.ActorUUID);
		}
	}
	else
	{
		for (const FDeletedActorDelta& Delta : Change.DeletedActors)
		{
			ChangedUUIDSet.insert(Delta.ActorUUID);
		}
		for (const FModifiedActorDelta& Delta : Change.ModifiedActors)
		{
			ChangedUUIDSet.insert(Delta.ActorUUID);
		}
	}

	for (uint32 SelectedUUID : PreferredSelection)
	{
		if (ChangedUUIDSet.find(SelectedUUID) != ChangedUUIDSet.end())
		{
			ChangedUUIDs.push_back(SelectedUUID);
			ChangedUUIDSet.erase(SelectedUUID);
		}
	}

	if (bSelectAfterChange)
	{
		for (const FCreatedActorDelta& Delta : Change.CreatedActors)
		{
			if (ChangedUUIDSet.find(Delta.ActorUUID) != ChangedUUIDSet.end())
			{
				ChangedUUIDs.push_back(Delta.ActorUUID);
			}
		}
	}
	else
	{
		for (const FDeletedActorDelta& Delta : Change.DeletedActors)
		{
			if (ChangedUUIDSet.find(Delta.ActorUUID) != ChangedUUIDSet.end())
			{
				ChangedUUIDs.push_back(Delta.ActorUUID);
			}
		}
	}

	for (const FModifiedActorDelta& Delta : Change.ModifiedActors)
	{
		if (ChangedUUIDSet.find(Delta.ActorUUID) != ChangedUUIDSet.end())
		{
			ChangedUUIDs.push_back(Delta.ActorUUID);
		}
	}

	return ChangedUUIDs;
}
