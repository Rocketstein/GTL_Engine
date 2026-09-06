#pragma once
#include "MovementComponent.h"

#include "Source/Engine/Component/Movement/PawnMovementComponent.generated.h"

class APawn;

UCLASS()
class UPawnMovementComponent : public UMovementComponent
{
public:
	GENERATED_BODY()

	// 입력 누적 — UE APawn::Internal_AddMovementInput 패턴. bForce 가 false 면
	// bMoveInputIgnored 일 때 무시한다.
	virtual void AddInputVector(FVector WorldVector, bool bForce = false);
	// 누적된 입력을 LastInputVector 로 옮기고 pending 을 비운 뒤 반환한다.
	// UE UPawnMovementComponent::ConsumeInputVector 대응.
	virtual FVector ConsumeInputVector();
	// 아직 소비되지 않은 누적 입력.
	FVector GetPendingInputVector() const { return PendingInputVector; }
	// 직전에 소비된 입력.
	FVector GetLastInputVector() const { return LastInputVector; }
	APawn* GetPawnOwner() const;
	virtual bool IsMoveInputIgnored() const { return bMoveInputIgnored; }
	virtual void SetUpdatedComponent(USceneComponent* NewUpdatedComponent) override;

protected:
	UPROPERTY(Edit, Category="Movement", DisplayName="Ignore Inputs")
	bool bMoveInputIgnored;

	FVector PendingInputVector = FVector::ZeroVector;
	FVector LastInputVector = FVector::ZeroVector;

};
