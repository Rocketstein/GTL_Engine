#pragma once

#include "Component/ActorComponent.h"
#include "Core/Types/CoreTypes.h"
#include "Math/Vector.h"
#include "Object/FName.h"
#include "Object/Ptr/WeakObjectPtr.h"

class AActor;

#include "Source/Engine/Component/AI/AIBlackboardComponent.generated.h"

// =============================================================================
//  UAIBlackboardComponent — 적 AI 공용 기억 저장소 (엔진 코어 메커니즘)
// =============================================================================
//  보고서 "Enemy Blackboard" 계층. 센서/전투/스쿼드 계층이 쓰고, 의사결정(Lua
//  Blueprint)·Utility 선택기·디버거가 읽는 단일 진실 공급원. 컴포넌트 자신은
//  "정책"을 갖지 않는다 — 순수 데이터 + 접근자뿐.
//
//  설계: 이름 기반 float/bool/vector 스토어(결정적 순회용 배열) + 최근 자기행동
//  ring buffer + 마지막 감지/청취 위치. FName 해시 의존을 피해 디버거가 안정적
//  순서로 키를 나열할 수 있도록 배열 백킹을 쓴다.
// =============================================================================

// 디버거가 키/값을 그대로 나열할 수 있는 경량 엔트리.
struct FBlackboardFloatEntry
{
    FName Key;
    float Value = 0.0f;
};

struct FBlackboardBoolEntry
{
    FName Key;
    bool  Value = false;
};

struct FBlackboardVectorEntry
{
    FName   Key;
    FVector Value = FVector::ZeroVector;
};

UCLASS()
class UAIBlackboardComponent : public UActorComponent
{
public:
    GENERATED_BODY()
    UAIBlackboardComponent()           = default;
    ~UAIBlackboardComponent() override = default;

    // ── float 키/값 (거리/각도/체간/체력/위협/페이즈 등) ──
    UFUNCTION(Callable, Category="AI|Blackboard")
    void SetFloat(const FName& Key, float Value);
    UFUNCTION(Pure, Category="AI|Blackboard")
    float GetFloat(const FName& Key) const;
    UFUNCTION(Pure, Category="AI|Blackboard")
    bool HasFloat(const FName& Key) const;

    // ── bool 키/값 (HasTarget/CanSee 등) ──
    UFUNCTION(Callable, Category="AI|Blackboard")
    void SetBool(const FName& Key, bool Value);
    UFUNCTION(Pure, Category="AI|Blackboard")
    bool GetBool(const FName& Key) const;

    // ── vector 키/값 ──
    UFUNCTION(Callable, Category="AI|Blackboard")
    void SetVector(const FName& Key, const FVector& Value);
    UFUNCTION(Pure, Category="AI|Blackboard")
    FVector GetVector(const FName& Key) const;

    // ── 타깃 ──
    UFUNCTION(Callable, Category="AI|Blackboard")
    void SetTarget(AActor* InTarget);
    UFUNCTION(Pure, Category="AI|Blackboard")
    AActor* GetTarget() const { return Target.Get(); }

    // ── 최근 자기 행동 ring buffer (반복 패널티 입력) ──
    UFUNCTION(Callable, Category="AI|Blackboard")
    void PushRecentMove(const FName& MoveId);
    UFUNCTION(Pure, Category="AI|Blackboard")
    int32 GetRecentMoveRepeat(const FName& MoveId) const;

    // ── 감지/청취 위치 기억 ──
    UFUNCTION(Callable, Category="AI|Blackboard")
    void SetLastSeenLocation(const FVector& Location) { LastSeenLocation = Location; }
    UFUNCTION(Pure, Category="AI|Blackboard")
    FVector GetLastSeenLocation() const { return LastSeenLocation; }
    UFUNCTION(Callable, Category="AI|Blackboard")
    void SetLastHeardLocation(const FVector& Location) { LastHeardLocation = Location; }
    UFUNCTION(Pure, Category="AI|Blackboard")
    FVector GetLastHeardLocation() const { return LastHeardLocation; }

    UFUNCTION(Callable, Category="AI|Blackboard")
    void ResetRuntime();

    UPROPERTY(Edit, Save, Category="AI|Blackboard", DisplayName="Recent Move Memory", Min=0.0f, Max=16.0f, Speed=1.0f)
    int32 RecentMoveMemory = 4;

    // ── 디버거/내부용 직접 접근 (리플렉션 거치지 않음) ──
    const TArray<FBlackboardFloatEntry>&  GetFloatEntries() const { return FloatEntries; }
    const TArray<FBlackboardBoolEntry>&   GetBoolEntries() const { return BoolEntries; }
    const TArray<FBlackboardVectorEntry>& GetVectorEntries() const { return VectorEntries; }
    const TArray<FName>&                  GetRecentMoves() const { return RecentMoves; }
    uint32                                GetChangeVersion() const { return ChangeVersion; }

private:
    int32 FindFloat(const FName& Key) const;
    int32 FindBool(const FName& Key) const;
    int32 FindVector(const FName& Key) const;

    TArray<FBlackboardFloatEntry>  FloatEntries;
    TArray<FBlackboardBoolEntry>   BoolEntries;
    TArray<FBlackboardVectorEntry> VectorEntries;
    TArray<FName>                  RecentMoves;
    TWeakObjectPtr<AActor>         Target;
    FVector                        LastSeenLocation  = FVector::ZeroVector;
    FVector                        LastHeardLocation = FVector::ZeroVector;
    uint32                         ChangeVersion     = 0;
};
