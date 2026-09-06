#pragma once

#include "AI/BehaviorTree/Runtime/BTNode.h"
#include "Object/FName.h"

struct FBTContext;

// 이름 → Brain_* 조건 평가(부수효과 없음). Condition leaf 와 Condition decorator 가 공유한다.
// 지원: CanSeeTarget / InAttackRange / TargetThreatening / IsAlerted / TargetInRecovery /
//       HasLineOfSight / IsInProximity / IsUnaware. 미지원 이름 / Enemy 없음 → false.
bool EvaluateBrainCondition(const FBTContext& Context, const FName& Name);

// =============================================================================
//  Phase 0 Task / Condition 카탈로그 — 전부 AEnemyCharacter::Brain_* 파사드에 위임
// =============================================================================
//  결정=BT, 실행=C++. 노드는 새 로직을 짜지 않고 Brain_* 동사를 호출/질의만 한다.
//  Phase 4 에서 Brain_* 전수로 카탈로그(Strafe/Reposition/Deflect/Investigate + 거리/
//  각도/위협/페이즈 Condition)를 확장한다. Tick 구현은 .cpp 에 둬서 이 헤더가
//  EnemyCharacter.h 에 의존하지 않게 한다.
// =============================================================================

// 타깃을 볼 수 있는가? (Brain_CanSeeTarget)
class FBTCondition_CanSeeTarget : public FBTNode_Base
{
public:
    const char* GetDebugName() const override { return "CanSeeTarget?"; }

protected:
    EBTNodeResult Tick(const FBTContext& Context) override;
};

// 타깃으로 추격. 사정권 안에 들면 Succeeded, 아니면 InProgress. (Brain_Chase)
class FBTTask_Chase : public FBTNode_Base
{
public:
    const char* GetDebugName() const override { return "Chase"; }

protected:
    EBTNodeResult Tick(const FBTContext& Context) override;
};

// 선택된 공격 실행. 공격 진행 중이면 InProgress. (Brain_FaceTarget + Brain_PlaySelectedAttack)
class FBTTask_Attack : public FBTNode_Base
{
public:
    const char* GetDebugName() const override { return "Attack"; }

protected:
    EBTNodeResult Tick(const FBTContext& Context) override;
};

// 폴백 대기. (Brain_Idle) — 다른 후보가 없을 때 계속 점유.
class FBTTask_Idle : public FBTNode_Base
{
public:
    const char* GetDebugName() const override { return "Idle"; }

protected:
    EBTNodeResult Tick(const FBTContext& Context) override;
};

// 타깃 기준 게걸음. (Brain_Strafe) — 중립 거리 싸움. 계속 점유(InProgress).
class FBTTask_Strafe : public FBTNode_Base
{
public:
    const char* GetDebugName() const override { return "Strafe"; }

protected:
    EBTNodeResult Tick(const FBTContext& Context) override;
};

// 링 슬롯/간격 재배치. (Brain_Reposition) — 계속 점유.
class FBTTask_Reposition : public FBTNode_Base
{
public:
    const char* GetDebugName() const override { return "Reposition"; }

protected:
    EBTNodeResult Tick(const FBTContext& Context) override;
};

// 마지막 목격/소음 위치로 수색 이동. (Brain_Investigate) — 계속 점유.
class FBTTask_Investigate : public FBTNode_Base
{
public:
    const char* GetDebugName() const override { return "Investigate"; }

protected:
    EBTNodeResult Tick(const FBTContext& Context) override;
};

// 사정권 안인가? (Brain_GetDistance <= Brain_GetAttackRange)
class FBTCondition_InAttackRange : public FBTNode_Base
{
public:
    const char* GetDebugName() const override { return "InAttackRange?"; }

protected:
    EBTNodeResult Tick(const FBTContext& Context) override;
};

// 타깃이 공격을 커밋했고 사정권인가(탄기/회피 근거). (Brain_TargetThreatening)
class FBTCondition_TargetThreatening : public FBTNode_Base
{
public:
    const char* GetDebugName() const override { return "TargetThreatening?"; }

protected:
    EBTNodeResult Tick(const FBTContext& Context) override;
};

// 전투 확정(Alert) 상태인가. (Brain_IsAlerted) — 평소엔 추격/공격을 시작하지 않게 게이팅.
class FBTCondition_IsAlerted : public FBTNode_Base
{
public:
    const char* GetDebugName() const override { return "IsAlerted?"; }

protected:
    EBTNodeResult Tick(const FBTContext& Context) override;
};


