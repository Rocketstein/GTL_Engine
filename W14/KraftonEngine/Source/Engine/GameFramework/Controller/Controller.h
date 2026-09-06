#pragma once

#include "GameFramework/AActor.h"
#include "Math/Rotator.h"
#include "Object/Ptr/WeakObjectPtr.h"

class APawn;

#include "Source/Engine/GameFramework/Controller/Controller.generated.h"

// ============================================================
// AController — Pawn을 조종하는 공통 베이스.
// UE 계층과 같이 PlayerController / AIController 모두 이 클래스를 상속한다.
// ============================================================
UCLASS()
class AController : public AActor
{
public:
	GENERATED_BODY()
	AController() = default;
	~AController() override = default;

	UFUNCTION(Callable, Category="Controller")
	virtual void Possess(APawn* Pawn);
	UFUNCTION(Callable, Category="Controller")
	virtual void UnPossess();

	UFUNCTION(Pure, Category="Controller")
	APawn* GetPawn() const { return PossessedPawn.Get(); }
	UFUNCTION(Pure, Category="Controller")
	APawn* GetPossessedPawn() const { return GetPawn(); }
	UFUNCTION(Pure, Category="Controller")
	bool HasPawn() const { return PossessedPawn.IsValid(); }

	UFUNCTION(Pure, Category="Controller|Control")
	virtual FRotator GetControlRotation() const { return ControlRotation; }
	UFUNCTION(Callable, Category="Controller|Control")
	virtual void SetControlRotation(const FRotator& NewRotation);

protected:
	virtual void OnPossess(APawn* Pawn) { (void)Pawn; }
	virtual void OnUnPossess(APawn* OldPawn) { (void)OldPawn; }

	TWeakObjectPtr<APawn> PossessedPawn = nullptr;
	FRotator ControlRotation = FRotator::ZeroRotator;
};
