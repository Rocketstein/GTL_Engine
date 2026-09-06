#pragma once

#include "Core/Types/CoreTypes.h"
#include "Math/Vector.h"
#include "Object/FName.h"

class AActor;
class UAnimMontage;
class UPrimitiveComponent;

#include "Source/Engine/Component/Combat/CombatTypes.generated.h"

UENUM()
enum class ECombatTeam : uint8
{
	Neutral = 0,
	Player = 1,
	Enemy = 2,
	World = 3,
};

inline const char* GCombatTeamNames[] = {
	"Neutral",
	"Player",
	"Enemy",
	"World",
};
inline constexpr uint32 GCombatTeamCount = sizeof(GCombatTeamNames) / sizeof(GCombatTeamNames[0]);



UENUM()
enum class ECombatDamageResult : uint8
{
	None = 0,
	Damaged = 1,
	Killed = 2,
	Rejected = 3,
	// 방어자가 탄기(deflect) 윈도우 안에서 받아넘김 — 체력/체간 피해 무효, 공격자에게 체간 반사.
	// (직렬화 호환을 위해 끝에 추가)
	Deflected = 4,
};

inline const char* GCombatDamageResultNames[] = {
	"None",
	"Damaged",
	"Killed",
	"Rejected",
	"Deflected",
};
inline constexpr uint32 GCombatDamageResultCount = sizeof(GCombatDamageResultNames) / sizeof(GCombatDamageResultNames[0]);

// ── 세키로식 전투 분류 (Phase 2) ───────────────────────────────────────────────
// 위험공격(perilous) 종류. 방어자는 종류별로 다른 대응을 해야 한다(찌르기=간파/탄기,
// 하단=점프, 잡기=거리이탈).
UENUM()
enum class EPerilousType : uint8
{
	None = 0,
	Thrust = 1,   // 찌르기
	Sweep = 2,    // 하단베기
	Grab = 3,     // 잡기
};

inline const char* GPerilousTypeNames[] = {
	"None",
	"Thrust",
	"Sweep",
	"Grab",
};
inline constexpr uint32 GPerilousTypeCount = sizeof(GPerilousTypeNames) / sizeof(GPerilousTypeNames[0]);

// 탄기 등급 — 윈도우 안에서 얼마나 정확했는지.
UENUM()
enum class EDeflectGrade : uint8
{
	None = 0,
	Perfect = 1,
	Good = 2,
	Late = 3,
};

inline const char* GDeflectGradeNames[] = {
	"None",
	"Perfect",
	"Good",
	"Late",
};
inline constexpr uint32 GDeflectGradeCount = sizeof(GDeflectGradeNames) / sizeof(GDeflectGradeNames[0]);

// 공격 프레임 데이터 타임라인 단계.
UENUM()
enum class ECombatMovePhase : uint8
{
	Inactive = 0,
	Startup = 1,    // 선딜
	Active = 2,     // 활성(타격)
	Recovery = 3,   // 후딜(punish 대상)
};

inline const char* GCombatMovePhaseNames[] = {
	"Inactive",
	"Startup",
	"Active",
	"Recovery",
};
inline constexpr uint32 GCombatMovePhaseCount = sizeof(GCombatMovePhaseNames) / sizeof(GCombatMovePhaseNames[0]);

USTRUCT()
struct FCombatDamageSpec
{
	GENERATED_BODY()

	UPROPERTY(Edit, Save, Category="Damage", DisplayName="Damage", Min=0.0f, Max=100000.0f, Speed=0.5f)
	float Damage = 10.0f;

	UPROPERTY(Edit, Save, Category="Damage", DisplayName="Poise Damage", Min=0.0f, Max=100000.0f, Speed=0.5f)
	float PoiseDamage = 10.0f;

	UPROPERTY(Transient, Category="Damage")
	AActor* DamageCauser = nullptr;

