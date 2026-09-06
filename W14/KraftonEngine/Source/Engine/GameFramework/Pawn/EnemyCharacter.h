#pragma once

#include "GameFramework/Pawn/BaseCombatCharacter.h"
#include "Component/Combat/CombatTypes.h"
#include "AI/CombatClock.h"
#include "Object/Ptr/WeakObjectPtr.h"

class AAIController;
class UClass;
class UEnemyAIBrainComponent;
class UEnemyAttackComponent;
class UEnemyHitComponent;
class ULuaScriptComponent;
class ULuaBlueprintComponent;
class UAIBlackboardComponent;
class UAIPerceptionComponent;
class UAIDecisionTraceComponent;
class UCombatMoveComponent;
class UExecutionComponent;
class UAwarenessComponent;
class UBehaviorTreeComponent;
class FArchive;

#include "Source/Engine/GameFramework/Pawn/EnemyCharacter.generated.h"

UCLASS()
class AEnemyCharacter : public ABaseCombatCharacter
{
public:
	GENERATED_BODY()
	AEnemyCharacter() = default;
	~AEnemyCharacter() override = default;

	void BeginPlay() override;
	void InitDefaultComponents(const FString& SkeletalMeshFileName) override;
	void InitDefaultComponents(const FString& SkeletalMeshFileName, const FString& ScriptFile);
	void PostDuplicate() override;
	void OnPostLoad(FArchive& Ar) override;

	UFUNCTION(Pure, Category="Enemy|Components")
	UEnemyAIBrainComponent* GetAIBrainComponent() const { return AIBrainComponent; }
	UFUNCTION(Pure, Category="Enemy|Components")
	UEnemyAttackComponent* GetAttackComponent() const { return AttackComponent; }
	UFUNCTION(Pure, Category="Enemy|Components")
	UEnemyHitComponent* GetHitComponent() const { return HitComponent; }
	UFUNCTION(Pure, Category="Enemy|Components")
	ULuaScriptComponent* GetLuaScriptComponent() const { return LuaScriptComponent; }
	UFUNCTION(Pure, Category="Enemy|Components")
	ULuaBlueprintComponent* GetBrainBlueprint() const { return BrainBlueprint; }

	// ── 세키로식 전투 AI 계층 (엔진 코어 컴포넌트) ──
	UFUNCTION(Pure, Category="Enemy|AICore")
	UAIBlackboardComponent* GetBlackboard() const { return Blackboard; }
	UFUNCTION(Pure, Category="Enemy|AICore")
	UAIPerceptionComponent* GetPerception() const { return Perception; }
	UFUNCTION(Pure, Category="Enemy|AICore")
	UAIDecisionTraceComponent* GetDecisionTrace() const { return DecisionTrace; }
	UFUNCTION(Pure, Category="Enemy|AICore")
	UCombatMoveComponent* GetCombatMove() const { return CombatMove; }
	UFUNCTION(Pure, Category="Enemy|AICore")
	UExecutionComponent* GetExecution() const { return Execution; }
	UFUNCTION(Pure, Category="Enemy|AICore")
	UAwarenessComponent* GetAwareness() const { return Awareness; }
	UFUNCTION(Pure, Category="Enemy|AICore")
	UBehaviorTreeComponent* GetBehaviorTree() const { return BehaviorTree.Get(); }
	UFUNCTION(Pure, Category="Enemy|AICore")
	int32 GetAIPhase() const { return GetCurrentAIPhase(); }

	UFUNCTION(Callable, Category="Enemy|AI")
	AAIController* SpawnDefaultAIController();
	UFUNCTION(Pure, Category="Enemy|AI")
	AAIController* GetEnemyAIController() const;

	UFUNCTION(Callable, Category="Enemy|Movement")
	void MoveToTarget(float Scale = 1.0f);
	UFUNCTION(Callable, Category="Enemy|Movement")
	void FaceTarget(float DeltaTime, float OverrideYawRate = -1.0f);
	UFUNCTION(Callable, Category="Enemy|Movement")
	void StopEnemyMovement();
	// 중립 거리 싸움용 동사 — Lua 두뇌가 호출한다. 타깃 기준 좌/우 게걸음, 타깃 반대로 후퇴.
	UFUNCTION(Callable, Category="Enemy|Movement")
	void StrafeAroundTarget(float Scale = 1.0f, bool bClockwise = true);
	UFUNCTION(Callable, Category="Enemy|Movement")
	void RetreatFromTarget(float Scale = 1.0f);
	UFUNCTION(Callable, Category="Enemy|Movement")
	bool RequestMoveToTarget(float AcceptanceRadius = -1.0f, bool bUsePathfinding = true);
	UFUNCTION(Callable, Category="Enemy|Movement")
	bool RequestMoveToActor(AActor* Target, float AcceptanceRadius = -1.0f, bool bUsePathfinding = true);
	UFUNCTION(Callable, Category="Enemy|Movement")
	bool RequestMoveToLocation(const FVector& Location, float AcceptanceRadius = -1.0f, bool bUsePathfinding = true);
	UFUNCTION(Pure, Category="Enemy|Movement")
	bool IsPathFollowing() const;

