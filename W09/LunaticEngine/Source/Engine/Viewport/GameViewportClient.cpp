#include "Viewport/GameViewportClient.h"

#include "Component/CameraComponent.h"
#include "Component/ScriptComponent.h"
#include "GameFramework/AActor.h"
#include "Engine/Input/InputManager.h"
#include "Engine/Input/InputModifier.h"
#include "Engine/Input/InputTrigger.h"
#include "Math/MathUtils.h"
#include "Object/Object.h"
#include "Viewport/Viewport.h"

#include <windows.h>

DEFINE_CLASS(UGameViewportClient, UObject)

void UGameViewportClient::OnBeginPIE(UCameraComponent* InitialTarget, FViewport* InViewport)
{
	Viewport = InViewport;
	Possess(InitialTarget);
	SetupInput();
	ResetInputState();
}

void UGameViewportClient::OnEndPIE()
{
	SetPossessed(false);
	UnPossess();
	ResetInputState();
	bHasCursorClipRect = false;
	Viewport = nullptr;

	EnhancedInputManager.ClearBindings();
	EnhancedInputManager.ClearAllMappingContexts();

	if (DefaultMappingContext)
	{
		for (auto& Mapping : DefaultMappingContext->Mappings)
		{
			for (auto* Trigger : Mapping.Triggers) delete Trigger;
			for (auto* Modifier : Mapping.Modifiers) delete Modifier;
		}
		delete DefaultMappingContext;
		DefaultMappingContext = nullptr;
	}

	delete ActionMove;
	delete ActionLook;
	delete ActionSprint;
	ActionMove = nullptr;
	ActionLook = nullptr;
	ActionSprint = nullptr;
}

bool UGameViewportClient::ProcessPIEInput(float DeltaTime)
{
	return Tick(DeltaTime);
}

void UGameViewportClient::SetPIEPossessedInputEnabled(bool bEnabled)
{
	SetPossessed(bEnabled);
}

UCameraComponent* UGameViewportClient::GetDrivingCamera() const
{
	return IsAliveObject(PossessedCamera) ? PossessedCamera : nullptr;
}

void UGameViewportClient::SetCursorClipRect(const FRect& InViewportScreenRect)
{
	if (InViewportScreenRect.Width <= 1.0f || InViewportScreenRect.Height <= 1.0f || !OwnerHWnd)
	{
		bHasCursorClipRect = false;
		if (bCursorCaptured)
		{
			ApplyCursorClip();
		}
		return;
	}

	// InViewportScreenRect는 ImGui의 Screen Space(전체 모니터 기준) 좌표
	// 하지만 UGameViewportClient는 내부적으로 ClientRect를 사용하며, 
	// 이후 ApplyCursorClip()이나 Tick()에서 ClientToScreen()을 한 번 더 호출
	// 따라서 여기서 미리 Client 좌표로 변환해 두어야 이중 오프셋 적용을 막dma
	POINT TopLeft = { static_cast<LONG>(InViewportScreenRect.X), static_cast<LONG>(InViewportScreenRect.Y) };
	::ScreenToClient(OwnerHWnd, &TopLeft);

	CursorClipClientRect.left = TopLeft.x;
	CursorClipClientRect.top = TopLeft.y;
	CursorClipClientRect.right = TopLeft.x + static_cast<LONG>(InViewportScreenRect.Width);
	CursorClipClientRect.bottom = TopLeft.y + static_cast<LONG>(InViewportScreenRect.Height);

	bHasCursorClipRect = CursorClipClientRect.right > CursorClipClientRect.left
		&& CursorClipClientRect.bottom > CursorClipClientRect.top;

	if (bCursorCaptured)
	{
		ApplyCursorClip();
	}
}

