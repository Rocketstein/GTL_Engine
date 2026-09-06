#pragma once
#include "Core/CoreGlobals.h"
#include "Editor/Viewport/SkeletalMeshViewer.h"
#include "UI/SWindow.h"
#include <memory>
#include <functional>
#include <d3d11.h>

class SSplitter;
class SWindow;
class FEditorViewportClient;
class FSelectionManager;
class FEditorSettings;
class FWindowsWindow;
class UEditorEngine;
class USkeletalMesh;

class FAssetViewerManager
{
public:
    void Initialize(UEditorEngine* InEditorEngine, ID3D11Device* InDevice);
    void Release();

    void Tick(float DeltaTime);
    void Render(float DeltaTime);

    FSkeletalMeshViewer* OpenSkeletalMeshEditor(USkeletalMesh* Mesh);
    FSkeletalMeshViewer* FindSkeletalMeshEditor();
    //FMaterialViewer* OpenMaterialViewer(UMaterial* Material);
    //FAnimationViewer* OpenAnimationViewer(USkeletalMesh* Mesh, UAnimationSequence* Animation);

    void ForEachOpenViewer(const std::function<void(IAssetViewer&)>& Visitor);

    bool IsMouseOverViewport() const;

private:
    UEditorEngine* EditorEngine = nullptr;
    ID3D11Device* Device = nullptr;
    uint32 NextViewerId = 1;

    TArray<std::unique_ptr<IAssetViewer>> Viewers;
};
