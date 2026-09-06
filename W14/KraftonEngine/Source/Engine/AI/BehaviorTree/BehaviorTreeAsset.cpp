#include "AI/BehaviorTree/BehaviorTreeAsset.h"

#include "Object/GarbageCollection.h"
#include "Serialization/Archive.h"
#include "SimpleJSON/json.hpp"

#include <algorithm>

namespace
{
    constexpr uint32 BehaviorTreeAssetMagic         = 0x42545231; // BTR1
    constexpr uint32 BehaviorTreeAssetFormatVersion = 1;

    const char* GraphNodeTypeDisplayName(EBTGraphNodeType Type)
    {
        switch (Type)
        {
        case EBTGraphNodeType::Root:      return "Root";
        case EBTGraphNodeType::Selector:  return "Selector";
        case EBTGraphNodeType::Sequence:  return "Sequence";
        case EBTGraphNodeType::Parallel:  return "Parallel";
        case EBTGraphNodeType::Task:      return "Task";
        case EBTGraphNodeType::Condition: return "Condition";
        default:                          return "Node";
        }
    }

    // FBTAuxNode(데코레이터/서비스) ↔ JSON.
    json::JSON AuxToJson(const FBTAuxNode& A)
    {
        json::JSON J = json::Object();
        J["kind"]        = static_cast<int>(A.Kind);
        J["name"]        = A.Name.ToString();
        J["keyValue"]    = A.KeyValue.ToString();
        J["boolValue"]   = A.BoolValue;
        J["intValue"]    = static_cast<int>(A.IntValue);
        J["floatValue"]  = static_cast<double>(A.FloatValue);
        J["stringValue"] = A.StringValue;
        return J;
    }

    FBTAuxNode AuxFromJson(json::JSON J)
    {
        FBTAuxNode A;
        A.Kind        = static_cast<EBTAuxKind>(J["kind"].ToInt());
        A.Name        = FName(FString(J["name"].ToString()));
        A.KeyValue    = FName(FString(J["keyValue"].ToString()));
        A.BoolValue   = J["boolValue"].ToBool();
        A.IntValue    = static_cast<int32>(J["intValue"].ToInt());
        A.FloatValue  = static_cast<float>(J["floatValue"].ToFloat());
        A.StringValue = FString(J["stringValue"].ToString());
        return A;
    }
}

// ── TArray<T> operator<< 전방 선언 (FBTGraphNode 가 Pins/Decorators/Services 를 품어 필요) ──
FArchive& operator<<(FArchive& Ar, TArray<FBTPin>& Array);
FArchive& operator<<(FArchive& Ar, TArray<FBTLink>& Array);
FArchive& operator<<(FArchive& Ar, TArray<FBTAuxNode>& Array);
FArchive& operator<<(FArchive& Ar, TArray<FBTGraphNode>& Array);
FArchive& operator<<(FArchive& Ar, TArray<FBTBlackboardKey>& Array);
FArchive& operator<<(FArchive& Ar, TArray<FBTDiagnostic>& Array);

FArchive& operator<<(FArchive& Ar, FBTPin& Pin)
{
    Ar << Pin.PinId;
    Ar << Pin.OwningNodeId;
    Ar << Pin.Kind;
    Ar << Pin.DisplayName;
    return Ar;
}

FArchive& operator<<(FArchive& Ar, FBTLink& Link)
{
    Ar << Link.LinkId;
    Ar << Link.FromPinId;
    Ar << Link.ToPinId;
    return Ar;
}

FArchive& operator<<(FArchive& Ar, FBTAuxNode& Aux)
{
    Ar << Aux.Kind;
    Ar << Aux.Name;
    Ar << Aux.KeyValue;
    Ar << Aux.BoolValue;
    Ar << Aux.IntValue;
    Ar << Aux.FloatValue;
    Ar << Aux.StringValue;
    return Ar;
}

FArchive& operator<<(FArchive& Ar, FBTGraphNode& Node)
{
    Ar << Node.NodeId;
    Ar << Node.Type;
    Ar << Node.DisplayName;
    Ar << Node.PosX;
    Ar << Node.PosY;
    Ar << Node.Pins;
    Ar << Node.NameValue;
    Ar << Node.KeyValue;
    Ar << Node.BoolValue;
    Ar << Node.IntValue;
    Ar << Node.FloatValue;
    Ar << Node.FloatValue2;
    Ar << Node.StringValue;
    Ar << Node.Decorators;
    Ar << Node.Services;
    return Ar;
}

