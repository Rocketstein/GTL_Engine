#pragma once
#include "Engine/Core/RayTypes.h"
#include "Object/ObjectFactory.h"
#include "Component/SceneComponent.h"
#include "Math/Matrix.h"
#include "Math/MathUtils.h"
#include "Math/Vector.h"
#include "Collision/ConvexVolume.h"
#include "Camera/MinimalViewInfo.h"

class UCameraComponent : public USceneComponent
{
public:
	DECLARE_CLASS(UCameraComponent, USceneComponent)

	UCameraComponent() = default;

	void BeginPlay() override;
	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void LookAt(const FVector& Target);

	// Function : Replace camera component state and sync transform
	// input : NewState
	// NewState : full camera POV stored by this component
	void SetCameraState(const FMinimalViewInfo& NewState);

	// Function : Read camera component state as POV
	// input : none
	// CameraState : returned after world transform is refreshed
	const FMinimalViewInfo& GetCameraState() const;

	void SetFOV(float InFOV) { CameraState.FOV = InFOV; }
	void SetOrthoWidth(float InWidth) { CameraState.OrthoWidth = InWidth; }
	void SetOrthographic(bool bOrtho) { CameraState.bIsOrthogonal = bOrtho; }

	void OnResize(int32 Width, int32 Height);

	FMatrix GetViewMatrix() const;
	FMatrix GetProjectionMatrix() const;
	FMatrix GetViewProjectionMatrix() const;
	FConvexVolume GetConvexVolume() const;

	// Function : Fill camera POV from component state
	// input : DeltaTime, OutView
	// DeltaTime : frame delta time for view providers
	// OutView : camera view data copied from this component
	void GetCameraView(float DeltaTime, FMinimalViewInfo& OutView) const;

	float GetFOV() const { return CameraState.FOV; }
	float GetNearPlane() const { return CameraState.NearZ; }
	float GetFarPlane() const { return CameraState.FarZ; }
	float GetOrthoWidth() const { return CameraState.OrthoWidth; }
	bool IsOrthogonal() const { return CameraState.bIsOrthogonal; }

	FRay DeprojectScreenToWorld(float MouseX, float MouseY, float ScreenWidth, float ScreenHeight);

private:
	// Function : Copy current world transform into camera state
	// input : none
	// CameraState : mutable cached POV owned by the component
	void RefreshCameraStateTransform() const;

private:
	mutable FMinimalViewInfo CameraState;
};
