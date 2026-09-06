#pragma once

#include "Component/ActorComponent.h"
#include "Component/Combat/CombatTypes.h"
#include "Core/Delegate.h"

#include "Source/Engine/Component/Combat/CombatStateComponent.generated.h"

class AActor;

DECLARE_MULTICAST_DELEGATE_TwoParams(FCombatStaggerSignature, class UCombatStateComponent*, float);
DECLARE_MULTICAST_DELEGATE_OneParam(FCombatStateSignature, class UCombatStateComponent*);
DECLARE_MULTICAST_DELEGATE_OneParam(FCombatParriedSignature, class UCombatStateComponent*);
DECLARE_MULTICAST_DELEGATE_TwoParams(FCombatPerilousSignature, class UCombatStateComponent*, EPerilousType);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FCombatDeflectSignature, class UCombatStateComponent*, EDeflectGrade, AActor*);
DECLARE_MULTICAST_DELEGATE_FourParams(FPostureChangedSignature, class UCombatStateComponent*, float, float, float);

UCLASS()
class UCombatStateComponent : public UActorComponent
{
public:
	GENERATED_BODY()
	UCombatStateComponent() = default;
	~UCombatStateComponent() override = default;

	void BeginPlay() override;
	void EndPlay() override;
	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;

	UFUNCTION(Pure, Category="Combat|State")
	bool CanReceiveDamage() const { return bDamageEnabled && !bInvincible && !IsTimedInvuln(); }

	// 자기만료 무적 윈도우 — 페이즈 전환 연출 등 "건드릴 수 없는" 짧은 순간에 사용. 게임시간 기준.
	UFUNCTION(Callable, Category="Combat|State")
	void OpenInvulnWindow(float Seconds);
	UFUNCTION(Pure, Category="Combat|State")
	bool IsTimedInvuln() const;
	UFUNCTION(Callable, Category="Combat|State")
	void SetDamageEnabled(bool bEnabled) { bDamageEnabled = bEnabled; }
	UFUNCTION(Callable, Category="Combat|State")
	void SetInvincible(bool bInInvincible) { bInvincible = bInInvincible; }
	UFUNCTION(Callable, Category="Combat|State")
	void SetSuperArmor(bool bInSuperArmor) { bSuperArmor = bInSuperArmor; }

	UFUNCTION(Pure, Category="Combat|State")
	bool IsInvincible() const { return bInvincible; }
	UFUNCTION(Pure, Category="Combat|State")
	bool HasSuperArmor() const { return bSuperArmor; }
	UFUNCTION(Pure, Category="Combat|State")
	bool IsStaggered() const { return bStaggered; }

	UFUNCTION(Callable, Category="Combat|Poise")
	bool ApplyPoiseDamage(float PoiseDamage);
	bool ApplyParryReflectPoise(float PoiseDamage);
	FCombatDamageReport ApplyParryReflectDamage(float Damage, AActor* DamageCauser, AActor* InstigatorActor, const FVector& HitLocation, float DamageScale = 1.0f);
	UFUNCTION(Callable, Category="Combat|Poise")
	void ResetPoise();
	UFUNCTION(Callable, Category="Combat|Poise")
	void StartStagger(float Duration);
	// 체간(자세=처형 게이지)을 건드리지 않는 무력화. 처형 직후 딜타임처럼 '체간과 무관한' 스턴에 쓴다.
	UFUNCTION(Callable, Category="Combat|Poise")
	void StartStaggerNoPoiseReset(float Duration);
	UFUNCTION(Callable, Category="Combat|Poise")
	void StopStagger();

	// 처형(데스블로우) 연출 중인지. 처형 동안에는 피격 리액션(플린치)을 막아 처형 몽타주가 끊기지 않게 한다.
	UFUNCTION(Callable, Category="Combat|Execution")
	void SetBeingExecuted(bool bInBeingExecuted) { bBeingExecuted = bInBeingExecuted; }
	UFUNCTION(Pure, Category="Combat|Execution")
	bool IsBeingExecuted() const { return bBeingExecuted; }

