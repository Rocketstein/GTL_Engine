#include "ObjViewer/ObjViewerViewportClient.h"
#include "Engine/Input/InputManager.h"
#include "Engine/Input/InputModifier.h"
#include "Engine/Input/InputTrigger.h"
#include "Component/CameraComponent.h"
#include "Viewport/Viewport.h"
#include "Math/MathUtils.h"
#include "imgui.h"

FObjViewerViewportClient::FObjViewerViewportClient()
{
	SetupInput();
}

FObjViewerViewportClient::~FObjViewerViewportClient()
{
	EnhancedInputManager.ClearBindings();
	EnhancedInputManager.ClearAllMappingContexts();

	if (ObjMappingContext)
	{
		for (auto& Mapping : ObjMappingContext->Mappings)
		{
			for (auto* Trigger : Mapping.Triggers) delete Trigger;
			for (auto* Modifier : Mapping.Modifiers) delete Modifier;
		}
		delete ObjMappingContext;
	}

	delete ActionObjOrbit;
	delete ActionObjPan;
	delete ActionObjZoom;
}

void FObjViewerViewportClient::Initialize(FWindowsWindow* InWindow)
{
	Window = InWindow;
}

void FObjViewerViewportClient::Release()
{
	DestroyCamera();
}

void FObjViewerViewportClient::CreateCamera()
{
	DestroyCamera();
	Camera = UObjectManager::Get().CreateObject<UCameraComponent>();
}

void FObjViewerViewportClient::DestroyCamera()
{
	if (Camera)
	{
		UObjectManager::Get().DestroyObject(Camera);
		Camera = nullptr;
	}
}

void FObjViewerViewportClient::ResetCamera()
{
	OrbitTarget = FVector(0, 0, 0);
	OrbitDistance = 5.0f;
	OrbitYaw = 0.0f;
	OrbitPitch = 30.0f;
}

void FObjViewerViewportClient::Tick(float DeltaTime)
{
	TickInput(DeltaTime);
}

void FObjViewerViewportClient::SetViewportRect(float X, float Y, float Width, float Height)
{
	ViewportX = X;
	ViewportY = Y;
	ViewportWidth = Width;
	ViewportHeight = Height;

	if (Camera)
	{
		Camera->OnResize(static_cast<int32>(Width), static_cast<int32>(Height));
	}
}

void FObjViewerViewportClient::RenderViewportImage()
{
	if (!Viewport || !Viewport->GetSRV()) return;

	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	ImVec2 Min(ViewportX, ViewportY);
	ImVec2 Max(ViewportX + ViewportWidth, ViewportY + ViewportHeight);

	DrawList->AddImage((ImTextureID)Viewport->GetSRV(), Min, Max);
}

void FObjViewerViewportClient::SetupInput()
{
	ActionObjOrbit = new FInputAction("IA_ObjOrbit", EInputActionValueType::Axis2D);
	ActionObjPan = new FInputAction("IA_ObjPan", EInputActionValueType::Axis2D);
	ActionObjZoom = new FInputAction("IA_ObjZoom", EInputActionValueType::Float);

	ObjMappingContext = new FInputMappingContext();
	ObjMappingContext->ContextName = "IMC_ObjViewer";

	// Orbit: MouseX/Y
	ObjMappingContext->AddMapping(ActionObjOrbit, static_cast<int32>(EInputKey::MouseX));
	ObjMappingContext->AddMapping(ActionObjOrbit, static_cast<int32>(EInputKey::MouseY)).Modifiers.push_back(new FModifierSwizzleAxis(FModifierSwizzleAxis::ESwizzleOrder::YXZ));

	// Pan: MouseX/Y
	ObjMappingContext->AddMapping(ActionObjPan, static_cast<int32>(EInputKey::MouseX));
	ObjMappingContext->AddMapping(ActionObjPan, static_cast<int32>(EInputKey::MouseY)).Modifiers.push_back(new FModifierSwizzleAxis(FModifierSwizzleAxis::ESwizzleOrder::YXZ));

	// Zoom: Wheel
	ObjMappingContext->AddMapping(ActionObjZoom, static_cast<int32>(EInputKey::MouseWheel));

	EnhancedInputManager.AddMappingContext(ObjMappingContext, 0);

	EnhancedInputManager.BindAction(ActionObjOrbit, ETriggerEvent::Triggered, [this](const FInputActionValue& V) { OnOrbit(V); });
	EnhancedInputManager.BindAction(ActionObjPan, ETriggerEvent::Triggered, [this](const FInputActionValue& V) { OnPan(V); });
	EnhancedInputManager.BindAction(ActionObjZoom, ETriggerEvent::Triggered, [this](const FInputActionValue& V) { OnZoom(V); });
}

void FObjViewerViewportClient::OnOrbit(const FInputActionValue& Value)
{
	if (FInputManager::Get().IsMouseButtonDown(VK_LBUTTON) || FInputManager::Get().IsMouseButtonDown(VK_RBUTTON))
	{
		OrbitAccumulator = OrbitAccumulator + Value.GetVector();
	}
}

void FObjViewerViewportClient::OnPan(const FInputActionValue& Value)
{
	if (FInputManager::Get().IsMouseButtonDown(VK_MBUTTON))
	{
		PanAccumulator = PanAccumulator + Value.GetVector();
	}
}

void FObjViewerViewportClient::OnZoom(const FInputActionValue& Value)
{
	ZoomAccumulator += Value.Get();
}

void FObjViewerViewportClient::TickInput(float DeltaTime)
{
	if (!Camera) return;

	FInputManager& Input = FInputManager::Get();
	if (Input.IsGuiUsingKeyboard()) return;

	OrbitAccumulator = FVector::ZeroVector;
	PanAccumulator = FVector::ZeroVector;
	ZoomAccumulator = 0.0f;

	EnhancedInputManager.ProcessInput(&Input, DeltaTime);

	// Orbit
	if (!OrbitAccumulator.IsNearlyZero())
	{
		OrbitYaw += OrbitAccumulator.X * 0.25f;
		OrbitPitch = Clamp(OrbitPitch + OrbitAccumulator.Y * 0.25f, -89.0f, 89.0f);
	}

	// Zoom
	if (std::abs(ZoomAccumulator) > 1e-6f)
	{
		OrbitDistance -= ZoomAccumulator * 0.5f;
		OrbitDistance = Clamp(OrbitDistance, 0.1f, 100.0f);
	}

	// Pan
	if (!PanAccumulator.IsNearlyZero())
	{
		float PanScale = OrbitDistance * 0.002f;
		FVector Right = Camera->GetRightVector();
		FVector Up = Camera->GetUpVector();
		OrbitTarget = OrbitTarget + (Right * (-PanAccumulator.X * PanScale)) + (Up * (PanAccumulator.Y * PanScale));
	}

	// Update Camera Transform
	FRotator Rotation(OrbitPitch, OrbitYaw, 0.0f);
	FVector Forward = Rotation.ToVector();
	FVector NewPos = OrbitTarget - Forward * OrbitDistance;

	Camera->SetWorldLocation(NewPos);
	Camera->SetRelativeRotation(Rotation);
}