	UPROPERTY(Transient, Category="Damage")
	AActor* InstigatorActor = nullptr;

	UPROPERTY(Transient, Category="Damage")
	FVector HitLocation = FVector::ZeroVector;

	UPROPERTY(Transient, Category="Damage")
	FVector HitDirection = FVector::ZeroVector;
};

USTRUCT()
struct FCombatDamageReport
{
	GENERATED_BODY()

	UPROPERTY(Transient, Category="Damage")
	ECombatDamageResult Result = ECombatDamageResult::None;

	UPROPERTY(Transient, Category="Damage")
	float RequestedDamage = 0.0f;

	UPROPERTY(Transient, Category="Damage")
	float AppliedDamage = 0.0f;

	UPROPERTY(Transient, Category="Damage")
	float PreviousHealth = 0.0f;

	UPROPERTY(Transient, Category="Damage")
	float NewHealth = 0.0f;

	UPROPERTY(Transient, Category="Damage")
	bool bKilled = false;
};

UENUM()
enum class ECombatImpactType : uint8
{
	EnemyHit = 0,
	WorldHit = 1,
	Parried = 2,
	Blocked = 3,
};

inline const char* GCombatImpactTypeNames[] = {
	"EnemyHit",
	"WorldHit",
	"Parried",
	"Blocked",
};
inline constexpr uint32 GCombatImpactTypeCount = sizeof(GCombatImpactTypeNames) / sizeof(GCombatImpactTypeNames[0]);

USTRUCT()
struct FCombatImpactEvent
{
	GENERATED_BODY()

	UPROPERTY(Transient, Category="Combat|Impact", DisplayName="Type", Enum=ECombatImpactType)
	ECombatImpactType Type = ECombatImpactType::EnemyHit;

	UPROPERTY(Transient, Category="Combat|Impact")
	AActor* Attacker = nullptr;

	UPROPERTY(Transient, Category="Combat|Impact")
	AActor* Target = nullptr;

	UPROPERTY(Transient, Category="Combat|Impact")
	UPrimitiveComponent* HitComponent = nullptr;

	UPROPERTY(Transient, Category="Combat|Impact")
	FVector HitLocation = FVector::ZeroVector;

	UPROPERTY(Transient, Category="Combat|Impact")
	FVector HitNormal = FVector::ZeroVector;

	UPROPERTY(Transient, Category="Combat|Impact")
	FCombatDamageSpec DamageSpec;

	UPROPERTY(Transient, Category="Combat|Impact")
	FCombatDamageReport DamageReport;

	UPROPERTY(Transient, Category="Combat|Impact")
	FName HitEventName = FName::None;
};

USTRUCT()
struct FEnemyAttackData
{
	GENERATED_BODY()

	UPROPERTY(Edit, Save, Category="Attack", DisplayName="Attack Name")
	FName AttackName = FName::None;

	UPROPERTY(Edit, Save, Category="Attack", DisplayName="Montage", Type=ObjectRef, AllowedClass=UAnimMontage)
	UAnimMontage* Montage = nullptr;

	UPROPERTY(Edit, Save, Category="Attack|Phase", DisplayName="Min Phase", Min=0.0f, Max=99.0f, Speed=1.0f)
	int32 MinPhase = 1;

	UPROPERTY(Edit, Save, Category="Attack|Phase", DisplayName="Max Phase", Min=0.0f, Max=99.0f, Speed=1.0f)
	int32 MaxPhase = 1;

	UPROPERTY(Edit, Save, Category="Attack|Range", DisplayName="Min Range", Min=0.0f, Max=100000.0f, Speed=1.0f)
	float MinRange = 0.0f;

	UPROPERTY(Edit, Save, Category="Attack|Range", DisplayName="Max Range", Min=0.0f, Max=100000.0f, Speed=1.0f)
	float MaxRange = 300.0f;

