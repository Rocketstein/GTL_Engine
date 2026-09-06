#pragma once

#include "Component/ActorComponent.h"
#include "Core/Types/CoreTypes.h"
#include "Math/Vector.h"
#include "Object/FName.h"
#include "Object/Ptr/WeakObjectPtr.h"

class AActor;
class UAIBlackboardComponent;

#include "Source/Engine/Component/AI/AIPerceptionComponent.generated.h"

// =============================================================================
//  UAIPerceptionComponent — 전투용 센서 계층 (엔진 코어 메커니즘)
// =============================================================================
//  보고서가 비어 있다고 진단한 "Sensor: 시야/청각/근접" 계층. 센서는 직접 행동을
//  고르지 않고 Blackboard 를 갱신한다(보고서 4대 원칙 ①). UpdateSenses() 한 번이
//  시야 콘(FOV+거리), 근접, 위협창, 자기/타깃 체간·체력·페이즈를 Blackboard 에
//  기록하고 자극 기억을 감쇠시킨다.
// =============================================================================

enum class EAISenseType : uint8
{
    Sight,
    Proximity,
    Damage,
    Hearing   // 청각 — 발소리/착지/세라믹/휘슬/전투음
};

// 디버거가 나열하는 경량 자극 레코드.
struct FAISenseStimulus
{
    EAISenseType           Type     = EAISenseType::Sight;
    TWeakObjectPtr<AActor> Source;
    FVector                Location = FVector::ZeroVector;
    float                  Strength = 0.0f;
    int32                  AgeTicks = 0;
};

UCLASS()
class UAIPerceptionComponent : public UActorComponent
{
public:
    GENERATED_BODY()
    UAIPerceptionComponent()           = default;
    ~UAIPerceptionComponent() override = default;

    // 한 think 스텝의 센싱 전체를 실행하고 Blackboard 를 갱신한다. Lua 두뇌가 호출.
    UFUNCTION(Callable, Category="AI|Perception")
    void UpdateSenses();

    // 청각 자극 입력 — 발소리/도구/전투음 발생원이 호출한다. 자극 기억에
    // Hearing 으로 기록하고, 소유자에 AwarenessComponent 가 있으면 suspicion 으로 전달한다.
    UFUNCTION(Callable, Category="AI|Perception")
    void RecordHearing(AActor* Source, const FVector& Location, float Strength);

    // 분류된 청각 자극. SoundClass: 0=Normal(발소리), 1=Important(사기그릇/휘슬/전투음).
    UFUNCTION(Callable, Category="AI|Perception")
    void RecordHearingClassified(AActor* Source, const FVector& Location, float Strength, int32 SoundClass);

    UFUNCTION(Pure, Category="AI|Perception")
    bool CanSeeTarget() const { return bCanSeeTarget; }
    UFUNCTION(Pure, Category="AI|Perception")
    bool HasLineOfSight() const { return bHasLineOfSight; }

    UPROPERTY(Edit, Save, Category="AI|Perception", DisplayName="Field Of View Degrees", Min=10.0f, Max=360.0f, Speed=1.0f)
    float FieldOfViewDegrees = 120.0f;

    UPROPERTY(Edit, Save, Category="AI|Perception", DisplayName="Sight Range", Min=0.0f, Max=2000.0f, Speed=0.5f)
    float SightRange = 45.0f;

    UPROPERTY(Edit, Save, Category="AI|Perception", DisplayName="Proximity Range", Min=0.0f, Max=200.0f, Speed=0.1f)
    float ProximityRange = 5.0f;

    UPROPERTY(Edit, Save, Category="AI|Perception", DisplayName="Stimulus Memory Ticks", Min=1.0f, Max=600.0f, Speed=1.0f)
    int32 StimulusMemoryTicks = 90;

    // 벽 너머 차단 — CanSee 가 거리·FOV 외에 가시선(static 충돌) 레이캐스트도 통과해야 true.
    UPROPERTY(Edit, Save, Category="AI|Perception", DisplayName="Require Line Of Sight")
    bool bRequireLineOfSight = true;
    UPROPERTY(Edit, Save, Category="AI|Perception", DisplayName="Eye Height", Min=0.0f, Max=5.0f, Speed=0.05f)
    float EyeHeight = 1.6f;

    // ── 디버거/내부용 직접 접근 ──
    const TArray<FAISenseStimulus>& GetStimuli() const { return Stimuli; }

private:
    void RecordStimulus(EAISenseType Type, AActor* Source, const FVector& Location, float Strength);
    void AgeStimuli();                       // 매 UpdateSenses 마다 노화/만료 — 감지 여부와 무관
    bool ComputeLineOfSight(AActor* Target); // owner 눈 → target 사이 static 차단 검사

    TArray<FAISenseStimulus> Stimuli;
    bool                     bCanSeeTarget   = false;
    bool                     bHasLineOfSight = false;
};
