#pragma once

#include "Core/Types/CoreTypes.h"
#include "AI/BehaviorTree/BehaviorTreeTypes.h"
#include "Object/Object.h"

#include "Source/Engine/AI/BehaviorTree/BehaviorTreeAsset.generated.h"

class FArchive;
class FReferenceCollector;

// =============================================================================
//  UBehaviorTreeAsset — 시각 노드 그래프로 기술된 Behavior Tree 자산 (.uasset)
// =============================================================================
//  데이터 모델만 보유한다. 런타임 FBTNode_* 트리는 FBehaviorTreeCompiler(Phase 2)가
//  UBehaviorTreeComponent 안에서 build 한다. 디스크 직렬화/그래프 편집/검증을 담당하며
//  구조는 ULuaBlueprintAsset / UAnimGraphAsset 를 그대로 본떴다.
//
//  Version / RuntimeVersion: 에디터가 그래프를 바꿀 때마다 증가. 컴포넌트가 매 틱
//  RuntimeVersion 을 비교해 다르면 트리를 폐기·재컴파일한다(in-editor live preview).
//  시각 정보만 바뀐 경우 BumpEditorVersion 으로 런타임 재컴파일을 피한다.
// =============================================================================
UCLASS()
class UBehaviorTreeAsset : public UObject
{
public:
    GENERATED_BODY()
    UBehaviorTreeAsset()           = default;
    ~UBehaviorTreeAsset() override = default;

    void           SetSourcePath(const FString& InPath) { SourcePath = InPath; }
    const FString& GetSourcePath() const { return SourcePath; }

    // ── Build API ──
    FBTGraphNode* AddNode(EBTGraphNodeType Type, const FName& DisplayName, float X, float Y);
    FBTPin*       AddPin(FBTGraphNode& Node, EBTPinKind Kind, const FName& DisplayName);
    FBTLink*      AddLink(uint32 FromPinId, uint32 ToPinId);
    // 노드 타입별 표준 핀 레이아웃까지 한 번에 박는 팩토리(UI 우클릭 / InitializeDefault 가 사용).
    FBTGraphNode* AddNodeOfType(EBTGraphNodeType Type, float X, float Y);

    FBTBlackboardKey* AddBlackboardKey(const FName& Name, EBTBlackboardKeyType Type);
    bool              RemoveBlackboardKey(const FName& Name);

    // 새로 만든 빈 자산을 사용 가능 상태로 초기화. Root → Selector 연결.
    void InitializeDefault();

    // ── Edit API ──
    bool RemoveNode(uint32 NodeId); // 노드의 핀이 걸린 link 도 cascade 삭제
    bool RemoveLink(uint32 LinkId);
    bool RemoveInvalidLinks();

    // ── Validation ──
    // pin 한 쌍에 link 를 허용해도 되는지(부모 Child ↔ 자식 Parent, 방향 자동 swap,
    // 사이클/다중 부모 방지). UI 의 QueryNewLink 응답에서 사용.
    bool CanLinkPins(uint32 PinAId, uint32 PinBId, uint32* OutFromPinId = nullptr, uint32* OutToPinId = nullptr) const;
    bool HasRootNode() const;
    // 그래프 검증 → Diagnostics 채움. 단일 Root, 미연결 노드, 자식 수 규칙 등.
    // 런타임 트리 build 는 컴파일러(Phase 2) 책임 — 여기선 정적 검증만.
    bool Compile();

    // JSON 그래프 입출력 — 사람이 읽고 버전관리/공유 가능한 텍스트. .uasset 바이너리와 별개이며
    // 노드/핀/링크 id 를 그대로 보존해 왕복한다. 에디터 툴바의 Export/Import JSON 이 사용.
    FString ExportGraphToJsonString() const;
    bool    ImportGraphFromJsonString(const FString& Text);

    // ── Lookup ──
    FBTGraphNode*       FindNode(uint32 NodeId);
    const FBTGraphNode* FindNode(uint32 NodeId) const;
    FBTPin*             FindPin(uint32 PinId);
    const FBTPin*       FindPin(uint32 PinId) const;
    FBTGraphNode*       FindNodeByPin(uint32 PinId);
    const FBTGraphNode* FindRootNode() const;
    // 부모 노드의 Child 핀에서 나가는 link 들이 가리키는 자식 노드들을 PosX 오름차순으로.
    void                GatherOrderedChildren(uint32 ParentNodeId, TArray<const FBTGraphNode*>& OutChildren) const;

    // ── Inspection ──
    const TArray<FBTGraphNode>&     GetNodes() const { return Nodes; }
    TArray<FBTGraphNode>&           GetMutableNodes() { return Nodes; }
    const TArray<FBTLink>&          GetLinks() const { return Links; }
    TArray<FBTLink>&                GetMutableLinks() { return Links; }
    const TArray<FBTBlackboardKey>& GetBlackboardKeys() const { return BlackboardKeys; }
    TArray<FBTBlackboardKey>&       GetMutableBlackboardKeys() { return BlackboardKeys; }
    const TArray<FBTDiagnostic>&    GetDiagnostics() const { return Diagnostics; }
    bool                            HasCompileErrors() const;
    bool                            WasLastCompileSuccessful() const { return bLastCompileSucceeded; }

    uint32 GetVersion() const { return Version; }
    uint32 GetRuntimeVersion() const { return RuntimeVersion; }
    bool   IsCompileDirty() const { return bCompileDirty; }

    void BumpVersion()
    {
        ++Version;
        ++RuntimeVersion;
        bCompileDirty = true;
    }
    // 시각 정보(노드 위치 등)만 바뀐 경우 — 런타임 재컴파일을 일으키지 않는다.
    void BumpEditorVersion() { ++Version; }

    void Serialize(FArchive& Ar) override;
    void AddReferencedObjects(FReferenceCollector& Collector) override;

private:
    uint32 AllocateId() { return NextId++; }
    void   SetCompileResult(TArray<FBTDiagnostic>&& InDiagnostics, bool bSuccess);

    TArray<FBTGraphNode>     Nodes;
    TArray<FBTLink>          Links;
    TArray<FBTBlackboardKey> BlackboardKeys;
    TArray<FBTDiagnostic>    Diagnostics;
    uint32                   NextId         = 1; // 0 == invalid sentinel
    uint32                   Version        = 0;
    uint32                   RuntimeVersion = 0;
    bool                     bCompileDirty  = true;
    bool                     bLastCompileSucceeded = false;
    FString                  SourcePath;
};