	// 수평 사거리와 별도로 허용할 최대 높이 차. 높낮이 맵에서 flat 거리만 보고
	// 벽 위/아래 타깃을 때리는 것을 막는다. 0 이하면 수직 제한을 적용하지 않는다.
	UPROPERTY(Edit, Save, Category="Attack|Range", DisplayName="Max Vertical Delta", Min=0.0f, Max=100000.0f, Speed=0.05f)
	float MaxVerticalDelta = 1.25f;

	UPROPERTY(Edit, Save, Category="Attack|Angle", DisplayName="Max Abs Angle", Min=0.0f, Max=180.0f, Speed=1.0f)
	float MaxAbsAngle = 70.0f;

	UPROPERTY(Edit, Save, Category="Attack|Timing", DisplayName="Cooldown", Min=0.0f, Max=60.0f, Speed=0.05f)
	float Cooldown = 2.0f;

	UPROPERTY(Edit, Save, Category="Attack|Timing", DisplayName="Recovery Time", Min=0.0f, Max=60.0f, Speed=0.05f)
	float RecoveryTime = 0.8f;

	UPROPERTY(Edit, Save, Category="Attack|Weight", DisplayName="Weight", Min=0.0f, Max=1000.0f, Speed=0.1f)
	float Weight = 1.0f;

	UPROPERTY(Edit, Save, Category="Attack|Weight", DisplayName="Repeat Weight Scale", Min=0.0f, Max=1.0f, Speed=0.05f)
	float RepeatWeightScale = 0.25f;

	UPROPERTY(Edit, Save, Category="Attack|Damage", DisplayName="Damage", Min=0.0f, Max=100000.0f, Speed=0.5f)
	float Damage = 10.0f;

	UPROPERTY(Edit, Save, Category="Attack|Damage", DisplayName="Poise Damage", Min=0.0f, Max=100000.0f, Speed=0.5f)
	float PoiseDamage = 10.0f;

	UPROPERTY(Edit, Save, Category="Attack|Behavior", DisplayName="Requires Target In Front")
	bool bRequiresTargetInFront = true;

	UPROPERTY(Edit, Save, Category="Attack|Behavior", DisplayName="Gap Closer")
	bool bIsGapCloser = false;


	// 전술 의미는 C++ enum이 아니라 Lua Blueprint/Data가 해석하는 태그다.
	UPROPERTY(Edit, Save, Category="Attack|Behavior", DisplayName="Tactic Tag")
	FName TacticTag = FName("Neutral");

	UPROPERTY(Edit, Save, Category="Attack|Behavior", DisplayName="Priority", Min=0.0f, Max=100.0f, Speed=0.1f)
	float Priority = 1.0f;

	UPROPERTY(Edit, Save, Category="Attack|Combo", DisplayName="Requires Previous Attack")
	bool bRequiresPreviousAttack = false;

	UPROPERTY(Edit, Save, Category="Attack|Combo", DisplayName="Required Previous Attack")
	FName RequiredPreviousAttack = FName::None;

	// ── 프레임 데이터 / 위험공격 분류 (Phase 2) ──
	// 60Hz 기준 프레임 수. 전투 시계가 이 타임라인을 진행시켜 위험표식·후딜 punish 창을 만든다.
	UPROPERTY(Edit, Save, Category="Attack|Frames", DisplayName="Startup Frames", Min=0.0f, Max=600.0f, Speed=1.0f)
	int32 StartupFrames = 12;

	UPROPERTY(Edit, Save, Category="Attack|Frames", DisplayName="Active Frames", Min=0.0f, Max=600.0f, Speed=1.0f)
	int32 ActiveFrames = 4;

	UPROPERTY(Edit, Save, Category="Attack|Frames", DisplayName="Recovery Frames", Min=0.0f, Max=600.0f, Speed=1.0f)
	int32 RecoveryFrames = 24;

