#include "PreviewViewportTool.h"

#include "Viewport/PreviewViewportClient.h"
#include "Viewport/SkeletalMeshViewer.h"
#include "Preview/PreviewSceneContext.h"
#include "Input/SkelViewerViewportInputController.h"
#include "Component/CameraComponent.h"
#include "Component/GizmoComponent.h"
#include "Viewport/Viewport.h"
#include <Profiling/PlatformTime.h>
#include <Collision/RayUtils.h>

bool FPreviewViewportTool::BuildMouseRay(FRay& OutRay) const
{
    if (!Owner || !Controller || !Owner->GetCamera())
    {
        return false;
    }

    FRect ViewportRect;
    if (!Owner->GetViewportRect(ViewportRect))
    {
        return false;
    }

    FViewport* Viewport = Owner->GetViewport();
    const float VPWidth = Viewport ? static_cast<float>(Viewport->GetWidth()) : ViewportRect.Width;
    const float VPHeight = Viewport ? static_cast<float>(Viewport->GetHeight()) : ViewportRect.Height;
    if (VPWidth <= 0.0f || VPHeight <= 0.0f)
    {
        return false;
    }

    const FViewportFrameInput& Input = Controller->GetCurrentInput();
    const float LocalMouseX = static_cast<float>(Input.MouseLocalPos.x);
    const float LocalMouseY = static_cast<float>(Input.MouseLocalPos.y);

    OutRay = Owner->GetCamera()->DeprojectScreenToWorld(LocalMouseX, LocalMouseY, VPWidth, VPHeight);
    return true;
}


bool FSkeletalMeshGizmoTool::HandleInput(float DeltaTime)
{
    if (!Owner || !Controller || !Owner->GetCamera() || !Owner->GetGizmo() || !Owner->GetWorld())
    {
        return false;
    }

    UCameraComponent* Camera = Owner->GetCamera();
    UGizmoComponent* Gizmo = Owner->GetGizmo();

    if (Owner->GetWorld()->GetWorldType() == EWorldType::PIE)
    {
        if (Gizmo->IsHolding())
        {
            Gizmo->DragEnd();
        }
        else if (Gizmo->IsPressedOnHandle())
        {
            Gizmo->SetPressedOnHandle(false);
        }

        return false;
    }

    Gizmo->ApplyScreenSpaceScaling(Camera->GetWorldLocation(), Camera->IsOrthogonal(), Camera->GetOrthoWidth());

    Gizmo->SetAxisMask(UGizmoComponent::ComputeAxisMask(ELevelViewportType::Perspective, Gizmo->GetMode()));

    FRay Ray;
    if (!BuildMouseRay(Ray))
    {
        return Gizmo->IsHolding() || Gizmo->IsPressedOnHandle();
    }

    const FViewportFrameInput& Input = Controller->GetCurrentInput();
    const bool bMouseMoved = Input.MouseAxisDelta.x != 0 || Input.MouseAxisDelta.y != 0;
    const bool bLeftDragging = Input.bLeftDown && bMouseMoved;

    if (!Gizmo->IsHolding())
    {
        FHitResult HoverHit{};
        if (!FRayUtils::RaycastComponent(Gizmo, Ray, HoverHit))
        {
            Gizmo->UpdateHoveredAxis(-1);
        }
    }

    if (Input.bLeftReleased)
    {
        if (Gizmo->IsHolding() && bMouseMoved)
        {
            Gizmo->UpdateDrag(Ray);
        }

        return HandleDragEnd();
    }

    if (Input.bLeftPressed)
    {
        return HandleDragStart(Ray);
    }

    if (bLeftDragging)
    {
        return HandleDragUpdate(Ray);
    }

    return Gizmo->IsHolding() || Gizmo->IsPressedOnHandle();
}

void FSkeletalMeshGizmoTool::ResetState()
{
    UGizmoComponent* Gizmo = Owner ? Owner->GetGizmo() : nullptr;
    if (!Gizmo)
    {
        return;
    }

    if (Gizmo->IsHolding())
    {
        Gizmo->DragEnd();
    }
    else if (Gizmo->IsPressedOnHandle())
    {
        Gizmo->SetPressedOnHandle(false);
    }
}

