#include "Editor/EditorEngine.h"
#include "Engine/Core/InputSystem.h"
#include "Render/Scene/RenderCollector.h"
#include "Render/Scene/RenderCollectorContext.h"
#include "World/Primitives/Primitives.h"

void FEditorEngine::Create(HWND InHWindow)
{
	HWindow = InHWindow;

	Renderer.Create(HWindow);
	FRenderCollector::Initialize(Renderer.GetFD3DDevice().GetDevice());
	RenderBus.Create(Renderer.GetFD3DDevice().GetDevice());
	EditorWorld = FObjectManager::Get().CreateObject<UWorld>();
	EditorWorld->Init();
	EditorWorld->SpawnPrimitiveActor<UCubeComponent>(FVector(-3.f, 0, 0));	// Test sample

	RECT rect;
	GetClientRect(HWindow, &rect);
	WindowWidth = static_cast<float>(rect.right - rect.left);
	WindowHeight = static_cast<float>(rect.bottom - rect.top);

	ViewportClient.Initialize(HWindow);
	ViewportClient.SetViewportSize(WindowWidth, WindowHeight);
	ViewportClient.SetScene(EditorWorld->GetActiveScene());

	ViewportClient.SetViewportSize(WindowWidth, WindowHeight);
    MainPanel.Create(HWindow, Renderer, this, &ViewportClient);

	FUISettingInitializer::LoadEditorViewSettings();
}

void FEditorEngine::OnWindowResized(uint32 Width, uint32 Height)
{
	if (Width <= 0 || Height <= 0)
	{
		return;
	}

	WindowWidth = static_cast<float>(Width);
	WindowHeight = static_cast<float>(Height);
	ViewportClient.SetViewportSize(WindowWidth, WindowHeight);

	Renderer.GetFD3DDevice().OnResizeViewport(Width, Height);
}

void FEditorEngine::NewScene() {
	EditorWorld->GetSceneManager().AddNewScene();
	ViewportClient.ResetViewport();
	ResetViewportScene();
}

void FEditorEngine::ResetViewportScene() {
	ViewportClient.SetScene(EditorWorld->GetActiveScene());
}

void FEditorEngine::Release()
{
	if (EditorWorld) {
		EditorWorld->EndPlay();
	}
	FRenderCollector::Release();
	MainPanel.Release();
	FUISettingInitializer::SaveEditorViewSettings();
	RenderBus.Release();
	Renderer.Release();
}

void FEditorEngine::BeginPlay()
{
	if (EditorWorld) {
		EditorWorld->BeginPlay();
	}
}

void FEditorEngine::BeginFrame(float DeltaTime)
{
	InputSystem::Update();
	ViewportClient.Tick(DeltaTime);
	MainPanel.Update();
	ViewportClient.SyncCameraFromRenderHandler();
}

void FEditorEngine::Update(float DeltaTime)
{
	UpdateWorld(DeltaTime);
}

void FEditorEngine::Render(float DeltaTime)
{
	RenderBus.Clear();
	BuildRenderCommands();

	Renderer.BeginFrame();
	Renderer.Render(RenderBus);
	MainPanel.Render(DeltaTime, ViewportClient.GetViewOutput());
	Renderer.RenderOverlay(RenderBus);	//	UI가 그려진 후 Overlay 그리기
	Renderer.EndFrame();
}

void FEditorEngine::EndFrame()
{
	//UObjectManager::Get().CollectGarbage();
}

void FEditorEngine::UpdateWorld(float DeltaTime)
{
	if (EditorWorld)
	{
		EditorWorld->Tick(DeltaTime);
	}
}

void FEditorEngine::BuildRenderCommands()
{
	FRenderCollectorContext Context;
	Context.Scene = EditorWorld->GetActiveScene();
	Context.Camera = ViewportClient.GetCamera();
	FGizmoManager& GizmoManager = ViewportClient.GetGizmoManager();
	Context.Gizmo = GizmoManager.GetComponent();
	Context.CursorOverlayState = &ViewportClient.GetCursorOverlayState();
	Context.ViewportHeight = WindowHeight;
	Context.ViewportWidth = WindowWidth;

	Context.SelectedComponent = &GizmoManager.GetSelectedTargets();
	Context.GridSize = FUISettingInitializer::GetGridSize();

	FRenderCollector::Collect(Context, RenderBus);
}
