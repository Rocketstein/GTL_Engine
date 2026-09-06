#pragma once

#include "Core/Math/Vector3.h"
#include "Viewport/ViewportCameraTransform.h"

class FViewportCameraController
{
  public:
    void Tick(float DeltaTime, const FCameraInput &Input);

    const FViewportCameraTransform &GetTransform() const { return Transform; }
    void SetTransform(const FViewportCameraTransform &InTransform) { Transform = InTransform; }

    const FCameraControllerConfig &GetConfig() const { return Config; }
    void SetConfig(const FCameraControllerConfig &InConfig) { Config = InConfig; }

  private:
    FViewportCameraTransform Transform;
    FCameraControllerConfig  Config;
    FVector3                 Velocity = {};
};
