#include "Component/AI/BehaviorTreeComponent.h"

#include "AI/BehaviorTree/BehaviorTreeAsset.h"
#include "AI/BehaviorTree/BehaviorTreeManager.h"
#include "AI/BehaviorTree/Runtime/BTContext.h"
#include "AI/BehaviorTree/Runtime/BTTasks.h"
#include "AI/BehaviorTree/Runtime/BehaviorTreeCompiler.h"
#include "Component/AI/AIBlackboardComponent.h"
#include "GameFramework/AActor.h"
#include "GameFramework/Pawn/EnemyCharacter.h"
#include "Object/GarbageCollection.h"

void UBehaviorTreeComponent::BeginPlay()
{
    UActorComponent::BeginPlay();
    RecompileTreeIfDirty();
}

void UBehaviorTreeComponent::EndPlay()
{
    ClearTree();
    UActorComponent::EndPlay();
}

void UBehaviorTreeComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
    UActorComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);

    RecompileTreeIfDirty(); // 최초 build + 에디터 핫리로드(RuntimeVersion 비교)
    if (!RootNode)
    {
        return;
    }

    ThinkClock.Accumulate(DeltaTime);
    while (ThinkClock.ConsumeStep())
    {
        FBTContext Context;
        BuildContext(Context, ThinkClock.StepSeconds);
        // Enemy 여부와 무관하게 평가. 범용 Task(Wait/Blackboard/Log 등)는 OwnerActor+Blackboard 만으로
        // 동작하고, Brain_* 전투 Task 는 Enemy 가 없으면 자체적으로 Failed 를 반환한다.
        RootNode->Execute(Context);
    }
}

void UBehaviorTreeComponent::AddReferencedObjects(FReferenceCollector& Collector)
{
    UActorComponent::AddReferencedObjects(Collector);
    if (BehaviorTreeAsset)
    {
        Collector.AddReferencedObject(BehaviorTreeAsset);
    }
    if (RootNode)
    {
        RootNode->AddReferencedObjects(Collector);
    }
}

void UBehaviorTreeComponent::SetBehaviorTreePath(const FString& InPath)
{
    if (BehaviorTreePath == InPath)
    {
        return;
    }
    BehaviorTreePath = InPath;
    // 다음 RecompileTreeIfDirty 가 새 경로로 재로드/재컴파일하도록 무효화.
    ClearTree();
    BehaviorTreeAsset      = nullptr;
    LoadedPath.clear();
    CompiledRuntimeVersion = 0;
}

void UBehaviorTreeComponent::RecompileTreeIfDirty()
{
    // 1) 경로 미지정 → 기본은 inert(트리 없음). 테스트 플래그가 켜졌을 때만 데모 트리.
    if (BehaviorTreePath.empty())
    {
        if (bUseDemoTreeWhenNoAsset && !RootNode)
        {
            BuildDemoTree();
            EnsureTreeInitialized();
        }
        return;
    }

    // 2) 경로가 바뀌었으면 자산 재로드(실패해도 재시도 폭주 방지를 위해 LoadedPath 갱신).
    if (LoadedPath != BehaviorTreePath)
    {
        BehaviorTreeAsset      = FBehaviorTreeManager::Get().Load(BehaviorTreePath);
        LoadedPath             = BehaviorTreePath;
        CompiledRuntimeVersion = 0; // 강제 재컴파일
        ClearTree();
    }
    if (!BehaviorTreeAsset)
    {
        return; // 로드 실패 — 트리 없음
    }

    // 3) RuntimeVersion 변경(에디터 편집/핫리로드) 시 재컴파일.
    if (!RootNode || BehaviorTreeAsset->GetRuntimeVersion() != CompiledRuntimeVersion)
    {
        ClearTree();
        CompiledRuntimeVersion = BehaviorTreeAsset->GetRuntimeVersion();
        RootNode               = FBehaviorTreeCompiler::Compile(*BehaviorTreeAsset, *this);
        EnsureTreeInitialized();
    }
}

void UBehaviorTreeComponent::EnsureTreeInitialized()
{
    if (RootNode && !bTreeInitialized)
    {
        FBTContext Context;
        BuildContext(Context, 0.0f);
        RootNode->Initialize(Context);
        bTreeInitialized = true;
    }
}

void UBehaviorTreeComponent::BuildContext(FBTContext& OutContext, float StepDelta)
{
    AActor* OwnerActor    = GetOwner();
    OutContext.OwnerActor = OwnerActor;
    OutContext.Enemy      = Cast<AEnemyCharacter>(OwnerActor);
    OutContext.Blackboard = OwnerActor ? OwnerActor->GetComponentByClass<UAIBlackboardComponent>() : nullptr;
    OutContext.DeltaTime  = StepDelta;
}

void UBehaviorTreeComponent::ClearTree()
{
    RootNode = nullptr;
    OwnedNodes.clear();
    OwnedServices.clear();
    bTreeInitialized = false;
}

void UBehaviorTreeComponent::BuildDemoTree()
{
    // selector { sequence { CanSeeTarget?, Chase, Attack }, Idle }
    FBTComposite_Selector* Root   = MakeNode<FBTComposite_Selector>();
    FBTComposite_Sequence* Engage = MakeNode<FBTComposite_Sequence>();

    Engage->AddChild(MakeNode<FBTCondition_CanSeeTarget>());
    Engage->AddChild(MakeNode<FBTTask_Chase>());
    Engage->AddChild(MakeNode<FBTTask_Attack>());

    Root->AddChild(Engage);
    Root->AddChild(MakeNode<FBTTask_Idle>());

    RootNode = Root;
}