FArchive& operator<<(FArchive& Ar, FBTBlackboardKey& Key)
{
    Ar << Key.Name;
    Ar << Key.Type;
    return Ar;
}

FArchive& operator<<(FArchive& Ar, FBTDiagnostic& Diagnostic)
{
    Ar << Diagnostic.Severity;
    Ar << Diagnostic.NodeId;
    Ar << Diagnostic.Message;
    return Ar;
}

FArchive& operator<<(FArchive& Ar, TArray<FBTPin>& Array)
{
    uint32 N = static_cast<uint32>(Array.size());
    Ar << N;
    if (Ar.IsLoading()) Array.resize(N);
    for (auto& Item : Array) Ar << Item;
    return Ar;
}

FArchive& operator<<(FArchive& Ar, TArray<FBTLink>& Array)
{
    uint32 N = static_cast<uint32>(Array.size());
    Ar << N;
    if (Ar.IsLoading()) Array.resize(N);
    for (auto& Item : Array) Ar << Item;
    return Ar;
}

FArchive& operator<<(FArchive& Ar, TArray<FBTAuxNode>& Array)
{
    uint32 N = static_cast<uint32>(Array.size());
    Ar << N;
    if (Ar.IsLoading()) Array.resize(N);
    for (auto& Item : Array) Ar << Item;
    return Ar;
}

FArchive& operator<<(FArchive& Ar, TArray<FBTGraphNode>& Array)
{
    uint32 N = static_cast<uint32>(Array.size());
    Ar << N;
    if (Ar.IsLoading()) Array.resize(N);
    for (auto& Item : Array) Ar << Item;
    return Ar;
}

FArchive& operator<<(FArchive& Ar, TArray<FBTBlackboardKey>& Array)
{
    uint32 N = static_cast<uint32>(Array.size());
    Ar << N;
    if (Ar.IsLoading()) Array.resize(N);
    for (auto& Item : Array) Ar << Item;
    return Ar;
}

FArchive& operator<<(FArchive& Ar, TArray<FBTDiagnostic>& Array)
{
    uint32 N = static_cast<uint32>(Array.size());
    Ar << N;
    if (Ar.IsLoading()) Array.resize(N);
    for (auto& Item : Array) Ar << Item;
    return Ar;
}

// =============================================================================
//  Build API
// =============================================================================
FBTGraphNode* UBehaviorTreeAsset::AddNode(EBTGraphNodeType Type, const FName& DisplayName, float X, float Y)
{
    FBTGraphNode Node;
    Node.NodeId      = AllocateId();
    Node.Type        = Type;
    Node.DisplayName = DisplayName.IsValid() ? DisplayName : FName(GraphNodeTypeDisplayName(Type));
    Node.PosX        = X;
    Node.PosY        = Y;
    Nodes.push_back(std::move(Node));
    BumpVersion();
    return &Nodes.back();
}

FBTPin* UBehaviorTreeAsset::AddPin(FBTGraphNode& Node, EBTPinKind Kind, const FName& DisplayName)
{
    FBTPin Pin;
    Pin.PinId        = AllocateId();
    Pin.OwningNodeId = Node.NodeId;
    Pin.Kind         = Kind;
    Pin.DisplayName  = DisplayName;
    Node.Pins.push_back(std::move(Pin));
    BumpVersion();
    return &Node.Pins.back();
}

FBTGraphNode* UBehaviorTreeAsset::AddNodeOfType(EBTGraphNodeType Type, float X, float Y)
{
    FBTGraphNode* Node = AddNode(Type, FName(GraphNodeTypeDisplayName(Type)), X, Y);
    if (!Node)
    {
        return nullptr;
    }

    // 표준 핀 레이아웃: Root 는 Child 만, leaf(Task/Condition)는 Parent 만, Comment 는 핀 없음, 나머지는 둘 다.
    const bool bHasParent = (Type != EBTGraphNodeType::Root && Type != EBTGraphNodeType::Comment);
    const bool bHasChild  = (Type != EBTGraphNodeType::Task && Type != EBTGraphNodeType::Condition && Type != EBTGraphNodeType::Comment);
    if (bHasParent)
    {
        AddPin(*Node, EBTPinKind::Parent, FName("In"));
    }
    if (bHasChild)
    {
        AddPin(*Node, EBTPinKind::Child, FName("Out"));
    }
    return Node;
}