	UFUNCTION(Callable, Category="Enemy|Attack")
	bool PlayAttackMontage(const FEnemyAttackData& Attack);
	UFUNCTION(Pure, Category="Enemy|Attack")
	bool HasCurrentAttack() const { return bCurrentAttackActive && CurrentAttack.AttackName.IsValid(); }
	UFUNCTION(Pure, Category="Enemy|Attack")
	bool HasCurrentAttackDamagedActor(AActor* Target) const;
	UFUNCTION(Callable, Category="Enemy|Attack")
	bool MarkCurrentAttackDamagedActor(AActor* Target);
	UFUNCTION(Callable, Category="Enemy|Attack")
	bool ApplyCurrentAttackDamageToActor(AActor* Target, const FVector& HitLocation);

	// ── Brain 동사 (Lua Blueprint 정책이 호출하는 파사드 — 결정은 Lua, 실행은 C++) ──
	// 모두 무인자/단순 반환이라 블루프린트 그래프에서 깔끔히 배선된다.
	UFUNCTION(Callable, Category="Enemy|Brain")
	void Brain_Sense();
	UFUNCTION(Callable, Category="Enemy|Brain")
	bool Brain_AcquireTarget();
	UFUNCTION(Callable, Category="Enemy|Brain")
	void Brain_FaceTarget();
	UFUNCTION(Pure, Category="Enemy|Brain")
	float Brain_GetDistance() const;
	UFUNCTION(Pure, Category="Enemy|Brain")
	float Brain_GetAttackRange() const;
	UFUNCTION(Pure, Category="Enemy|Brain")
	float Brain_GetAbsAngle() const;
	UFUNCTION(Pure, Category="Enemy|Brain")
	float Brain_GetVerticalDelta() const;
	UFUNCTION(Pure, Category="Enemy|Brain")
	float Brain_GetSelfHealthRatio() const;
	UFUNCTION(Pure, Category="Enemy|Brain")
	float Brain_GetTargetHealthRatio() const;
	UFUNCTION(Pure, Category="Enemy|Brain")
	float Brain_GetTargetPostureRatio() const;
	UFUNCTION(Pure, Category="Enemy|Brain")
	bool Brain_CanSeeTarget() const;
	UFUNCTION(Pure, Category="Enemy|Brain")
	bool Brain_HasLineOfSight() const;
	UFUNCTION(Pure, Category="Enemy|Brain")
	bool Brain_IsInProximity() const;
	UFUNCTION(Pure, Category="Enemy|Brain")
	int32 Brain_GetPhase() const;
	UFUNCTION(Pure, Category="Enemy|Brain")
	bool Brain_IsBoss() const { return IsBossCharacter(); }
	UFUNCTION(Pure, Category="Enemy|Brain")
	float Brain_GetStateTime() const;
	UFUNCTION(Pure, Category="Enemy|Brain")
	int32 Brain_GetLODLevel() const;

	// 공격 카탈로그 열람/선택 파사드. Lua Blueprint가 점수를 계산하고 C++은 데이터 검증/실행만 수행한다.
	UFUNCTION(Pure, Category="Enemy|Brain|Attack")
	int32 Brain_GetAttackCount() const;
	UFUNCTION(Pure, Category="Enemy|Brain|Attack")
	FName Brain_GetAttackName(int32 Index) const;
	UFUNCTION(Pure, Category="Enemy|Brain|Attack")
	bool Brain_CanUseAttackIndex(int32 Index) const;
	UFUNCTION(Pure, Category="Enemy|Brain|Attack")
	float Brain_GetAttackBaseWeight(int32 Index) const;
	UFUNCTION(Pure, Category="Enemy|Brain|Attack")
	float Brain_GetAttackPriority(int32 Index) const;
	UFUNCTION(Pure, Category="Enemy|Brain|Attack")
	float Brain_GetAttackMinRange(int32 Index) const;
	UFUNCTION(Pure, Category="Enemy|Brain|Attack")
	float Brain_GetAttackMaxRange(int32 Index) const;
	UFUNCTION(Pure, Category="Enemy|Brain|Attack")
	float Brain_GetAttackMaxAbsAngle(int32 Index) const;
	UFUNCTION(Pure, Category="Enemy|Brain|Attack")
	float Brain_GetAttackMaxVerticalDelta(int32 Index) const;
	UFUNCTION(Pure, Category="Enemy|Brain|Attack")
	float Brain_GetAttackRepeatWeightScale(int32 Index) const;
	UFUNCTION(Pure, Category="Enemy|Brain|Attack")
	FName Brain_GetAttackTacticTag(int32 Index) const;
	UFUNCTION(Pure, Category="Enemy|Brain|Attack")
	bool Brain_IsAttackGapCloser(int32 Index) const;
	UFUNCTION(Pure, Category="Enemy|Brain|Attack")
	int32 Brain_GetAttackPerilousType(int32 Index) const;
	UFUNCTION(Pure, Category="Enemy|Brain|Attack")
	bool Brain_AttackRequiresPreviousAttack(int32 Index) const;
	UFUNCTION(Pure, Category="Enemy|Brain|Attack")
	FName Brain_GetAttackRequiredPreviousAttack(int32 Index) const;
	UFUNCTION(Pure, Category="Enemy|Brain|Attack")
	int32 Brain_GetRecentAttackRepeat(const FName& AttackName) const;
	UFUNCTION(Pure, Category="Enemy|Brain|Attack")
	FName Brain_GetLastAttackName() const;
	UFUNCTION(Callable, Category="Enemy|Brain|Attack")
	bool Brain_SetSelectedAttack(const FName& AttackName);
	UFUNCTION(Callable, Category="Enemy|Brain|Attack")
	bool Brain_PlayAttackByName(const FName& AttackName);

