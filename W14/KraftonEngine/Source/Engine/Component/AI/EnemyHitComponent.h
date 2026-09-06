#pragma once

#include "Component/ActorComponent.h"
#include "Core/Delegate.h"
#include "Object/Ptr/WeakObjectPtr.h"

class AActor;
class UAnimMontage;
class UHealthComponent;
class UEnemyAttackComponent;
class UCombatStateComponent;

#include "Source/Engine/Component/AI/EnemyHitComponent.generated.h"

UENUM()
enum class EEnemyHitDirection : uint8
{
	Front = 0,
	Back = 1,
	Left = 2,
	Right = 3,
};

USTRUCT()
struct FEnemyDirectionalMontages
{
	GENERATED_BODY()

	UPROPERTY(Edit, Save, Category="EnemyHit", DisplayName="Front", Type=ObjectRef, AllowedClass=UAnimMontage)
	UAnimMontage* Front = nullptr;

	UPROPERTY(Edit, Save, Category="EnemyHit", DisplayName="Back", Type=ObjectRef, AllowedClass=UAnimMontage)
	UAnimMontage* Back = nullptr;

	UPROPERTY(Edit, Save, Category="EnemyHit", DisplayName="Left", Type=ObjectRef, AllowedClass=UAnimMontage)
	UAnimMontage* Left = nullptr;

	UPROPERTY(Edit, Save, Category="EnemyHit", DisplayName="Right", Type=ObjectRef, AllowedClass=UAnimMontage)
	UAnimMontage* Right = nullptr;
};

UCLASS()
class UEnemyHitComponent : public UActorComponent
{
public:
	GENERATED_BODY()
	UEnemyHitComponent() = default;
	~UEnemyHitComponent() override = default;

	void BeginPlay() override;
	void EndPlay() override;

	UFUNCTION(Callable, Category="EnemyAI|Hit")
	void BindHealthEvents();
	UFUNCTION(Callable, Category="EnemyAI|Hit")
	void UnbindHealthEvents();
	UFUNCTION(Pure, Category="EnemyAI|Hit")
	EEnemyHitDirection ResolveHitDirection(AActor* DamageCauser, AActor* InstigatorActor) const;
	UFUNCTION(Callable, Category="EnemyAI|Hit")
	bool PlayHitMontage(AActor* DamageCauser, AActor* InstigatorActor);
	UFUNCTION(Callable, Category="EnemyAI|Hit")
	bool PlayDeathMontage(AActor* DamageCauser, AActor* InstigatorActor);

	UPROPERTY(Edit, Save, Category="EnemyAI|Hit", DisplayName="Hit Montages")
	FEnemyDirectionalMontages HitMontages;

	UPROPERTY(Edit, Save, Category="EnemyAI|Hit", DisplayName="Death Montages")
	FEnemyDirectionalMontages DeathMontages;

	UPROPERTY(Edit, Save, Category="EnemyAI|Hit", DisplayName="Hit Montage Play Rate", Min=0.01f, Max=10.0f, Speed=0.05f)
	float HitMontagePlayRate = 1.0f;

	UPROPERTY(Edit, Save, Category="EnemyAI|Hit", DisplayName="Death Montage Play Rate", Min=0.01f, Max=10.0f, Speed=0.05f)
	float DeathMontagePlayRate = 1.0f;

	// 가드(block) 중 피격 시 재생할 리액션(막기 흔들림). null 이면 방향별 HitMontages 로 폴백.
	UPROPERTY(Edit, Save, Category="EnemyAI|Hit", DisplayName="Guard Hit Montage", Type=ObjectRef, AllowedClass=UAnimMontage)
	UAnimMontage* GuardHitMontage = nullptr;

	// 슈퍼아머 보스가 자기 공격을 펼치는 중이면 일반 피격에 경직되지 않고 포즈를 유지한다(공세 보존).
	UPROPERTY(Edit, Save, Category="EnemyAI|Hit", DisplayName="Poise Through During Own Attack")
	bool bPoiseThroughDuringOwnAttack = true;

