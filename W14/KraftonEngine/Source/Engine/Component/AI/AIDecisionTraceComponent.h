#pragma once

#include "Component/ActorComponent.h"
#include "Core/Types/CoreTypes.h"
#include "Object/FName.h"

#include "Source/Engine/Component/AI/AIDecisionTraceComponent.generated.h"

// =============================================================================
//  UAIDecisionTraceComponent — 의사결정 트레이스 레코더 (엔진 코어 메커니즘)
// =============================================================================
//  보고서 "Decision Trace / Combat Trace Recorder". 매 think 스텝의 결정을
//  순환 버퍼에 기록해 AI 디버거가 시간순 로그로 보여 준다. 무엇을 후보로 두었고
//  (top-3 점수), 무엇을 골랐으며, 그때 상태/자극이 어땠는지를 남긴다.
// =============================================================================

struct FDecisionTraceRecord
{
    int64 Tick          = 0;
    FName State;
    FName Chosen;
    FName TopName[3];
    float TopScore[3]   = { 0.0f, 0.0f, 0.0f };
    int32 CandidateCount = 0;
    int32 StimulusCount  = 0;
};

// 가장 최근 결정의 "전체" 후보/점수 스냅샷. top-3 만으로는 점수 막대/히트맵을 그릴 수 없어,
// 디버거가 한 프레임의 모든 후보를 점수순으로 나열할 수 있도록 별도로 보관한다.
struct FDecisionCandidate
{
    FName Name;
    float Score = 0.0f;
};

UCLASS()
class UAIDecisionTraceComponent : public UActorComponent
{
public:
    GENERATED_BODY()
    UAIDecisionTraceComponent()           = default;
    ~UAIDecisionTraceComponent() override = default;

    // Lua Blueprint 정책이 후보와 점수를 직접 기록한다. C++은 보관/정렬/표시만 담당한다.
    UFUNCTION(Callable, Category="AI|Trace")
    void BeginDecision(const FName& State);
    UFUNCTION(Callable, Category="AI|Trace")
    void AddCandidate(const FName& ActionName, float Score);
    UFUNCTION(Callable, Category="AI|Trace")
    void CommitDecision(const FName& ChosenAction);
    UFUNCTION(Callable, Category="AI|Trace")
    void RecordDecision();

    UFUNCTION(Callable, Category="AI|Trace")
    void ClearRecords();

    UPROPERTY(Edit, Save, Category="AI|Trace", DisplayName="Max Records", Min=8.0f, Max=2048.0f, Speed=1.0f)
    int32 MaxRecords = 256;

    // ── 디버거/내부용 직접 접근 (가장 최근이 front) ──
    const TArray<FDecisionTraceRecord>& GetRecords() const { return Records; }

    // 가장 최근 결정의 전체 후보(점수 내림차순), 채택 행동, 상태. 점수 막대/히트맵용.
    const TArray<FDecisionCandidate>& GetLastCandidates() const { return LastCandidates; }
    const FName&                      GetLastChosen() const { return LastChosen; }
    const FName&                      GetLastState() const { return LastState; }

private:
    struct FPendingCandidate
    {
        FName ActionName;
        float Score = 0.0f;
    };

    TArray<FDecisionTraceRecord> Records;
    TArray<FPendingCandidate>    PendingCandidates;
    TArray<FDecisionCandidate>   LastCandidates; // 직전 결정의 전체 후보(점수 내림차순)
    FName                        LastChosen = FName::None;
    FName                        LastState  = FName::None;
    FName                        PendingState = FName::None;
    int64                        NextTick = 0;
};