	// ── 패링(반격) 유예 (요청 #6) ──
	// 이 액터가 막 패링당했을 때(공격이 상대에게 받아넘겨짐) ParryGraceDuration 동안 "유예" 윈도우를
	// 연다. 이 윈도우 동안에는 체간 피해(ApplyPoiseDamage)와 피격 경직(EnemyHitComponent)이 무시되어,
	// 뒤따르는 상대의 반격(riposte)이 보스를 불능 상태로 빠뜨리지 않는다(체력 피해는 정상 적용).
	// ParryGraceDuration=0(기본) 이면 동작하지 않는다 — 일반 적은 영향 없음, 보스만 scene 에서 활성.
	UFUNCTION(Callable, Category="Combat|Poise")
	void MarkParried();
	UFUNCTION(Pure, Category="Combat|Poise")
	bool IsInParryGrace() const;
	UFUNCTION(Pure, Category="Combat|Poise")
	float GetPoiseRatio() const;
	UFUNCTION(Pure, Category="Combat|Poise")
	float GetPostureRatio() const;
	UFUNCTION(Pure, Category="Combat|Poise")
	float GetCurrentPoise() const { return CurrentPoise; }
	UFUNCTION(Pure, Category="Combat|Poise")
	float GetMaxPoise() const { return MaxPoise; }

	// ── Attack threat broadcast ──
	// 공격을 시작한 액터가 자신의 CombatState 에 "지금 공격 중" 을 표시한다(MarkAttacking).
	// 적 AI 는 타깃의 CombatState 를 폴링해 IsAttacking / GetAttackThreatRemaining 으로
	// 회피·패링 타이밍을 잡는다. 게임시간 기준 self-expiring 이라 Tick 의존이 없다.
	UFUNCTION(Callable, Category="Combat|Threat")
	void MarkAttacking(float WindowSeconds);
	UFUNCTION(Pure, Category="Combat|Threat")
	bool IsAttacking() const;
	UFUNCTION(Pure, Category="Combat|Threat")
	float GetAttackThreatRemaining() const;

	// ── 위험공격(perilous) 표시 (Phase 2) ──
	// 공격자가 위험공격 active 동안 자신을 위험으로 표시. 방어자 AI 는 타깃의 이것을 폴링해
	// 종류별 대응(찌르기=탄기/간파, 하단=점프, 잡기=이탈)을 고른다. 게임시간 self-expiring.
	UFUNCTION(Callable, Category="Combat|Perilous")
	void MarkPerilous(EPerilousType InType, float Duration);
	UFUNCTION(Pure, Category="Combat|Perilous")
	bool IsPerilousActive() const;
	UFUNCTION(Pure, Category="Combat|Perilous")
	EPerilousType GetActivePerilousType() const;
	UFUNCTION(Pure, Category="Combat|Perilous")
	int32 GetActivePerilousTypeInt() const { return static_cast<int32>(GetActivePerilousType()); }

	// ── 탄기(deflect) (Phase 2) ──
	// 방어자가 들어오는 공격을 받아넘기기 위해 짧은 윈도우를 연다. 윈도우 안에서 피격되면
	// HealthComponent 가 ConsumeDeflect 로 등급 산정 → 체력/체간 피해 무효 + 공격자 체간 반사.
	UFUNCTION(Callable, Category="Combat|Deflect")
	void OpenDeflectWindow(float Seconds);
	UFUNCTION(Pure, Category="Combat|Deflect")
	bool IsDeflecting() const;
	UFUNCTION(Callable, Category="Combat|Deflect")
	EDeflectGrade ConsumeDeflect(AActor* Attacker);
	UFUNCTION(Pure, Category="Combat|Deflect")
	int32 GetLastDeflectGradeInt() const { return static_cast<int32>(LastDeflectGrade); }

	// ── 가드(block) ──
	// 탄기(받아치기)와 달리 가드는 들어오는 공격을 "막기만" 한다: 정면(GuardMaxAbsAngle 이내)
	// 피해를 GuardDamageMultiplier 로 감쇄하고 체간은 GuardPoiseMultiplier 로 깎이되,
	// 공격자에게 체간 반사는 없다(보스는 패링 못 함, 가드만). HealthComponent::ApplyDamageSpec
	// 가 윈도우/정면 여부를 보고 분기한다. 게임시간 self-expiring.
	UFUNCTION(Callable, Category="Combat|Guard")
	void OpenGuardWindow(float Seconds);
	UFUNCTION(Callable, Category="Combat|Guard")
	void CloseGuard();
	UFUNCTION(Pure, Category="Combat|Guard")
	bool IsGuarding() const;
	// 실제로 정면 가드로 피해를 막은 순간 호출(HealthComponent). 보스 AI가 이걸 보고 반격을 연다
	// — 가드 "선택"만으로 반격하지 않고 실제 "막음" 직후에만 반격(riposte)하게 한다.
	UFUNCTION(Callable, Category="Combat|Guard")
	void NotifyGuardBlocked();
	UFUNCTION(Pure, Category="Combat|Guard")
	bool WasGuardBlockedWithin(float Seconds) const;

