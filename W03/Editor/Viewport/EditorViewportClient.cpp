#include "Editor/Viewport/EditorViewportClient.h"

#include <iostream>

#include "Editor/Core/EditorConsole.h"
#include "Engine/Core/InputSystem.h"

#include "Engine/Scene/Camera.h"
#include "World.h"
#include "World/PrimitiveComponent.h"

void FEditorViewportClient::Initialize(HWND InHWindow)
{
	HWindow = InHWindow;

	UGizmoComponent* GizmoComp = FObjectManager::Get().CreateObject<UGizmoComponent>();
	GizmoComp->SetWorldLocation(FVector(0.0f, 0.0f, 0.0f));
	GizmoManager.Initialize(GizmoComp);
	GizmoManager.Deactivate();

	Camera = FObjectManager::Get().CreateObject<UCamera>();
	ResetCamera(Camera);
	Camera->ApplyCameraState();

	UE_LOG("Hello ZZup Engine! %d", 2026);
}

void FEditorViewportClient::SetViewportSize(float InWidth, float InHeight)
{
	if (InWidth  > 0.0f) WindowWidth  = InWidth;
	if (InHeight > 0.0f) WindowHeight = InHeight;

	if (Camera)
		Camera->OnResize(static_cast<int>(WindowWidth), static_cast<int>(WindowHeight));
}

void FEditorViewportClient::Tick(float DeltaTime)
{
	TickInput(DeltaTime);
	TickInteraction(DeltaTime);
	TickCursorOverlay(DeltaTime);
}


void FEditorViewportClient::TickInput(float DeltaTime)
{
	if (!Camera) return;

	if (InputSystem::GuiInputState.bUsingKeyboard) return;

	FCameraState& CameraState = Camera->GetCameraState();

	FVector move = FVector(0, 0, 0);
	float CameraVelocity = FUISettingInitializer::GetViewCamMoveSpeed();
	if (InputSystem::GetKey('W') && !CameraState.bIsOrthogonal) move.X += CameraVelocity;
	if (InputSystem::GetKey('A'))                               move.Y -= CameraVelocity;
	if (InputSystem::GetKey('S') && !CameraState.bIsOrthogonal) move.X -= CameraVelocity;
	if (InputSystem::GetKey('D'))                               move.Y += CameraVelocity;
	if (InputSystem::GetKey('Q'))                               move.Z -= CameraVelocity;
	if (InputSystem::GetKey('E'))                               move.Z += CameraVelocity;

	move *= DeltaTime;
	Camera->MoveLocal(move);

	FVector rotation      = FVector(0, 0, 0);
	FVector mouseRotation = FVector(0, 0, 0);

	float AngleVelocity = 50.0f;
	if (InputSystem::GetKey(VK_UP))    rotation.Z -= AngleVelocity;
	if (InputSystem::GetKey(VK_LEFT))  rotation.Y -= AngleVelocity;
	if (InputSystem::GetKey(VK_DOWN))  rotation.Z += AngleVelocity;
	if (InputSystem::GetKey(VK_RIGHT)) rotation.Y += AngleVelocity;

	float CameraAngleVelocity = FUISettingInitializer::GetViewCamRotSpeed();
	if (InputSystem::GetKey(VK_RBUTTON))
	{
		float deltaX = static_cast<float>(InputSystem::MouseDeltaX());
		float deltaY = static_cast<float>(InputSystem::MouseDeltaY());
		mouseRotation.Y = Clamp(deltaX * CameraAngleVelocity, -89.0f, 89.0f);
		mouseRotation.Z = Clamp(deltaY * CameraAngleVelocity, -89.0f, 89.0f);
	}

	if (InputSystem::GetKeyUp(VK_SPACE))
		GizmoManager.SetNextMode();

	rotation *= DeltaTime;
	Camera->Rotate(rotation.Y + mouseRotation.Y, rotation.Z + mouseRotation.Z);

	if (InputSystem::GetKeyDown('O'))
		CameraState.bIsOrthogonal = !CameraState.bIsOrthogonal;
}