bool FSkeletalMeshGizmoTool::HandleDragStart(const FRay& Ray)
{
    UGizmoComponent* Gizmo = Owner ? Owner->GetGizmo() : nullptr;
    if (!Gizmo)
    {
        return false;
    }

    FHitResult HitResult{};

    if (FRayUtils::RaycastComponent(Gizmo, Ray, HitResult))
    {
        Gizmo->SetPressedOnHandle(true);
        return true;
    }

    Gizmo->SetPressedOnHandle(false);
    return false;
}

bool FSkeletalMeshGizmoTool::HandleDragUpdate(const FRay& Ray)
{
    UGizmoComponent* Gizmo = Owner ? Owner->GetGizmo() : nullptr;
    if (!Gizmo)
    {
        return false;
    }

    if (Gizmo->IsPressedOnHandle() && !Gizmo->IsHolding())
    {
        Gizmo->SetHolding(true);
    }

    if (Gizmo->IsHolding())
    {
        Gizmo->UpdateDrag(Ray);
        return true;
    }

    return false;
}

bool FSkeletalMeshGizmoTool::HandleDragEnd()
{
    UGizmoComponent* Gizmo = Owner ? Owner->GetGizmo() : nullptr;
    if (!Gizmo)
    {
        return false;
    }

    if (Gizmo->IsHolding())
    {
        Gizmo->DragEnd();
        return true;
    }

    if (Gizmo->IsPressedOnHandle())
    {
        Gizmo->SetPressedOnHandle(false);
        return true;
    }

    return false;
}

bool FSkeletalMeshViewerBoneSelectTool::HandleInput(float DeltaTime)
{
    (void)DeltaTime;

    if (!Owner || !Controller || !Owner->GetCamera() || !Owner->GetWorld())
    {
        return false;
    }

    const FViewportFrameInput& Input = Controller->GetCurrentInput();
    if (!Input.bLeftPressed)
    {
        return false;
    }

    if (Owner->GetWorld()->GetWorldType() != EWorldType::Preview)
    {
        return false;
    }

    FRay Ray;
    if (!BuildMouseRay(Ray))
    {
        return false;
    }

    return HandleBonePicking(Ray);
}

bool FSkeletalMeshViewerBoneSelectTool::HandleBonePicking(const FRay& Ray) 
{
    if (!Owner || !Controller || !Viewer)
    {
        return false;
    }

    int32 BestBoneIndex = INDEX_NONE;

    FScopeCycleCounter PickCounter;

    const bool bHit = Viewer->RaycastBonePicking(Ray, BestBoneIndex);

    if (!bHit || BestBoneIndex == INDEX_NONE)
    {
        Viewer->ClearBoneSelection();
    }
    else
    {
        Viewer->SelectBone(BestBoneIndex);
    }

    return true;
}

bool FSkeletalMeshNavigationTool::HandleInput(float DeltaTime)
{
    if (!Owner || !Controller || !Owner->GetCamera())
    {
        return false;
    }

    bool bHandled = false;
    bHandled |= HandleKeyboardAndMouseNavigation(DeltaTime);
    bHandled |= HandleWheelZoom(DeltaTime);
    return bHandled;
}