	UFUNCTION(Callable, Category="Combat|Team")
	void SetTeam(ECombatTeam InTeam) { Team = InTeam; }
	UFUNCTION(Pure, Category="Combat|Team")
	ECombatTeam GetTeam() const { return Team; }
	UFUNCTION(Pure, Category="Combat|Team")
	bool IsHostileTo(const UCombatStateComponent* Other) const;

	UPROPERTY(Edit, Save, Category="Combat|Team", DisplayName="Team", Enum=ECombatTeam)
	ECombatTeam Team = ECombatTeam::Neutral;

	UPROPERTY(Edit, Save, Category="Combat|Damage", DisplayName="Damage Enabled")
	bool bDamageEnabled = true;

	UPROPERTY(Edit, Save, Category="Combat|Damage", DisplayName="Invincible")
	bool bInvincible = false;

	UPROPERTY(Edit, Save, Category="Combat|Poise", DisplayName="Super Armor")
	bool bSuperArmor = false;

	UPROPERTY(Edit, Save, Category="Combat|Poise", DisplayName="Max Poise", Min=0.0f, Max=100000.0f, Speed=1.0f)
	float MaxPoise = 100.0f;

	UPROPERTY(Edit, Save, Category="Combat|Poise", DisplayName="Current Poise", Min=0.0f, Max=100000.0f, Speed=1.0f)
	float CurrentPoise = 100.0f;

	UPROPERTY(Edit, Save, Category="Combat|Poise", DisplayName="Poise Recovery Per Second", Min=0.0f, Max=100000.0f, Speed=1.0f)
	float PoiseRecoveryPerSecond = 20.0f;

	UPROPERTY(Edit, Save, Category="Combat|Poise", DisplayName="Default Stagger Duration", Min=0.0f, Max=10.0f, Speed=0.05f)
	float DefaultStaggerDuration = 0.45f;

	// 자세(체간)가 붕괴할 때 무력화(stagger)할지. 끄면 붕괴해도 무력화 없이 처형 가능(DeathblowReady)만 열린다.
	// 보스는 false — 무력화는 "처형 이후 딜타임"에만 일어나게 한다.
	UPROPERTY(Edit, Save, Category="Combat|Poise", DisplayName="Stagger On Posture Break")
	bool bStaggerOnPostureBreak = true;

	// 패링당한 직후 "반격에 불능화되지 않는" 유예 시간(초). 플레이어가 패링→반격하는 흐름에서 반격이
	// 닿는 타이밍(≈0.2~1.0초 뒤)을 덮도록 ~1.2 정도를 권장. 0 = 비활성(기본). 보스 전용으로 scene 에서 설정.
	UPROPERTY(Edit, Save, Category="Combat|Poise", DisplayName="Parry Grace Duration", Min=0.0f, Max=5.0f, Speed=0.05f)
	float ParryGraceDuration = 0.0f;

	// 체간(맷집) 회복 지연. 마지막으로 체간 피해를 받은 뒤 이 시간(초)이 지나야 회복이 시작된다. 0=즉시(기존).
	// 보스에선 ~2초로 두어 "연속으로 두들기면 못 채우고 무너지지만, 2~3초 안 때리면 빠르게 회복"되게 한다(요청 #3).
	UPROPERTY(Edit, Save, Category="Combat|Poise", DisplayName="Poise Regen Delay", Min=0.0f, Max=10.0f, Speed=0.05f)
	float PoiseRegenDelay = 0.0f;

	// 체력이 낮을수록 체간 회복이 느려진다(보고서: 체력-체간 회복 연계). 회복 배율 = lerp.
	UPROPERTY(Edit, Save, Category="Combat|Poise", DisplayName="Posture Recovery Scales With Health")
	bool bPostureRecoveryScalesWithHealth = true;


	UPROPERTY(Edit, Save, Category="Combat|Deflect", DisplayName="Deflect Window Seconds", Min=0.0f, Max=2.0f, Speed=0.01f)
	float DeflectWindowSeconds = 0.18f;

	UPROPERTY(Edit, Save, Category="Combat|Deflect", DisplayName="Deflect Reflect Poise", Min=0.0f, Max=100000.0f, Speed=0.5f)
	float DeflectReflectPoise = 25.0f;