	// Lua Blueprint 의사결정 디버그 트레이스. 후보/점수는 Lua가 기록한다.
	UFUNCTION(Callable, Category="Enemy|Brain|Trace")
	void Brain_BeginDecisionTrace();
	UFUNCTION(Callable, Category="Enemy|Brain|Trace")
	void Brain_AddDecisionCandidate(const FName& ActionName, float Score);
	UFUNCTION(Callable, Category="Enemy|Brain|Trace")
	void Brain_CommitDecisionTrace(const FName& ChosenAction);
	UFUNCTION(Callable, Category="Enemy|Brain")
	bool Brain_ConsumeCombatStep();
	UFUNCTION(Pure, Category="Enemy|Brain")
	bool Brain_IsBusy() const;
	// 지정 시간 동안 두뇌를 강제로 busy 로 잠근다(페이즈 전환 연출 등 — 다음 행동이 몽타주를 덮지 못하게).
	UFUNCTION(Callable, Category="Enemy|Brain")
	void LockBrainBusy(float Seconds);
	UFUNCTION(Callable, Category="Enemy|Brain")
	bool Brain_PlaySelectedAttack();
	UFUNCTION(Callable, Category="Enemy|Brain")
	void Brain_Chase();
	UFUNCTION(Callable, Category="Enemy|Brain")
	void Brain_Strafe();
	UFUNCTION(Callable, Category="Enemy|Brain")
	void Brain_Reposition();
	UFUNCTION(Callable, Category="Enemy|Brain")
	void Brain_Idle();

	// ── Phase 2 반응 동사 (체간/탄기/위험공격) ──
	// 타깃이 공격을 커밋했고 사정권이면 탄기 반응을 고려할 근거.
	UFUNCTION(Pure, Category="Enemy|Brain")
	bool Brain_TargetThreatening() const;
	// 타깃이 후딜(Recovery) 중인가 — punish 기회.
	UFUNCTION(Pure, Category="Enemy|Brain")
	bool Brain_TargetInRecovery() const;
	// 타깃이 지금 몽타주(공격/가드/피격 등 능동 동작)를 재생 중인가 — 플레이어가 공격을 마킹하지
	// 않아도 보스가 "상대가 동작 중"임을 감지해 반응형 가드를 열 수 있게 한다.
	UFUNCTION(Pure, Category="Enemy|Brain")
	bool Brain_IsTargetActing() const;
	// 타깃의 활성 위험공격 종류(EPerilousType, 0=None).
	UFUNCTION(Pure, Category="Enemy|Brain")
	int32 Brain_GetTargetPerilous() const;
	// 탄기 윈도우를 연다(방어자 반응). 들어오는 피격이 윈도우 안이면 받아넘긴다.
	UFUNCTION(Callable, Category="Enemy|Brain")
	void Brain_OpenDeflect();
	// 가드(block) 자세에 들어간다 — 정면 피해를 감쇄하되 반사는 없다(패링 아님). 보스 전용 방어.
	UFUNCTION(Callable, Category="Enemy|Brain")
	void Brain_OpenGuard();
	// 회피(백스텝) — 타깃 반대로 빠르게 빠지며 회피 몽타주를 재생. 가드로 못 막는 위험공격
	// (하단/잡기) 대응이나 거리 재설정에 사용. 날렵하게 피하는 성향 표현.
	UFUNCTION(Callable, Category="Enemy|Brain")
	void Brain_Dodge();
	// 백점프 — 타깃 반대로 크게 도약해 거리를 멀리 벌린다(자원 고갈/압박 시 하드 디스인게이지).
	// 강한 후방 대시 + 점프(수직 팝) + 도약 몽타주.
	UFUNCTION(Callable, Category="Enemy|Brain")
	void Brain_LeapBack();
	// 이동 속도에 따라 idle/walk/run 루핑 몽타주를 슬롯에 재생(그래프 모드 유지 → 공격 몽타주와
	// 공존). 브레인이 공격/가드/회피가 아닌 이동·대기 분기에서 매 틱 호출한다. PlayAnimationByPath
	// (단일노드) 로코모션은 몽타주 시스템을 꺼버리므로 쓰지 않는다.
	UFUNCTION(Callable, Category="Enemy|Brain")
	void Brain_DriveLocomotion();
	// 현재 가드 윈도우가 열려 있는가.
	UFUNCTION(Pure, Category="Enemy|Brain")
	bool Brain_IsGuarding() const;
	// 최근 0.35초 내에 실제로 가드로 막았는가 — Lua가 이걸 보고 반격(riposte) 윈도우를 연다.
	UFUNCTION(Pure, Category="Enemy|Brain")
	bool Brain_GuardBlockedRecently() const;

	// ── 지형/공간 인지 동사 (Lua 정책이 후퇴/회피/포지셔닝 판단에 사용) ──
	// 등 뒤로 직선 이동 가능한 여유 거리(벽/장애물까지). 작을수록 코너에 몰림.
	UFUNCTION(Pure, Category="Enemy|Brain")
	float Brain_GetBackClearance() const;
	// 등 뒤 여유가 임계 이하라 후퇴 대신 측면/전진으로 빠져야 하는 상황.
	UFUNCTION(Pure, Category="Enemy|Brain")
	bool Brain_IsCornered() const;
	// 지정 방위(0=전,1=후,2=좌,3=우)로의 여유 거리. 측면 회피 방향 선택에 사용.
	UFUNCTION(Pure, Category="Enemy|Brain")
	float Brain_GetClearanceInDirection(int32 Direction) const;