FBTLink* UBehaviorTreeAsset::AddLink(uint32 FromPinId, uint32 ToPinId)
{
    uint32 ResolvedFromPinId = 0;
    uint32 ResolvedToPinId   = 0;
    if (!CanLinkPins(FromPinId, ToPinId, &ResolvedFromPinId, &ResolvedToPinId))
    {
        return nullptr;
    }

    // 자식은 부모가 하나뿐: 대상 Parent 핀에 이미 들어오는 link 가 있으면 제거(교체).
    Links.erase(
        std::remove_if(
            Links.begin(),
            Links.end(),
            [ResolvedToPinId](const FBTLink& Existing) { return Existing.ToPinId == ResolvedToPinId; }
        ),
        Links.end()
    );

    FBTLink Link;
    Link.LinkId    = AllocateId();
    Link.FromPinId = ResolvedFromPinId;
    Link.ToPinId   = ResolvedToPinId;
    Links.push_back(std::move(Link));
    BumpVersion();
    return &Links.back();
}

FBTBlackboardKey* UBehaviorTreeAsset::AddBlackboardKey(const FName& Name, EBTBlackboardKeyType Type)
{
    if (!Name.IsValid())
    {
        return nullptr;
    }
    for (FBTBlackboardKey& Existing : BlackboardKeys)
    {
        if (Existing.Name == Name)
        {
            Existing.Type = Type;
            BumpEditorVersion();
            return &Existing;
        }
    }
    FBTBlackboardKey Key;
    Key.Name = Name;
    Key.Type = Type;
    BlackboardKeys.push_back(std::move(Key));
    BumpEditorVersion();
    return &BlackboardKeys.back();
}

bool UBehaviorTreeAsset::RemoveBlackboardKey(const FName& Name)
{
    const size_t Before = BlackboardKeys.size();
    BlackboardKeys.erase(
        std::remove_if(
            BlackboardKeys.begin(),
            BlackboardKeys.end(),
            [&Name](const FBTBlackboardKey& Key) { return Key.Name == Name; }
        ),
        BlackboardKeys.end()
    );
    if (BlackboardKeys.size() != Before)
    {
        BumpEditorVersion();
        return true;
    }
    return false;
}

