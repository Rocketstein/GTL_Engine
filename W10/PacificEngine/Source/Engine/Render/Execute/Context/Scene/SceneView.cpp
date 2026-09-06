// 렌더 영역의 세부 동작을 구현합니다.
#include "Render/Execute/Context/Scene/SceneView.h"
#include "Classes/Camera/CameraManager.h"
#include "Component/CameraComponent.h"
#include "Viewport/Viewport.h"

void FSceneView::SetCameraInfo(const UCameraComponent* Camera)
{
    View           = Camera->GetViewMatrix();
    Proj           = Camera->GetProjectionMatrix();
    CameraPosition = Camera->GetWorldLocation();
    CameraForward  = Camera->GetForwardVector();
    CameraRight    = Camera->GetRightVector();
    CameraUp       = Camera->GetUpVector();
    bIsOrtho       = Camera->IsOrthogonal();
    OrthoWidth     = Camera->GetOrthoWidth();
    NearClip       = Camera->GetCameraState().NearZ;
    FarClip        = Camera->GetCameraState().FarZ;
    FOV            = Camera->GetFOV();
    FrustumVolume.UpdateFromMatrix(View * Proj);
}

void FSceneView::SetCameraInfo(const FMinimalViewInfo& ViewInfo)
{
    const FVector Forward = ViewInfo.Rotation.GetForwardVector();
    const FVector Right = ViewInfo.Rotation.GetRightVector();
    const FVector Up = ViewInfo.Rotation.GetUpVector();
    const FVector Eye = ViewInfo.Location;

    View = FMatrix(
        Right.X, Up.X, Forward.X, 0,
        Right.Y, Up.Y, Forward.Y, 0,
        Right.Z, Up.Z, Forward.Z, 0,
        -Eye.Dot(Right), -Eye.Dot(Up), -Eye.Dot(Forward), 1);

    const float AspectRatio = ViewInfo.AspectRatio > 0.0f ? ViewInfo.AspectRatio : 1.0f;
    if (!ViewInfo.bOrthographic)
    {
        Proj = FMatrix::MakePerspective(ViewInfo.FOV, AspectRatio, ViewInfo.NearZ, ViewInfo.FarZ);
    }
    else
    {
        const float Width = ViewInfo.OrthoWidth;
        const float Height = Width / AspectRatio;
        Proj = FMatrix::MakeOrthographic(Width, Height, ViewInfo.NearZ, ViewInfo.FarZ);
    }

    CameraPosition = Eye;
    CameraForward = Forward;
    CameraRight = Right;
    CameraUp = Up;
    bIsOrtho = ViewInfo.bOrthographic;
    OrthoWidth = ViewInfo.OrthoWidth;
    NearClip = ViewInfo.NearZ;
    FarClip = ViewInfo.FarZ;
    FOV = ViewInfo.FOV;
    FrustumVolume.UpdateFromMatrix(View * Proj);
}

void FSceneView::SetViewportInfo(const FViewport* VP)
{
    if (!VP)
    {
        ViewportWidth  = 0.0f;
        ViewportHeight = 0.0f;
        return;
    }

    ViewportWidth  = static_cast<float>(VP->GetWidth());
    ViewportHeight = static_cast<float>(VP->GetHeight());
}