	// 은신 인지 상태 (AwarenessComponent 가 갱신, Lua 가 행동 게이팅) ──
	// 0=Unaware 1=Suspicious 2=Investigating 3=Alert 4=Searching 5=Returning
	UFUNCTION(Pure, Category="Enemy|Brain")
	int32 Brain_GetAwarenessState() const;
	UFUNCTION(Pure, Category="Enemy|Brain")
	float Brain_GetSuspicion() const;
	// 전투 확정(Alert) 여부 — 평소엔 추격/공격을 시작하지 않고 수상/탐색만.
	UFUNCTION(Pure, Category="Enemy|Brain")
	bool Brain_IsAlerted() const;
	// 미인지(Unaware/Suspicious) — 스텔스 데스블로우 대상 자격.
	UFUNCTION(Pure, Category="Enemy|Brain")
	bool Brain_IsUnaware() const;
	// 마지막 소음/목격 위치로 이동(수색/조사). 경로탐색을 사용.
	UFUNCTION(Callable, Category="Enemy|Brain")
	void Brain_Investigate();

	// ── 공격 문법 분기 신호 (직전 공격 결과) ──
	// 직전 내 공격이 탄기당했는가 — 다음은 가드/이탈로 분기할 근거.
	UFUNCTION(Pure, Category="Enemy|Brain")
	bool Brain_LastAttackWasDeflected() const;
	// 직전 내 공격이 적중했는가 — 콤보/압박으로 이어갈 근거.
	UFUNCTION(Pure, Category="Enemy|Brain")
	bool Brain_LastAttackConnected() const;

	// ── 데스블로우 스톡 (Lua 가 페이즈/연출 트리거로 사용) ──
	UFUNCTION(Pure, Category="Enemy|Brain")
	int32 Brain_GetStocks() const;
	UFUNCTION(Pure, Category="Enemy|Brain")
	bool Brain_IsDeathblowReady() const;

	// ── Phase 3 협동 (공격 토큰) ──
	// 공격 토큰을 시도/획득. 타깃에 이미 동시공격 상한만큼 나가 있으면 false → 지원/재배치.
	UFUNCTION(Callable, Category="Enemy|Brain")
	bool Brain_AcquireAttackToken();
	UFUNCTION(Callable, Category="Enemy|Brain")
	void Brain_ReleaseAttackToken();
	UFUNCTION(Pure, Category="Enemy|Brain")
	int32 Brain_GetSquadSlot() const;
	// 전투 역할: 0 Closer(전열) / 1 Harasser(차열) / 2 Reserve(후열). 링 슬롯에서 파생.
	// Reserve 는 무리하게 파고들지 않고 거리를 유지 → 다수전 화면 가독성↑.
	UFUNCTION(Pure, Category="Enemy|Brain")
	int32 Brain_GetSquadRole() const;

	UPROPERTY(Edit, Save, Category="Enemy|Movement", DisplayName="Can Move")
	bool bCanMove = true;
	UPROPERTY(Edit, Save, Category="Enemy|Movement", DisplayName="Can Rotate")
	bool bCanRotate = true;
	UPROPERTY(Edit, Save, Category="Enemy|Attack", DisplayName="Can Attack")
	bool bCanAttack = true;
	UPROPERTY(Edit, Save, Category="Enemy|Movement", DisplayName="Turn Speed", Min=0.0f, Max=3600.0f, Speed=5.0f)
	float TurnSpeed = 540.0f;
	UPROPERTY(Edit, Save, Category="Enemy|AI", DisplayName="Auto Possess AI")
	bool bAutoPossessAI = true;
	UPROPERTY(Edit, Save, Category="Enemy|AI", DisplayName="AI Controller Class", Type=ClassRef, AllowedClass=AAIController)
	UClass* AIControllerClass = nullptr;
	UPROPERTY(Edit, Save, Category="Enemy|AI", DisplayName="Brain Script", AssetType="Script")
	FString BrainScriptFile = "AI/enemy_brain.lua";
	// 정책을 Lua Blueprint(.uasset)로 구동한다. 비우면 위 Brain Script(raw Lua)를 쓴다.
	// 그래프는 에디터에서 작성/JSON Import 후 .uasset 으로 저장한 것을 가리킨다.
	// 지정 시 ULuaBlueprintComponent 가 그 에셋을 컴파일해 두뇌를 구동하고 raw-lua 경로는 끈다.
	// 블루프린트가 로드/컴파일/구동 불가하면 안전하게 Brain Script 로 폴백한다.
	UPROPERTY(Edit, Save, Category="Enemy|AI", DisplayName="Brain Blueprint", AssetType="ULuaBlueprintAsset")
	FString BrainBlueprintFile = "";
	// 정책을 Behavior Tree(.uasset)로 구동한다(최우선 드라이버). 지정·로드 가능하면 BT 가 두뇌를
	// 구동하고 Blueprint/Script 는 끈다. 비우면 기존 Blueprint→Script 경로로 폴백한다.
	UPROPERTY(Edit, Save, Category="Enemy|AI", DisplayName="Brain Behavior Tree", AssetType="UBehaviorTreeAsset")
	FString BrainBehaviorTreeFile = "";
	UPROPERTY(Edit, Save, Category="Enemy|AI", DisplayName="Target Search Range", Min=0.0f, Max=1000.0f, Speed=0.5f)
	float TargetSearchRange = 16.0f;
	// 은신 인지 게이팅. false면 기존처럼 즉시 전투(하위호환). true면 Awareness 가
	// Alert 가 되기 전에는 추격/공격을 시작하지 않고 수상/탐색만 한다.
	UPROPERTY(Edit, Save, Category="Enemy|AI", DisplayName="Use Awareness Gating")
	bool bUseAwarenessGating = false;
	UPROPERTY(Edit, Save, Category="Enemy|Combat", DisplayName="Default Attack Range", Min=0.0f, Max=1000.0f, Speed=0.1f)
	float DefaultAttackRange = 1.7f;
	// 추격 경로 재탐색 최소 간격(초). Brain_Chase 는 이미 이동 중이면 이 간격이 지나기 전엔 경로를 다시
	// 짜지 않고 기존 경로를 계속 따른다 — 매 틱 재탐색(과도한 길찾기)을 막는다. 0 이면 매 틱 갱신.
	UPROPERTY(Edit, Save, Category="Enemy|AI", DisplayName="Chase Repath Interval", Min=0.0f, Max=2.0f, Speed=0.01f)
	float ChaseRepathInterval = 0.35f;