void UBehaviorTreeAsset::InitializeDefault()
{
    Nodes.clear();
    Links.clear();
    BlackboardKeys.clear();
    Diagnostics.clear();
    NextId = 1;

    // 즉시 동작하는 스타터 트리(에디터 없이도 사용 가능):
    //   selector { sequence { CanSeeTarget?, Chase, Attack }, Idle }
    // 노드 추가 시 Nodes 벡터가 재할당될 수 있으므로 포인터를 들고 다니지 않고 NodeId 로 다룬다.
    const uint32 RootId     = AddNodeOfType(EBTGraphNodeType::Root,      0.0f,    0.0f)->NodeId;
    const uint32 SelectorId = AddNodeOfType(EBTGraphNodeType::Selector,  240.0f,  0.0f)->NodeId;
    const uint32 SequenceId = AddNodeOfType(EBTGraphNodeType::Sequence,  480.0f, -100.0f)->NodeId;
    const uint32 IdleId     = AddNodeOfType(EBTGraphNodeType::Task,      520.0f,  140.0f)->NodeId;
    const uint32 CanSeeId   = AddNodeOfType(EBTGraphNodeType::Condition, 720.0f, -220.0f)->NodeId;
    const uint32 ChaseId    = AddNodeOfType(EBTGraphNodeType::Task,      760.0f, -100.0f)->NodeId;
    const uint32 AttackId   = AddNodeOfType(EBTGraphNodeType::Task,      820.0f,   20.0f)->NodeId;

    // leaf 의 구체 task/condition id 와 표시명을 채운다(컴파일러 MakeLeaf 매핑과 일치).
    auto SetLeaf = [this](uint32 NodeId, const char* TaskId)
    {
        if (FBTGraphNode* Node = FindNode(NodeId))
        {
            Node->NameValue   = FName(TaskId);
            Node->DisplayName = FName(TaskId);
        }
    };
    SetLeaf(IdleId,   "Idle");
    SetLeaf(CanSeeId, "CanSeeTarget");
    SetLeaf(ChaseId,  "Chase");
    SetLeaf(AttackId, "Attack");

    // NodeId → 해당 종류 핀 id (재할당 안전한 재조회).
    auto PinId = [this](uint32 NodeId, EBTPinKind Kind) -> uint32
    {
        if (const FBTGraphNode* Node = FindNode(NodeId))
        {
            for (const FBTPin& Pin : Node->Pins)
            {
                if (Pin.Kind == Kind) return Pin.PinId;
            }
        }
        return 0;
    };

    AddLink(PinId(RootId, EBTPinKind::Child),     PinId(SelectorId, EBTPinKind::Parent));
    AddLink(PinId(SelectorId, EBTPinKind::Child), PinId(SequenceId, EBTPinKind::Parent));
    AddLink(PinId(SelectorId, EBTPinKind::Child), PinId(IdleId,     EBTPinKind::Parent));
    AddLink(PinId(SequenceId, EBTPinKind::Child), PinId(CanSeeId,   EBTPinKind::Parent));
    AddLink(PinId(SequenceId, EBTPinKind::Child), PinId(ChaseId,    EBTPinKind::Parent));
    AddLink(PinId(SequenceId, EBTPinKind::Child), PinId(AttackId,   EBTPinKind::Parent));

    BumpVersion();
}

// =============================================================================
//  Edit API
// =============================================================================
bool UBehaviorTreeAsset::RemoveNode(uint32 NodeId)
{
    const FBTGraphNode* Node = FindNode(NodeId);
    if (!Node)
    {
        return false;
    }

    // 이 노드의 핀이 걸린 link 를 모두 cascade 삭제.
    TArray<uint32> OwnedPins;
    for (const FBTPin& Pin : Node->Pins)
    {
        OwnedPins.push_back(Pin.PinId);
    }
    Links.erase(
        std::remove_if(
            Links.begin(),
            Links.end(),
            [&OwnedPins](const FBTLink& Link)
            {
                for (uint32 PinId : OwnedPins)
                {
                    if (Link.FromPinId == PinId || Link.ToPinId == PinId) return true;
                }
                return false;
            }
        ),
        Links.end()
    );

    Nodes.erase(
        std::remove_if(
            Nodes.begin(),
            Nodes.end(),
            [NodeId](const FBTGraphNode& N) { return N.NodeId == NodeId; }
        ),
        Nodes.end()
    );
    BumpVersion();
    return true;
}

bool UBehaviorTreeAsset::RemoveLink(uint32 LinkId)
{
    const size_t Before = Links.size();
    Links.erase(
        std::remove_if(
            Links.begin(),
            Links.end(),
            [LinkId](const FBTLink& Link) { return Link.LinkId == LinkId; }
        ),
        Links.end()
    );
    if (Links.size() != Before)
    {
        BumpVersion();
        return true;
    }
    return false;
}

bool UBehaviorTreeAsset::RemoveInvalidLinks()
{
    const size_t Before = Links.size();
    Links.erase(
        std::remove_if(
            Links.begin(),
            Links.end(),
            [this](const FBTLink& Link)
            {
                return FindPin(Link.FromPinId) == nullptr || FindPin(Link.ToPinId) == nullptr;
            }
        ),
        Links.end()
    );
    return Links.size() != Before;
}

