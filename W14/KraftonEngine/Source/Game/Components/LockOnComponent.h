#pragma once
#include "Core/Delegate.h"
#include "Engine/Component/ActorComponent.h"
#include "Engine/Math/Vector.h"
#include "Object/Ptr/WeakObjectPtr.h"

#include "Source/Game/Components/LockOnComponent.generated.h"

class UBillboardComponent;
class UHealthComponent;
class USpringArmComponent;
class AEnemyCharacter;

// Note: Mouse-look will fight the lock. If ACharacter::Tick adds yaw/pitch input after this component ticks, the two writes to ControlRotation compete

UENUM()
enum class EMarkerScaleType
{
	Fixed,
	Relative,
};

UCLASS()
class ULockOnComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	void BeginPlay() override;
	void EndPlay() override;

	// 입력/Lua 에서 바인딩하는 토글·해제 동사.
	UFUNCTION(Callable, Category="LockOn")
	void ToggleLockOn();
	UFUNCTION(Callable, Category="LockOn")
	void ClearLockOn();
	UFUNCTION(Callable, Category="LockOn")
	bool AddSwitchInput(float MouseDeltaX);
	UFUNCTION(Callable, Category="LockOn")
	bool SwitchLockTarget(int Direction);

	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;


private:
	AActor* FindBestTarget() const;
	AEnemyCharacter* FindSwitchTarget(int Direction) const;
	FVector GetLockOnPoint(AActor* Target) const;
	bool IsValidLockTarget(AActor* Target) const;
	void SetLockedTarget(AEnemyCharacter* Target);
	void UpdateCameraLock(float DeltaTime);
	void UpdateMarker();
	void RestoreCamera(float DeltaTime);
	void BindOwnerHealthEvents();
	void UnbindOwnerHealthEvents();
	void HandleOwnerDeath(UHealthComponent* Component, AActor* DamageCauser, AActor* InstigatorActor);


public:
	// 락온 마커로 쓸 머티리얼(.uasset) — LockOnMarker.png 를 참조하는 머티리얼을 에디터에서 작성해 지정.
	UPROPERTY(Edit, Save, Category="LockOn", DisplayName="Marker Material", AssetType="Material")
	FString MarkerMaterialPath = "Content/Game/Material/LockOnMarker.uasset";
	UPROPERTY(Edit, Save, Category="LockOn", DisplayName="Marker Scale", Min=0.0f, Max=20.0f, Speed=0.1f)
	float MarkerScale = 2.0f;
	UPROPERTY(Edit, Save, Category="LockOn", DisplayName="Marker Scale Type")
	EMarkerScaleType ScaleType = EMarkerScaleType::Fixed;

	// 후보 탐색 — 이 거리/시야각 안의 적만 락온 대상이 된다.
	UPROPERTY(Edit, Save, Category="LockOn", DisplayName="Max Lock Distance", Min=0.0f, Max=200.0f, Speed=0.1f)
	float MaxLockDistance = 28.0f;
	UPROPERTY(Edit, Save, Category="LockOn", DisplayName="Acquire Angle (Deg)", Min=0.0f, Max=180.0f, Speed=0.5f)
	float AcquireAngleDegrees = 55.0f;
	// 락온 유지 — 이 거리를 넘거나 시야를 이 시간 이상 잃으면 자동 해제.
	UPROPERTY(Edit, Save, Category="LockOn", DisplayName="Break Distance", Min=0.0f, Max=200.0f, Speed=0.1f)
	float BreakDistance = 34.0f;
	UPROPERTY(Edit, Save, Category="LockOn|Switch", DisplayName="Switch Mouse Threshold", Min=0.0f, Max=500.0f, Speed=1.0f)
	float SwitchMouseThreshold = 80.0f;
	UPROPERTY(Edit, Save, Category="LockOn|Switch", DisplayName="Switch Cooldown Seconds", Min=0.0f, Max=2.0f, Speed=0.01f)
	float SwitchCooldownSeconds = 0.25f;
	UPROPERTY(Edit, Save, Category="LockOn|Switch", DisplayName="Switch Side Dot Min", Min=0.0f, Max=1.0f, Speed=0.01f)
	float SwitchSideDotMin = 0.15f;
	UPROPERTY(Edit, Save, Category="LockOn|Switch", DisplayName="Switch Forward Dot Min", Min=-1.0f, Max=1.0f, Speed=0.01f)
	float SwitchForwardDotMin = -0.2f;

	// Not worth it given the time constraint
	//UPROPERTY(Edit, Save, Category="LockOn", DisplayName="Lost Sight Grace (s)", Min=0.0f, Max=5.0f, Speed=0.05f)
	//float LostSightGraceSeconds = 0.6f;

	UPROPERTY(Edit, Save, Category="LockOn|Camera", DisplayName="Normal Arm Length", Min=0.0f, Max=100.0f, Speed=0.1f)
	float NormalArmLength = 10.0f;
	UPROPERTY(Edit, Save, Category="LockOn|Camera", DisplayName="Locked Min Arm Length", Min=0.0f, Max=100.0f, Speed=0.1f)
	float LockedMinArmLength = 8.0f;
	UPROPERTY(Edit, Save, Category="LockOn|Camera", DisplayName="Locked Max Arm Length", Min=0.0f, Max=100.0f, Speed=0.1f)
	float LockedMaxArmLength = 14.0f;

	UPROPERTY(Edit, Save, Category="LockOn|Camera", DisplayName="Normal Target Offset", Type=Vec3, Min=0.0f, Max=0.0f, Speed=0.1f)
	FVector NormalTargetOffset = FVector(0, 0, 0);
	UPROPERTY(Edit, Save, Category="LockOn|Camera", DisplayName="Locked Target Offset", Type=Vec3, Min=0.0f, Max=0.0f, Speed=0.1f)
	FVector LockedTargetOffset = FVector(0, 0, 0);

	UPROPERTY(Edit, Save, Category="LockOn|Camera", DisplayName="Camera Rotation Interp Speed", Min=0.0f, Max=100.0f, Speed=0.1f)
	float CameraRotationInterpSpeed = 12.0f;
	UPROPERTY(Edit, Save, Category="LockOn|Camera", DisplayName="Arm Interp Speed", Min=0.0f, Max=100.0f, Speed=0.1f)
	float ArmInterpSpeed = 8.0f;

private:
	// 런타임 상태 — 직렬화/에디터 노출 제외.
	TWeakObjectPtr<USpringArmComponent> SpringArmComponent;
	TWeakObjectPtr<UBillboardComponent> MarkerComponent;
	TWeakObjectPtr<AActor> LockedTarget;
	TWeakObjectPtr<UHealthComponent> BoundHealthComponent;
	FDelegateHandle OwnerDeathHandle;
	float SwitchInputX = 0.0f;
	float SwitchCooldownRemaining = 0.0f;
};