	// 가드(block) 자세에서 재생할 몽타주(비주얼). Brain_OpenGuard 가 재생한다. null 이면 포즈 없이
	// 가드 윈도우만 열린다(기능은 동작).
	UPROPERTY(Edit, Save, Category="Enemy|Combat", DisplayName="Guard Montage", Type=ObjectRef, AllowedClass=UAnimMontage)
	UAnimMontage* GuardMontage = nullptr;

	// 회피(백스텝) 몽타주. Brain_Dodge 가 재생한다(예: Run Back).
	UPROPERTY(Edit, Save, Category="Enemy|Combat", DisplayName="Dodge Montage", Type=ObjectRef, AllowedClass=UAnimMontage)
	UAnimMontage* DodgeMontage = nullptr;

	// 회피 시 타깃 반대로 빠지는 대시 세기.
	UPROPERTY(Edit, Save, Category="Enemy|Combat", DisplayName="Dodge Dash Scale", Min=0.0f, Max=5.0f, Speed=0.05f)
	float DodgeDashScale = 2.2f;

	// ── 패링 리코일 ──
	// 공격이 패링당하면 진행 중이던(점프/갭클로저/킥 제외) 공격 몽타주의 "현재 시점"에서
	// 아주 짧은 구간만 뒤로 감아 친 칼이 튕겨 나오는 리코일을 만든다. 0 이면 비활성.
	// 실제 누적은 raw time 기준이라 전역 슬로모에 눌리지 않는다.
	UPROPERTY(Edit, Save, Category="Enemy|Combat", DisplayName="Parry Recoil Duration", Min=0.0f, Max=1.0f, Speed=0.01f)
	float ParryRecoilDuration = 0.30f;

	// 패링 순간에서 얼마나 과거로 되감을지(초). 0.10초는 대략 60fps 기준 6프레임, 30fps 기준 3프레임이다.
	// 이 값만큼만 뒤로 가며, 몽타주 섹션 0초까지 무조건 돌아가지 않도록 보호한다.
	UPROPERTY(Edit, Save, Category="Enemy|Combat", DisplayName="Parry Recoil Rewind Seconds", Min=0.0f, Max=0.5f, Speed=0.005f)
	float ParryRecoilRewindSeconds = 0.10f;

	// 안전용 속도 상한. 최종 되감기 거리는 ParryRecoilRewindSeconds 가 우선이며,
	// 이 값은 Duration 대비 과도한 되감기만 막는 보조 제한으로 사용한다.
	UPROPERTY(Edit, Save, Category="Enemy|Combat", DisplayName="Parry Recoil Max Reverse Rate", Min=0.0f, Max=60.0f, Speed=0.5f)
	float ParryRecoilMaxReverseRate = 20.0f;

	// 백점프(Brain_LeapBack) 비주얼 몽타주(폴백). 보통은 아래 JumpUp/Down 으로 공중 단계가 구동된다.
	UPROPERTY(Edit, Save, Category="Enemy|Combat", DisplayName="Leap Montage", Type=ObjectRef, AllowedClass=UAnimMontage)
	UAnimMontage* LeapMontage = nullptr;

	// 세분화된 점프: 공중 상승(JumpUp)/하강(JumpDown) 단계별 몽타주. Brain_DriveLocomotion 이
	// 수직 속도로 골라 재생한다(상승=Up, 하강=Down).
	UPROPERTY(Edit, Save, Category="Enemy|Locomotion", DisplayName="Jump Up Montage", Type=ObjectRef, AllowedClass=UAnimMontage)
	UAnimMontage* JumpUpMontage = nullptr;
	// 공중 낙하(상승과 착지 사이의 체공/낙하 단계).
	UPROPERTY(Edit, Save, Category="Enemy|Locomotion", DisplayName="Jump Fall Montage", Type=ObjectRef, AllowedClass=UAnimMontage)
	UAnimMontage* JumpFallMontage = nullptr;
	UPROPERTY(Edit, Save, Category="Enemy|Locomotion", DisplayName="Jump Down Montage", Type=ObjectRef, AllowedClass=UAnimMontage)
	UAnimMontage* JumpDownMontage = nullptr;