// =============================================================================
//  Validation
// =============================================================================
bool UBehaviorTreeAsset::CanLinkPins(uint32 PinAId, uint32 PinBId, uint32* OutFromPinId, uint32* OutToPinId) const
{
    const FBTPin* PinA = FindPin(PinAId);
    const FBTPin* PinB = FindPin(PinBId);
    if (!PinA || !PinB || PinAId == PinBId)
    {
        return false;
    }
    // 하나는 부모의 Child(출력), 하나는 자식의 Parent(입력)여야 한다.
    if (PinA->Kind == PinB->Kind)
    {
        return false;
    }
    const FBTPin* FromPin = (PinA->Kind == EBTPinKind::Child) ? PinA : PinB;
    const FBTPin* ToPin   = (PinA->Kind == EBTPinKind::Child) ? PinB : PinA;
    if (FromPin->OwningNodeId == ToPin->OwningNodeId)
    {
        return false; // 자기 자신
    }

    // 사이클 방지: 자식이 될 노드(To)가 부모가 될 노드(From)의 조상이면 안 된다.
    uint32 Ancestor = FromPin->OwningNodeId;
    while (Ancestor != 0)
    {
        if (Ancestor == ToPin->OwningNodeId)
        {
            return false;
        }
        // 부모로 한 칸 올라간다(노드의 Parent 핀으로 들어오는 link 의 출발 노드).
        uint32 NextAncestor = 0;
        for (const FBTLink& Link : Links)
        {
            const FBTPin* LinkToPin = FindPin(Link.ToPinId);
            if (LinkToPin && LinkToPin->OwningNodeId == Ancestor && LinkToPin->Kind == EBTPinKind::Parent)
            {
                const FBTPin* LinkFromPin = FindPin(Link.FromPinId);
                if (LinkFromPin)
                {
                    NextAncestor = LinkFromPin->OwningNodeId;
                }
                break;
            }
        }
        Ancestor = NextAncestor;
    }

    if (OutFromPinId) *OutFromPinId = FromPin->PinId;
    if (OutToPinId)   *OutToPinId   = ToPin->PinId;
    return true;
}

bool UBehaviorTreeAsset::HasRootNode() const
{
    return FindRootNode() != nullptr;
}

bool UBehaviorTreeAsset::Compile()
{
    TArray<FBTDiagnostic> NewDiagnostics;

    // 단일 Root 검증.
    int32 RootCount = 0;
    for (const FBTGraphNode& Node : Nodes)
    {
        if (Node.Type == EBTGraphNodeType::Root) ++RootCount;
    }
    if (RootCount == 0)
    {
        NewDiagnostics.push_back({EBTDiagnosticSeverity::Error, 0, "Behavior Tree has no Root node."});
    }
    else if (RootCount > 1)
    {
        NewDiagnostics.push_back({EBTDiagnosticSeverity::Error, 0, "Behavior Tree has more than one Root node."});
    }

    for (const FBTGraphNode& Node : Nodes)
    {
        const bool bIsLeaf = (Node.Type == EBTGraphNodeType::Task || Node.Type == EBTGraphNodeType::Condition);

        // Root 가 아닌 노드는 부모가 있어야 한다.
        if (Node.Type != EBTGraphNodeType::Root)
        {
            bool bHasParent = false;
            for (const FBTPin& Pin : Node.Pins)
            {
                if (Pin.Kind != EBTPinKind::Parent) continue;
                for (const FBTLink& Link : Links)
                {
                    if (Link.ToPinId == Pin.PinId) { bHasParent = true; break; }
                }
            }
            if (!bHasParent)
            {
                NewDiagnostics.push_back({EBTDiagnosticSeverity::Warning, Node.NodeId, "Node has no parent (unreachable)."});
            }
        }

        // leaf 는 구체 task/condition id(NameValue)가 있어야 한다.
        if (bIsLeaf && !Node.NameValue.IsValid())
        {
            NewDiagnostics.push_back({EBTDiagnosticSeverity::Warning, Node.NodeId, "Leaf node has no task/condition assigned."});
        }

        // Composite/Root 는 자식이 있어야 의미가 있다.
        if (!bIsLeaf)
        {
            TArray<const FBTGraphNode*> Children;
            GatherOrderedChildren(Node.NodeId, Children);
            if (Children.empty())
            {
                NewDiagnostics.push_back({EBTDiagnosticSeverity::Warning, Node.NodeId, "Composite node has no children."});
            }
        }
    }

    bool bSuccess = true;
    for (const FBTDiagnostic& D : NewDiagnostics)
    {
        if (D.Severity == EBTDiagnosticSeverity::Error) { bSuccess = false; break; }
    }
    SetCompileResult(std::move(NewDiagnostics), bSuccess);
    return bSuccess;
}