	UPROPERTY(Edit, Save, Category="Combat|Deflect", DisplayName="Deflect Reflect Damage", Min=0.0f, Max=100000.0f, Speed=0.5f)
	float DeflectReflectDamage = 10.0f;

	UPROPERTY(Edit, Save, Category="Combat|Guard", DisplayName="Guard Window Seconds", Min=0.0f, Max=5.0f, Speed=0.05f)
	float GuardWindowSeconds = 0.7f;

	// 가드 정면 피해 배수(0.25 → 75% 경감). 측후면 공격엔 적용되지 않는다.
	UPROPERTY(Edit, Save, Category="Combat|Guard", DisplayName="Guard Damage Multiplier", Min=0.0f, Max=1.0f, Speed=0.01f)
	float GuardDamageMultiplier = 0.25f;

	// 가드해도 체간은 깎인다(연속 가드 → 가드 브레이크). 1.0 보다 작게.
	UPROPERTY(Edit, Save, Category="Combat|Guard", DisplayName="Guard Poise Multiplier", Min=0.0f, Max=2.0f, Speed=0.01f)
	float GuardPoiseMultiplier = 0.6f;

	// 이 각도(도) 이내의 정면 공격만 막는다.
	UPROPERTY(Edit, Save, Category="Combat|Guard", DisplayName="Guard Max Abs Angle", Min=0.0f, Max=180.0f, Speed=1.0f)
	float GuardMaxAbsAngle = 110.0f;

	FCombatStaggerSignature OnStaggerStarted;
	FCombatStateSignature OnStaggerEnded;
	// 이 액터의 공격이 상대에게 패링(deflect)당한 순간 발행 — EnemyCharacter 가 받아 공격 몽타주를
	// 잠깐 역재생(리코일)한다(요청 A). MarkParried 에서 항상 broadcast(유예 윈도우와 무관).
	FCombatParriedSignature OnParried;
	FCombatPerilousSignature OnPerilousCue;
	FCombatDeflectSignature OnDeflectResolved;
	FPostureChangedSignature OnPostureChanged;
	// 자세가 0이 되는 순간 발행 — ExecutionComponent 가 받아 데스블로우 창을 연다.
	// 자세 붕괴와 "처형으로 전환"을 분리하기 위한 신호. 리스너가 없으면 종전처럼 stagger 만 일어난다.
	FCombatStateSignature OnPostureBroken;

private:
	float GetHealthRatioSafe() const;
	bool ApplyPoiseDamageInternal(float PoiseDamage, bool bIgnoreParryGrace);
	bool SetPoiseValue(float NewPoise);

	bool bStaggered = false;
	float StaggerRemainingTime = 0.0f;
	float AttackThreatUntilSeconds = 0.0f;

	// 위험공격 표시(게임시간 만료).
	EPerilousType ActivePerilousType = EPerilousType::None;
	float PerilousUntilSeconds = 0.0f;

	// 탄기 윈도우(게임시간).
	float DeflectUntilSeconds = 0.0f;
	float DeflectOpenedAtSeconds = 0.0f;
	float DeflectWindowLen = 0.0f;
	EDeflectGrade LastDeflectGrade = EDeflectGrade::None;

	// 가드 윈도우(게임시간).
	float GuardUntilSeconds = 0.0f;
	// 마지막으로 실제 가드로 막은 시각(게임시간). 음수=아직 없음.
	float LastGuardBlockSeconds = -1000.0f;

	// 타이머 무적 윈도우(게임시간).
	float InvulnUntilSeconds = 0.0f;

	// 패링 유예 윈도우 만료 시각(게임시간). MarkParried 가 연다.
	float ParryGraceUntilSeconds = 0.0f;

	// 마지막으로 체간 피해를 받은 게임시각(초). PoiseRegenDelay(맷집 회복 지연) 판정에 사용. 음수=아직 없음.
	float LastPoiseDamageTimeSeconds = -1000.0f;

	// 현재 stagger 가 끝날 때 보이는 자세(poise)를 리셋할지. 처형 자세 붕괴 stagger=true,
	// 처형 직후 딜타임 stagger=false(체간 진행도를 건드리지 않는다).
	bool bStaggerResetsPoiseOnEnd = true;
	// 처형(데스블로우) 연출 중 — 피격 리액션을 막아 처형 몽타주가 끊기지 않게 한다.
	bool bBeingExecuted = false;
};
