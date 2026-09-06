#include "Editor/UI/Asset/BehaviorTree/BehaviorTreeEditorWidget.h"

#include "AI/BehaviorTree/BehaviorTreeAsset.h"
#include "AI/BehaviorTree/BehaviorTreeManager.h"
#include "AI/BehaviorTree/BTNodeRegistry.h"
#include "Object/Object.h"
#include "Object/Reflection/UClass.h"
#include "Serialization/MemoryArchive.h"
#include "Platform/Paths.h"
#include "Editor/UI/Util/EditorFileUtils.h"

#include "imgui.h"
#include "imgui_node_editor.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace ed = ax::NodeEditor;

namespace
{
    // 데이터 모델의 동일 id 공간을 그대로 node-editor 의 NodeId/PinId/LinkId 로 캐스팅(0==invalid 공유).
    inline ed::NodeId ToNodeId(uint32 Id) { return static_cast<ed::NodeId>(Id); }
    inline ed::PinId  ToPinId (uint32 Id) { return static_cast<ed::PinId >(Id); }
    inline ed::LinkId ToLinkId(uint32 Id) { return static_cast<ed::LinkId>(Id); }
    inline uint32 NodeIdToU32(ed::NodeId Id) { return static_cast<uint32>(Id.Get()); }
    inline uint32 PinIdToU32 (ed::PinId  Id) { return static_cast<uint32>(Id.Get()); }
    inline uint32 LinkIdToU32(ed::LinkId Id) { return static_cast<uint32>(Id.Get()); }

    const char* GraphNodeTypeLabel(EBTGraphNodeType Type)
    {
        switch (Type)
        {
        case EBTGraphNodeType::Root:      return "Root";
        case EBTGraphNodeType::Selector:  return "Selector";
        case EBTGraphNodeType::Sequence:  return "Sequence";
        case EBTGraphNodeType::Parallel:  return "Parallel";
        case EBTGraphNodeType::Task:      return "Task";
        case EBTGraphNodeType::Condition: return "Condition";
        case EBTGraphNodeType::Comment:   return "Comment";
        default:                          return "Node";
        }
    }

    // 노드 카테고리 색(헤더/테두리). UE Blueprint 의 카테고리 컬러 컨벤션 차용.
    ImVec4 BTNodeHeaderColor(EBTGraphNodeType Type)
    {
        switch (Type)
        {
        case EBTGraphNodeType::Root:      return ImVec4(0.95f, 0.45f, 0.45f, 1.0f); // 빨강 — 진입점
        case EBTGraphNodeType::Selector:  return ImVec4(0.40f, 0.75f, 1.00f, 1.0f); // 파랑 — Fallback
        case EBTGraphNodeType::Sequence:  return ImVec4(0.55f, 0.72f, 1.00f, 1.0f); // 파랑 — AND
        case EBTGraphNodeType::Parallel:  return ImVec4(0.75f, 0.60f, 0.95f, 1.0f); // 보라 — 동시
        case EBTGraphNodeType::Task:      return ImVec4(0.55f, 0.90f, 0.55f, 1.0f); // 녹색 — 행동
        case EBTGraphNodeType::Condition: return ImVec4(0.95f, 0.80f, 0.45f, 1.0f); // 호박 — 판정
        default:                          return ImVec4(0.80f, 0.80f, 0.80f, 1.0f);
        }
    }

    // 어두운 캔버스/격자/노드 바디 + 호버·선택 외곽선. 반환값 = 팝할 색 개수.
    int PushBTCanvasStyle()
    {
        ed::PushStyleColor(ed::StyleColor_Bg,            ImColor(31, 32, 39, 255));
        ed::PushStyleColor(ed::StyleColor_Grid,          ImColor(48, 50, 58, 150));
        ed::PushStyleColor(ed::StyleColor_NodeBg,        ImColor(18, 19, 22, 235));
        ed::PushStyleColor(ed::StyleColor_NodeBorder,    ImColor(82, 82, 82, 150));
        ed::PushStyleColor(ed::StyleColor_HovNodeBorder, ImColor(190, 210, 230, 220));
        ed::PushStyleColor(ed::StyleColor_SelNodeBorder, ImColor(255, 230, 105, 235));
        ed::PushStyleColor(ed::StyleColor_HovLinkBorder, ImColor(255, 255, 255, 220));
        ed::PushStyleColor(ed::StyleColor_SelLinkBorder, ImColor(255, 230, 105, 235));
        return 8;
    }
}

FBehaviorTreeEditorWidget::~FBehaviorTreeEditorWidget()
{
    DestroyContext();
}

bool FBehaviorTreeEditorWidget::CanEdit(UObject* Object) const
{
    return Object && Object->IsA<UBehaviorTreeAsset>();
}

UBehaviorTreeAsset* FBehaviorTreeEditorWidget::GetAsset() const
{
    return Cast<UBehaviorTreeAsset>(EditedObject);
}

bool FBehaviorTreeEditorWidget::IsEditingObject(UObject* Object) const
{
    if (FAssetEditorWidget::IsEditingObject(Object))
    {
        return true;
    }
    const UBehaviorTreeAsset* Cur = Cast<UBehaviorTreeAsset>(EditedObject);
    const UBehaviorTreeAsset* Req = Cast<UBehaviorTreeAsset>(Object);
    if (!IsOpen() || !Cur || !Req)
    {
        return false;
    }
    const FString& CurPath = Cur->GetSourcePath();
    return !CurPath.empty() && CurPath == Req->GetSourcePath();
}

void FBehaviorTreeEditorWidget::Open(UObject* Object)
{
    if (!CanEdit(Object))
    {
        return;
    }
    FAssetEditorWidget::Open(Object);
    EnsureContext();
    bPositionsPushed = false;
    InspectedNodeId  = 0;
    UndoStack.clear();
    RedoStack.clear();
    SelectedNodeIds.clear();
    if (UBehaviorTreeAsset* Asset = GetAsset())
    {
        Asset->Compile(); // 진단 갱신
        TArray<uint8> Initial = MakeSnapshot(Asset);
        if (!Initial.empty())
        {
            UndoStack.push_back(Initial);
        }
        LastCommittedVersion = Asset->GetRuntimeVersion();
    }
}

