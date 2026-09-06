#pragma once

#include "Component/ActorComponent.h"
#include "Core/Types/CoreTypes.h"
#include "Math/Vector.h"
#include "Object/FName.h"

class AActor;
class UAIBlackboardComponent;

#include "Source/Engine/Component/AI/AwarenessComponent.generated.h"

// =============================================================================
//  UAwarenessComponent — 은신 인지 상태머신
// =============================================================================
//  AIPerceptionComponent 는 "전투용 센서"(보이냐/안 보이냐)다. 그 위에 "얼마나
//  알고 있느냐"를 얹어, 무지각→수상함→탐색→전투확정→수색→복귀 의 계단을 만든다.
//  이 계단이 있어야 은신 접근·한 명씩 제거·스텔스 데스블로우가 성립한다.
//
//  설계: 센서(블랙보드 CanSee/InProximity)와 청각 소음을 입력으로 suspicion 게이지를
//  누적/감쇠시키고 상태를 갱신한다. 정책(이동/추격/수색)은 직접 하지 않고 블랙보드에
//  AwarenessState/Suspicion/InvestigateLocation/bIsUnawareTarget 키를 써서 Lua 가 읽게 한다.
// =============================================================================

UENUM()
enum class EAwarenessState : uint8
{
	Unaware       = 0, // 존재를 모름 — 스텔스 데스블로우 가능
	Suspicious    = 1, // 수상함 — 아직 확신 없음
	Investigating = 2, // 소음/마지막 위치를 확인하러 이동
	Alert         = 3, // 전투 확정 — 타깃 인지
	Searching     = 4, // 놓침 — 마지막 위치 주변 수색
	Returning     = 5, // 수색 실패 — 원위치 복귀
};

inline const char* GAwarenessStateNames[] = {
	"Unaware",
	"Suspicious",
	"Investigating",
	"Alert",
	"Searching",
	"Returning",
};
inline constexpr uint32 GAwarenessStateCount = sizeof(GAwarenessStateNames) / sizeof(GAwarenessStateNames[0]);

// 일반 소리 vs 중요 소리는 서로 다른 goal action 을 가진다.
// 일반(발소리)은 약하게 누적, 중요(사기그릇/휘슬/전투음)는 강하게 누적하고 즉시 조사하러 간다.
UENUM()
enum class EAISoundClass : uint8
{
	Normal    = 0,
	Important  = 1,
};

inline const char* GAISoundClassNames[] = { "Normal", "Important" };
inline constexpr uint32 GAISoundClassCount = sizeof(GAISoundClassNames) / sizeof(GAISoundClassNames[0]);

UCLASS()
class UAwarenessComponent : public UActorComponent
{
public:
	GENERATED_BODY()
	UAwarenessComponent()           = default;
	~UAwarenessComponent() override = default;

	void BeginPlay() override;
	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;

	// 청각 자극 보고 — 발소리/착지/세라믹/휘슬/전투음. 게임플레이/Lua 가 호출한다.
	// Strength 1.0 ≈ 가까운 분명한 소리. 위치를 InvestigateLocation 으로 기록한다.
	// ReportNoise = 일반(Normal) 소리. 분류가 필요하면 ReportNoiseClassified 를 쓴다.
	UFUNCTION(Callable, Category="AI|Awareness")
	void ReportNoise(const FVector& Location, float Strength);

	// 분류된 소음 보고. SoundClass: 0=Normal(약한 누적), 1=Important(강한 누적 + 즉시 조사).
	UFUNCTION(Callable, Category="AI|Awareness")
	void ReportNoiseClassified(const FVector& Location, float Strength, int32 SoundClass);

	UFUNCTION(Pure, Category="AI|Awareness")
	int32 GetLastSoundClass() const { return LastSoundClass; }

	// 상태/게이지 강제 설정(연출/디버그/리셋).
	UFUNCTION(Callable, Category="AI|Awareness")
	void ForceState(EAwarenessState NewState);
	UFUNCTION(Callable, Category="AI|Awareness")
	void ResetAwareness();

