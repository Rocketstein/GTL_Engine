#pragma once
#include "Object/Object.h"
#include "Object/ObjectFactory.h"

#include "Engine/Math/Vector.h"
#include "Engine/Math/Rotator.h"

class AActor;
class APlayerCameraManager;
class UWorld;
struct FMinimalViewInfo;

class UCameraModifier : public UObject
{
public:
    DECLARE_CLASS(UCameraModifier, UObject)

protected:
    /** If true, do not apply this modifier to the camera. */
    bool bDisabled = false;

    /** If true, this modifier will disable itself when finished interpolating out. */
    bool bPendingDisable = false;

public:
    /** Priority value that determines the order in which modifiers are applied. 0 = highest priority, 255 = lowest. */
    uint8 Priority;

protected:
    /** Camera this object is associated with. */
    APlayerCameraManager* CameraOwner;

    /** When blending in, alpha proceeds from 0 to 1 over this time */
    float AlphaInTime;

    /** When blending out, alpha proceeds from 1 to 0 over this time */
    float AlphaOutTime;

    /** Current blend alpha. */
    float Alpha;

    void OnCameraOwnerDestroyed(AActor* InOwner);

protected:
    /** @return Returns the ideal blend alpha for this modifier. Interpolation will seek this value. */
    virtual float GetTargetAlpha();

public:
    UCameraModifier()
        : UObject(), bDisabled(false), bPendingDisable(false), Priority(127), CameraOwner(nullptr), AlphaInTime(0.0f), AlphaOutTime(0.0f), Alpha(1.0f)
    {
    }
    /**
     * Allows camera modifiers to output debug text during "showdebug camera"
     * @param Canvas - The canvas to write debug info on.
     * @param DebugDisplay - Meta data about the current debug display.
     * @param YL - Vertical spacing.
     * @param YPos - Current vertical space to start writing.
     */
    // virtual void DisplayDebug(class UCanvas* Canvas, const FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos);

    /**
     * Allows any custom initialization. Called immediately after creation.
     * @param Camera - The camera this modifier should be associated with.
     */
    virtual void AddedToCamera(APlayerCameraManager* Camera);

    /**
     * Directly modifies variables in the owning camera
     * @param	DeltaTime	Change in time since last update
     * @param	InOutPOV	Current Point of View, to be updated.
     * @return	bool		True if should STOP looping the chain, false otherwise
     */
    virtual bool ModifyCamera(float DeltaTime, struct FMinimalViewInfo& InOutPOV);

    /** @return Returns true if modifier is disabled, false otherwise. */
    virtual bool IsDisabled() const;

    /** @return Returns true if modifier is pending disable, false otherwise. */
    virtual bool IsPendingDisable() const;

    /** @return Returns the actor the camera is currently viewing. */
    virtual AActor* GetViewTarget() const;

    /**
     *  Disables this modifier.
     *  @param  bImmediate  - true to disable with no blend out, false (default) to allow blend out
     */
    virtual void DisableModifier(bool bImmediate = false);

    /** Enables this modifier. */
    virtual void EnableModifier();

    /** Toggled disabled/enabled state of this modifier. */
    virtual void ToggleModifier();

    /**
     * Called to give modifiers a chance to adjust view rotation updates before they are applied.
     *
     * Default just returns ViewRotation unchanged
     * @param ViewTarget - Current view target.
     * @param DeltaTime - Frame time in seconds.
     * @param OutViewRotation - In/out. The view rotation to modify.
     * @param OutDeltaRot - In/out. How much the rotation changed this frame.
     * @return Return true to prevent subsequent (lower priority) modifiers to further adjust rotation, false otherwise.
     */
    virtual bool ProcessViewRotation(class AActor* ViewTarget, float DeltaTime, FRotator& OutViewRotation, FRotator& OutDeltaRot);
    virtual void UpdateAlpha(float DeltaTime);

    UWorld* GetWorld() const;

protected:
    virtual void ModifyCamera(float DeltaTime, FVector ViewLocation, FRotator ViewRotation, float FOV, FVector& NewViewLocation, FRotator& NewViewRotation, float& NewFOV);
};