void FBehaviorTreeEditorWidget::Close()
{
    DestroyContext();
    FAssetEditorWidget::Close();
}

void FBehaviorTreeEditorWidget::EnsureContext()
{
    if (NodeEditorContext) return;
    ed::Config Cfg;
    Cfg.SettingsFile  = nullptr; // 자산 자체에 직렬화 — node-editor 디스크 캐시 비활성.
    NodeEditorContext = ed::CreateEditor(&Cfg);
}

void FBehaviorTreeEditorWidget::DestroyContext()
{
    if (NodeEditorContext)
    {
        ed::DestroyEditor(NodeEditorContext);
        NodeEditorContext = nullptr;
    }
}

void FBehaviorTreeEditorWidget::RenderDocument(float DeltaTime)
{
    bRenderingDocument = true;
    Render(DeltaTime);
    bRenderingDocument = false;
}

FString FBehaviorTreeEditorWidget::GetDocumentTitle() const
{
    const UBehaviorTreeAsset* Asset = Cast<UBehaviorTreeAsset>(EditedObject);
    const FString SourcePath = Asset ? Asset->GetSourcePath() : FString();
    return SourcePath.empty() ? FString("Behavior Tree") : FString("Behavior Tree - " + SourcePath);
}

FString FBehaviorTreeEditorWidget::GetDocumentPayloadId() const
{
    const UBehaviorTreeAsset* Asset = Cast<UBehaviorTreeAsset>(EditedObject);
    const FString SourcePath = Asset ? Asset->GetSourcePath() : FString();
    return SourcePath.empty() ? FAssetEditorWidget::GetDocumentPayloadId() : SourcePath;
}

void FBehaviorTreeEditorWidget::Render(float DeltaTime)
{
    (void)DeltaTime;
    if (!IsOpen() || !EditedObject || !NodeEditorContext)
    {
        return;
    }
    UBehaviorTreeAsset* Asset = static_cast<UBehaviorTreeAsset*>(EditedObject);

    char WindowTitle[128];
    std::snprintf(WindowTitle, sizeof(WindowTitle), "Behavior Tree Editor##%p", static_cast<const void*>(Asset));
    if (ConsumeFocusRequest())
    {
        ImGui::SetNextWindowFocus();
    }

    bool bOpenFlag = true;
    if (!bRenderingDocument && !ImGui::Begin(WindowTitle, &bOpenFlag))
    {
        ImGui::End();
        if (!bOpenFlag) Close();
        return;
    }

    RenderToolbar(Asset);

    const float PaletteWidth   = 175.0f;
    const float InspectorWidth = 280.0f;
    const float Spacing        = ImGui::GetStyle().ItemSpacing.x;
    const float TotalWidth     = ImGui::GetContentRegionAvail().x;
    float       CanvasWidth    = TotalWidth - PaletteWidth - InspectorWidth - Spacing * 2.0f;
    if (CanvasWidth < 200.0f)
    {
        CanvasWidth = (std::max)(200.0f, TotalWidth - PaletteWidth - Spacing);
    }

    ImGui::BeginChild("##BTPalette", ImVec2(PaletteWidth, 0), ImGuiChildFlags_None);
    RenderPalette(Asset);
    ImGui::EndChild();
    ImGui::SameLine();

    ImGui::BeginChild("##BTCanvas", ImVec2(CanvasWidth, 0), ImGuiChildFlags_None);
    RenderGraph(Asset);
    ImGui::EndChild();
    ImGui::SameLine();

    ImGui::BeginChild("##BTInspector", ImVec2(0, 0), ImGuiChildFlags_None);
    RenderInspector(Asset);
    ImGui::EndChild();

    // 구조 변경(Version 증가) 시 프레임당 1회 undo 스냅샷 커밋(위치 드래그는 Version 불변 → 제외).
    if (!bRestoringSnapshot && Asset->GetRuntimeVersion() != LastCommittedVersion)
    {
        CommitEdit(Asset);
        LastCommittedVersion = Asset->GetRuntimeVersion();
    }

    if (!bRenderingDocument)
    {
        ImGui::End();
    }
}

