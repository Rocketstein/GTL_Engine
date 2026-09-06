#pragma once
#include "Object/Reflection/ObjectFactory.h"
#include "Component/SceneComponent.h"
#include "Math/MathUtils.h"
#include "Math/Vector.h"
#include "GameFramework/Camera/CameraTypes.h"

struct FMinimalViewInfo;

struct FCameraState
{
	float FOV = 3.14159265358979f / 3.0f;
	float AspectRatio = 16.0f / 9.0f;
	float NearZ = 0.1f;
	float FarZ = 1000.0f;
	float OrthoWidth = 10.0f;
	bool bIsOrthogonal = false;
};

#include "Source/Engine/Component/Camera/CameraComponent.generated.h"

UCLASS()
class UCameraComponent : public USceneComponent
{
public:
	GENERATED_BODY()

	UCameraComponent() = default;

	void BeginPlay() override;
	void EndPlay() override;


	UFUNCTION(Callable, Category="Camera")
	void LookAt(const FVector& Target);
	void SetCameraState(const FCameraState& NewState);
	const FCameraState& GetCameraState() const { return CameraState; }

	// 카메라 POV 통화 산출 — UE: UCameraComponent::GetCameraView.
	// CameraManager / RenderPipeline 이 이걸 받아 매트릭스/프러스텀을 빌드한다.
	// DeltaTime 은 향후 카메라 lag / interpolation 에 쓰이도록 시그니처 보존.
	void GetCameraView(float DeltaTime, FMinimalViewInfo& OutPOV) const;

	UFUNCTION(Callable, Exec, Category="Camera")
	void SetFOV(float InFOV) { CameraState.FOV = InFOV; }
	UFUNCTION(Callable, Exec, Category="Camera")
	void SetOrthoWidth(float InWidth) { CameraState.OrthoWidth = InWidth; }
	UFUNCTION(Callable, Exec, Category="Camera")
	void SetOrthographic(bool bOrtho) { CameraState.bIsOrthogonal = bOrtho; }
	UFUNCTION(Callable, Exec, Category="Camera")
	void SetAspectRatio(float InAspectRatio) { CameraState.AspectRatio = InAspectRatio > 0.0f ? InAspectRatio : CameraState.AspectRatio; }
	UFUNCTION(Callable, Exec, Category="Camera")
	void SetNearPlane(float InNearZ) { CameraState.NearZ = InNearZ > 0.0f ? InNearZ : CameraState.NearZ; }
	UFUNCTION(Callable, Exec, Category="Camera")
	void SetFarPlane(float InFarZ) { CameraState.FarZ = InFarZ > CameraState.NearZ ? InFarZ : CameraState.FarZ; }

	UFUNCTION(Callable, Exec, Category="Camera|Letterbox")
	void SetLetterboxEnabled(bool bEnabled) { Letterbox.bEnabled = bEnabled; }
	UFUNCTION(Callable, Exec, Category="Camera|Letterbox")
	void SetLetterboxAmount(float Amount) { Letterbox.Amount = FMath::Clamp(Amount, 0.0f, 1.0f); }
	UFUNCTION(Callable, Exec, Category="Camera|Letterbox")
	void SetLetterboxThickness(float Thickness) { Letterbox.Thickness = FMath::Clamp(Thickness, 0.0f, 0.5f); }
	UFUNCTION(Callable, Exec, Category="Camera|Letterbox")
	void SetLetterboxColor(FLinearColor Color) { Letterbox.Color = Color; }
	const FCameraLetterboxState& GetLetterboxSettings() const { return Letterbox; }
	UFUNCTION(Pure, Category="Camera|Letterbox")
	bool IsLetterboxEnabled() const { return Letterbox.bEnabled; }
	UFUNCTION(Pure, Category="Camera|Letterbox")
	float GetLetterboxAmount() const { return Letterbox.Amount; }
	UFUNCTION(Pure, Category="Camera|Letterbox")
	float GetLetterboxThickness() const { return Letterbox.Thickness; }
	UFUNCTION(Pure, Category="Camera|Letterbox")
	FLinearColor GetLetterboxColor() const { return Letterbox.Color; }

	UFUNCTION(Callable, Exec, Category="Camera")
	void OnResize(int32 Width, int32 Height);

	UFUNCTION(Pure, Category="Camera")
	float GetFOV() const { return CameraState.FOV; }
	UFUNCTION(Pure, Category="Camera")
	float GetAspectRatio() const { return CameraState.AspectRatio; }
	UFUNCTION(Pure, Category="Camera")
	float GetNearPlane() const { return CameraState.NearZ; }
	UFUNCTION(Pure, Category="Camera")
	float GetFarPlane() const { return CameraState.FarZ; }
	UFUNCTION(Pure, Category="Camera")
	float GetOrthoWidth() const { return CameraState.OrthoWidth; }
	UFUNCTION(Pure, Category="Camera")
	bool IsOrthogonal() const { return CameraState.bIsOrthogonal; }

private:
	UPROPERTY(Edit, Save, Category="Camera", DisplayName="FOV", Member=CameraState.FOV, Type=Float, Min=0.1f, Max=3.14f, Speed=0.01f);
	UPROPERTY(Edit, Save, Category="Camera", DisplayName="Near Z", Member=CameraState.NearZ, Type=Float, Min=0.01f, Max=100.0f, Speed=0.01f);
	UPROPERTY(Edit, Save, Category="Camera", DisplayName="Far Z", Member=CameraState.FarZ, Type=Float, Min=1.0f, Max=100000.0f, Speed=10.0f);
	UPROPERTY(Edit, Save, Category="Camera", DisplayName="Orthographic", Member=CameraState.bIsOrthogonal, Type=Bool);
	UPROPERTY(Edit, Save, Category="Camera", DisplayName="Ortho Width", Member=CameraState.OrthoWidth, Type=Float, Min=0.1f, Max=1000.0f, Speed=0.5f);
	FCameraState CameraState;
	UPROPERTY(Edit, Save, Category="Camera|Letterbox", DisplayName="Enable Letterbox", Member=Letterbox.bEnabled, Type=Bool);
	UPROPERTY(Edit, Save, Category="Camera|Letterbox", DisplayName="Letterbox Amount", Member=Letterbox.Amount, Type=Float, Min=0.0f, Max=1.0f, Speed=0.01f);
	UPROPERTY(Edit, Save, Category="Camera|Letterbox", DisplayName="Letterbox Thickness", Member=Letterbox.Thickness, Type=Float, Min=0.0f, Max=0.5f, Speed=0.01f);
	UPROPERTY(Edit, Save, Category="Camera|Letterbox", DisplayName="Letterbox Color", Member=Letterbox.Color, Type=Color4);
	FCameraLetterboxState Letterbox;
};