	// 백점프 후방 대시 세기(구버전 폴백 — 현재는 아래 임펄스 방식 사용).
	UPROPERTY(Edit, Save, Category="Enemy|Combat", DisplayName="Leap Dash Scale", Min=0.0f, Max=10.0f, Speed=0.1f)
	float LeapDashScale = 5.0f;

	// 백점프 수평 임펄스 세기(타깃 반대 방향, m/s). 공중에선 수평 마찰이 없어 그대로 거리로 환산됨.
	// 거대 보스(5×)는 큰 도약이 필요 → 기본/상한 상향(패널에서 더 크게 튜닝 가능).
	UPROPERTY(Edit, Save, Category="Enemy|Combat", DisplayName="Leap Horizontal Force", Min=0.0f, Max=100.0f, Speed=0.5f)
	float LeapHorizontalForce = 40.0f;

	// 백점프 수직 도약력(m/s). JumpZVelocity 보다 크게 줘서 발사각을 세워 낮은 턱/단차를 넘는다.
	// 0 이하이면 캐릭터 JumpZVelocity 로 폴백. 너무 크면 체공이 길어지니(거리=Z 비례) 적당히.
	UPROPERTY(Edit, Save, Category="Enemy|Combat", DisplayName="Leap Vertical Force", Min=0.0f, Max=40.0f, Speed=0.5f)
	float LeapVerticalForce = 16.0f;

	// 백점프 준비동작(프렙) 시간(초). JumpUp 몽타주의 도약 직전까지 기다렸다가 실제로 발사한다
	// (준비 중에 떠버리는 문제 방지). 애님의 takeoff 프레임에 맞춰 조절.
	UPROPERTY(Edit, Save, Category="Enemy|Combat", DisplayName="Leap Prep Delay", Min=0.0f, Max=1.0f, Speed=0.01f)
	float LeapPrepDelay = 0.25f;

	// 백점프 착지 안정화: 도약 전에 등 뒤 부채꼴을 네비메시로 스캔해, 연속된 walkable 회랑이 있는
	// 방향/거리로만 도약한다(낭떠러지·맵 밖·장애물 위 착지 방지). 안정 지형이 없으면 제자리 팝만 한다.
	UPROPERTY(Edit, Save, Category="Enemy|Combat", DisplayName="Validate Leap Landing")
	bool bValidateLeapLanding = true;
	// 안정 회랑 끝에서 안쪽으로 당길 여유(m). 가장자리에 아슬아슬하게 착지하지 않도록 도달거리를 줄인다.
	UPROPERTY(Edit, Save, Category="Enemy|Combat", DisplayName="Leap Landing Safety Margin", Min=0.0f, Max=10.0f, Speed=0.05f)
	float LeapLandingSafetyMargin = 0.75f;
	// 도약 회랑에서 허용하는 셀 간 최대 높이차(m). 이보다 큰 턱/낭떠러지가 나오면 회랑을 거기서 끊는다.
	UPROPERTY(Edit, Save, Category="Enemy|Combat", DisplayName="Leap Landing Max Step Height", Min=0.1f, Max=20.0f, Speed=0.05f)
	float LeapLandingMaxStepHeight = 1.0f;

	// ── 몽타주 기반 로코모션 (그래프 모드에서 walk/run + 공격 몽타주 공존) ──
	// 각 몽타주는 NextSection=자기자신으로 무한 루프하도록 저작해야 한다.
	UPROPERTY(Edit, Save, Category="Enemy|Locomotion", DisplayName="Loco Idle Montage", Type=ObjectRef, AllowedClass=UAnimMontage)
	UAnimMontage* LocoIdleMontage = nullptr;
	UPROPERTY(Edit, Save, Category="Enemy|Locomotion", DisplayName="Loco Walk Montage", Type=ObjectRef, AllowedClass=UAnimMontage)
	UAnimMontage* LocoWalkMontage = nullptr;
	UPROPERTY(Edit, Save, Category="Enemy|Locomotion", DisplayName="Loco Run Montage", Type=ObjectRef, AllowedClass=UAnimMontage)
	UAnimMontage* LocoRunMontage = nullptr;
	UPROPERTY(Edit, Save, Category="Enemy|Locomotion", DisplayName="Loco Walk Speed Threshold", Min=0.0f, Max=100.0f, Speed=0.05f)
	float LocoWalkSpeedThreshold = 0.2f;
	UPROPERTY(Edit, Save, Category="Enemy|Locomotion", DisplayName="Loco Run Speed Threshold", Min=0.0f, Max=100.0f, Speed=0.05f)
	float LocoRunSpeedThreshold = 4.0f;

	// 등 뒤 여유가 이 거리 이하이면 Brain_IsCornered 가 true — 후퇴 대신 측면/전진을 선택하게 한다.
	UPROPERTY(Edit, Save, Category="Enemy|Combat", DisplayName="Cornered Clearance Threshold", Min=0.0f, Max=20.0f, Speed=0.05f)
	float CorneredClearanceThreshold = 2.5f;


