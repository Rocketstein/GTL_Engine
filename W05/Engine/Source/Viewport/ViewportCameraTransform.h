#pragma once

#include "ApplicationCore/InputEvent.h"
#include "Core/Math/Transform/Rotator.h"
#include "Core/Math/Vector3.h"
#include "Core/Platform/PlatformTypes.h"

enum class EProjectionMode : uint8
{
    Perspective,
    Orthographic,
};

enum class ECameraControlMode : uint8
{
    None,
    Rotate,
    Pan,
    Zoom,
};

struct FCameraViewState
{
    EProjectionMode ProjectionMode = EProjectionMode::Perspective;

    float VerticalFovDegrees = 60.0f;
    float NearPlane = 1.0f;
    float FarPlane = 100000.0f;
    float OrthoWidth = 2048.0f;
};

struct FCameraInput
{
    float MoveForward = 0.0f;
    float MoveRight = 0.0f;
    float MoveUpLocal = 0.0f;
    float MoveUpWorld = 0.0f;

    float YawDelta = 0.0f;
    float PitchDelta = 0.0f;
    float RollDelta = 0.0f;

    float Zoom = 0.0f;
    float PanX = 0.0f;
    float PanY = 0.0f;

    ECameraControlMode ControlMode = ECameraControlMode::None;

    void ResetFrameInput()
    {
        MoveForward = 0.0f;
        MoveRight = 0.0f;
        MoveUpLocal = 0.0f;
        MoveUpWorld = 0.0f;

        YawDelta = 0.0f;
        PitchDelta = 0.0f;
        RollDelta = 0.0f;
        PanX = 0.0f;
        PanY = 0.0f;
        Zoom = 0.0f;
        ControlMode = ECameraControlMode::None;
    }
};

struct FCameraControllerConfig
{
    float TranslationAcceleration = 2500.0f;
    float TranslationDamping = 10.0f;

    float RotationSpeed = 0.1f;
    float ZoomSpeed = 2000.0f;

    float MinPitchDegrees = -89.0f;
    float MaxPitchDegrees = 89.0f;

    float PanSpeed = 0.3f;
    float ZoomTranslationScale = 1.0f;
};

class FViewportCameraTransform
{
  public:
    const FVector3 &GetLocation() const { return ViewLocation; }
    const FRotator &GetRotation() const { return ViewRotation; }

    void SetLocation(const FVector3 &InLocation) { ViewLocation = InLocation; }
    void SetRotation(const FRotator &InRotation) { ViewRotation = InRotation; }

  private:
    FVector3 ViewLocation = FVector3(360.0f, 360.0f, 360.0f);
    FRotator ViewRotation = FRotator(-45.0f, -135.0f, 0.0f);
};
