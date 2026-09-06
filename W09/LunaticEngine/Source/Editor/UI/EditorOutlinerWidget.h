#pragma once

#include "Editor/UI/EditorWidget.h"
#include "Core/CoreTypes.h"

class AActor;

class FEditorOutlinerWidget : public FEditorWidget
{
public:
	virtual void Initialize(UEditorEngine* InEditorEngine) override;
	virtual void Render(float DeltaTime) override;

private:
	struct FOutlinerDragPayload
	{
		enum class EItemType : uint8
		{
			Actor,
			Folder
		};

		EItemType ItemType = EItemType::Actor;
		AActor* Actor = nullptr;
		char FolderName[128] = {};
	};

	void SelectAllVisibleActors();
	void RenderActorOutliner();
	bool DrawVisibilityToggle(const char* Id, bool bVisible) const;
	void StartActorRename(AActor* Actor);
	void StartFolderRename(const FString& FolderName);
	void CommitActorRename();
	void CommitFolderRename();
	void CancelActorRename();
	void CancelFolderRename();
	void HandleActorDrop(AActor* DraggedActor, AActor* TargetActor) const;
	void HandleFolderDrop(AActor* DraggedActor, const FString& FolderName) const;
	void HandleFolderDrop(const FString& DraggedFolder, const FString& TargetFolder) const;
	void HandleRootDrop(AActor* DraggedActor) const;
	void HandleRootDrop(const FString& DraggedFolder) const;

	TArray<int32> ValidActorIndices;
	char SearchBuffer[128] = {};
	char NewFolderBuffer[128] = {};
	char RenameBuffer[128] = {};
	char FolderRenameBuffer[128] = {};
	FString TypeFilter = "All Types";
	AActor* RenamingActor = nullptr;
	FString RenamingFolder;
	bool bFocusRenameInput = false;
};
