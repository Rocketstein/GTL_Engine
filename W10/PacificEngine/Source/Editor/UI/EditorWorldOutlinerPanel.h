// Defines the editor World Outliner panel.
#pragma once

#include "Core/CoreTypes.h"
#include "Editor/UI/EditorPanel.h"

class AActor;
struct ID3D11Device;
struct ID3D11ShaderResourceView;

class FEditorWorldOutlinerPanel : public FEditorPanel
{
public:
    void Initialize(UEditorEngine* InEditorEngine) override;
    void Initialize(UEditorEngine* InEditorEngine, ID3D11Device* InDevice);
    void Release();
    void Render(float DeltaTime) override;

private:
    void DrawToolbar(int32 TotalActorCount, int32 FilteredActorCount, int32 SelectedActorCount);
    void DrawActorList(const TArray<AActor*>& Actors, const TArray<AActor*>& FilteredActors);
    void DrawFolderBranch(const FString& FolderPath,
                          const TArray<FString>& VisibleFolders,
                          const TArray<AActor*>& Actors,
                          const TArray<AActor*>& FilteredActors,
                          const TArray<AActor*>& VisibleSelectionRange);
    void DrawFolderRow(const FString& FolderPath, const TArray<AActor*>& FolderActors, bool bHasChildFolders);
    void DrawActorRow(AActor* Actor, const TArray<AActor*>& FilteredActors);
    void DrawRootDropTarget();
    void DrawBackgroundContextMenu();
    void DrawCreateFolderPopup();

    void BeginRename(AActor* Actor);
    bool DrawRenameInput(AActor* Actor);
    void BeginRenameFolder(const FString& FolderPath);
    bool DrawFolderRenameInput(const FString& FolderPath);
    void SelectActorFromRow(AActor* Actor, const TArray<AActor*>& FilteredActors);
    void DuplicateActor(AActor* Actor);
    void DeleteActor(AActor* Actor);
    void DeleteSelectedActors();
    void DeleteActors(const TArray<AActor*>& ActorsToDelete);
    void LoadIconTextures(ID3D11Device* InDevice);
    void OpenCreateFolderPopup(const FString& ParentFolderPath = FString());
    void CreateFolderFromBuffer();
    void RenameFolder(const FString& OldFolderPath, const FString& NewFolderName);
    void RemoveFolder(const FString& FolderPath);
    void MoveActorToFolder(AActor* Actor, const FString& FolderPath);
    void MoveSelectedActorsToFolder(const FString& FolderPath, AActor* FallbackActor);

    TArray<AActor*> BuildFilteredActors() const;
    TArray<FString> BuildVisibleFolders(const TArray<AActor*>& Actors, const TArray<AActor*>& FilteredActors) const;
    TArray<AActor*> BuildVisibleActorsForFolder(const FString& FolderPath, const TArray<AActor*>& Actors, const TArray<AActor*>& FilteredActors) const;
    TArray<AActor*> BuildVisibleRootActors(const TArray<AActor*>& Actors, const TArray<AActor*>& FilteredActors) const;
    TArray<FString> GetSortedFolderPaths() const;
    void AppendVisibleActorsForFolderBranch(const FString& FolderPath,
                                            const TArray<FString>& VisibleFolders,
                                            const TArray<AActor*>& Actors,
                                            const TArray<AActor*>& FilteredActors,
                                            TArray<AActor*>& OutVisibleActors) const;
    bool MatchesSearch(AActor* Actor) const;
    bool FolderMatchesSearch(const FString& FolderPath) const;
    bool ActorIsInList(AActor* Actor, const TArray<AActor*>& Actors) const;
    bool FolderExists(const FString& FolderPath) const;
    FString NormalizeFolderName(const FString& FolderName) const;
    FString MakeUniqueFolderName(const FString& BaseName, const FString& IgnoredExistingFolder = FString()) const;
    FString GetActorDisplayName(AActor* Actor) const;
    FString GetActorClassName(AActor* Actor) const;

private:
    char SearchBuffer[128] = {};
    char RenameBuffer[128] = {};
    char NewFolderNameBuffer[128] = {};
    char FolderRenameBuffer[128] = {};
    AActor* RenamingActor = nullptr;
    AActor* ActorToDuplicate = nullptr;
    AActor* ActorToDelete = nullptr;
    bool bRenameFocusPending = false;
    bool bNewFolderFocusPending = false;
    bool bFolderRenameFocusPending = false;
    bool bCreateFolderPopupRequested = false;
    FString CreateFolderParentPath;
    FString RenamingFolderPath;
    TSet<FString> OpenFolderPaths;

    ID3D11ShaderResourceView* DirectionalLightIcon = nullptr;
    ID3D11ShaderResourceView* PointLightIcon = nullptr;
    ID3D11ShaderResourceView* SpotLightIcon = nullptr;
    ID3D11ShaderResourceView* AmbientLightIcon = nullptr;
    ID3D11ShaderResourceView* DecalIcon = nullptr;
    ID3D11ShaderResourceView* HeightFogIcon = nullptr;
};
