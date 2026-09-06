#pragma once

#include "Component/ActorComponent.h"
#include "Core/Delegate.h"

#include "Source/Engine/Component/AI/EncounterComponent.generated.h"

UENUM()
enum class EEncounterState : uint8
{
	Inactive = 0,
	Active = 1,
	Completed = 2,
};

inline const char* GEncounterStateNames[] = {
	"Inactive",
	"Active",
	"Completed",
};
inline constexpr uint32 GEncounterStateCount = sizeof(GEncounterStateNames) / sizeof(GEncounterStateNames[0]);

DECLARE_MULTICAST_DELEGATE_OneParam(FEncounterSignature, class UEncounterComponent*);

UCLASS()
class UEncounterComponent : public UActorComponent
{
public:
	GENERATED_BODY()
	UEncounterComponent() = default;
	~UEncounterComponent() override = default;

	UFUNCTION(Callable, Category="EnemyAI|Encounter")
	bool StartEncounter();
	UFUNCTION(Callable, Category="EnemyAI|Encounter")
	bool CompleteEncounter();
	UFUNCTION(Callable, Category="EnemyAI|Encounter")
	void ResetEncounter();

	UFUNCTION(Pure, Category="EnemyAI|Encounter")
	bool IsEncounterActive() const { return State == EEncounterState::Active; }
	UFUNCTION(Pure, Category="EnemyAI|Encounter")
	bool IsEncounterCompleted() const { return State == EEncounterState::Completed; }
	UFUNCTION(Pure, Category="EnemyAI|Encounter")
	EEncounterState GetEncounterState() const { return State; }

	UPROPERTY(Edit, Save, Category="EnemyAI|Encounter", DisplayName="Auto Start On Begin Play")
	bool bAutoStartOnBeginPlay = false;

	UPROPERTY(Edit, Save, Category="EnemyAI|Encounter", DisplayName="State", Enum=EEncounterState)
	EEncounterState State = EEncounterState::Inactive;

	FEncounterSignature OnEncounterStarted;
	FEncounterSignature OnEncounterCompleted;
	FEncounterSignature OnEncounterReset;

protected:
	void BeginPlay() override;
};