	UFUNCTION(Pure, Category="AI|Awareness")
	int32 GetAwarenessStateInt() const { return static_cast<int32>(State); }
	UFUNCTION(Pure, Category="AI|Awareness")
	float GetSuspicion() const { return Suspicion; }
	UFUNCTION(Pure, Category="AI|Awareness")
	bool IsUnaware() const { return State == EAwarenessState::Unaware || State == EAwarenessState::Suspicious; }
	UFUNCTION(Pure, Category="AI|Awareness")
	bool IsInCombat() const { return State == EAwarenessState::Alert; }
	UFUNCTION(Pure, Category="AI|Awareness")
	FVector GetInvestigateLocation() const { return InvestigateLocation; }

	// ── 튜닝 ──
	UPROPERTY(Edit, Save, Category="AI|Awareness", DisplayName="Sight Gain Per Second", Min=0.0f, Max=20.0f, Speed=0.05f)
	float SightGainPerSecond = 1.5f;
	UPROPERTY(Edit, Save, Category="AI|Awareness", DisplayName="Proximity Gain Per Second", Min=0.0f, Max=20.0f, Speed=0.05f)
	float ProximityGainPerSecond = 3.0f;
	UPROPERTY(Edit, Save, Category="AI|Awareness", DisplayName="Suspicion Decay Per Second", Min=0.0f, Max=20.0f, Speed=0.05f)
	float SuspicionDecayPerSecond = 0.5f;
	UPROPERTY(Edit, Save, Category="AI|Awareness", DisplayName="Noise Gain Scale", Min=0.0f, Max=5.0f, Speed=0.05f)
	float NoiseGainScale = 0.6f;
	// 중요 소리(사기그릇/휘슬/전투음)는 일반 소리 대비 이 배율로 더 크게 의심도를 올린다.
	UPROPERTY(Edit, Save, Category="AI|Awareness", DisplayName="Important Noise Scale", Min=1.0f, Max=8.0f, Speed=0.05f)
	float ImportantNoiseScale = 2.5f;
	UPROPERTY(Edit, Save, Category="AI|Awareness", DisplayName="Suspicious Threshold", Min=0.0f, Max=1.0f, Speed=0.01f)
	float SuspiciousThreshold = 0.3f;
	UPROPERTY(Edit, Save, Category="AI|Awareness", DisplayName="Confirm Threshold", Min=0.0f, Max=1.0f, Speed=0.01f)
	float ConfirmThreshold = 1.0f;
	// 시야에서 놓친 뒤 Alert 를 유지하는 유예(초). 지나면 수색으로 전환.
	UPROPERTY(Edit, Save, Category="AI|Awareness", DisplayName="Lost Grace Seconds", Min=0.0f, Max=20.0f, Speed=0.05f)
	float LostGraceSeconds = 3.0f;
	UPROPERTY(Edit, Save, Category="AI|Awareness", DisplayName="Search Seconds", Min=0.0f, Max=60.0f, Speed=0.1f)
	float SearchSeconds = 6.0f;
	UPROPERTY(Edit, Save, Category="AI|Awareness", DisplayName="Return Seconds", Min=0.0f, Max=60.0f, Speed=0.1f)
	float ReturnSeconds = 3.0f;

	// 동료 경계 전파: Alert 진입 시 이 반경 안 아군에게 의심을 전달한다. 0이면 비활성.
	UPROPERTY(Edit, Save, Category="AI|Awareness", DisplayName="Alert Propagation Radius", Min=0.0f, Max=200.0f, Speed=0.5f)
	float AlertPropagationRadius = 12.0f;
	UPROPERTY(Edit, Save, Category="AI|Awareness", DisplayName="Alert Propagation Strength", Min=0.0f, Max=5.0f, Speed=0.05f)
	float AlertPropagationStrength = 0.5f;

private:
	UAIBlackboardComponent* GetBlackboard() const;
	void SetState(EAwarenessState NewState);
	void PublishToBlackboard();
	void PropagateAlertToAllies();

	EAwarenessState State = EAwarenessState::Unaware;
	int32 LastSoundClass  = -1;   // 마지막으로 들은 소리 분류(-1=없음, 0=Normal, 1=Important)
	float Suspicion       = 0.0f;
	float LostTimer       = 0.0f;
	float SearchTimer     = 0.0f;
	float ReturnTimer     = 0.0f;
	FVector InvestigateLocation = FVector::ZeroVector;
	FVector HomeAnchor          = FVector::ZeroVector;
	bool    bHomeAnchorSet      = false;
};
