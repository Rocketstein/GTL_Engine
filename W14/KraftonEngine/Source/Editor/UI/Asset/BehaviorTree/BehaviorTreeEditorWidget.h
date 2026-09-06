#pragma once

#include "Editor/UI/Asset/AssetEditorWidget.h"
#include "Object/FName.h"
#include "AI/BehaviorTree/BehaviorTreeTypes.h" // 클립보드 멤버(FBTGraphNode/FBTLink) 완전형 필요

#include "imgui.h" // ImVec2 — 신규 노드 spawn 위치 캐시용.

namespace ax { namespace NodeEditor { struct EditorContext; } }

class UBehaviorTreeAsset;
struct FBTGraphNode;
enum class EBTGraphNodeType : uint8;

// UBehaviorTreeAsset 의 시각 노드 그래프 에디터(imgui-node-editor).
// FAnimGraphEditorWidget / FLuaBlueprintEditorWidget 를 본뜬 포커스 버전:
//   - 툴바(Save/Compile), 우클릭 배경 메뉴로 노드 추가, 핀 드래그로 링크 생성/삭제
//   - 우측 인스펙터에서 선택 노드의 Task/Condition id 편집, 진단 표시
// 자유 편집의 진입점. 구조 변경 시 자산 RuntimeVersion 이 올라가 컴포넌트가 핫리로드한다.
class FBehaviorTreeEditorWidget : public FAssetEditorWidget
{
public:
    FBehaviorTreeEditorWidget() = default;
    ~FBehaviorTreeEditorWidget() override;

    bool CanEdit(UObject* Object) const override;
    void Open(UObject* Object) override;
    void Close() override;
    void Render(float DeltaTime) override;
    void RenderDocument(float DeltaTime) override;
    bool AllowsMultipleInstances() const override { return true; }
    bool IsEditingObject(UObject* Object) const override;
    FString GetDocumentTitle() const override;
    FString GetDocumentPayloadId() const override;
    EEditorDocumentTabKind GetDocumentTabKind() const override { return EEditorDocumentTabKind::BehaviorTreeEditor; }

private:
    void EnsureContext();
    void DestroyContext();
    UBehaviorTreeAsset* GetAsset() const;

    void RenderToolbar(UBehaviorTreeAsset* Asset);
    void RenderPalette(UBehaviorTreeAsset* Asset);
    void SpawnFromPalette(UBehaviorTreeAsset* Asset, EBTGraphNodeType Type, const FName& LeafId);
    void RenderGraph(UBehaviorTreeAsset* Asset);
    void RenderInspector(UBehaviorTreeAsset* Asset);
    void RenderDecorators(UBehaviorTreeAsset* Asset, FBTGraphNode& Node);
    void RenderServices(UBehaviorTreeAsset* Asset, FBTGraphNode& Node);
    void RenderAddNodeMenu(UBehaviorTreeAsset* Asset);

    // Undo/Redo (FMemoryArchive 스냅샷) + Copy/Paste/Duplicate.
    TArray<uint8> MakeSnapshot(UBehaviorTreeAsset* Asset) const;
    bool          RestoreSnapshot(UBehaviorTreeAsset* Asset, const TArray<uint8>& Snapshot);
    void          CommitEdit(UBehaviorTreeAsset* Asset);
    void          UndoEdit(UBehaviorTreeAsset* Asset);
    void          RedoEdit(UBehaviorTreeAsset* Asset);
    void          CopySelected(UBehaviorTreeAsset* Asset);
    void          PasteClipboard(UBehaviorTreeAsset* Asset);
    void          DuplicateSelected(UBehaviorTreeAsset* Asset);

    ax::NodeEditor::EditorContext* NodeEditorContext = nullptr;
    bool   bPositionsPushed   = false;
    uint32 InspectedNodeId    = 0;
    ImVec2 PendingNewNodePos  = ImVec2(0, 0);
    bool   bRenderingDocument = false;
    char   PaletteSearchBuf[64] = {};
    ImVec2 PaletteSpawnCursor   = ImVec2(40.0f, 40.0f);

    TArray<TArray<uint8>> UndoStack;
    TArray<TArray<uint8>> RedoStack;
    uint32                LastCommittedVersion = 0;
    bool                  bRestoringSnapshot   = false;
    TArray<uint32>        SelectedNodeIds;     // RenderGraph 가 매 프레임 캡쳐
    TArray<FBTGraphNode>  ClipboardNodes;
    TArray<FBTLink>       ClipboardLinks;

    // 핀 드래그→노드 생성
    bool   bShowPinSpawnMenu   = false;
    uint32 PendingPinSpawnPinId = 0;
    // Comment 텍스트 인스펙터 편집 버퍼
    char   CommentTextBuf[256] = {};
    uint32 CommentBufNodeId    = 0;
    // leaf 파라미터(Key/Text) 인스펙터 편집 버퍼
    char   KeyBuf[128]      = {};
    char   StrBuf[256]      = {};
    uint32 ParamBufNodeId   = 0;
};
