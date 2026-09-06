#include "GameFramework/Level.h"
#include "Object/ObjectFactory.h"
#include <GameFramework/World.h>
#include "Serialization/Archive.h"
IMPLEMENT_CLASS(ULevel, UObject)

ULevel::ULevel(UWorld* OwingWorld)
	: OwingWorld(OwingWorld)
{
	Actors.clear();
}

ULevel::ULevel(const TArray<AActor*>& Actors, UWorld* World)
	: Actors(Actors)
{
	OwingWorld = World;
}

ULevel::~ULevel()
{
	Clear();
	OwingWorld = nullptr;
}

void ULevel::AddActor(AActor* Actor)
{
	if (!Actor) return;

	auto It = std::find(Actors.begin(), Actors.end(), Actor);
	if (It != Actors.end())
	{
		return;
	}
	
	Actor->SetOuter(this);
	Actors.push_back(Actor);
}

void ULevel::RemoveActor(AActor* Actor)
{
	if (!Actor) return;

	auto It = std::find(Actors.begin(), Actors.end(), Actor);
	if (It == Actors.end())
	{
		return;
	}

	Actors.erase(It);
}

bool ULevel::MoveActorBefore(AActor* ActorToMove, AActor* BeforeActor)
{
	if (!ActorToMove || !BeforeActor || ActorToMove == BeforeActor)
	{
		return false;
	}

	auto MoveIt = std::find(Actors.begin(), Actors.end(), ActorToMove);
	auto BeforeIt = std::find(Actors.begin(), Actors.end(), BeforeActor);
	if (MoveIt == Actors.end() || BeforeIt == Actors.end())
	{
		return false;
	}

	AActor* MovedActor = *MoveIt;
	Actors.erase(MoveIt);
	BeforeIt = std::find(Actors.begin(), Actors.end(), BeforeActor);
	Actors.insert(BeforeIt, MovedActor);
	return true;
}

bool ULevel::MoveActorToIndex(AActor* ActorToMove, size_t TargetIndex)
{
	if (!ActorToMove)
	{
		return false;
	}

	auto MoveIt = std::find(Actors.begin(), Actors.end(), ActorToMove);
	if (MoveIt == Actors.end())
	{
		return false;
	}

	AActor* MovedActor = *MoveIt;
	Actors.erase(MoveIt);

	if (TargetIndex > Actors.size())
	{
		TargetIndex = Actors.size();
	}

	Actors.insert(Actors.begin() + static_cast<std::ptrdiff_t>(TargetIndex), MovedActor);
	return true;
}

bool ULevel::AddOutlinerFolder(const FString& FolderName)
{
	if (FolderName.empty())
	{
		return false;
	}

	if (std::find(OutlinerFolders.begin(), OutlinerFolders.end(), FolderName) != OutlinerFolders.end())
	{
		return false;
	}

	OutlinerFolders.push_back(FolderName);
	return true;
}

bool ULevel::RenameOutlinerFolder(const FString& OldFolderName, const FString& NewFolderName)
{
	if (OldFolderName.empty() || NewFolderName.empty() || OldFolderName == NewFolderName)
	{
		return false;
	}

	auto OldIt = std::find(OutlinerFolders.begin(), OutlinerFolders.end(), OldFolderName);
	if (OldIt == OutlinerFolders.end())
	{
		return false;
	}

	if (std::find(OutlinerFolders.begin(), OutlinerFolders.end(), NewFolderName) != OutlinerFolders.end())
	{
		return false;
	}

	*OldIt = NewFolderName;
	return true;
}

bool ULevel::MoveOutlinerFolderBefore(const FString& FolderToMove, const FString& BeforeFolder)
{
	if (FolderToMove.empty() || BeforeFolder.empty() || FolderToMove == BeforeFolder)
	{
		return false;
	}

	auto MoveIt = std::find(OutlinerFolders.begin(), OutlinerFolders.end(), FolderToMove);
	auto BeforeIt = std::find(OutlinerFolders.begin(), OutlinerFolders.end(), BeforeFolder);
	if (MoveIt == OutlinerFolders.end() || BeforeIt == OutlinerFolders.end())
	{
		return false;
	}

	FString MovedFolder = *MoveIt;
	OutlinerFolders.erase(MoveIt);
	BeforeIt = std::find(OutlinerFolders.begin(), OutlinerFolders.end(), BeforeFolder);
	OutlinerFolders.insert(BeforeIt, MovedFolder);
	return true;
}