void FEditorViewportClient::TickInteraction(float DeltaTime)
{
	(void)DeltaTime;
	if (!Camera || !Scene) return;

	GizmoManager.ApplyScreenSpaceScaling(Camera->GetWorldLocation());

	if (InputSystem::GuiInputState.bUsingMouse) return;

	FCameraState& CameraState = Camera->GetCameraState();
	uint32 zoomspeed = 30;

	float scrollNotches = InputSystem::GetScrollNotches();
	if (scrollNotches != 0.0f)
	{
		if (CameraState.bIsOrthogonal)
		{
			float newWidth = CameraState.OrthoWidth - scrollNotches * zoomspeed * DeltaTime;
			CameraState.OrthoWidth = Clamp(newWidth, 0.1f, 1000.0f);
		}
		else
		{
			float newFOV = CameraState.FOV - scrollNotches * zoomspeed * DeltaTime;
			CameraState.FOV = Clamp(newFOV, 1.f * DEG_TO_RAD, 90.0f * DEG_TO_RAD);
		}
	}

	POINT mousepoint = InputSystem::mousePos;
	ScreenToClient(HWindow, &mousepoint);

	CursorOverlayState.ScreenX = static_cast<float>(mousepoint.x);
	CursorOverlayState.ScreenY = static_cast<float>(mousepoint.y);

	if (InputSystem::GetKeyDown(VK_LBUTTON))
	{
		CursorOverlayState.bPressed     = true;
		CursorOverlayState.bVisible     = true;
		CursorOverlayState.TargetRadius = CursorOverlayState.MaxRadius;
		CursorOverlayState.Color        = FVector4(1.0f, 1.0f, 0.0f, 1.0f);
		if (bIsCursorVisible) { while (ShowCursor(FALSE) >= 0); bIsCursorVisible = false; }
	}

	if (InputSystem::GetKeyUp(VK_LBUTTON))
	{
		CursorOverlayState.bPressed     = false;
		CursorOverlayState.TargetRadius = 0.0f;
		if (!bIsCursorVisible) { while (ShowCursor(TRUE) < 0); bIsCursorVisible = true; }
	}

	if (InputSystem::GetKeyDown(VK_RBUTTON))
	{
		CursorOverlayState.bPressed     = true;
		CursorOverlayState.bVisible     = true;
		CursorOverlayState.TargetRadius = CursorOverlayState.MaxRadius;
		CursorOverlayState.Color        = FVector4(0.0f, 0.0f, 1.0f, 1.0f);
		if (bIsCursorVisible) { while (ShowCursor(FALSE) >= 0); bIsCursorVisible = false; }
	}

	if (InputSystem::GetKeyUp(VK_RBUTTON))
	{
		CursorOverlayState.bPressed     = false;
		CursorOverlayState.TargetRadius = 0.0f;
		if (!bIsCursorVisible) { while (ShowCursor(TRUE) < 0); bIsCursorVisible = true; }
	}

	FRay ray = Camera->DeprojectScreenToWorld(mousepoint.x, mousepoint.y, WindowWidth, WindowHeight);
	FHitResult hitResult;

	// Hover — only when not dragging so SelectedAxis stays locked
	if (!GizmoManager.IsHolding())
	{
		if (UGizmoComponent* Comp = GizmoManager.GetComponent())
			Comp->Raycast(ray, hitResult);
	}

	if (InputSystem::GetKeyDown(VK_LBUTTON))
	{
		HandleDragStart(ray);
	}
	else if (InputSystem::GetLeftDragging())
	{
		if (GizmoManager.IsPressedOnHandle() && !GizmoManager.IsHolding())
			GizmoManager.SetHolding(true);

		if (GizmoManager.IsHolding())
			GizmoManager.UpdateDrag(ray);
	}
	else if (InputSystem::GetLeftDragEnd())
	{
		GizmoManager.DragEnd();
	}
}