bool UGameViewportClient::TryGetCursorViewportPosition(float& OutViewportX, float& OutViewportY) const
{
	OutViewportX = 0.0f;
	OutViewportY = 0.0f;

	if (!Viewport || !bHasCursorClipRect || !OwnerHWnd)
	{
		return false;
	}

	const float ViewportWidth = static_cast<float>(Viewport->GetWidth());
	const float ViewportHeight = static_cast<float>(Viewport->GetHeight());
	const float RectWidth = static_cast<float>(CursorClipClientRect.right - CursorClipClientRect.left);
	const float RectHeight = static_cast<float>(CursorClipClientRect.bottom - CursorClipClientRect.top);
	if (ViewportWidth <= 0.0f || ViewportHeight <= 0.0f || RectWidth <= 0.0f || RectHeight <= 0.0f)
	{
		return false;
	}

	POINT CursorPoint{};
	if (!::GetCursorPos(&CursorPoint))
	{
		return false;
	}

	if (!::ScreenToClient(OwnerHWnd, &CursorPoint))
	{
		return false;
	}

	const float LocalX = static_cast<float>(CursorPoint.x - CursorClipClientRect.left);
	const float LocalY = static_cast<float>(CursorPoint.y - CursorClipClientRect.top);
	if (LocalX < 0.0f || LocalY < 0.0f || LocalX >= RectWidth || LocalY >= RectHeight)
	{
		return false;
	}

	OutViewportX = LocalX * (ViewportWidth / RectWidth);
	OutViewportY = LocalY * (ViewportHeight / RectHeight);
	return true;
}

void UGameViewportClient::SetPossessed(bool bPossessed)
{
	if (bPIEPossessedInputEnabled == bPossessed)
	{
		return;
	}

	bPIEPossessedInputEnabled = bPossessed;
	// 본 프로젝트는 마우스 미사용: possess 여부와 무관하게 커서/마우스 트래킹 비활성.
	SetCursorCaptured(false);
	FInputManager::Get().SetTrackingMouse(false);

	ResetInputState();
}

void UGameViewportClient::Possess(UCameraComponent* TargetCamera)
{
	if (PossessedCamera == TargetCamera)
	{
		return;
	}

	PossessedCamera = TargetCamera;
	ResetInputState();
}

void UGameViewportClient::UnPossess()
{
	PossessedCamera = nullptr;
	SetCursorCaptured(false);
	FInputManager::Get().SetTrackingMouse(false);
	ResetInputState();
}

void UGameViewportClient::ResetInputState()
{
	MoveInputAccumulator = FVector::ZeroVector;
	LookInputAccumulator = FVector::ZeroVector;
}