bool ULevel::MoveOutlinerFolderToIndex(const FString& FolderToMove, size_t TargetIndex)
{
	if (FolderToMove.empty())
	{
		return false;
	}

	auto MoveIt = std::find(OutlinerFolders.begin(), OutlinerFolders.end(), FolderToMove);
	if (MoveIt == OutlinerFolders.end())
	{
		return false;
	}

	FString MovedFolder = *MoveIt;
	OutlinerFolders.erase(MoveIt);
	if (TargetIndex > OutlinerFolders.size())
	{
		TargetIndex = OutlinerFolders.size();
	}

	OutlinerFolders.insert(OutlinerFolders.begin() + static_cast<std::ptrdiff_t>(TargetIndex), MovedFolder);
	return true;
}

void ULevel::SetOutlinerFolders(const TArray<FString>& InFolders)
{
	OutlinerFolders.clear();
	for (const FString& FolderName : InFolders)
	{
		if (FolderName.empty())
		{
			continue;
		}

		if (std::find(OutlinerFolders.begin(), OutlinerFolders.end(), FolderName) == OutlinerFolders.end())
		{
			OutlinerFolders.push_back(FolderName);
		}
	}
}

void ULevel::Clear()
{
	for (AActor* Actor : Actors)
	{
		if (Actor)
		{
			Actor->SetOuter(nullptr);
		}
	}

	Actors.clear();
	OutlinerFolders.clear();
}

void ULevel::Tick(float DeltaTime) {
	for (AActor* Actor : Actors)
	{
		if (Actor)
		{
			Actor->Tick(DeltaTime);
		}
	}
}

UObject* ULevel::Duplicate(UObject* NewOuter) const
{
	ULevel* DupLevel = UObjectManager::Get().CreateObject<ULevel>(NewOuter);
	if (!DupLevel) return nullptr;

	if (UWorld* WorldOuter = Cast<UWorld>(NewOuter))
	{
		DupLevel->SetWorld(WorldOuter);
	}

	DupLevel->SetGameModeClassName(GetGameModeClassName());
	DupLevel->SetOutlinerFolders(GetOutlinerFolders());

	for (AActor* SrcActor : Actors)
	{
		if (!SrcActor) continue;
		AActor* DupActor = Cast<AActor>(SrcActor->Duplicate(DupLevel));
		if (DupActor)
		{
			DupLevel->AddActor(DupActor);
		}
	}

	return DupLevel;
}

void ULevel::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	if (Ar.IsSaving())
	{
		Ar << GameModeClassName;

		int32 ActorCount = static_cast<int32>(Actors.size());
		Ar << ActorCount;

		for (AActor* actor : Actors)
		{
			FString ClassName = actor->GetClass()->GetName();
			Ar << ClassName;
			actor->Serialize(Ar);
		}

	}
	else if (Ar.IsLoading())
	{
		Ar << GameModeClassName;

		int32 ActorCount = 0;
		Ar << ActorCount;

		for (int i = 0; i < ActorCount; ++i)
		{
			FString ClassName;
			Ar << ClassName;

			UObject* Obj = FObjectFactory::Get().Create(ClassName, this);
			AActor * NewActor = Cast<AActor>(Obj);

			if (NewActor)
			{
				NewActor->Serialize(Ar);
				AddActor(NewActor);
			}
		}
	}
}

void ULevel::BeginPlay()
{
	const size_t InitialCount = Actors.size();
	for (size_t i = 0; i < InitialCount; ++i)
	{
		AActor* Actor = Actors[i];
		if (Actor && !Actor->HasActorBegunPlay())
		{
			Actor->BeginPlay();
		}
	}
}

void ULevel::EndPlay()
{
	for (AActor* Actor : Actors)
	{
		if (Actor && IsAliveObject(Actor))
		{
			Actor->EndPlay();
		}
	}

	for (AActor* Actor : Actors)
	{
		if (Actor && IsAliveObject(Actor))
		{
			UObjectManager::Get().DestroyObject(Actor);
		}
	}
	Actors.clear();
}