bool FSkeletalMeshNavigationTool::HandleKeyboardAndMouseNavigation(float DeltaTime)
{
    UCameraComponent* Camera = Owner->GetCamera();
    if (!Camera)
    {
        return false;
    }

    const FViewportFrameInput& Input = Controller->GetCurrentInput();
    const FCameraState& CameraState = Camera->GetCameraState();
    const bool bIsOrtho = CameraState.bIsOrthogonal;

    const float MoveSensitivity = CameraMoveSensitivity;
    const float CameraSpeed = CameraMoveSpeed * MoveSensitivity;
    const float PanMouseScale = CameraSpeed * 0.01f;

    bool bHandled = false;

    if (!bIsOrtho)
    {
        FVector LocalMove = FVector(0, 0, 0);
        float WorldVerticalMove = 0.0f;

        if (Input.KeyDown['W'])
            LocalMove.X += CameraSpeed;
        if (Input.KeyDown['A'])
            LocalMove.Y -= CameraSpeed;
        if (Input.KeyDown['S'])
            LocalMove.X -= CameraSpeed;
        if (Input.KeyDown['D'])
            LocalMove.Y += CameraSpeed;
        if (Input.KeyDown['Q'])
            WorldVerticalMove -= CameraSpeed;
        if (Input.KeyDown['E'])
            WorldVerticalMove += CameraSpeed;

        if (LocalMove.X != 0.0f || LocalMove.Y != 0.0f || LocalMove.Z != 0.0f)
        {
            LocalMove *= DeltaTime;
            Camera->MoveLocal(LocalMove);
            bHandled = true;
        }

        if (WorldVerticalMove != 0.0f)
        {
            Camera->AddWorldOffset(FVector(0.0f, 0.0f, WorldVerticalMove * DeltaTime));
            bHandled = true;
        }

        if (Input.bMiddleDown)
        {
            const float DeltaX = static_cast<float>(Input.MouseAxisDelta.x);
            const float DeltaY = static_cast<float>(Input.MouseAxisDelta.y);
            if (DeltaX != 0.0f || DeltaY != 0.0f)
            {
                Camera->MoveLocal(FVector(0.0f, -DeltaX * PanMouseScale * 0.05f, DeltaY * PanMouseScale * 0.05f));
                bHandled = true;
            }
        }

        FVector Rotation = FVector(0, 0, 0);

        const float RotateSensitivity = CameraRotateSensitivity;
        const float AngleVelocity = CameraRotationSpeed * RotateSensitivity;
        if (Input.KeyDown[VK_UP])
            Rotation.Z -= AngleVelocity;
        if (Input.KeyDown[VK_LEFT])
            Rotation.Y -= AngleVelocity;
        if (Input.KeyDown[VK_DOWN])
            Rotation.Z += AngleVelocity;
        if (Input.KeyDown[VK_RIGHT])
            Rotation.Y += AngleVelocity;

        FVector MouseRotation = FVector(0, 0, 0);
        const float MouseRotationSpeed = 0.15f * RotateSensitivity;

        if (Input.bRightDown)
        {
            const float DeltaX = static_cast<float>(Input.MouseAxisDelta.x);
            const float DeltaY = static_cast<float>(Input.MouseAxisDelta.y);

            MouseRotation.Y += DeltaX * MouseRotationSpeed;
            MouseRotation.Z += DeltaY * MouseRotationSpeed;
        }

        Rotation *= DeltaTime;
        if (Rotation.Y != 0.0f || Rotation.Z != 0.0f || MouseRotation.Y != 0.0f || MouseRotation.Z != 0.0f)
        {
            Camera->Rotate(Rotation.Y + MouseRotation.Y, Rotation.Z + MouseRotation.Z);
            bHandled = true;
        }
    }
    else
    {
        if (Input.bRightDown)
        {
            const float DeltaX = static_cast<float>(Input.MouseAxisDelta.x);
            const float DeltaY = static_cast<float>(Input.MouseAxisDelta.y);
            if (DeltaX != 0.0f || DeltaY != 0.0f)
            {
                const float PanScale = CameraState.OrthoWidth * 0.002f * MoveSensitivity;
                Camera->MoveLocal(FVector(0, -DeltaX * PanScale, DeltaY * PanScale));
                bHandled = true;
            }
        }
    }

    return bHandled;
}

bool FSkeletalMeshNavigationTool::HandleWheelZoom(float DeltaTime)
{
    UCameraComponent* Camera = Owner ? Owner->GetCamera() : nullptr;
    if (!Camera)
    {
        return false;
    }

    const FViewportFrameInput& Input = Controller->GetCurrentInput();
    const float ScrollNotches = Input.MouseWheelNotches;
    if (ScrollNotches == 0.0f)
    {
        return false;
    }

    const float ZoomSpeed = CameraZoomSpeed;

    if (Camera->IsOrthogonal())
    {
        const float NewWidth = Camera->GetOrthoWidth() - ScrollNotches * ZoomSpeed * 0.015f;
        Camera->SetOrthoWidth(Clamp(NewWidth, 0.1f, 1000.0f));
    }
    else
    {
        Camera->MoveLocal(FVector(ScrollNotches * ZoomSpeed * 0.015f, 0.0f, 0.0f));
    }

    return true;
}

bool FSkeletalMeshCommandTool::HandleInput(float DeltaTime)
{
    if (!Owner || !Controller)
    {
        return false;
    }

    const FViewportFrameInput& Input = Controller->GetCurrentInput();

    if (Input.KeyReleased[VK_SPACE])
    {
        if (UGizmoComponent* Gizmo = Owner->GetGizmo())
        {
            Gizmo->SetNextMode();
            return true;
        }
    }
    return false;
}
