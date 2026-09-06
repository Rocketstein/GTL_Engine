#include "CameraModifier.h"
#include "CameraManager.h"

#include <algorithm>


IMPLEMENT_CLASS(UCameraModifier, UObject)

bool UCameraModifier::ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV)
{
    // Update the alpha
    UpdateAlpha(DeltaTime);

    FVector NewViewLocation = InOutPOV.Location;
    FRotator NewViewRotation = InOutPOV.Rotation;
    float NewFOV = InOutPOV.FOV;

    ModifyCamera(
        DeltaTime,
        InOutPOV.Location,
        InOutPOV.Rotation,
        InOutPOV.FOV,
        NewViewLocation,
        NewViewRotation,
        NewFOV);

    InOutPOV.Location = FMath::Lerp(InOutPOV.Location, NewViewLocation, Alpha);

    const FRotator DeltaRot = FRotator::GetBlendDelta(InOutPOV.Rotation, NewViewRotation);
    InOutPOV.Rotation = InOutPOV.Rotation + DeltaRot * Alpha;

    InOutPOV.FOV = FMath::Lerp(InOutPOV.FOV, NewFOV, Alpha);

    // If pending disable and fully alpha'd out, truly disable this modifier
    if (bPendingDisable && (Alpha <= 0.f))
    {
        DisableModifier(true);
    }

    // allow subsequent modifiers to update
    return false;
}

float UCameraModifier::GetTargetAlpha()
{
    return bPendingDisable ? 0.0f : 1.f;
}

void UCameraModifier::UpdateAlpha(float DeltaTime)
{
    float const TargetAlpha = GetTargetAlpha();
    float const BlendTime = (TargetAlpha == 0.f) ? AlphaOutTime : AlphaInTime;

    // interpolate!
    if (BlendTime <= 0.f)
    {
        // no blendtime means no blending, just go directly to target alpha
        Alpha = TargetAlpha;
    }
    else if (Alpha > TargetAlpha)
    {
        // interpolate downward to target, while protecting against overshooting
        Alpha = std::max<float>(Alpha - DeltaTime / BlendTime, TargetAlpha);
    }
    else
    {
        // interpolate upward to target, while protecting against overshooting
        Alpha = std::min<float>(Alpha + DeltaTime / BlendTime, TargetAlpha);
    }
}

bool UCameraModifier::IsDisabled() const
{
    return bDisabled;
}

bool UCameraModifier::IsPendingDisable() const
{
    return bPendingDisable;
}

AActor* UCameraModifier::GetViewTarget() const
{
    return CameraOwner ? CameraOwner->GetViewTarget() : nullptr;
}

void UCameraModifier::AddedToCamera(APlayerCameraManager* Camera)
{
    if (CameraOwner)
    {
        CameraOwner->OnDestroyed.RemoveDynamic(this);
    }
    CameraOwner = Camera;
    if (CameraOwner)
    {
        CameraOwner->OnDestroyed.AddDynamic(this, &UCameraModifier::OnCameraOwnerDestroyed);
    }
}

void UCameraModifier::OnCameraOwnerDestroyed(AActor* InOwner)
{
    if (InOwner == CameraOwner)
    {
        CameraOwner = nullptr;
    }
}

UWorld* UCameraModifier::GetWorld() const
{
    return CameraOwner ? CameraOwner->GetWorld() : nullptr;
}

void UCameraModifier::DisableModifier(bool bImmediate)
{
    if (bImmediate)
    {
        bDisabled = true;
        bPendingDisable = false;
    }
    else if (!bDisabled)
    {
        bPendingDisable = true;
    }
}

void UCameraModifier::EnableModifier()
{
    bDisabled = false;
    bPendingDisable = false;
}

void UCameraModifier::ToggleModifier()
{
    if (bDisabled)
    {
        EnableModifier();
    }
    else
    {
        DisableModifier();
    }
}

bool UCameraModifier::ProcessViewRotation(AActor* ViewTarget, float DeltaTime, FRotator& OutViewRotation, FRotator& OutDeltaRot)
{
    return false;
}

void UCameraModifier::ModifyCamera(float DeltaTime, FVector ViewLocation, FRotator ViewRotation, float FOV, FVector& NewViewLocation, FRotator& NewViewRotation, float& NewFOV)
{
}

//void UCameraModifier::ModifyPostProcess(float DeltaTime, float& PostProcessBlendWeight, FPostProcessSettings& PostProcessSettings)
//{
//}
//
//void UCameraModifier::DisplayDebug(class UCanvas* Canvas, const FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos)
//{
//}