void FBehaviorTreeEditorWidget::RenderToolbar(UBehaviorTreeAsset* Asset)
{
    const bool bHasPath = !Asset->GetSourcePath().empty();
    if (!bHasPath) ImGui::BeginDisabled();
    if (ImGui::Button("Save"))
    {
        Asset->Compile();
        FBehaviorTreeManager::Get().Save(Asset);
        ClearDirty();
    }
    if (!bHasPath) ImGui::EndDisabled();

    ImGui::SameLine();
    if (ImGui::Button("Compile"))
    {
        Asset->Compile();
    }
    ImGui::SameLine();
    if (ImGui::Button("Undo"))      UndoEdit(Asset);
    ImGui::SameLine();
    if (ImGui::Button("Redo"))      RedoEdit(Asset);
    ImGui::SameLine();
    if (ImGui::Button("Copy"))      CopySelected(Asset);
    ImGui::SameLine();
    if (ImGui::Button("Paste"))     PasteClipboard(Asset);
    ImGui::SameLine();
    if (ImGui::Button("Duplicate")) DuplicateSelected(Asset);

    ImGui::SameLine();
    if (ImGui::Button("Export JSON"))
    {
        const std::wstring     InitDir = (std::filesystem::path(FPaths::RootDir()) / L"Content").wstring();
        FEditorFileDialogOptions Options;
        Options.Title                        = L"Export Behavior Tree JSON";
        Options.Filter                       = L"Behavior Tree JSON (*.json)\0*.json\0All Files (*.*)\0*.*\0";
        Options.DefaultExtension             = L"json";
        Options.DefaultFileName              = L"BehaviorTree_Export.json";
        Options.InitialDirectory             = InitDir.c_str();
        Options.bFileMustExist               = false;
        Options.bPathMustExist               = true;
        Options.bPromptOverwrite             = true;
        Options.bReturnRelativeToProjectRoot = false;

        const FString OutPath = FEditorFileUtils::SaveFileDialog(Options);
        if (!OutPath.empty())
        {
            const std::filesystem::path P(FPaths::ToWide(OutPath));
            std::error_code             Ec;
            std::filesystem::create_directories(P.parent_path(), Ec);
            std::ofstream File(P, std::ios::binary | std::ios::trunc);
            if (File.is_open())
            {
                File << Asset->ExportGraphToJsonString().c_str();
            }
        }
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Export the graph to a .json file (re-importable text).");

    ImGui::SameLine();
    if (ImGui::Button("Import JSON"))
    {
        const std::wstring     InitDir = (std::filesystem::path(FPaths::RootDir()) / L"Content").wstring();
        FEditorFileDialogOptions Options;
        Options.Title                        = L"Import Behavior Tree JSON";
        Options.Filter                       = L"Behavior Tree JSON (*.json)\0*.json\0All Files (*.*)\0*.*\0";
        Options.DefaultExtension             = L"json";
        Options.InitialDirectory             = InitDir.c_str();
        Options.bFileMustExist               = true;
        Options.bPathMustExist               = true;
        Options.bReturnRelativeToProjectRoot = false;

        const FString InPath = FEditorFileUtils::OpenFileDialog(Options);
        if (!InPath.empty())
        {
            std::ifstream File(std::filesystem::path(FPaths::ToWide(InPath)), std::ios::binary);
            if (File.is_open())
            {
                std::stringstream Buffer;
                Buffer << File.rdbuf();
                if (Asset->ImportGraphFromJsonString(FString(Buffer.str())))
                {
                    Asset->Compile();
                    bPositionsPushed = false;
                    InspectedNodeId  = 0;
                    MarkDirty();
                }
            }
        }
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Import a .json behavior tree, replacing the current graph.");

    ImGui::SameLine();
    if (bHasPath)
    {
        ImGui::TextDisabled("%s", Asset->GetSourcePath().c_str());
    }
    else
    {
        ImGui::TextDisabled("(unsaved — right-click canvas to add nodes)");
    }
    ImGui::Separator();
}

void FBehaviorTreeEditorWidget::SpawnFromPalette(UBehaviorTreeAsset* Asset, EBTGraphNodeType Type, const FName& LeafId)
{
    FBTGraphNode* N = Asset->AddNodeOfType(Type, PaletteSpawnCursor.x, PaletteSpawnCursor.y);
    if (!N)
    {
        return;
    }
    if (LeafId.IsValid())
    {
        N->NameValue   = LeafId;
        N->DisplayName = LeafId;
    }
    InspectedNodeId = N->NodeId;

    // 다음 스폰 위치 cascade(겹침 방지).
    PaletteSpawnCursor.y += 70.0f;
    if (PaletteSpawnCursor.y > 430.0f)
    {
        PaletteSpawnCursor.y = 40.0f;
        PaletteSpawnCursor.x += 200.0f;
    }
    // 새 노드를 포함해 다음 프레임 RenderGraph 가 모델 좌표를 재푸시.
    bPositionsPushed = false;
    MarkDirty();
}

void FBehaviorTreeEditorWidget::RenderPalette(UBehaviorTreeAsset* Asset)
{
    ImGui::TextUnformatted("Palette");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputText("##bt_palette_search", PaletteSearchBuf, sizeof(PaletteSearchBuf));

    auto Matches = [this](const char* Label, const char* Category) -> bool
    {
        if (PaletteSearchBuf[0] == '\0')
        {
            return true;
        }
        std::string Hay = std::string(Label ? Label : "") + " " + std::string(Category ? Category : "");
        std::string Needle = PaletteSearchBuf;
        std::transform(Hay.begin(),   Hay.end(),   Hay.begin(),   [](unsigned char C){ return static_cast<char>(std::tolower(C)); });
        std::transform(Needle.begin(), Needle.end(), Needle.begin(), [](unsigned char C){ return static_cast<char>(std::tolower(C)); });
        return Hay.find(Needle) != std::string::npos;
    };

    ImGui::Separator();
    ImGui::TextDisabled("Composites");
    if (Matches("Selector", "Composite") && ImGui::Selectable("Selector")) SpawnFromPalette(Asset, EBTGraphNodeType::Selector, FName());
    if (Matches("Sequence", "Composite") && ImGui::Selectable("Sequence")) SpawnFromPalette(Asset, EBTGraphNodeType::Sequence, FName());
    if (Matches("Parallel", "Composite") && ImGui::Selectable("Parallel")) SpawnFromPalette(Asset, EBTGraphNodeType::Parallel, FName());

    ImGui::Separator();
    ImGui::TextDisabled("Tasks");
    for (const FBTLeafType& T : GetBTLeafCatalog())
    {
        if (T.bIsCondition) continue;
        if (!Matches(T.Display.c_str(), T.Category.c_str())) continue;
        if (ImGui::Selectable(T.Display.c_str()))
        {
            SpawnFromPalette(Asset, EBTGraphNodeType::Task, T.Id);
        }
    }

    ImGui::Separator();
    ImGui::TextDisabled("Conditions");
    for (const FBTLeafType& T : GetBTLeafCatalog())
    {
        if (!T.bIsCondition) continue;
        if (!Matches(T.Display.c_str(), T.Category.c_str())) continue;
        if (ImGui::Selectable(T.Display.c_str()))
        {
            SpawnFromPalette(Asset, EBTGraphNodeType::Condition, T.Id);
        }
    }

    ImGui::Separator();
    ImGui::TextDisabled("Misc");
    if (Matches("Comment", "Misc") && ImGui::Selectable("Comment"))
    {
        SpawnFromPalette(Asset, EBTGraphNodeType::Comment, FName());
    }
}

void FBehaviorTreeEditorWidget::RenderGraph(UBehaviorTreeAsset* Asset)
{
    ed::SetCurrentEditor(NodeEditorContext);
    const int CanvasStyleColors = PushBTCanvasStyle();
    ed::Begin("BehaviorTreeCanvas");

    // 첫 프레임: 모델 좌표를 ed 로 push (이후 매 프레임 GetNodePosition 으로 pull).
    if (!bPositionsPushed)
    {
        for (const FBTGraphNode& Node : Asset->GetNodes())
        {
            ed::SetNodePosition(ToNodeId(Node.NodeId), ImVec2(Node.PosX, Node.PosY));
        }
        bPositionsPushed = true;
    }

    // ── 노드 (카테고리 색 헤더 + 데코레이터/서비스 배지) ──
    for (FBTGraphNode& Node : Asset->GetMutableNodes())
    {
        // Comment 는 ed::Group 으로 — 내부에 겹친 노드를 함께 드래그(UE BP Comment). 핀/컴파일 없음.
        if (Node.Type == EBTGraphNodeType::Comment)
        {
            ed::PushStyleColor(ed::StyleColor_NodeBg,     ImColor(110, 95, 30, 80));
            ed::PushStyleColor(ed::StyleColor_NodeBorder, ImColor(220, 200, 100, 200));
            ed::BeginNode(ToNodeId(Node.NodeId));
            ImGui::PushID(static_cast<int>(Node.NodeId));
            ImGui::TextColored(ImVec4(1.0f, 0.95f, 0.5f, 1.0f), "%s",
                Node.StringValue.empty() ? "Comment" : Node.StringValue.c_str());
            const ImVec2 GroupSize((std::max)(140.0f, Node.FloatValue), (std::max)(70.0f, Node.FloatValue2));
            ed::Group(GroupSize);
            ImGui::PopID();
            ed::EndNode();
            const ImVec2 ActualSize = ed::GetNodeSize(ToNodeId(Node.NodeId));
            if (ActualSize.x > 0.0f && ActualSize.y > 0.0f &&
                (std::fabs(Node.FloatValue - ActualSize.x) > 0.5f || std::fabs(Node.FloatValue2 - ActualSize.y) > 0.5f))
            {
                Node.FloatValue  = ActualSize.x;
                Node.FloatValue2 = ActualSize.y;
                Asset->BumpEditorVersion(); // 크기만 → 재컴파일 X
            }
            ed::PopStyleColor(2);
            continue;
        }

        const ImVec4 HeaderCol = BTNodeHeaderColor(Node.Type);
        ed::PushStyleColor(ed::StyleColor_NodeBorder, ImColor(HeaderCol.x, HeaderCol.y, HeaderCol.z, 0.85f));
        ed::BeginNode(ToNodeId(Node.NodeId));
        ImGui::PushID(static_cast<int>(Node.NodeId));

        // 데코레이터 배지(헤더 위) — observer-abort 는 '*'.
        for (const FBTAuxNode& Dec : Node.Decorators)
        {
            const FString DStr = Dec.Name.IsValid() ? Dec.Name.ToString() : FString("?");
            if (Dec.IntValue != 0)
            {
                ImGui::TextColored(ImVec4(0.95f, 0.80f, 0.45f, 1.0f), "{%s *}", DStr.c_str());
            }
            else
            {
                ImGui::TextColored(ImVec4(0.85f, 0.78f, 0.55f, 1.0f), "{%s}", DStr.c_str());
            }
        }

        // 헤더: 카테고리 색 제목 + role 서브타이틀.
        const bool    bLeaf = (Node.Type == EBTGraphNodeType::Task || Node.Type == EBTGraphNodeType::Condition);
        const FString Title = (bLeaf && Node.NameValue.IsValid()) ? Node.NameValue.ToString()
                                                                   : FString(GraphNodeTypeLabel(Node.Type));
        ImGui::TextColored(HeaderCol, "%s", Title.c_str());
        if (bLeaf)
        {
            ImGui::TextDisabled("%s", GraphNodeTypeLabel(Node.Type));
        }

        // 핀.
        uint32 ParentPin = 0;
        uint32 ChildPin  = 0;
        for (const FBTPin& Pin : Node.Pins)
        {
            if (Pin.Kind == EBTPinKind::Parent)     ParentPin = Pin.PinId;
            else if (Pin.Kind == EBTPinKind::Child) ChildPin  = Pin.PinId;
        }
        if (ParentPin)
        {
            ed::BeginPin(ToPinId(ParentPin), ed::PinKind::Input);
            ImGui::TextUnformatted("-> In");
            ed::EndPin();
        }
        if (ChildPin)
        {
            if (ParentPin) ImGui::SameLine();
            ed::BeginPin(ToPinId(ChildPin), ed::PinKind::Output);
            ImGui::TextUnformatted("Out ->");
            ed::EndPin();
        }

        // 서비스 배지(하단).
        for (const FBTAuxNode& Svc : Node.Services)
        {
            const FString SStr = Svc.Name.IsValid() ? Svc.Name.ToString() : FString("?");
            ImGui::TextColored(ImVec4(0.45f, 0.85f, 0.95f, 1.0f), "(svc %s)", SStr.c_str());
        }

        ImGui::PopID();
        ed::EndNode();
        ed::PopStyleColor(1);
    }

    // ── 링크 ──
    for (const FBTLink& Link : Asset->GetLinks())
    {
        ed::Link(ToLinkId(Link.LinkId), ToPinId(Link.FromPinId), ToPinId(Link.ToPinId),
                 ImVec4(0.60f, 0.80f, 1.0f, 1.0f), 2.0f);
    }

    // ── 핀 드래그로 링크 생성 ──
    if (ed::BeginCreate())
    {
        ed::PinId StartId, EndId;
        if (ed::QueryNewLink(&StartId, &EndId))
        {
            if (StartId && EndId)
            {
                uint32 FromU = 0, ToU = 0;
                if (Asset->CanLinkPins(PinIdToU32(StartId), PinIdToU32(EndId), &FromU, &ToU))
                {
                    if (ed::AcceptNewItem())
                    {
                        Asset->AddLink(FromU, ToU);
                        MarkDirty();
                    }
                }
                else
                {
                    ed::RejectNewItem(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), 2.0f);
                }
            }
        }

        // 핀을 빈 곳으로 드래그 → 노드 생성 메뉴(드래그한 핀에 자동 연결).
        ed::PinId NewNodePinId = 0;
        if (ed::QueryNewNode(&NewNodePinId))
        {
            if (NewNodePinId && ed::AcceptNewItem(ImVec4(0.45f, 0.75f, 1.0f, 1.0f), 1.5f))
            {
                PendingPinSpawnPinId = PinIdToU32(NewNodePinId);
                PendingNewNodePos    = ed::ScreenToCanvas(ImGui::GetMousePos());
                bShowPinSpawnMenu    = true;
            }
        }
    }
    ed::EndCreate();

    // ── Delete 키 / 메뉴 → 링크·노드 삭제 (Root 보호) ──
    if (ed::BeginDelete())
    {
        ed::LinkId DeletedLink;
        while (ed::QueryDeletedLink(&DeletedLink))
        {
            if (ed::AcceptDeletedItem())
            {
                Asset->RemoveLink(LinkIdToU32(DeletedLink));
                MarkDirty();
            }
        }
        ed::NodeId DeletedNode;
        while (ed::QueryDeletedNode(&DeletedNode))
        {
            const uint32        NodeId = NodeIdToU32(DeletedNode);
            const FBTGraphNode* N      = Asset->FindNode(NodeId);
            if (N && N->Type == EBTGraphNodeType::Root)
            {
                continue; // Root 는 삭제 불가
            }
            if (ed::AcceptDeletedItem())
            {
                Asset->RemoveNode(NodeId);
                if (InspectedNodeId == NodeId) InspectedNodeId = 0;
                MarkDirty();
            }
        }
    }
    ed::EndDelete();

    // ── 위치 동기화 (ed → model) ── (위치만 바뀐 건 RuntimeVersion 안 올림 = 재컴파일 X)
    for (FBTGraphNode& Node : Asset->GetMutableNodes())
    {
        const ImVec2 P = ed::GetNodePosition(ToNodeId(Node.NodeId));
        if (std::fabs(Node.PosX - P.x) > 0.01f || std::fabs(Node.PosY - P.y) > 0.01f)
        {
            Node.PosX = P.x;
            Node.PosY = P.y;
            MarkDirty();
        }
    }

    // 선택 노드 캡쳐(Copy/Paste/Duplicate 용 — 툴바에서 다음 프레임에 사용).
    {
        ed::NodeId SelBuf[64];
        const int  SelCount = ed::GetSelectedNodes(SelBuf, 64);
        SelectedNodeIds.clear();
        for (int i = 0; i < SelCount; ++i)
        {
            SelectedNodeIds.push_back(NodeIdToU32(SelBuf[i]));
        }
    }

    // ── 컨텍스트 메뉴 ──
    ed::NodeId CtxNodeId = 0;
    ed::Suspend();
    if (ed::ShowNodeContextMenu(&CtxNodeId))
    {
        InspectedNodeId = NodeIdToU32(CtxNodeId);
        ImGui::OpenPopup("BTNodeMenu");
    }
    else if (ed::ShowBackgroundContextMenu())
    {
        PendingNewNodePos = ed::ScreenToCanvas(ImGui::GetMousePos());
        ImGui::OpenPopup("BTBackgroundMenu");
    }

    if (ImGui::BeginPopup("BTNodeMenu"))
    {
        if (const FBTGraphNode* N = Asset->FindNode(InspectedNodeId))
        {
            ImGui::TextDisabled("%s", GraphNodeTypeLabel(N->Type));
            ImGui::Separator();
            const bool bRoot = (N->Type == EBTGraphNodeType::Root);
            if (bRoot) ImGui::BeginDisabled();
            if (ImGui::MenuItem("Delete Node"))
            {
                Asset->RemoveNode(InspectedNodeId);
                InspectedNodeId = 0;
                MarkDirty();
            }
            if (bRoot) ImGui::EndDisabled();
        }
        ImGui::EndPopup();
    }
    if (ImGui::BeginPopup("BTBackgroundMenu"))
    {
        RenderAddNodeMenu(Asset);
        ImGui::EndPopup();
    }

    // 핀 드래그→생성 메뉴(드래그한 핀에 자동 연결).
    if (bShowPinSpawnMenu)
    {
        ImGui::OpenPopup("BTPinSpawnMenu");
        bShowPinSpawnMenu = false;
    }
    if (ImGui::BeginPopup("BTPinSpawnMenu"))
    {
        auto SpawnLinked = [&](EBTGraphNodeType Type, const FName& LeafId)
        {
            FBTGraphNode* N = Asset->AddNodeOfType(Type, PendingNewNodePos.x, PendingNewNodePos.y);
            if (N)
            {
                if (LeafId.IsValid())
                {
                    N->NameValue   = LeafId;
                    N->DisplayName = LeafId;
                }
                // 드래그한 핀과 호환되는 새 노드 핀을 자동 연결.
                for (const FBTPin& P : N->Pins)
                {
                    uint32 F = 0, T = 0;
                    if (Asset->CanLinkPins(PendingPinSpawnPinId, P.PinId, &F, &T))
                    {
                        Asset->AddLink(F, T);
                        break;
                    }
                }
                InspectedNodeId  = N->NodeId;
                bPositionsPushed = false;
                MarkDirty();
            }
            ImGui::CloseCurrentPopup();
        };
        if (ImGui::MenuItem("Selector")) SpawnLinked(EBTGraphNodeType::Selector, FName());
        if (ImGui::MenuItem("Sequence")) SpawnLinked(EBTGraphNodeType::Sequence, FName());
        if (ImGui::MenuItem("Parallel")) SpawnLinked(EBTGraphNodeType::Parallel, FName());
        ImGui::Separator();
        for (const FBTLeafType& LT : GetBTLeafCatalog())
        {
            if (ImGui::MenuItem(LT.Display.c_str()))
            {
                SpawnLinked(LT.bIsCondition ? EBTGraphNodeType::Condition : EBTGraphNodeType::Task, LT.Id);
            }
        }
        ImGui::EndPopup();
    }
    ed::Resume();

    ed::End();
    ed::PopStyleColor(CanvasStyleColors);
    ed::SetCurrentEditor(nullptr);
}

void FBehaviorTreeEditorWidget::RenderAddNodeMenu(UBehaviorTreeAsset* Asset)
{
    auto Spawn = [&](EBTGraphNodeType Type, const char* LeafId)
    {
        FBTGraphNode* N = Asset->AddNodeOfType(Type, PendingNewNodePos.x, PendingNewNodePos.y);
        if (!N) return;
        if (LeafId)
        {
            N->NameValue   = FName(LeafId);
            N->DisplayName = FName(LeafId);
        }
        ed::SetNodePosition(ToNodeId(N->NodeId), PendingNewNodePos);
        InspectedNodeId = N->NodeId;
        MarkDirty();
    };

    if (ImGui::MenuItem("Selector")) Spawn(EBTGraphNodeType::Selector, nullptr);
    if (ImGui::MenuItem("Sequence")) Spawn(EBTGraphNodeType::Sequence, nullptr);
    if (ImGui::MenuItem("Parallel")) Spawn(EBTGraphNodeType::Parallel, nullptr);
    ImGui::Separator();
    if (ImGui::BeginMenu("Task"))
    {
        for (const FBTLeafType& LT : GetBTLeafCatalog())
        {
            if (!LT.bIsCondition && ImGui::MenuItem(LT.Display.c_str()))
            {
                Spawn(EBTGraphNodeType::Task, LT.Id.ToString().c_str());
            }
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Condition"))
    {
        for (const FBTLeafType& LT : GetBTLeafCatalog())
        {
            if (LT.bIsCondition && ImGui::MenuItem(LT.Display.c_str()))
            {
                Spawn(EBTGraphNodeType::Condition, LT.Id.ToString().c_str());
            }
        }
        ImGui::EndMenu();
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Comment")) Spawn(EBTGraphNodeType::Comment, nullptr);
}

void FBehaviorTreeEditorWidget::RenderInspector(UBehaviorTreeAsset* Asset)
{
    ImGui::TextUnformatted("Inspector");
    ImGui::Separator();

    if (FBTGraphNode* Node = Asset->FindNode(InspectedNodeId))
    {
        ImGui::Text("Type: %s", GraphNodeTypeLabel(Node->Type));
        const bool bLeaf = (Node->Type == EBTGraphNodeType::Task || Node->Type == EBTGraphNodeType::Condition);
        if (Node->Type == EBTGraphNodeType::Comment)
        {
            if (CommentBufNodeId != InspectedNodeId)
            {
                std::snprintf(CommentTextBuf, sizeof(CommentTextBuf), "%s", Node->StringValue.c_str());
                CommentBufNodeId = InspectedNodeId;
            }
            if (ImGui::InputText("Text", CommentTextBuf, sizeof(CommentTextBuf)))
            {
                Node->StringValue = CommentTextBuf;
                Asset->BumpEditorVersion();
                MarkDirty();
            }
        }
        if (bLeaf)
        {
            const FString Cur = Node->NameValue.IsValid() ? Node->NameValue.ToString() : FString("(none)");
            if (ImGui::BeginCombo("Action", Cur.c_str()))
            {
                const bool bWantCondition = (Node->Type == EBTGraphNodeType::Condition);
                for (const FBTLeafType& LT : GetBTLeafCatalog())
                {
                    if (LT.bIsCondition != bWantCondition) continue;
                    if (ImGui::Selectable(LT.Display.c_str(), Node->NameValue == LT.Id))
                    {
                        Node->NameValue   = LT.Id;
                        Node->DisplayName = LT.Id;
                        Asset->BumpVersion(); // 구조 의미 변경 → 컴포넌트 핫리로드
                        MarkDirty();
                    }
                }
                ImGui::EndCombo();
            }

            // Task/Condition 파라미터 — 레지스트리 힌트(ParamFlags)에 따라 필드 노출.
            if (const FBTLeafType* LT = FindBTLeafType(Node->NameValue))
            {
                if (ParamBufNodeId != InspectedNodeId)
                {
                    std::snprintf(KeyBuf, sizeof(KeyBuf), "%s", Node->KeyValue.IsValid() ? Node->KeyValue.ToString().c_str() : "");
                    std::snprintf(StrBuf, sizeof(StrBuf), "%s", Node->StringValue.c_str());
                    ParamBufNodeId = InspectedNodeId;
                }
                if (LT->ParamFlags & BTParam_Key)
                {
                    if (ImGui::InputText("Key", KeyBuf, sizeof(KeyBuf)))
                    {
                        Node->KeyValue = FName(KeyBuf);
                        Asset->BumpVersion();
                        MarkDirty();
                    }
                }
                if (LT->ParamFlags & BTParam_Bool)
                {
                    if (ImGui::Checkbox("Value", &Node->BoolValue)) { Asset->BumpVersion(); MarkDirty(); }
                }
                if (LT->ParamFlags & BTParam_Float)
                {
                    if (ImGui::DragFloat("Number", &Node->FloatValue, 0.05f)) { Asset->BumpVersion(); MarkDirty(); }
                }
                if (LT->ParamFlags & BTParam_String)
                {
                    if (ImGui::InputText("Text", StrBuf, sizeof(StrBuf)))
                    {
                        Node->StringValue = StrBuf;
                        Asset->BumpVersion();
                        MarkDirty();
                    }
                }
            }
        }

        if (Node->Type != EBTGraphNodeType::Comment)
        {
            RenderDecorators(Asset, *Node);
            if (!bLeaf)
            {
                RenderServices(Asset, *Node); // 서비스는 Composite 에만 의미
            }
        }
    }
    else
    {
        ImGui::TextDisabled("Right-click a node to inspect.");
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Diagnostics");
    for (const FBTDiagnostic& D : Asset->GetDiagnostics())
    {
        const ImVec4 Col = (D.Severity == EBTDiagnosticSeverity::Error)
            ? ImVec4(1.0f, 0.4f, 0.4f, 1.0f)
            : (D.Severity == EBTDiagnosticSeverity::Warning)
                ? ImVec4(1.0f, 0.8f, 0.3f, 1.0f)
                : ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
        ImGui::TextColored(Col, "- %s", D.Message.c_str());
    }
}

void FBehaviorTreeEditorWidget::RenderDecorators(UBehaviorTreeAsset* Asset, FBTGraphNode& Node)
{
    static const char* const kCondNames[] = {
        "CanSeeTarget", "InAttackRange", "TargetThreatening", "IsAlerted",
        "TargetInRecovery", "HasLineOfSight", "IsInProximity", "IsUnaware"
    };

    ImGui::Separator();
    ImGui::TextUnformatted("Decorators (gate / observer-abort)");

    int RemoveIndex = -1;
    for (int i = 0; i < static_cast<int>(Node.Decorators.size()); ++i)
    {
        FBTAuxNode& Aux = Node.Decorators[i];
        ImGui::PushID(i);
        ImGui::BulletText("%s", Aux.Name.IsValid() ? Aux.Name.ToString().c_str() : "?");

        if (Aux.Name == FName("Cooldown"))
        {
            if (ImGui::DragFloat("Seconds", &Aux.FloatValue, 0.05f, 0.0f, 60.0f))
            {
                Asset->BumpVersion();
                MarkDirty();
            }
        }
        else if (Aux.Name == FName("Condition"))
        {
            const FString Cur = Aux.KeyValue.IsValid() ? Aux.KeyValue.ToString() : FString("(none)");
            if (ImGui::BeginCombo("Cond", Cur.c_str()))
            {
                for (const char* Name : kCondNames)
                {
                    if (ImGui::Selectable(Name, Cur == Name))
                    {
                        Aux.KeyValue = FName(Name);
                        Asset->BumpVersion();
                        MarkDirty();
                    }
                }
                ImGui::EndCombo();
            }
            bool bObs = (Aux.IntValue != 0);
            if (ImGui::Checkbox("Observer Abort", &bObs))
            {
                Aux.IntValue = bObs ? 1 : 0;
                Asset->BumpVersion();
                MarkDirty();
            }
        }

        if (ImGui::SmallButton("Remove"))
        {
            RemoveIndex = i;
        }
        ImGui::PopID();
        ImGui::Separator();
    }
    if (RemoveIndex >= 0)
    {
        Node.Decorators.erase(Node.Decorators.begin() + RemoveIndex);
        Asset->BumpVersion();
        MarkDirty();
    }

    if (ImGui::Button("Add Decorator"))
    {
        ImGui::OpenPopup("AddDecoratorPopup");
    }
    if (ImGui::BeginPopup("AddDecoratorPopup"))
    {
        auto AddSimple = [&](const char* DecName)
        {
            FBTAuxNode A;
            A.Kind = EBTAuxKind::Decorator;
            A.Name = FName(DecName);
            Node.Decorators.push_back(A);
            Asset->BumpVersion();
            MarkDirty();
        };
        if (ImGui::MenuItem("Condition (observer-abort gate)"))
        {
            FBTAuxNode A;
            A.Kind     = EBTAuxKind::Decorator;
            A.Name     = FName("Condition");
            A.KeyValue = FName("TargetThreatening");
            A.IntValue = 1; // 게이트는 기본적으로 observer-abort 켬
            Node.Decorators.push_back(A);
            Asset->BumpVersion();
            MarkDirty();
        }
        if (ImGui::MenuItem("Cooldown"))
        {
            FBTAuxNode A;
            A.Kind       = EBTAuxKind::Decorator;
            A.Name       = FName("Cooldown");
            A.FloatValue = 2.0f;
            Node.Decorators.push_back(A);
            Asset->BumpVersion();
            MarkDirty();
        }
        if (ImGui::MenuItem("Inverter"))     AddSimple("Inverter");
        if (ImGui::MenuItem("ForceSuccess")) AddSimple("ForceSuccess");
        if (ImGui::MenuItem("ForceFailure")) AddSimple("ForceFailure");
        ImGui::EndPopup();
    }
}

void FBehaviorTreeEditorWidget::RenderServices(UBehaviorTreeAsset* Asset, FBTGraphNode& Node)
{
    ImGui::Separator();
    ImGui::TextUnformatted("Services (periodic — composites)");

    int RemoveIndex = -1;
    for (int i = 0; i < static_cast<int>(Node.Services.size()); ++i)
    {
        FBTAuxNode&   Aux  = Node.Services[i];
        const FString SStr = Aux.Name.IsValid() ? Aux.Name.ToString() : FString("?");
        ImGui::PushID(2000 + i);
        ImGui::BulletText("%s", SStr.c_str());
        if (Aux.Name == FName("BrainSense"))
        {
            if (ImGui::DragFloat("Interval", &Aux.FloatValue, 0.01f, 0.0f, 5.0f))
            {
                Asset->BumpVersion();
                MarkDirty();
            }
        }
        if (ImGui::SmallButton("Remove"))
        {
            RemoveIndex = i;
        }
        ImGui::PopID();
        ImGui::Separator();
    }
    if (RemoveIndex >= 0)
    {
        Node.Services.erase(Node.Services.begin() + RemoveIndex);
        Asset->BumpVersion();
        MarkDirty();
    }

    if (ImGui::Button("Add Service"))
    {
        ImGui::OpenPopup("AddServicePopup");
    }
    if (ImGui::BeginPopup("AddServicePopup"))
    {
        if (ImGui::MenuItem("Brain Sense"))
        {
            FBTAuxNode A;
            A.Kind       = EBTAuxKind::Service;
            A.Name       = FName("BrainSense");
            A.FloatValue = 0.25f;
            Node.Services.push_back(A);
            Asset->BumpVersion();
            MarkDirty();
        }
        ImGui::EndPopup();
    }
}

// =============================================================================
//  Undo/Redo (FMemoryArchive 스냅샷) + Copy/Paste/Duplicate
// =============================================================================
TArray<uint8> FBehaviorTreeEditorWidget::MakeSnapshot(UBehaviorTreeAsset* Asset) const
{
    TArray<uint8> Empty;
    if (!Asset) return Empty;
    FMemoryArchive Saver(true);
    Asset->Serialize(Saver);
    return Saver.GetBuffer();
}

bool FBehaviorTreeEditorWidget::RestoreSnapshot(UBehaviorTreeAsset* Asset, const TArray<uint8>& Snapshot)
{
    if (!Asset || Snapshot.empty()) return false;
    bRestoringSnapshot = true;
    FMemoryArchive Loader(Snapshot, false);
    Asset->Serialize(Loader);
    Asset->BumpVersion();
    bRestoringSnapshot   = false;
    bPositionsPushed     = false;
    InspectedNodeId      = 0;
    LastCommittedVersion = Asset->GetRuntimeVersion();
    MarkDirty();
    return true;
}

void FBehaviorTreeEditorWidget::CommitEdit(UBehaviorTreeAsset* Asset)
{
    if (!Asset || bRestoringSnapshot) return;
    TArray<uint8> Snap = MakeSnapshot(Asset);
    if (!Snap.empty() && (UndoStack.empty() || UndoStack.back() != Snap))
    {
        UndoStack.push_back(Snap);
        if (UndoStack.size() > 128)
        {
            UndoStack.erase(UndoStack.begin());
        }
    }
    RedoStack.clear();
}

void FBehaviorTreeEditorWidget::UndoEdit(UBehaviorTreeAsset* Asset)
{
    if (!Asset || UndoStack.size() <= 1) return;
    RedoStack.push_back(UndoStack.back());
    UndoStack.pop_back();
    RestoreSnapshot(Asset, UndoStack.back());
}

void FBehaviorTreeEditorWidget::RedoEdit(UBehaviorTreeAsset* Asset)
{
    if (!Asset || RedoStack.empty()) return;
    TArray<uint8> Snap = RedoStack.back();
    RedoStack.pop_back();
    UndoStack.push_back(Snap);
    RestoreSnapshot(Asset, Snap);
}

void FBehaviorTreeEditorWidget::CopySelected(UBehaviorTreeAsset* Asset)
{
    ClipboardNodes.clear();
    ClipboardLinks.clear();
    if (SelectedNodeIds.empty()) return;

    std::unordered_set<uint32> SelPins;
    for (uint32 Id : SelectedNodeIds)
    {
        if (const FBTGraphNode* N = Asset->FindNode(Id))
        {
            ClipboardNodes.push_back(*N);
            for (const FBTPin& Pin : N->Pins)
            {
                SelPins.insert(Pin.PinId);
            }
        }
    }
    for (const FBTLink& L : Asset->GetLinks())
    {
        if (SelPins.count(L.FromPinId) && SelPins.count(L.ToPinId))
        {
            ClipboardLinks.push_back(L);
        }
    }
}

void FBehaviorTreeEditorWidget::PasteClipboard(UBehaviorTreeAsset* Asset)
{
    if (ClipboardNodes.empty()) return;

    std::unordered_map<uint32, uint32> PinMap; // 옛 pin id → 새 pin id
    uint32 FirstNewNode = 0;
    for (const FBTGraphNode& CN : ClipboardNodes)
    {
        if (CN.Type == EBTGraphNodeType::Root) continue; // 루트 중복 금지
        FBTGraphNode* NewNode = Asset->AddNodeOfType(CN.Type, CN.PosX + 40.0f, CN.PosY + 40.0f);
        if (!NewNode) continue;
        NewNode->NameValue   = CN.NameValue;
        NewNode->KeyValue    = CN.KeyValue;
        NewNode->BoolValue   = CN.BoolValue;
        NewNode->IntValue    = CN.IntValue;
        NewNode->FloatValue  = CN.FloatValue;
        NewNode->FloatValue2 = CN.FloatValue2;
        NewNode->StringValue = CN.StringValue;
        NewNode->DisplayName = CN.DisplayName;
        NewNode->Decorators  = CN.Decorators;
        NewNode->Services    = CN.Services;
        for (const FBTPin& OldPin : CN.Pins)
        {
            for (const FBTPin& NewPin : NewNode->Pins)
            {
                if (OldPin.Kind == NewPin.Kind) PinMap[OldPin.PinId] = NewPin.PinId;
            }
        }
        if (FirstNewNode == 0) FirstNewNode = NewNode->NodeId;
    }
    for (const FBTLink& L : ClipboardLinks)
    {
        auto F = PinMap.find(L.FromPinId);
        auto T = PinMap.find(L.ToPinId);
        if (F != PinMap.end() && T != PinMap.end())
        {
            Asset->AddLink(F->second, T->second);
        }
    }
    bPositionsPushed = false;
    if (FirstNewNode != 0) InspectedNodeId = FirstNewNode;
    MarkDirty();
}

void FBehaviorTreeEditorWidget::DuplicateSelected(UBehaviorTreeAsset* Asset)
{
    CopySelected(Asset);
    PasteClipboard(Asset);
}
