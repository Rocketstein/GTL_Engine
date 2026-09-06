#include "Component/CameraComponent.h"

#include <cmath>

IMPLEMENT_CLASS(UCameraComponent, USceneComponent)
HIDE_FROM_COMPONENT_LIST(UCameraComponent)

FMatrix UCameraComponent::GetViewMatrix() const
{
	UpdateWorldMatrix();
	RefreshCameraStateTransform();
	return FMatrix::MakeViewMatrix(
		CameraState.Rotation.GetRightVector(),
		CameraState.Rotation.GetUpVector(),
		CameraState.Rotation.GetForwardVector(),
		CameraState.Location);
}

FMatrix UCameraComponent::GetProjectionMatrix() const
{
	if (!CameraState.bIsOrthogonal)
	{
		return FMatrix::PerspectiveFovLH(CameraState.FOV, CameraState.AspectRatio, CameraState.NearZ, CameraState.FarZ);
	}

	const float HalfW = CameraState.OrthoWidth * 0.5f;
	const float HalfH = HalfW / CameraState.AspectRatio;
	return FMatrix::OrthoLH(HalfW * 2.0f, HalfH * 2.0f, CameraState.NearZ, CameraState.FarZ);
}

FMatrix UCameraComponent::GetViewProjectionMatrix() const
{
	return GetViewMatrix() * GetProjectionMatrix();
}

FConvexVolume UCameraComponent::GetConvexVolume() const
{
	FConvexVolume ConvexVolume;
	ConvexVolume.UpdateFromMatrix(GetViewMatrix() * GetProjectionMatrix());
	return ConvexVolume;
}

void UCameraComponent::RefreshCameraStateTransform() const
{
	CameraState.Location = GetWorldLocation();
	CameraState.Rotation = FRotator(GetWorldRotation());
}

const FMinimalViewInfo& UCameraComponent::GetCameraState() const
{
	RefreshCameraStateTransform();
	return CameraState;
}

void UCameraComponent::GetCameraView(float DeltaTime, FMinimalViewInfo& OutView) const
{
	OutView = GetCameraState();
}

void UCameraComponent::LookAt(const FVector& Target)
{
	FVector Position = GetWorldLocation();
	FVector Diff = (Target - Position).Normalized();

	constexpr float Rad2Deg = 180.0f / 3.14159265358979f;

	FRotator LookRotation = GetRelativeRotation();
	LookRotation.Pitch = -asinf(Diff.Z) * Rad2Deg;

	if (fabsf(Diff.Z) < 0.999f)
	{
		LookRotation.Yaw = atan2f(Diff.Y, Diff.X) * Rad2Deg;
	}

	SetRelativeRotation(LookRotation);
}

void UCameraComponent::OnResize(int32 Width, int32 Height)
{
	if (Height > 0)
	{
		CameraState.AspectRatio = static_cast<float>(Width) / static_cast<float>(Height);
	}
}

void UCameraComponent::SetCameraState(const FMinimalViewInfo& NewState)
{
	CameraState = NewState;
	SetWorldLocation(CameraState.Location);
	SetWorldRotation(CameraState.Rotation);
	RefreshCameraStateTransform();
}

FRay UCameraComponent::DeprojectScreenToWorld(float MouseX, float MouseY, float ScreenWidth, float ScreenHeight)
{
	FRay Ray{};
	if (ScreenWidth <= 0.0f || ScreenHeight <= 0.0f)
	{
		Ray.Origin = GetWorldLocation();
		Ray.Direction = GetForwardVector();
		return Ray;
	}

	float NdcX = (2.0f * MouseX) / ScreenWidth - 1.0f;
	float NdcY = 1.0f - (2.0f * MouseY) / ScreenHeight;

	// Reversed-Z projection: near plane maps to 1, far plane maps to 0.
	const FVector NdcNear(NdcX, NdcY, 1.0f);
	const FVector NdcFar(NdcX, NdcY, 0.0f);

	const FMatrix InverseViewProjection = (GetViewMatrix() * GetProjectionMatrix()).GetInverse();
	const FVector WorldNear = InverseViewProjection.TransformPositionWithW(NdcNear);
	const FVector WorldFar = InverseViewProjection.TransformPositionWithW(NdcFar);

	FVector Dir = WorldFar - WorldNear;
	const float Length = std::sqrt(Dir.X * Dir.X + Dir.Y * Dir.Y + Dir.Z * Dir.Z);
	Dir = (Length > 1e-4f) ? (Dir / Length) : GetForwardVector();

	if (CameraState.bIsOrthogonal)
	{
		Ray.Origin = WorldNear;
		Ray.Direction = GetForwardVector();
	}
	else
	{
		Ray.Origin = GetWorldLocation();
		Ray.Direction = Dir;
	}

	return Ray;
}

void UCameraComponent::BeginPlay()
{
	USceneComponent::BeginPlay();
	SetComponentTickEnabled(true);
}

void UCameraComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
	USceneComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);
	RefreshCameraStateTransform();
}

void UCameraComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	USceneComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "FOV",          EPropertyType::Float, &CameraState.FOV, 0.1f, 3.14f, 0.01f });
	OutProps.push_back({ "Near Z",       EPropertyType::Float, &CameraState.NearZ, 0.01f, 100.0f, 0.01f });
	OutProps.push_back({ "Far Z",        EPropertyType::Float, &CameraState.FarZ, 1.0f, 100000.0f, 10.0f });
	OutProps.push_back({ "Orthographic", EPropertyType::Bool,  &CameraState.bIsOrthogonal });
	OutProps.push_back({ "Ortho Width",  EPropertyType::Float, &CameraState.OrthoWidth, 0.1f, 1000.0f, 0.5f });
}