	// ── Phase 3 협동 / Phase 4 LOD 튜닝 ──
	UPROPERTY(Edit, Save, Category="Enemy|Squad", DisplayName="Max Simultaneous Attackers", Min=0.0f, Max=16.0f, Speed=1.0f)
	int32 SquadMaxSimultaneousAttackers = 2;
	UPROPERTY(Edit, Save, Category="Enemy|Squad", DisplayName="Squad Token Duration", Min=0.0f, Max=10.0f, Speed=0.05f)
	float SquadTokenDuration = 1.0f;
	// 토큰 없는 적은 타깃 중심이 아니라 (AttackRange * 이 비율) 반경의 링 슬롯으로 이동.
	UPROPERTY(Edit, Save, Category="Enemy|Squad", DisplayName="Combat Ring Radius Scale", Min=0.0f, Max=3.0f, Speed=0.05f)
	float CombatRingRadiusScale = 0.9f;
	UPROPERTY(Edit, Save, Category="Enemy|Squad", DisplayName="Separation Radius", Min=0.0f, Max=10.0f, Speed=0.05f)
	float SeparationRadius = 1.6f;
	UPROPERTY(Edit, Save, Category="Enemy|Squad", DisplayName="Separation Strength", Min=0.0f, Max=2.0f, Speed=0.05f)
	float SeparationStrength = 0.6f;
	// 갭클로저: 이 배율까지의 거리에서 돌진 공격을 허용하고(사정권 밖이라도), 시작 시 대시한다.
	UPROPERTY(Edit, Save, Category="Enemy|Combat", DisplayName="Gap Closer Range Scale", Min=1.0f, Max=6.0f, Speed=0.05f)
	float GapCloserRangeScale = 2.5f;
	UPROPERTY(Edit, Save, Category="Enemy|Combat", DisplayName="Gap Closer Dash Scale", Min=0.0f, Max=3.0f, Speed=0.05f)
	float GapCloserDashScale = 1.2f;
	// 공격 가능 판단의 공통 수직 허용값. 개별 공격 FEnemyAttackData::MaxVerticalDelta 가 더 작으면 그 값을 우선한다.
	UPROPERTY(Edit, Save, Category="Enemy|Combat", DisplayName="Attack Vertical Tolerance", Min=0.0f, Max=100.0f, Speed=0.05f)
	float AttackVerticalTolerance = 1.25f;
	// 토큰 만료가 공격 회복보다 먼저 오는 것을 막기 위한 여유 시간. 실제 토큰 길이는
	// max(SquadTokenDuration, SelectedAttackDuration + 이 값)이다.
	UPROPERTY(Edit, Save, Category="Enemy|Squad", DisplayName="Attack Token Safety Buffer", Min=0.0f, Max=5.0f, Speed=0.05f)
	float AttackTokenSafetyBuffer = 0.15f;
	UPROPERTY(Edit, Save, Category="Enemy|LOD", DisplayName="LOD Near Distance", Min=0.0f, Max=1000.0f, Speed=0.5f)
	float LODNearDistance = 25.0f;
	UPROPERTY(Edit, Save, Category="Enemy|LOD", DisplayName="LOD Far Distance", Min=0.0f, Max=2000.0f, Speed=0.5f)
	float LODFarDistance = 80.0f;

protected:
	void Tick(float DeltaTime) override;
	void HandleDeath(UHealthComponent* Component, AActor* DamageCauser, AActor* InstigatorActor) override;
	virtual int32 GetCurrentAIPhase() const { return 1; }
	// 파생 보스 클래스가 true 로 재정의. Brain_IsBoss 가 이 가상 함수를 통해 안전히 분기한다.
	virtual bool IsBossCharacter() const { return false; }
	void RebindEnemyComponents();
	// 두뇌 드라이버 선택: BrainBlueprintFile 이 지정되고 구동 가능하면 Lua Blueprint 로,
	// 아니면 raw Lua(BrainScriptFile)로 구동한다. ExplicitScriptOverride 가 있으면 최우선.
	void ConfigureBrainDriver(const FString& ExplicitScriptOverride);
	void UpdateAttackExecution(float DeltaTime);
	// 패링당했을 때 호출(CombatState::OnParried 바인딩). 진행 중인 비-점프 공격을 역재생 리코일로 전환.
	void HandleParried(UCombatStateComponent* Combat);
	// 리코일 진행(매 틱): ease-out 커브로 공격 몽타주를 역재생하다가 끝나면 EndParryRecoil 로 공격을 푼다.
	void UpdateParryRecoil(float DeltaTime);
	void EndParryRecoil();
	// Phase 4: 타깃과의 거리로 LOD 를 정해 전투 시계 스텝 주기를 조절(원거리 적은 저빈도 think).
	void UpdateAILOD();
	// 다수 적 포지셔닝: 아군과 겹치지 않게 매 프레임 분리 입력 + 타깃 주위 링 슬롯 위치 계산.
	void ApplySeparationSteering();
	FVector ComputeSlotLocation(AActor* Target) const;
	// 백점프 안정 착지 탐색: 등 뒤(DesiredAway) 기준 부채꼴 방향을 네비메시로 스캔해, 원점에서
	// 연속으로 walkable 한(낭떠러지/장애물 없는) 회랑이 가장 길게 이어지는 방향과 그 도달거리를
	// 돌려준다. 후보가 없으면(코너에 몰림) false. MaxDistance 는 탄도 도달거리(상한)다.
	bool FindStableLeapDirection(const FVector& Origin, const FVector& DesiredAway, float MaxDistance, FVector& OutDir, float& OutDistance) const;
	bool StartAttackExecution(const FEnemyAttackData& Attack);
	float GetCurrentHealthRatio() const;
	bool IsTargetHostileDamageReceiver(AActor* Target) const;
	void StopPathFollowingOnly();
	bool ResolveCurrentAttackWorldHit(float Reach);
	bool SetRuntimeState(const FName& StateName, bool bForce = false);
	bool AcquireAttackTokenForDuration(float DesiredDuration);
	const FEnemyAttackData* GetAttackDataByIndex(int32 Index) const;
	bool CanUseAttackDataNow(const FEnemyAttackData& Attack) const;