void UBehaviorTreeAsset::SetCompileResult(TArray<FBTDiagnostic>&& InDiagnostics, bool bSuccess)
{
    Diagnostics           = std::move(InDiagnostics);
    bLastCompileSucceeded = bSuccess;
    bCompileDirty         = false;
}

bool UBehaviorTreeAsset::HasCompileErrors() const
{
    for (const FBTDiagnostic& D : Diagnostics)
    {
        if (D.Severity == EBTDiagnosticSeverity::Error) return true;
    }
    return false;
}

// =============================================================================
//  Lookup
// =============================================================================
FBTGraphNode* UBehaviorTreeAsset::FindNode(uint32 NodeId)
{
    for (FBTGraphNode& Node : Nodes)
    {
        if (Node.NodeId == NodeId) return &Node;
    }
    return nullptr;
}

const FBTGraphNode* UBehaviorTreeAsset::FindNode(uint32 NodeId) const
{
    for (const FBTGraphNode& Node : Nodes)
    {
        if (Node.NodeId == NodeId) return &Node;
    }
    return nullptr;
}

FBTPin* UBehaviorTreeAsset::FindPin(uint32 PinId)
{
    for (FBTGraphNode& Node : Nodes)
    {
        for (FBTPin& Pin : Node.Pins)
        {
            if (Pin.PinId == PinId) return &Pin;
        }
    }
    return nullptr;
}

const FBTPin* UBehaviorTreeAsset::FindPin(uint32 PinId) const
{
    for (const FBTGraphNode& Node : Nodes)
    {
        for (const FBTPin& Pin : Node.Pins)
        {
            if (Pin.PinId == PinId) return &Pin;
        }
    }
    return nullptr;
}

FBTGraphNode* UBehaviorTreeAsset::FindNodeByPin(uint32 PinId)
{
    for (FBTGraphNode& Node : Nodes)
    {
        for (const FBTPin& Pin : Node.Pins)
        {
            if (Pin.PinId == PinId) return &Node;
        }
    }
    return nullptr;
}

const FBTGraphNode* UBehaviorTreeAsset::FindRootNode() const
{
    for (const FBTGraphNode& Node : Nodes)
    {
        if (Node.Type == EBTGraphNodeType::Root) return &Node;
    }
    return nullptr;
}

void UBehaviorTreeAsset::GatherOrderedChildren(uint32 ParentNodeId, TArray<const FBTGraphNode*>& OutChildren) const
{
    OutChildren.clear();
    const FBTGraphNode* Parent = FindNode(ParentNodeId);
    if (!Parent)
    {
        return;
    }
    // 부모의 Child 핀들에서 나가는 link 가 가리키는 자식 노드 수집.
    for (const FBTPin& Pin : Parent->Pins)
    {
        if (Pin.Kind != EBTPinKind::Child) continue;
        for (const FBTLink& Link : Links)
        {
            if (Link.FromPinId != Pin.PinId) continue;
            const FBTPin* ToPin = FindPin(Link.ToPinId);
            if (!ToPin) continue;
            if (const FBTGraphNode* Child = FindNode(ToPin->OwningNodeId))
            {
                OutChildren.push_back(Child);
            }
        }
    }
    // 실행 순서 = 자식 노드의 PosX 오름차순(UE 동일).
    std::sort(
        OutChildren.begin(),
        OutChildren.end(),
        [](const FBTGraphNode* A, const FBTGraphNode* B) { return A->PosX < B->PosX; }
    );
}

