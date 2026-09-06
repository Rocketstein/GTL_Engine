#pragma once
#include "Object/Object.h"
#include <cstddef>
#include <memory>

class AActor;
class UWorld;
class FSpatialPartition;
class FArchive;
class ULevel :
    public UObject
{
public:
	DECLARE_CLASS(ULevel, UObject)

	ULevel() = default;
	ULevel(UWorld* OwingWorld);
	ULevel(const TArray<AActor*>& Actors, UWorld* OwingWorld);
	~ULevel();

	void AddActor(AActor* Actor);
	void RemoveActor(AActor* Actor);
	bool MoveActorBefore(AActor* ActorToMove, AActor* BeforeActor);
	bool MoveActorToIndex(AActor* ActorToMove, size_t TargetIndex);
	bool AddOutlinerFolder(const FString& FolderName);
	bool RenameOutlinerFolder(const FString& OldFolderName, const FString& NewFolderName);
	bool MoveOutlinerFolderBefore(const FString& FolderToMove, const FString& BeforeFolder);
	bool MoveOutlinerFolderToIndex(const FString& FolderToMove, size_t TargetIndex);
	void SetOutlinerFolders(const TArray<FString>& InFolders);
	void Clear();

	const TArray<AActor*>& GetActors() const { return Actors; }
	const TArray<FString>& GetOutlinerFolders() const { return OutlinerFolders; }
	UWorld* GetWorld() const { return OwingWorld; }
	void SetWorld(UWorld* World) { OwingWorld = World;}
	void Serialize(FArchive& Ar);
	virtual UObject* Duplicate(UObject* NewOuter = nullptr) const override;
	void BeginPlay();
	void EndPlay();
	void Tick(float DeltaTime);

	// GameMode 클래스 — 비어있으면 ProjectSettings의 DefaultGameModeClass 사용
	const FString& GetGameModeClassName() const { return GameModeClassName; }
	void SetGameModeClassName(const FString& InName) { GameModeClassName = InName; }

private:
	FName LevelName;
	TArray<AActor*> Actors;
	TArray<FString> OutlinerFolders;
	UWorld* OwingWorld = nullptr;
	FString GameModeClassName;
};