	TWeakObjectPtr<UEnemyAIBrainComponent> AIBrainComponent = nullptr;
	TWeakObjectPtr<UEnemyAttackComponent> AttackComponent = nullptr;
	TWeakObjectPtr<UEnemyHitComponent> HitComponent = nullptr;
	TWeakObjectPtr<ULuaScriptComponent> LuaScriptComponent = nullptr;
	TWeakObjectPtr<ULuaBlueprintComponent> BrainBlueprint = nullptr;
	TWeakObjectPtr<UAIBlackboardComponent> Blackboard = nullptr;
	TWeakObjectPtr<UAIPerceptionComponent> Perception = nullptr;
	TWeakObjectPtr<UAIDecisionTraceComponent> DecisionTrace = nullptr;
	TWeakObjectPtr<UCombatMoveComponent> CombatMove = nullptr;
	TWeakObjectPtr<UExecutionComponent> Execution = nullptr;
	TWeakObjectPtr<UAwarenessComponent> Awareness = nullptr;
	TWeakObjectPtr<UBehaviorTreeComponent> BehaviorTree = nullptr;

private:
	bool bCurrentAttackActive = false;
	float CurrentAttackElapsed = 0.0f;
	FEnemyAttackData CurrentAttack;
	// 패링 리코일 상태. 활성 동안 UpdateAttackExecution 이 정상 진행 대신 리코일(역재생)을 구동한다.
	bool bParryRecoilActive = false;
	float ParryRecoilElapsed = 0.0f;       // 역재생 경과(게임시간, 디버그/상한용).
	float ParryRecoilWait = 0.0f;          // 패링 직후 슬로모(카운터) 시작을 기다린 시간(실시간). 너무 길면 프리즈 해제.
	float ParryRecoilRawElapsed = 0.0f;    // 실제 되감기 보간 경과(실시간). 전역 슬로모에 눌리지 않게 raw delta 로 누적.
	float ParryRecoilStartSectionTime = 0.0f;
	float ParryRecoilTargetSectionTime = 0.0f;
	float ParryRecoilSectionLength = 0.0f;
	UAnimMontage* ParryRecoilMontage = nullptr;
	bool bParryRecoilSawSlomo = false;     // 리코일 도중 반격 슬로모를 한 번이라도 만났는지.
	// CombatState::OnParried 중복 바인딩 방지.
	bool bBoundParried = false;
	TArray<TWeakObjectPtr<AActor>> CurrentAttackDamagedActors;
	// 현재 공격의 타격 시점(초, 비주얼 스윙에 맞춤)과 이미 타격을 적용했는지.
	float CurrentAttackHitTime = 0.0f;
	bool bCurrentAttackHitApplied = false;
	bool bCurrentAttackWorldHitApplied = false;
	// 마지막으로 재생한 공격 몽타주(꼬리 식별용 — 이동 시 걷기로 덮어쓸지/리액션에 양보할지 구분).
	UAnimMontage* CurrentAttackMontage = nullptr;
	// 직전 틱에 공중(낙하)이었는가 — 착지 순간 감지해 JumpDown(랜딩)을 1회 재생하기 위함.
	bool bWasFalling = false;
	// 백점프 지연 발사: 준비동작 후 실제 점프/임펄스를 가하기 위한 대기 시간과 임펄스 벡터.
	float PendingLeapTime = 0.0f;
	FVector PendingLeapImpulse = FVector::ZeroVector;
	// 두뇌 강제 busy 락 만료 시각(게임시간). 페이즈 전환 연출 등에서 다음 행동을 막는다.
	float BrainBusyUntilSeconds = 0.0f;
	// Brain_Chase 경로 실패 사유 로그 중복 방지 — 같은 사유가 반복되면 한 번만 찍는다.
	FString LastChaseFailReason;
	// 다음 추격 경로 재탐색이 허용되는 게임시각(초). 이 시각 전엔 기존 경로를 따른다(ChaseRepathInterval).
	float NextChaseRepathTime = 0.0f;

	// 직전 공격 결과(공격 문법 분기용). 공격 시작 시 리셋, 피해 적용 시 갱신.
	ECombatDamageResult LastAttackResult = ECombatDamageResult::None;
	bool bLastAttackConnected = false;
	// 현재 공격이 몽타주 없는 timed 공격인가. 몽타주 미제작 데이터에서도 AI가 공격하도록,
	// 이 경우 active 프레임에 폴백 데미지를 적용한다(몽타주가 있으면 노티파이가 전담 → 충돌 없음).
	bool bCurrentAttackMontageless = false;

	// 전투 고정틱 시계 + Lua 두뇌 파사드 런타임 상태.
	FCombatClock CombatClock;
	float LastTickDelta = 0.0f;
	FName SelectedAttackName = FName::None;
	bool bStrafeClockwise = true;
	int32 CurrentLODLevel = 0; // 0=near(60Hz), 1=mid(30Hz), 2=far(10Hz)
	int32 CachedSquadSlot = -1;
	int32 CachedEngagerCount = 0;
};