bool UGameViewportClient::Tick(float DeltaTime)
{
	if (!HasPossessedTarget())
	{
		return false;
	}

	bool bChanged = false;

	// 본 프로젝트는 마우스 입력을 사용하지 않으므로 항상 커서 노출 (캡처/클립 해제)
	const bool bScriptDrivesCamera = true;
	SetCursorCaptured(false);

	if (bPIEPossessedInputEnabled)
	{
		// Reset accumulators before processing
		MoveInputAccumulator = FVector::ZeroVector;
		LookInputAccumulator = FVector::ZeroVector;

		// Process Enhanced Input
		EnhancedInputManager.ProcessInput(&FInputManager::Get(), DeltaTime);

		// Apply Accumulated Input
		UCameraComponent* TargetCamera = GetDrivingCamera();

		if (TargetCamera && !bScriptDrivesCamera)
		{
			// Movement
			if (!MoveInputAccumulator.IsNearlyZero())
			{
				MoveInputAccumulator = MoveInputAccumulator.Normalized();

				FVector FlatForward = TargetCamera->GetForwardVector();
				FVector FlatRight = TargetCamera->GetRightVector();
				FlatForward.Z = 0.0f;
				FlatRight.Z = 0.0f;
				if (!FlatForward.IsNearlyZero()) FlatForward = FlatForward.Normalized();
				if (!FlatRight.IsNearlyZero()) FlatRight = FlatRight.Normalized();

				const float SpeedBoost = bIsSprinting ? InputSettings.SprintMultiplier : 1.0f;
				const FVector WorldDelta = (FlatForward * MoveInputAccumulator.X + FlatRight * MoveInputAccumulator.Y + FVector::UpVector * MoveInputAccumulator.Z)
					* (InputSettings.MoveSpeed * SpeedBoost * DeltaTime);

				TargetCamera->SetWorldLocation(TargetCamera->GetWorldLocation() + WorldDelta);
				bChanged = true;
			}

			// Look
			if (!LookInputAccumulator.IsNearlyZero())
			{
				FRotator Rotation = TargetCamera->GetRelativeRotation();
				Rotation.Yaw += LookInputAccumulator.X * InputSettings.LookSensitivity;
				Rotation.Pitch = Clamp(
					Rotation.Pitch + LookInputAccumulator.Y * InputSettings.LookSensitivity,
					InputSettings.MinPitch,
					InputSettings.MaxPitch);
				Rotation.Roll = 0.0f;
				TargetCamera->SetRelativeRotation(Rotation);
				bChanged = true;
			}
		}
	}

	// Recenter mouse after processing to allow continuous rotation
	if (bCursorCaptured && OwnerHWnd)
	{
		POINT center{};
		if (bHasCursorClipRect)
		{
			center.x = (CursorClipClientRect.left + CursorClipClientRect.right) / 2;
			center.y = (CursorClipClientRect.top + CursorClipClientRect.bottom) / 2;
		}
		else
		{
			RECT rect;
			::GetClientRect(OwnerHWnd, &rect);
			center.x = (rect.left + rect.right) / 2;
			center.y = (rect.top + rect.bottom) / 2;
		}
		::ClientToScreen(OwnerHWnd, &center);
		::SetCursorPos(center.x, center.y);
		FInputManager::Get().SetLastMousePos(center);
	}

	return bChanged;
}

void UGameViewportClient::SetupInput()
{
	// Create Actions
	ActionMove = new FInputAction("IA_Move", EInputActionValueType::Axis3D);
	ActionLook = new FInputAction("IA_Look", EInputActionValueType::Axis2D);
	ActionSprint = new FInputAction("IA_Sprint", EInputActionValueType::Bool);

	// Create Mapping Context
	DefaultMappingContext = new FInputMappingContext();
	DefaultMappingContext->ContextName = "IMC_Default";

	// Move Mappings
	// W: Forward (X=1)
	DefaultMappingContext->AddMapping(ActionMove, 'W');
	// S: Backward (X=-1)
	DefaultMappingContext->AddMapping(ActionMove, 'S').Modifiers.push_back(new FModifierScale(FVector(-1, 1, 1)));
	// D: Right (Y=1)
	{
		auto& Mapping = DefaultMappingContext->AddMapping(ActionMove, 'D');
		Mapping.Modifiers.push_back(new FModifierSwizzleAxis(FModifierSwizzleAxis::ESwizzleOrder::YXZ));
	}
	// A: Left (Y=-1)
	{
		auto& Mapping = DefaultMappingContext->AddMapping(ActionMove, 'A');
		Mapping.Modifiers.push_back(new FModifierSwizzleAxis(FModifierSwizzleAxis::ESwizzleOrder::YXZ));
		Mapping.Modifiers.push_back(new FModifierScale(FVector(1, -1, 1)));
	}
	// E / Space: Up (Z=1)
	{
		auto& Mapping = DefaultMappingContext->AddMapping(ActionMove, 'E');
		Mapping.Modifiers.push_back(new FModifierSwizzleAxis(FModifierSwizzleAxis::ESwizzleOrder::ZYX));
	}
	DefaultMappingContext->AddMapping(ActionMove, VK_SPACE).Modifiers.push_back(new FModifierSwizzleAxis(FModifierSwizzleAxis::ESwizzleOrder::ZYX));

	// Q / Ctrl: Down (Z=-1)
	{
		auto& Mapping = DefaultMappingContext->AddMapping(ActionMove, 'Q');
		Mapping.Modifiers.push_back(new FModifierSwizzleAxis(FModifierSwizzleAxis::ESwizzleOrder::ZYX));
		Mapping.Modifiers.push_back(new FModifierScale(FVector(1, 1, -1)));
	}
	DefaultMappingContext->AddMapping(ActionMove, VK_CONTROL).Modifiers.push_back(new FModifierSwizzleAxis(FModifierSwizzleAxis::ESwizzleOrder::ZYX));
	DefaultMappingContext->Mappings.back().Modifiers.push_back(new FModifierScale(FVector(1, 1, -1)));

	// Look Mappings
	// MouseX -> Yaw
	DefaultMappingContext->AddMapping(ActionLook, static_cast<int32>(EInputKey::MouseX));
	// MouseY -> Pitch (Needs YXZ swizzle to map to Axis2D.Y)
	DefaultMappingContext->AddMapping(ActionLook, static_cast<int32>(EInputKey::MouseY)).Modifiers.push_back(new FModifierSwizzleAxis(FModifierSwizzleAxis::ESwizzleOrder::YXZ));

	// Sprint Mappings
	DefaultMappingContext->AddMapping(ActionSprint, VK_SHIFT);

	//Add Context to Manager
	EnhancedInputManager.AddMappingContext(DefaultMappingContext, 0);

	// Bind Actions
	EnhancedInputManager.BindAction(ActionMove, ETriggerEvent::Triggered, [this](const FInputActionValue& Value) { OnMove(Value); });
	EnhancedInputManager.BindAction(ActionLook, ETriggerEvent::Triggered, [this](const FInputActionValue& Value) { OnLook(Value); });
	EnhancedInputManager.BindAction(ActionSprint, ETriggerEvent::Started, [this](const FInputActionValue& Value) { OnSprintStarted(Value); });
	EnhancedInputManager.BindAction(ActionSprint, ETriggerEvent::Completed, [this](const FInputActionValue& Value) { OnSprintCompleted(Value); });
	EnhancedInputManager.BindAction(ActionSprint, ETriggerEvent::Canceled, [this](const FInputActionValue& Value) { OnSprintCompleted(Value); });
}