	// 위험표식이 켜지는 선딜 내 프레임(텔레그래프 길이). PerilousType 이 None 이면 무시.
	UPROPERTY(Edit, Save, Category="Attack|Frames", DisplayName="Perilous Cue Frame", Min=0.0f, Max=600.0f, Speed=1.0f)
	int32 PerilousCueFrame = 6;

	UPROPERTY(Edit, Save, Category="Attack|Perilous", DisplayName="Perilous Type", Enum=EPerilousType)
	EPerilousType PerilousType = EPerilousType::None;

	UPROPERTY(Edit, Save, Category="Attack|Perilous", DisplayName="Can Be Deflected")
	bool bCanBeDeflected = true;

	// 비주얼 몽타주를 재생하면서도 타격 판정은 프레임 데이터(Startup/Active/Recovery) 타임라인으로
	// 구동한다. 소스 시퀀스에 AttackHitWindow 노티파이를 저작하지 않아도 active 프레임 동안
	// 사정권 타깃에 1회 데미지가 적용된다(montageless 폴백 경로 재사용). 노티파이가 있는
	// 공격은 false 로 두어 노티파이가 전담하게 한다.
	UPROPERTY(Edit, Save, Category="Attack|Behavior", DisplayName="Damage From Frame Data")
	bool bDamageFromFrameData = false;

	// 몽타주 재생 배속. 1보다 크면 더 빠르게(다이내믹한 전투), 공격 지속시간도 같은 비율로 짧아진다.
	UPROPERTY(Edit, Save, Category="Attack|Behavior", DisplayName="Play Rate", Min=0.1f, Max=4.0f, Speed=0.05f)
	float PlayRate = 1.0f;

	// 타격(데미지)이 들어가는 시점 — 몽타주 (배속 반영) 길이의 비율. 0.4 면 스윙 중간쯤.
	// 이걸로 데미지를 실제 칼이 닿는 비주얼에 맞춘다. 공격의 "커밋" 길이는 이 타격 시점 +
	// 짧은 캔슬 버퍼로 정해져, 보스는 타격 직후 자유로워져 다음 공격을 블렌딩으로 잇는다(빠른 연속 교전).
	UPROPERTY(Edit, Save, Category="Attack|Behavior", DisplayName="Hit Time Fraction", Min=0.05f, Max=0.95f, Speed=0.01f)
	float HitTimeFraction = 0.42f;

	// 공격 최소 커밋(busy) 시간(초). 기본 0이면 "타격시점+캔슬버퍼"의 빠른 커밋을 쓴다(스냅). 강공격
	// /마무리기는 이 값을 크게 줘서 후딜(회수) 모션이 걷기로 덮이지 않고 충분히 보이게 한다(애니 씹힘 방지).
	UPROPERTY(Edit, Save, Category="Attack|Behavior", DisplayName="Min Commit Time", Min=0.0f, Max=10.0f, Speed=0.05f)
	float MinCommitTime = 0.0f;
};

USTRUCT()
struct FEnemyPhaseData
{
	GENERATED_BODY()

	UPROPERTY(Edit, Save, Category="Phase", DisplayName="Phase")
	int32 Phase = 1;

	UPROPERTY(Edit, Save, Category="Phase", DisplayName="Phase Name")
	FName PhaseName = FName::None;

};

// ── 데스블로우/처형
// ───────────────────────────────────────────
// 세키로 핵심 승패: 자세가 0이 되면 즉사가 아니라 "처형 가능(DeathblowReady)" 상태로
// 짧은 창이 열린다. 그 창 안에서 처형되면 스톡을 1개 소비한다. 졸개는 스톡 1개라
// 즉사, 강적은 여러 스톡을 거쳐야 죽는다. 창이 만료되면 자세를 회복해 전투로 복귀.
UENUM()
enum class EExecutionState : uint8
{
	None = 0,           // 평시
	DeathblowReady = 1, // 자세 붕괴 — 처형 창 열림
	Executed = 2,       // 마지막 스톡 소비, 사망 확정
};

