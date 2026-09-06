#pragma once

#include "Viewport/ViewportClient.h"
#include "Math/Vector.h"
#include "Input/EnhancedInputManager.h"
#include "Input/InputAction.h"
#include "Input/InputMappingContext.h"

class UCameraComponent;
class FWindowsWindow;
class FViewport;

// ObjViewer용 간이 뷰포트 클라이언트 — 마우스 오빗/줌/팬
class FObjViewerViewportClient : public FViewportClient
{
public:
	FObjViewerViewportClient();
	~FObjViewerViewportClient() override;

	void Initialize(FWindowsWindow* InWindow);
	void Release();

	// Camera lifecycle
	void CreateCamera();
	void DestroyCamera();
	void ResetCamera();
	UCameraComponent* GetCamera() const { return Camera; }

	// Viewport
	void SetViewport(FViewport* InViewport) { Viewport = InViewport; }
	FViewport* GetViewport() const { return Viewport; }

	void Tick(float DeltaTime);

	// 뷰포트 영역 설정 (ImGui 패널에서 호출)
	void SetViewportRect(float X, float Y, float Width, float Height);

	// ImDrawList에 SRV를 그려주는 헬퍼
	void RenderViewportImage();

private:
	void SetupInput();
	void OnOrbit(const FInputActionValue& Value);
	void OnPan(const FInputActionValue& Value);
	void OnZoom(const FInputActionValue& Value);

	void TickInput(float DeltaTime);

private:
	FViewport* Viewport = nullptr;
	FWindowsWindow* Window = nullptr;
	UCameraComponent* Camera = nullptr;

	// 오빗 파라미터
	FVector OrbitTarget = FVector(0, 0, 0);
	float OrbitDistance = 5.0f;
	float OrbitYaw = 0.0f;		// degrees
	float OrbitPitch = 30.0f;	// degrees

	// 뷰포트 스크린 영역
	float ViewportX = 0.0f;
	float ViewportY = 0.0f;
	float ViewportWidth = 800.0f;
	float ViewportHeight = 600.0f;

	// Enhanced Input
	FEnhancedInputManager EnhancedInputManager;
	FInputMappingContext* ObjMappingContext = nullptr;

	FInputAction* ActionObjOrbit = nullptr;
	FInputAction* ActionObjPan = nullptr;
	FInputAction* ActionObjZoom = nullptr;

	FVector OrbitAccumulator = FVector::ZeroVector;
	FVector PanAccumulator = FVector::ZeroVector;
	float ZoomAccumulator = 0.0f;
};
