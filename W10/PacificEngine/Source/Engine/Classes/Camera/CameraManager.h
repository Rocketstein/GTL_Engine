#pragma once
#include "Core/CoreTypes.h"
#include "Object/ObjectFactory.h"
#include "GameFramework/AActor.h"
#include "Math/Color.h"
#include "Math/MathUtils.h"
#include "Math/Rotator.h"
#include "Math/Vector.h"
#include "Render/PostProcess/PostProcessController.h"
#include <memory>


class UCameraModifier;
class UCameraShakeBase;
class UCameraModifier_CameraShake;
class UClass;
struct FAddCameraShakeParams;


enum EViewTargetBlendFunction
{
    Linear,
    Cubic,
    EaseIn,
    EaseOut,
    EaseInOut
};

struct FMinimalViewInfo
{
    FVector Location = FVector(0, 0, 0);
    FRotator Rotation = FRotator();
    float FOV = 90.0f;
    float AspectRatio = 16.0f / 9.0f;
    float NearZ = 0.1f;
    float FarZ = 1000.0f;

    bool bConstrainAspectRatio = false;
    bool bOrthographic = false;
    float OrthoWidth = 10.0f;

	FMinimalViewInfo() = default;
    void BlendViewInfo(const FMinimalViewInfo& OtherInfo, float OtherWeight)
    {
        Location = FMath::Lerp(Location, OtherInfo.Location, OtherWeight);

        const FRotator DeltaAng = FRotator::GetBlendDelta(Rotation, OtherInfo.Rotation);
        Rotation = Rotation + DeltaAng * OtherWeight;

        FOV = FMath::Lerp(FOV, OtherInfo.FOV, OtherWeight);

        AspectRatio = FMath::Lerp(AspectRatio, OtherInfo.AspectRatio, OtherWeight);
        bConstrainAspectRatio |= OtherInfo.bConstrainAspectRatio;
    }
};

struct FViewTargetTransitionParams
{
    float BlendTime = 0.0f;
    EViewTargetBlendFunction BlendFunction = EViewTargetBlendFunction::Cubic;
    float BlendExp = 2.0f;
    bool bLockOutgoing = false;

    float GetBlendAlpha(float TimePct) const;
};

struct FCameraCacheEntry
{
    bool bValid = false;
    float TimeStamp = 0.0f;
    FMinimalViewInfo POV;
};

struct FViewTarget
{
    AActor* Target = nullptr;
    FMinimalViewInfo POV;

    bool IsValid() const { return Target != nullptr; }

    void CheckViewTarget(AActor* Actor)
    {
        if (Target)
        {
            return;
        }

        Target = Actor ? Actor : nullptr;
    }

    void CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult)
    {
        (void)DeltaTime;

        if (!Target)
        {
            return;
        }

        Target->CalcCamera(DeltaTime, OutResult);
    }
};

class APlayerCameraManager : public AActor
{
public:
    DECLARE_CLASS(APlayerCameraManager, AActor)

public:
    APlayerCameraManager();
    ~APlayerCameraManager() override;

    void InitDefaultComponents() override;
    void BindScriptFunctions(UScriptComponent& ScriptComponent) override;

    void Serialize(FArchive& Ar) override;
    void InitializeFor(AActor* InOwner);
    void UpdateCamera(float DeltaTime);

    void SetViewTarget(AActor* NewTarget, const FViewTargetTransitionParams& Params = {});
    AActor* GetViewTarget() const;

    const FMinimalViewInfo& GetCameraCacheView() const;
    const FMinimalViewInfo& GetLastFrameCameraCacheView() const;
    bool HasValidCameraCache() const;

    void GetCameraViewPoint(FVector& OutLocation, FRotator& OutRotation) const;
    FVector GetCameraLocation() const;
    FRotator GetCameraRotation() const;

    FPostProcessController& GetPostProcessController() { return PostProcessController; }
    const FPostProcessController& GetPostProcessController() const { return PostProcessController; }
    FPostProcessSettings GetPostProcessSettings() const { return PostProcessController.GetSettings(); }

    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
    void PostEditProperty(const char* PropertyName) override;

    void SetGameCameraCutThisFrame() { bGameCameraCutThisFrame = true; }
    bool IsGameCameraCutThisFrame() const { return bGameCameraCutThisFrame; }
    void ClearGameCameraCutThisFrame() { bGameCameraCutThisFrame = false; }

    uint32 AddCameraModifier(UClass* ModifierUClass);
    uint32 AddCameraModifier(const char* ModifierName);

    uint32 AddCameraModifierToList(UCameraModifier* NewModifier);

	uint32 GetModifierHandle(const char* ModifierUclassName) const;
	bool DisableCameraModifier(uint32 ModifierHandle, bool bImmediate = false);
    bool RemoveCameraModifier(uint32 ModifierHandle);
    UCameraShakeBase* StartCameraShakeFromAsset(const FString& Path, const FAddCameraShakeParams& Params);
    void ApplyCameraModifiersToPOV(float DeltaTime, FMinimalViewInfo& InOutPOV);

    virtual void Tick(float DeltaTime) override;

private:
    void DoUpdateCamera(float DeltaTime);
    void UpdateViewTarget(FViewTarget& OutVT, float DeltaTime);
    void ApplyCameraModifiers(float DeltaTime, FMinimalViewInfo& InOutPOV);
    UCameraModifier_CameraShake* GetOrCreateCameraShakeModifier();

    void SetCameraCachePOV(const FMinimalViewInfo& InPOV);
    void SetLastFrameCameraCachePOV(const FMinimalViewInfo& InPOV);
    void FillCameraCache(const FMinimalViewInfo& NewInfo);

    void AssignViewTarget(AActor* NewTarget, FViewTarget& OutViewTarget, const FViewTargetTransitionParams& Params);

private:
    AActor* PCOwner = nullptr;

    // ViewTarget 상태
    FViewTarget ViewTarget;
    FViewTarget PendingViewTarget;

    float BlendTimeToGo = 0.0f;
    FViewTargetTransitionParams BlendParams;

    // 기본 카메라 값
    float DefaultAspectRatio = 16.0f / 9.0f;
    bool bDefaultConstrainAspectRatio = false;
    float DefaultFOV = 90.0f;

    // 카메라 스타일
    FName CameraStyle;

    // 카메라 컷 플래그
    bool bGameCameraCutThisFrame = false;

    // 카메라 캐시
    FCameraCacheEntry CameraCachePrivate;
    FCameraCacheEntry LastFrameCameraCachePrivate;

    FPostProcessController PostProcessController;

    // 모디파이어
    TArray<UCameraModifier*> Modifiers;

	uint32 NextModifierHandle = 1;
    TMap<uint32, UCameraModifier*> ModifierHandleMap; // Lua handle용 추가

    static constexpr const char* CameraManagerScriptPath = "CameraManagerComponent.lua";
};
