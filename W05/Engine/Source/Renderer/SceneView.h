#pragma once

#include "Core/Math/Matrix.h"
#include "Core/Math/Vector3.h"
#include "Core/Math/Vector4.h"

class UGizmo;

class FSceneView
{
  public:
    const FMatrix &GetViewMatrix() const { return ViewMatrix; }
    void           SetViewMatrix(const FMatrix &InViewMatrix) { ViewMatrix = InViewMatrix; }

    const FMatrix &GetProjectionMatrix() const { return ProjectionMatrix; }
    void           SetProjectionMatrix(const FMatrix &InProjectionMatrix)
    {
        ProjectionMatrix = InProjectionMatrix;
    }

    const FMatrix &GetViewProjectionMatrix() const { return ViewProjectionMatrix; }
    void UpdateViewProjectionMatrix() { ViewProjectionMatrix = ViewMatrix * ProjectionMatrix; }

    const FVector3 &GetEyePosition() const { return EyePosition; }
    void            SetEyePosition(const FVector3 &InEyePosition) { EyePosition = InEyePosition; }

    float GetCameraWidth() const { return ViewWidth; }
    void  SetCameraWidth(float InViewWidth) { ViewWidth = InViewWidth; }

    float GetCameraHeight() const { return ViewHeight; }
    void  SetCameraHeight(float InViewHeight) { ViewHeight = InViewHeight; }

    void SetCameraSize(float InViewWidth, float InViewHeight)
    {
        ViewWidth = InViewWidth;
        ViewHeight = InViewHeight;
    }

    void SetGizmo(UGizmo* InGizmo) { Gizmo = InGizmo; }
    void SetShowGizmo(bool bInShowGizmo) { bShowGizmo = bInShowGizmo; }
    bool IsShowGizmo() const { return bShowGizmo; }
    UGizmo* GetGizmo() const
    {
        if (Gizmo)
            return Gizmo;
        return nullptr;
    }

  private:
    FMatrix  ViewMatrix;
    FMatrix  ProjectionMatrix;
    FMatrix  ViewProjectionMatrix;                     // Caching
    FVector3 EyePosition = FVector3(0.0f, 0.0f, 0.0f); // Caching
    UGizmo*  Gizmo = nullptr;
    float    ViewWidth = 1.0f;
    float    ViewHeight = 1.0f;
    bool     bShowGizmo = true;
    
};
