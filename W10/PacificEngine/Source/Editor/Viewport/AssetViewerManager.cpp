#include "AssetViewerManager.h"
#include "Editor/Viewport/SkeletalMeshViewer.h"
#include "Editor/Viewport/PreviewViewportClient.h"
#include "ImGui/imgui.h"

void FAssetViewerManager::Initialize(UEditorEngine* InEditorEngine, ID3D11Device* InDevice)
{
    EditorEngine = InEditorEngine;
    Device = InDevice;
    NextViewerId = 1;
}

void FAssetViewerManager::Release()
{
    for (std::unique_ptr<IAssetViewer>& Viewer : Viewers)
    {
        if (Viewer)
        {
            Viewer->Release();
        }
    }

    Viewers.clear();
    EditorEngine = nullptr;
    Device = nullptr;
}

void FAssetViewerManager::Tick(float DeltaTime)
{
    if (Viewers.empty()) return;
    for (std::unique_ptr<IAssetViewer>& Viewer : Viewers)
    {
        if (Viewer && Viewer->IsOpen())
        {
            Viewer->Tick(DeltaTime);
        }
    }
}

void FAssetViewerManager::Render(float DeltaTime)
{
    for (auto It = Viewers.begin(); It != Viewers.end();)
    {
        IAssetViewer* Viewer = It->get();
        if (!Viewer)
        {
            It = Viewers.erase(It);
            continue;
        }

        if (!Viewer->IsOpen())
        {
            Viewer->Release();
            It = Viewers.erase(It);
            continue;
        }

        Viewer->Render(DeltaTime);
        ++It;
    }
}

FSkeletalMeshViewer* FAssetViewerManager::OpenSkeletalMeshEditor(USkeletalMesh* Mesh)
{
	//이미 존재하는지 확인
    if (FSkeletalMeshViewer* Existing = FindSkeletalMeshEditor())
    {
        Existing->SetSkeletalMesh(Mesh);
        return Existing;
    }

    auto Editor = std::make_unique<FSkeletalMeshViewer>();
    FSkeletalMeshViewer* RawEditor = Editor.get();

    RawEditor->Initialize(NextViewerId++, EditorEngine, Device, Mesh);
    Viewers.push_back(std::move(Editor));

    return RawEditor;
}

FSkeletalMeshViewer* FAssetViewerManager::FindSkeletalMeshEditor()
{
    for (const std::unique_ptr<IAssetViewer>& Viewer : Viewers)
    {
        if (!Viewer || Viewer->GetViewerType() != EAssetViewerType::SkeletalMesh)
        {
            continue;
        }

        FSkeletalMeshViewer* SkeletalViewer = static_cast<FSkeletalMeshViewer*>(Viewer.get());
        return SkeletalViewer;
    }
	return nullptr;
}

void FAssetViewerManager::ForEachOpenViewer(const std::function<void(IAssetViewer&)>& Visitor)
{
    if (!Visitor)
    {
        return;
    }

    for (std::unique_ptr<IAssetViewer>& Viewer : Viewers)
    {
        if (Viewer && Viewer->IsOpen())
        {
            Visitor(*Viewer);
        }
    }
}

bool FAssetViewerManager::IsMouseOverViewport() const
{
    const ImVec2 MousePos = ImGui::GetIO().MousePos;

    for (const std::unique_ptr<IAssetViewer>& Viewer : Viewers)
    {
        if (!Viewer || !Viewer->IsOpen())
        {
            continue;
        }

        FRect Rect{};
        if (!Viewer->GetViewportRect(Rect))
        {
            continue;
        }

        if (MousePos.x >= Rect.X &&
            MousePos.y >= Rect.Y &&
            MousePos.x < Rect.X + Rect.Width &&
            MousePos.y < Rect.Y + Rect.Height)
        {
            return true;
        }
    }

    return false;
}