// =============================================================================
//  Serialization
// =============================================================================
// =============================================================================
//  JSON 입출력 (id 보존 왕복)
// =============================================================================
FString UBehaviorTreeAsset::ExportGraphToJsonString() const
{
    json::JSON Root = json::Object();
    Root["format"]  = "KraftonBehaviorTree";
    Root["version"] = 1;

    json::JSON NodeArr = json::Array();
    unsigned   NodeIndex = 0;
    for (const FBTGraphNode& Node : Nodes)
    {
        json::JSON J = json::Object();
        J["id"]          = static_cast<int>(Node.NodeId);
        J["type"]        = static_cast<int>(Node.Type);
        J["displayName"] = Node.DisplayName.ToString();
        json::JSON Pos = json::Array();
        Pos[0] = static_cast<double>(Node.PosX);
        Pos[1] = static_cast<double>(Node.PosY);
        J["pos"]         = Pos;
        J["nameValue"]   = Node.NameValue.ToString();
        J["keyValue"]    = Node.KeyValue.ToString();
        J["boolValue"]   = Node.BoolValue;
        J["intValue"]    = static_cast<int>(Node.IntValue);
        J["floatValue"]  = static_cast<double>(Node.FloatValue);
        J["floatValue2"] = static_cast<double>(Node.FloatValue2);
        J["stringValue"] = Node.StringValue;

        json::JSON PinArr = json::Array();
        unsigned   PinIndex = 0;
        for (const FBTPin& Pin : Node.Pins)
        {
            json::JSON P = json::Object();
            P["id"]           = static_cast<int>(Pin.PinId);
            P["owningNodeId"] = static_cast<int>(Pin.OwningNodeId);
            P["kind"]         = static_cast<int>(Pin.Kind);
            P["displayName"]  = Pin.DisplayName.ToString();
            PinArr[PinIndex++] = P;
        }
        J["pins"] = PinArr;

        json::JSON DecArr = json::Array();
        unsigned   DecIndex = 0;
        for (const FBTAuxNode& D : Node.Decorators) DecArr[DecIndex++] = AuxToJson(D);
        J["decorators"] = DecArr;

        json::JSON SvcArr = json::Array();
        unsigned   SvcIndex = 0;
        for (const FBTAuxNode& S : Node.Services) SvcArr[SvcIndex++] = AuxToJson(S);
        J["services"] = SvcArr;

        NodeArr[NodeIndex++] = J;
    }
    Root["nodes"] = NodeArr;

    json::JSON LinkArr = json::Array();
    unsigned   LinkIndex = 0;
    for (const FBTLink& Link : Links)
    {
        json::JSON L = json::Object();
        L["id"]   = static_cast<int>(Link.LinkId);
        L["from"] = static_cast<int>(Link.FromPinId);
        L["to"]   = static_cast<int>(Link.ToPinId);
        LinkArr[LinkIndex++] = L;
    }
    Root["links"] = LinkArr;

    json::JSON KeyArr = json::Array();
    unsigned   KeyIndex = 0;
    for (const FBTBlackboardKey& K : BlackboardKeys)
    {
        json::JSON Kj = json::Object();
        Kj["name"] = K.Name.ToString();
        Kj["type"] = static_cast<int>(K.Type);
        KeyArr[KeyIndex++] = Kj;
    }
    Root["blackboardKeys"] = KeyArr;

    return FString(Root.dump());
}

