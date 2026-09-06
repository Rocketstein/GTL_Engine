#pragma once

#include "Component/ActorComponent.h"
#include "Component/Combat/CombatTypes.h"
#include "Core/Delegate.h"

#include "Source/Engine/Component/AI/PhaseComponent.generated.h"

DECLARE_MULTICAST_DELEGATE_ThreeParams(FEnemyPhaseChangedSignature, class UPhaseComponent*, int32, int32);

UCLASS()
class UPhaseComponent : public UActorComponent
{
public:
	GENERATED_BODY()
	UPhaseComponent() = default;
	~UPhaseComponent() override = default;

	void BeginPlay() override;

	UFUNCTION(Callable, Category="EnemyAI|Phase")
	bool SetPhase(int32 NewPhase);
	UFUNCTION(Pure, Category="EnemyAI|Phase")
	int32 GetCurrentPhase() const { return CurrentPhase; }
	UFUNCTION(Pure, Category="EnemyAI|Phase")
	FEnemyPhaseData GetCurrentPhaseData() const;
	UPROPERTY(Edit, Save, Category="EnemyAI|Phase", DisplayName="Initial Phase", Min=1.0f, Max=99.0f, Speed=1.0f)
	int32 InitialPhase = 1;

	UPROPERTY(Edit, Save, Category="EnemyAI|Phase", DisplayName="Current Phase", Min=1.0f, Max=99.0f, Speed=1.0f)
	int32 CurrentPhase = 1;

	UPROPERTY(Edit, Save, Category="EnemyAI|Phase", DisplayName="Phases", Type=Array)
	TArray<FEnemyPhaseData> Phases;

	FEnemyPhaseChangedSignature OnPhaseChanged;
};
