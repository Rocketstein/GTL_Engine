#pragma once

#include "Core/CoreTypes.h"
#include "Math/Vector.h"
#include "Math/Rotator.h"
#include "Collision/OverlapInfo.h"

class UActorComponent;
struct FLuaActorProxy;

namespace sol
{
	template <typename T>
	class optional;
}

struct FLuaComponentProxy
{
	UActorComponent* Component = nullptr;

	bool IsValid() const;
	UActorComponent* GetComponent() const;

	FString GetName() const;
	FString GetTypeName() const;
	FLuaActorProxy GetOwner() const;

	bool SetActive(bool bActive);
	bool IsActive() const;
	bool SetVisible(bool bVisible);
	bool IsVisible() const;

	sol::optional<FVector> GetWorldLocation() const;
	bool SetWorldLocation(const FVector& InLocation);
	bool SetWorldLocationXYZ(float X, float Y, float Z);
	sol::optional<FVector> GetLocalLocation() const;
	bool SetLocalLocation(const FVector& InLocation);
	bool SetLocalLocationXYZ(float X, float Y, float Z);
	bool AddWorldOffset(const FVector& Delta);
	bool AddWorldOffsetXYZ(float X, float Y, float Z);
	bool AddLocalOffset(const FVector& Delta);
	bool AddLocalOffsetXYZ(float X, float Y, float Z);
	sol::optional<FRotator> GetWorldRotation() const;
	bool SetWorldRotation(const FRotator& InRotation);
	bool SetWorldRotationXYZ(float Pitch, float Yaw, float Roll);
	sol::optional<FRotator> GetLocalRotation() const;
	bool SetLocalRotation(const FRotator& InRotation);
	bool SetLocalRotationXYZ(float Pitch, float Yaw, float Roll);
	sol::optional<FVector> GetWorldScale() const;
	bool SetWorldScale(const FVector& InScale);
	bool SetWorldScaleXYZ(float X, float Y, float Z);
	sol::optional<FVector> GetLocalScale() const;
	bool SetLocalScale(const FVector& InScale);
	bool SetLocalScaleXYZ(float X, float Y, float Z);

	sol::optional<FVector> GetForwardVector() const;
	sol::optional<FVector> GetRightVector() const;
	sol::optional<FVector> GetUpVector() const;

	bool SetCollisionEnabled(bool bEnabled);
	bool SetGenerateOverlapEvents(bool bEnabled);
	bool IsOverlappingActor(const FLuaActorProxy& OtherActor) const;

	FString GetShapeType() const;
	sol::optional<float> GetShapeHalfHeight() const;
	bool SetShapeHalfHeight(float HalfHeight);
	sol::optional<float> GetShapeRadius() const;
	bool SetShapeRadius(float Radius);
	sol::optional<FVector> GetShapeExtent() const;
	bool SetShapeExtent(const FVector& Extent);

	bool SetStaticMesh(const FString& MeshPath);

	bool SetText(const FString& Text);
	sol::optional<FString> GetText() const;

	sol::optional<FVector> GetScreenPosition() const;
	bool SetScreenPosition(const FVector& InScreenPosition);
	bool SetScreenPositionXYZ(float X, float Y, float Z);
	sol::optional<FVector> GetScreenSize() const;
	bool SetScreenSize(const FVector& InScreenSize);
	bool SetScreenSizeXYZ(float X, float Y, float Z);

	bool SetTexture(const FString& TexturePath);
	sol::optional<FString> GetTexturePath() const;
	bool SetTint(const FVector& TintRGB);
	bool SetTintRGBA(float R, float G, float B, float A);
	bool SetLabel(const FString& Label);
	sol::optional<FString> GetLabel() const;
	bool IsHovered() const;
	bool IsPressed() const;
	bool WasClicked() const;
	bool SetAudioPath(const FString& AudioPath);
	sol::optional<FString> GetAudioPath() const;
	bool SetAudioCategory(const FString& CategoryName);
	sol::optional<FString> GetAudioCategory() const;
	bool SetAudioLooping(bool bLooping);
	bool IsAudioLooping() const;
	bool PlayAudio();
	bool PlayAudioPath(const FString& AudioPath);
	bool StopAudio();
	bool PauseAudio();
	bool ResumeAudio();
	bool IsAudioPlaying() const;

	bool SetSpeed(float Speed);
	sol::optional<float> GetSpeed() const;
	bool MoveTo(const FVector& Target);
	bool MoveToXYZ(float X, float Y, float Z);
	bool MoveBy(const FVector& Delta);
	bool MoveByXYZ(float X, float Y, float Z);
	bool StopMove();
	bool IsMoveDone() const;
	//ToDelete
	/*bool StartCameraShake(float Intensity, float Duration);
	bool AddHitEffect(float Intensity, float Duration);*/

	bool SetBoxExtent(const FVector& Extent);
	bool SetBoxExtentXYZ(float X, float Y, float Z);
	sol::optional<FVector> GetBoxExtent() const;
};
 