void FEditorViewportClient::ResetCamera(UCamera* InCamera)
{
	if (InCamera)
	{
		InCamera->SetWorldLocation(InitViewPos);
		InCamera->LookAt(InitLookAt);
	}
}

void FEditorViewportClient::SyncCameraFromRenderHandler()
{
	if (Camera)
		Camera->ApplyCameraState();
}

void FEditorViewportClient::ResetViewport()
{
	GizmoManager.Deactivate();
	if (UGizmoComponent* Comp = GizmoManager.GetComponent())
		Comp->SetWorldLocation(FVector(0.0f, 0.0f, 0.0f));

	Camera->bPendingKill = true;
	//UObjectManager::Get().CollectGarbage();

	Camera = FObjectManager::Get().CreateObject<UCamera>();
	SetViewportSize(WindowWidth, WindowHeight);
	Camera->ApplyCameraState();
	ResetCamera(Camera);
	SyncCameraFromRenderHandler();
}

void FEditorViewportClient::HandleDragStart(const FRay& Ray)
{
	UGizmoComponent* Comp = GizmoManager.GetComponent();
	if (!Comp) return;

	FHitResult hitResult{};
	if (Comp->Raycast(Ray, hitResult))
	{
		GizmoManager.SetPressedOnHandle(true);
		FString PickLog = "Gizmo is Holding: " + ViewOutput.ObjectPicked;
		UE_LOG(PickLog.c_str(), true);
		return;
	}

	// Find closest hit object in scene
	UPrimitiveComponent* bestTarget  = nullptr;
	float closestDistance = FLT_MAX;

	if (!Scene) return;

	for (auto* it : Scene->GetActors())
	{
		if (!it || it->bPendingKill || (it->GetRootComponent() && it->GetRootComponent()->bPendingKill))
			continue;

		UPrimitiveComponent* primitiveComp = dynamic_cast<UPrimitiveComponent*>(it->GetRootComponent());
		if (!primitiveComp) continue;

		hitResult = {};
		if (primitiveComp->Raycast(Ray, hitResult) && hitResult.Distance < closestDistance)
		{
			closestDistance = hitResult.Distance;
			bestTarget      = primitiveComp;
		}
	}

	bool bCtrlHeld = InputSystem::GetKey(VK_CONTROL);

	if (bCtrlHeld)
	{
		if (bestTarget != nullptr)
		{
			GizmoManager.AddTarget(bestTarget);
			ViewOutput.ObjectPicked = bestTarget->GetTypeInfo()->name;
			ViewOutput.Object       = bestTarget;
		}
	}
	else
	{
		if (bestTarget == nullptr)
		{
			GizmoManager.Deactivate();
			ViewOutput.ObjectPicked = "";
			ViewOutput.Object = nullptr;
		}
		else
		{
			GizmoManager.SetTarget(bestTarget);
			ViewOutput.ObjectPicked = bestTarget->GetTypeInfo()->name;
			ViewOutput.Object       = bestTarget;
		}
	}
}

void FEditorViewportClient::TickCursorOverlay(float DeltaTime)
{
	const float Alpha = std::min(1.0f, DeltaTime * CursorOverlayState.LerpSpeed);
	CursorOverlayState.CurrentRadius += (CursorOverlayState.TargetRadius - CursorOverlayState.CurrentRadius) * Alpha;

	if (!CursorOverlayState.bPressed && CursorOverlayState.CurrentRadius < 0.01f)
	{
		CursorOverlayState.CurrentRadius = 0.0f;
		CursorOverlayState.bVisible      = false;
	}
}

void FEditorViewportClient::CloseViewport()
{
	if (UGizmoComponent* Comp = GizmoManager.GetComponent())
		Comp->bPendingKill = true;
}