bool UBehaviorTreeAsset::ImportGraphFromJsonString(const FString& Text)
{
    json::JSON Root = json::JSON::Load(Text);
    if (Root.JSONType() != json::JSON::Class::Object || !Root.hasKey("nodes"))
    {
        return false;
    }

    Nodes.clear();
    Links.clear();
    BlackboardKeys.clear();
    Diagnostics.clear();

    uint32 MaxId = 0;

    json::JSON NodeArr = Root["nodes"];
    for (unsigned i = 0; i < static_cast<unsigned>(NodeArr.size()); ++i)
    {
        json::JSON   J = NodeArr[i];
        FBTGraphNode Node;
        Node.NodeId      = static_cast<uint32>(J["id"].ToInt());
        Node.Type        = static_cast<EBTGraphNodeType>(J["type"].ToInt());
        Node.DisplayName = FName(FString(J["displayName"].ToString()));
        if (J.hasKey("pos") && J["pos"].size() >= 2)
        {
            Node.PosX = static_cast<float>(J["pos"][0].ToFloat());
            Node.PosY = static_cast<float>(J["pos"][1].ToFloat());
        }
        Node.NameValue   = FName(FString(J["nameValue"].ToString()));
        Node.KeyValue    = FName(FString(J["keyValue"].ToString()));
        Node.BoolValue   = J["boolValue"].ToBool();
        Node.IntValue    = static_cast<int32>(J["intValue"].ToInt());
        Node.FloatValue  = static_cast<float>(J["floatValue"].ToFloat());
        Node.FloatValue2 = static_cast<float>(J["floatValue2"].ToFloat());
        Node.StringValue = FString(J["stringValue"].ToString());

        if (J.hasKey("pins"))
        {
            json::JSON PinArr = J["pins"];
            for (unsigned p = 0; p < static_cast<unsigned>(PinArr.size()); ++p)
            {
                json::JSON Pj = PinArr[p];
                FBTPin     Pin;
                Pin.PinId        = static_cast<uint32>(Pj["id"].ToInt());
                Pin.OwningNodeId = static_cast<uint32>(Pj["owningNodeId"].ToInt());
                Pin.Kind         = static_cast<EBTPinKind>(Pj["kind"].ToInt());
                Pin.DisplayName  = FName(FString(Pj["displayName"].ToString()));
                if (Pin.OwningNodeId == 0)
                {
                    Pin.OwningNodeId = Node.NodeId;
                }
                MaxId = (std::max)(MaxId, Pin.PinId);
                Node.Pins.push_back(Pin);
            }
        }
        if (J.hasKey("decorators"))
        {
            json::JSON A = J["decorators"];
            for (unsigned d = 0; d < static_cast<unsigned>(A.size()); ++d) Node.Decorators.push_back(AuxFromJson(A[d]));
        }
        if (J.hasKey("services"))
        {
            json::JSON A = J["services"];
            for (unsigned s = 0; s < static_cast<unsigned>(A.size()); ++s) Node.Services.push_back(AuxFromJson(A[s]));
        }
        MaxId = (std::max)(MaxId, Node.NodeId);
        Nodes.push_back(Node);
    }

    if (Root.hasKey("links"))
    {
        json::JSON LinkArr = Root["links"];
        for (unsigned i = 0; i < static_cast<unsigned>(LinkArr.size()); ++i)
        {
            json::JSON L = LinkArr[i];
            FBTLink    Link;
            Link.LinkId    = static_cast<uint32>(L["id"].ToInt());
            Link.FromPinId = static_cast<uint32>(L["from"].ToInt());
            Link.ToPinId   = static_cast<uint32>(L["to"].ToInt());
            MaxId = (std::max)(MaxId, Link.LinkId);
            Links.push_back(Link);
        }
    }

    if (Root.hasKey("blackboardKeys"))
    {
        json::JSON KeyArr = Root["blackboardKeys"];
        for (unsigned i = 0; i < static_cast<unsigned>(KeyArr.size()); ++i)
        {
            json::JSON       Kj = KeyArr[i];
            FBTBlackboardKey K;
            K.Name = FName(FString(Kj["name"].ToString()));
            K.Type = static_cast<EBTBlackboardKeyType>(Kj["type"].ToInt());
            BlackboardKeys.push_back(K);
        }
    }

    NextId = MaxId + 1;
    RemoveInvalidLinks();
    BumpVersion();
    return true;
}

void UBehaviorTreeAsset::Serialize(FArchive& Ar)
{
    if (Ar.IsSaving())
    {
        uint32 Magic         = BehaviorTreeAssetMagic;
        uint32 FormatVersion = BehaviorTreeAssetFormatVersion;
        Ar << Magic;
        Ar << FormatVersion;
        Ar << NextId;
        Ar << Version;
        Ar << RuntimeVersion;
        Ar << bLastCompileSucceeded;
        Ar << Nodes;
        Ar << Links;
        Ar << BlackboardKeys;
        return;
    }

    uint32 Magic = 0;
    Ar << Magic;
    if (Magic != BehaviorTreeAssetMagic)
    {
        // 알 수 없는 포맷 — 기본 상태 유지(빈 그래프).
        return;
    }
    uint32 FormatVersion = 0;
    Ar << FormatVersion;
    Ar << NextId;
    Ar << Version;
    Ar << RuntimeVersion;
    Ar << bLastCompileSucceeded;
    Ar << Nodes;
    Ar << Links;
    Ar << BlackboardKeys;
    bCompileDirty = true; // 로드 후 매니저가 검증/컴파일하도록.
}

void UBehaviorTreeAsset::AddReferencedObjects(FReferenceCollector& Collector)
{
    UObject::AddReferencedObjects(Collector);
    // 자산은 UObject 참조를 보유하지 않는다(순수 데이터 그래프).
}
