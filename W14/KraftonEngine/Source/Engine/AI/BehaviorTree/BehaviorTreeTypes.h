#pragma once

#include "Core/Types/CoreTypes.h"
#include "Object/FName.h"

class FArchive;

// =============================================================================
//  Behavior Tree 자산의 정적 데이터 모델 — 런타임 FBTNode_* 트리와는 분리.
// =============================================================================
//  LuaBlueprintTypes.h / AnimGraphTypes.h 를 본뜬 순수 데이터. 에디터 그래프와
//  컴파일러(FBehaviorTreeCompiler, Phase 2)가 공유한다. 컴파일러가 이 그래프를
//  Root 에서 링크를 따라 내려가며 위상 build → FBTNode_* 트리로 변환한다.
//
//  id 정책: Node / Pin / Link 가 동일 NextId 공간에서 발급(imgui-node-editor 가
//  link 양 끝 pin id 를 같은 namespace 로 식별). id 0 == invalid sentinel.
//
//  BT 는 데이터 흐름이 아니라 단일 실행 흐름이므로 데이터 핀 타입이 없다. 각 노드는
//  부모(Parent) 입력 핀 1개와(Root 제외) 자식(Child) 출력 핀 1개를(leaf 제외) 갖고,
//  Composite 의 여러 자식은 출발 노드의 PosX 오름차순으로 정렬해 실행 순서를 정한다
//  (UE 동일). Decorator/Service 는 노드 내부 배열로 부착한다(UE 의 스택 표현).
// =============================================================================

enum class EBTPinKind : uint8
{
    Parent, // 노드의 입력(부모로부터)
    Child   // 노드의 출력(자식으로)
};

// 그래프 노드 종류. 구체 Task/Condition/Decorator/Service 클래스는 NameValue 로 식별.
enum class EBTGraphNodeType : uint8
{
    Root,      // 트리 진입점 — 자식 1개
    Selector,  // OR/Fallback
    Sequence,  // AND
    Parallel,  // 동시 실행
    Task,      // leaf 행동 (NameValue = 구체 task id)
    Condition, // leaf 판정 (NameValue = 구체 condition id)
    Comment    // 주석/그룹 박스 — 핀 없음, 컴파일 제외. 텍스트=StringValue, 크기=FloatValue/FloatValue2
};

// 노드에 부착되는 보조 노드(데코레이터/서비스) 종류.
enum class EBTAuxKind : uint8
{
    Decorator, // 자식 실행을 조건부로 막거나 결과 변형 / observer-abort
    Service    // 활성 서브트리 동안 주기 틱(블랙보드 갱신 등)
};

enum class EBTDiagnosticSeverity : uint8
{
    Info,
    Warning,
    Error
};

// 트리가 사용하는 블랙보드 키 선언(에디터 편의 — 런타임 값은 UAIBlackboardComponent 에).
enum class EBTBlackboardKeyType : uint8
{
    Float,
    Bool,
    Vector,
    Object
};

struct FBTPin
{
    uint32     PinId        = 0;
    uint32     OwningNodeId = 0;
    EBTPinKind Kind         = EBTPinKind::Parent;
    FName      DisplayName;

    friend FArchive& operator<<(FArchive& Ar, FBTPin& Pin);
};

struct FBTLink
{
    uint32 LinkId    = 0;
    uint32 FromPinId = 0; // 부모의 Child 핀
    uint32 ToPinId   = 0; // 자식의 Parent 핀

    friend FArchive& operator<<(FArchive& Ar, FBTLink& Link);
};

// 데코레이터/서비스 공용 보조 노드. Name 이 구체 클래스를 식별한다.
struct FBTAuxNode
{
    EBTAuxKind Kind = EBTAuxKind::Decorator;
    FName      Name;       // 예: "Blackboard", "Inverter", "Cooldown", "LowestPriority"
    FName      KeyValue;   // 블랙보드 키(조건 데코레이터 등)
    bool       BoolValue  = false;
    int32      IntValue   = 0;
    float      FloatValue = 0.0f;
    FString    StringValue;

    friend FArchive& operator<<(FArchive& Ar, FBTAuxNode& Aux);
};

struct FBTGraphNode
{
    uint32                NodeId = 0;
    EBTGraphNodeType      Type   = EBTGraphNodeType::Task;
    FName                 DisplayName;
    float                 PosX = 0.0f;
    float                 PosY = 0.0f;
    TArray<FBTPin>        Pins;

    // leaf(Task/Condition) 페이로드. 구체 클래스 식별 + 공통 파라미터 슬롯.
    FName   NameValue;    // 예: "Chase", "CanSeeTarget", "PlaySelectedAttack"
    FName   KeyValue;     // 블랙보드 키(필요 시)
    bool    BoolValue  = false;
    int32   IntValue   = 0;
    float   FloatValue = 0.0f;
    float   FloatValue2 = 0.0f; // 범위/보조 파라미터(예: min/max range)
    FString StringValue;

    // 노드에 부착된 데코레이터/서비스(UE 의 스택 표현).
    TArray<FBTAuxNode> Decorators;
    TArray<FBTAuxNode> Services;

    friend FArchive& operator<<(FArchive& Ar, FBTGraphNode& Node);
};

struct FBTBlackboardKey
{
    FName                Name;
    EBTBlackboardKeyType Type = EBTBlackboardKeyType::Float;

    friend FArchive& operator<<(FArchive& Ar, FBTBlackboardKey& Key);
};

struct FBTDiagnostic
{
    EBTDiagnosticSeverity Severity = EBTDiagnosticSeverity::Info;
    uint32                NodeId   = 0;
    FString               Message;

    friend FArchive& operator<<(FArchive& Ar, FBTDiagnostic& Diagnostic);
};
