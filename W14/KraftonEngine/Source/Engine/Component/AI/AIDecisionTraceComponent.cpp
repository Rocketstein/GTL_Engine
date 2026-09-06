#include "Component/AI/AIDecisionTraceComponent.h"

#include "Component/AI/AIPerceptionComponent.h"
#include "GameFramework/AActor.h"

#include <algorithm>

void UAIDecisionTraceComponent::ClearRecords()
{
    Records.clear();
    PendingCandidates.clear();
}

void UAIDecisionTraceComponent::BeginDecision(const FName& State)
{
    PendingState = State;
    PendingCandidates.clear();
}

void UAIDecisionTraceComponent::AddCandidate(const FName& ActionName, float Score)
{
    if (!ActionName.IsValid())
    {
        return;
    }
    FPendingCandidate Candidate;
    Candidate.ActionName = ActionName;
    Candidate.Score = Score;
    PendingCandidates.push_back(Candidate);
}

void UAIDecisionTraceComponent::CommitDecision(const FName& ChosenAction)
{
    FDecisionTraceRecord Record;
    Record.Tick = NextTick++;
    Record.State = PendingState;
    Record.Chosen = ChosenAction;
    Record.CandidateCount = static_cast<int32>(PendingCandidates.size());

    TArray<int32> Order;
    Order.reserve(PendingCandidates.size());
    for (int32 Index = 0; Index < static_cast<int32>(PendingCandidates.size()); ++Index)
    {
        Order.push_back(Index);
    }
    std::sort(Order.begin(), Order.end(), [this](int32 A, int32 B)
    {
        return PendingCandidates[A].Score > PendingCandidates[B].Score;
    });
    for (int32 Slot = 0; Slot < 3 && Slot < static_cast<int32>(Order.size()); ++Slot)
    {
        const FPendingCandidate& Candidate = PendingCandidates[Order[Slot]];
        Record.TopName[Slot] = Candidate.ActionName;
        Record.TopScore[Slot] = Candidate.Score;
    }

    // 디버거용: 직전 결정의 "전체" 후보를 점수 내림차순으로 스냅샷한다(top-3 와 별개).
    LastCandidates.clear();
    LastCandidates.reserve(Order.size());
    for (int32 Index : Order)
    {
        FDecisionCandidate Snapshot;
        Snapshot.Name = PendingCandidates[Index].ActionName;
        Snapshot.Score = PendingCandidates[Index].Score;
        LastCandidates.push_back(Snapshot);
    }
    LastChosen = ChosenAction;
    LastState = PendingState;

    if (AActor* Owner = GetOwner())
    {
        if (UAIPerceptionComponent* Perception = Owner->GetComponentByClass<UAIPerceptionComponent>())
        {
            Record.StimulusCount = static_cast<int32>(Perception->GetStimuli().size());
        }
    }

    Records.insert(Records.begin(), Record);
    const int32 Limit = MaxRecords > 0 ? MaxRecords : 0;
    while (static_cast<int32>(Records.size()) > Limit)
    {
        Records.pop_back();
    }

    PendingCandidates.clear();
    PendingState = FName::None;
}

void UAIDecisionTraceComponent::RecordDecision()
{
    CommitDecision(FName::None);
}