inline const char* GExecutionStateNames[] = {
	"None",
	"DeathblowReady",
	"Executed",
};
inline constexpr uint32 GExecutionStateCount = sizeof(GExecutionStateNames) / sizeof(GExecutionStateNames[0]);

// ── 위험공격 종류별 결과표 ─────────────────────────────────────
// 위험공격은 "강한 공격"이 아니라 "대응을 검사하는 질문"이다. 찌르기/하단/잡기 각각
// 대응 성공·실패의 결과가 다르다. 방어자·공격자 양쪽 결과를 종류별로 분리해 둔다.
// 런타임 소비처: 탄기 해소(공격자 체간 반사 배수), 피해 적용(대응 실패 시 피해 배수).
USTRUCT()
struct FPerilousResolution
{
	GENERATED_BODY()

	UPROPERTY(Edit, Save, Category="Perilous", DisplayName="Type", Enum=EPerilousType)
	EPerilousType Type = EPerilousType::None;

	// 대응 실패(그냥 맞음) 시 방어자가 받는 체간 피해 배수.
	UPROPERTY(Edit, Save, Category="Perilous", DisplayName="Defender Fail Poise Scale", Min=0.0f, Max=10.0f, Speed=0.05f)
	float DefenderFailPoiseScale = 1.5f;

	// 대응 실패 시 방어자가 받는 체력 피해 배수.
	UPROPERTY(Edit, Save, Category="Perilous", DisplayName="Defender Fail Health Scale", Min=0.0f, Max=10.0f, Speed=0.05f)
	float DefenderFailHealthScale = 1.25f;

	// 올바른 대응 성공(찌르기=간파/탄기, 하단=점프, 잡기=이탈) 시 공격자 체간 반사 배수.
	UPROPERTY(Edit, Save, Category="Perilous", DisplayName="Answer Reflect Poise Scale", Min=0.0f, Max=10.0f, Speed=0.05f)
	float AnswerReflectPoiseScale = 1.5f;

	// 대응 성공 후 공격자에게 열리는 빈틈(초) — punish 창.
	UPROPERTY(Edit, Save, Category="Perilous", DisplayName="Answer Punish Window", Min=0.0f, Max=5.0f, Speed=0.05f)
	float AnswerPunishWindow = 0.6f;
};

// 종류별 기본 결과. 디자이너가 per-actor 테이블로 덮어쓰기 전의 합리적 기본값.
// 찌르기: 간파/탄기 성공 시 공격자 자세 대손실. 하단: 점프 성공 시 큰 punish. 잡기: 이탈 성공 시 최대 빈틈.
inline FPerilousResolution GetPerilousResolution(EPerilousType Type)
{
	FPerilousResolution R;
	R.Type = Type;
	switch (Type)
	{
	case EPerilousType::Thrust:
		R.DefenderFailPoiseScale = 1.6f;  R.DefenderFailHealthScale = 1.4f;
		R.AnswerReflectPoiseScale = 2.0f; R.AnswerPunishWindow = 0.5f;
		break;
	case EPerilousType::Sweep:
		R.DefenderFailPoiseScale = 1.4f;  R.DefenderFailHealthScale = 1.2f;
		R.AnswerReflectPoiseScale = 1.2f; R.AnswerPunishWindow = 0.8f;
		break;
	case EPerilousType::Grab:
		R.DefenderFailPoiseScale = 2.0f;  R.DefenderFailHealthScale = 1.8f;
		R.AnswerReflectPoiseScale = 0.0f; R.AnswerPunishWindow = 1.0f;
		break;
	case EPerilousType::None:
	default:
		R.DefenderFailPoiseScale = 1.0f;  R.DefenderFailHealthScale = 1.0f;
		R.AnswerReflectPoiseScale = 1.0f; R.AnswerPunishWindow = 0.0f;
		break;
	}
	return R;
}