	// 이동(걷기/달리기) 중이면 일반 피격 경직을 생략한다(걷기 모션이 풀바디 경직에 덮여 슬라이딩처럼
	// 보이는 것 방지). 가드 리액션은 제외(항상 재생). 정지 상태에서만 경직.
	UPROPERTY(Edit, Save, Category="EnemyAI|Hit", DisplayName="Skip Hit While Moving")
	bool bSkipHitWhileMoving = true;

	UPROPERTY(Edit, Save, Category="EnemyAI|Hit", DisplayName="Stop Current Montage Before Hit")
	bool bStopCurrentMontageBeforeHit = true;

	UPROPERTY(Edit, Save, Category="EnemyAI|Hit", DisplayName="Stop Current Montage Before Death")
	bool bStopCurrentMontageBeforeDeath = true;

	UPROPERTY(Edit, Save, Category="EnemyAI|Hit", DisplayName="Play Hit When Damage Is Zero")
	bool bPlayHitWhenDamageIsZero = false;

	UPROPERTY(Edit, Save, Category="EnemyAI|Hit", DisplayName="Restart Attack Cooldowns On Hit")
	bool bClearAttackCooldownsOnHit = true;

	// 이 페이즈 이하에서만 피격 리액션(경직)을 재생. 초과 페이즈는 하이퍼아머(경직 없음).
	// 예: 보스 1 → P1 만 경직, P2/P3 는 끊김 없이 압박. 기본 99 = 전 페이즈 경직(기존 동작).
	UPROPERTY(Edit, Save, Category="EnemyAI|Hit", DisplayName="Hit React Max Phase", Min=1.0f, Max=99.0f, Speed=1.0f)
	int32 HitReactMaxPhase = 99;

	// 피격 경직(플린치) 재생 후 이 시간(초) 동안은 추가 피격이 들어와도 플린치를 생략한다(하이퍼아머
	// 윈도우). 플레이어가 난타하면 매 타격마다 플린치 몽타주가 재생·갱신되어 보스가 영영 공격을 시작
	// 못하는 "무한 경직"(요청 #1) 문제를 막는다 — 한 번 경직 뒤엔 잠깐 칩 피해를 흘리며 반격 틈을
	// 확보한다. 0 = 쿨다운 없음(매 타격 경직, 기존 동작). 보스는 scene 에서 0.85 정도를 권장.
	UPROPERTY(Edit, Save, Category="EnemyAI|Hit", DisplayName="Hit Reaction Cooldown", Min=0.0f, Max=10.0f, Speed=0.05f)
	float HitReactionCooldown = 0.0f;

private:
	void HandleDamaged(UHealthComponent* Component, float Damage, float NewHealth, AActor* DamageCauser, AActor* InstigatorActor);
	void HandleDeath(UHealthComponent* Component, AActor* DamageCauser, AActor* InstigatorActor);
	// 맷집(체간)이 붕괴해 stagger(무력화)가 시작될 때 호출 — 피격 모션을 1회 재생한다(요청 #3).
	void HandleStaggerStarted(UCombatStateComponent* Combat, float Duration);
	void RestartAttackCooldowns();
	UAnimMontage* SelectMontage(const FEnemyDirectionalMontages& Montages, EEnemyHitDirection Direction) const;
	bool PlayDirectionalMontage(const FEnemyDirectionalMontages& Montages, EEnemyHitDirection Direction, bool bStopCurrentMontage, float PlayRate);

	TWeakObjectPtr<UHealthComponent> BoundHealthComponent = nullptr;
	TWeakObjectPtr<UCombatStateComponent> BoundCombatComponent = nullptr;
	FDelegateHandle DamagedHandle;
	FDelegateHandle DeathHandle;
	FDelegateHandle StaggerStartedHandle;
	bool bBoundHealthEvents = false;
	// 마지막으로 플린치(피격 경직)를 재생한 게임시각(초). HitReactionCooldown 쿨다운 판정에 사용. 음수=아직 없음.
	float LastHitReactGameTime = -1000.0f;
};