// ── 보스 전용 의사결정 Task ────────────────────────────────────────────────
// 공격 카탈로그 전체를 점수화해 이번 틱에 실행할 공격을 SelectedAttack 으로 확정한다.
// 거리/각도/페이즈/타깃 후딜/플레이어 공격 커밋/최근 반복/전술 태그를 함께 본다.
class FBTTask_SelectAttack : public FBTNode_Base
{
public:
    const char* GetDebugName() const override { return "SelectAttack"; }

protected:
    EBTNodeResult Tick(const FBTContext& Context) override;
};

// 플레이어의 공격 커밋을 보고 확률·상황 기반으로 탄기/가드를 연다.
// 단순 조건 노드가 아니라 실제 반응 행동까지 처리하므로 성공 시 상위 Selector 를 선점한다.
class FBTTask_ReactiveDeflect : public FBTNode_Base
{
public:
    const char* GetDebugName() const override { return "ReactiveDeflect"; }

protected:
    EBTNodeResult Tick(const FBTContext& Context) override;
};

// 공격할 수 없을 때 거리/체력/페이즈/직전 공격 결과를 보고 추격·횡이동·후퇴를 고른다.
// BT 의 하위 leaf 를 여러 개 늘어놓는 대신 전술 이동 선택을 하나의 안정적 Task 로 캡슐화한다.
class FBTTask_TacticalMove : public FBTNode_Base
{
public:
    const char* GetDebugName() const override { return "TacticalMove"; }

protected:
    EBTNodeResult Tick(const FBTContext& Context) override;
};

// ── 제네릭(데이터 구동) 노드 — C++ 클래스 추가 없이 자극/동작 연결 ──

// VerbId(그래프 노드 KeyValue)로 Brain_* 동사를 호출. 한 노드로 여러 동작 표현.
// 이동류(Chase/Strafe/...)는 InProgress, 단발(PlaySelectedAttack/AcquireTarget)은 결과 반환.
class FBTTask_BrainVerb : public FBTNode_Base
{
public:
    FName VerbId;
    const char* GetDebugName() const override { return "BrainVerb"; }

protected:
    EBTNodeResult Tick(const FBTContext& Context) override;
};

// 블랙보드 bool 키 비교(leaf). 외부 자극(노티파이/Lua/지각)이 SetBool 한 키를 읽어 분기.
class FBTCondition_Blackboard : public FBTNode_Base
{
public:
    FName Key;
    bool  bExpected = true;
    const char* GetDebugName() const override { return "Blackboard?"; }

protected:
    EBTNodeResult Tick(const FBTContext& Context) override;
};

// ── 범용 Task (특정 게임플레이 비의존 — 블랙보드 + OwnerActor + ctx 만 사용) ──

// Duration 초 동안 대기 후 성공. 그동안 InProgress.
class FBTTask_Wait : public FBTNode_Base
{
public:
    float Duration = 1.0f;
    const char* GetDebugName() const override { return "Wait"; }

protected:
    void          OnEnter(const FBTContext& Context) override;
    EBTNodeResult Tick(const FBTContext& Context) override;

private:
    float Elapsed = 0.0f;
};

// 블랙보드 bool 키에 값 쓰기(외부/내부 상태 신호). 항상 성공.
class FBTTask_SetBlackboardBool : public FBTNode_Base
{
public:
    FName Key;
    bool  Value = true;
    const char* GetDebugName() const override { return "Set BB Bool"; }

protected:
    EBTNodeResult Tick(const FBTContext& Context) override;
};

// 블랙보드 float 키에 값 쓰기. 항상 성공.
class FBTTask_SetBlackboardFloat : public FBTNode_Base
{
public:
    FName Key;
    float Value = 0.0f;
    const char* GetDebugName() const override { return "Set BB Float"; }

protected:
    EBTNodeResult Tick(const FBTContext& Context) override;
};

// 흐름 제어: 항상 성공 / 항상 실패.
class FBTTask_Succeed : public FBTNode_Base
{
public:
    const char* GetDebugName() const override { return "Succeed"; }

protected:
    EBTNodeResult Tick(const FBTContext& /*Context*/) override { return EBTNodeResult::Succeeded; }
};

class FBTTask_Fail : public FBTNode_Base
{
public:
    const char* GetDebugName() const override { return "Fail"; }

protected:
    EBTNodeResult Tick(const FBTContext& /*Context*/) override { return EBTNodeResult::Failed; }
};

// 디버그 로그 출력 후 성공.
class FBTTask_Log : public FBTNode_Base
{
public:
    FString Message;
    const char* GetDebugName() const override { return "Log"; }

protected:
    EBTNodeResult Tick(const FBTContext& Context) override;
};
