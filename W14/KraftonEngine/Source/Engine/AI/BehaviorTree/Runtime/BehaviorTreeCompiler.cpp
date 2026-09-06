#include "AI/BehaviorTree/Runtime/BehaviorTreeCompiler.h"

#include "AI/BehaviorTree/BehaviorTreeAsset.h"
#include "AI/BehaviorTree/BTNodeRegistry.h"
#include "AI/BehaviorTree/Runtime/BTNode.h"
#include "AI/BehaviorTree/Runtime/BTDecorators.h" // FBTDecorator_Base::SetChild
#include "AI/BehaviorTree/Runtime/BTTasks.h"      // Idle 폴백
#include "Component/AI/BehaviorTreeComponent.h"

namespace
{
    FBTNode_Base* BuildNode(const UBehaviorTreeAsset& Asset, const FBTGraphNode& Node, UBehaviorTreeComponent& Owner);

    // leaf: 레지스트리에서 NameValue 로 팩토리 조회(파라미터는 팩토리가 Node 에서 주입). 미지원 → Idle.
    FBTNode_Base* MakeLeaf(const FBTGraphNode& Node, UBehaviorTreeComponent& Owner)
    {
        if (const FBTLeafType* T = FindBTLeafType(Node.NameValue))
        {
            if (T->Make)
            {
                return T->Make(Owner, Node);
            }
        }
        return Owner.MakeNode<FBTTask_Idle>();
    }

    FBTCompositeNode* MakeComposite(EBTGraphNodeType Type, UBehaviorTreeComponent& Owner)
    {
        switch (Type)
        {
        case EBTGraphNodeType::Selector: return Owner.MakeNode<FBTComposite_Selector>();
        case EBTGraphNodeType::Sequence: return Owner.MakeNode<FBTComposite_Sequence>();
        case EBTGraphNodeType::Parallel: return Owner.MakeNode<FBTComposite_Parallel>();
        default:                         return nullptr;
        }
    }

    // 데코레이터: 레지스트리에서 Name 으로 조회 → 자식 wrap. 미지원 → 자식 그대로.
    FBTNode_Base* MakeDecorator(const FBTAuxNode& Aux, FBTNode_Base* ChildNode, UBehaviorTreeComponent& Owner)
    {
        if (const FBTDecoratorType* T = FindBTDecoratorType(Aux.Name))
        {
            if (T->Make)
            {
                if (FBTDecorator_Base* Dec = T->Make(Owner, Aux))
                {
                    Dec->SetChild(ChildNode);
                    return Dec;
                }
            }
        }
        return ChildNode;
    }

    FBTNode_Base* BuildCore(const UBehaviorTreeAsset& Asset, const FBTGraphNode& Node, UBehaviorTreeComponent& Owner)
    {
        switch (Node.Type)
        {
        case EBTGraphNodeType::Task:
        case EBTGraphNodeType::Condition:
            return MakeLeaf(Node, Owner);

        case EBTGraphNodeType::Selector:
        case EBTGraphNodeType::Sequence:
        case EBTGraphNodeType::Parallel:
        {
            FBTCompositeNode* Composite = MakeComposite(Node.Type, Owner);
            if (!Composite)
            {
                return nullptr;
            }
            TArray<const FBTGraphNode*> Children;
            Asset.GatherOrderedChildren(Node.NodeId, Children);
            for (const FBTGraphNode* Child : Children)
            {
                if (!Child) continue;
                if (FBTNode_Base* ChildNode = BuildNode(Asset, *Child, Owner))
                {
                    Composite->AddChild(ChildNode);
                }
            }
            // 서비스 부착(레지스트리 조회).
            for (const FBTAuxNode& Svc : Node.Services)
            {
                if (const FBTServiceType* ST = FindBTServiceType(Svc.Name))
                {
                    if (ST->Make)
                    {
                        if (FBTService_Base* S = ST->Make(Owner, Svc))
                        {
                            Composite->AddService(S);
                        }
                    }
                }
            }
            return Composite;
        }

        case EBTGraphNodeType::Root:
        {
            TArray<const FBTGraphNode*> Children;
            Asset.GatherOrderedChildren(Node.NodeId, Children);
            if (Children.empty() || !Children[0])
            {
                return nullptr;
            }
            return BuildNode(Asset, *Children[0], Owner);
        }

        default:
            return nullptr;
        }
    }

    // 코어 노드 build 후 부착된 데코레이터로 감싼다(첫 번째 데코레이터가 가장 바깥).
    FBTNode_Base* BuildNode(const UBehaviorTreeAsset& Asset, const FBTGraphNode& Node, UBehaviorTreeComponent& Owner)
    {
        FBTNode_Base* Core = BuildCore(Asset, Node, Owner);
        if (!Core)
        {
            return nullptr;
        }
        for (int32 Index = static_cast<int32>(Node.Decorators.size()) - 1; Index >= 0; --Index)
        {
            Core = MakeDecorator(Node.Decorators[Index], Core, Owner);
        }
        return Core;
    }
}

FBTNode_Base* FBehaviorTreeCompiler::Compile(const UBehaviorTreeAsset& Asset, UBehaviorTreeComponent& Owner)
{
    const FBTGraphNode* Root = Asset.FindRootNode();
    if (!Root)
    {
        return nullptr;
    }
    return BuildNode(Asset, *Root, Owner);
}