void UGameViewportClient::OnMove(const FInputActionValue& Value)
{
	MoveInputAccumulator = MoveInputAccumulator + Value.GetVector();
}

void UGameViewportClient::OnLook(const FInputActionValue& Value)
{
	LookInputAccumulator = LookInputAccumulator + Value.GetVector();
}

void UGameViewportClient::OnSprintStarted(const FInputActionValue& Value)
{
	bIsSprinting = true;
}

void UGameViewportClient::OnSprintCompleted(const FInputActionValue& Value)
{
	bIsSprinting = false;
}

void UGameViewportClient::SetCursorCaptured(bool bCaptured)
{
	if (bCursorCaptured == bCaptured)
	{
		if (bCaptured)
		{
			ApplyCursorClip();
		}
		return;
	}

	bCursorCaptured = bCaptured;
	if (bCursorCaptured)
	{
		while (::ShowCursor(FALSE) >= 0) {}
		ApplyCursorClip();
		return;
	}

	while (::ShowCursor(TRUE) < 0) {}
	::ClipCursor(nullptr);
}

void UGameViewportClient::ApplyCursorClip()
{
	if (!OwnerHWnd)
	{
		return;
	}

	RECT ClientRect = {};
	if (bHasCursorClipRect)
	{
		ClientRect = CursorClipClientRect;
	}
	else if (!::GetClientRect(OwnerHWnd, &ClientRect))
	{
		return;
	}

	POINT TopLeft = { ClientRect.left, ClientRect.top };
	POINT BottomRight = { ClientRect.right, ClientRect.bottom };
	if (!::ClientToScreen(OwnerHWnd, &TopLeft) || !::ClientToScreen(OwnerHWnd, &BottomRight))
	{
		return;
	}

	RECT ScreenRect = { TopLeft.x, TopLeft.y, BottomRight.x, BottomRight.y };
	if (ScreenRect.right > ScreenRect.left && ScreenRect.bottom > ScreenRect.top)
	{
		::ClipCursor(&ScreenRect);
	}
}

