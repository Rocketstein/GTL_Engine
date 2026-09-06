#pragma once

#include "Component/ActorComponent.h"
#include "AI/CombatClock.h"
#include "AI/BehaviorTree/Runtime/BTNode.h"

#include <memory>
#include <utility>

struct FBTContext;
class FReferenceCollector;
class UBehaviorTreeAsset;
class FBTService_Base;

#include "Source/Engine/Component/AI/BehaviorTreeComponent.generated.h"

// =============================================================================
//  UBehaviorTreeComponent — 데이터 기반 Behavior Tree 를 평가하는 AI 정책 드라이버
// =============================================================================
//  세키로식 적 AI 의 제3 정책 드라이버로, BrainScript / BrainBlueprint 와 공존한다
//  (EnemyCharacter::ConfigureBrainDriver 가 우선순위로 하나만 켠다 — Phase 2).
//
//  런타임 노드 트리(FBTNode_Base)를 OwnedNodes(unique_ptr)로 단일 소유하고, 부모-자식
//  참조는 raw pointer 다 (= UAnimInstance::OwnedNodes / MakeNode 패턴). FCombatClock
//  고정 스텝마다 루트를 Execute 해 프레임레이트와 무관하게 결정적으로 think 한다.
//
//  Phase 0: BeginPlay 에서 코드로 데모 트리를 조립한다(에셋/컴파일러 없이 코어 증명).
//  Phase 2: UBehaviorTreeAsset 을 FBehaviorTreeCompiler 로 컴파일해 트리를 만들고
//           Asset.Version 비교로 핫리로드한다.
// =============================================================================
UCLASS()
class UBehaviorTreeComponent : public UActorComponent
{
public:
    GENERATED_BODY()
    UBehaviorTreeComponent()           = default;
    ~UBehaviorTreeComponent() override = default;

    void BeginPlay() override;
    void EndPlay() override;
    void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;
    void AddReferencedObjects(FReferenceCollector& Collector) override;

    // 노드 빌더 — 생성 후 OwnedNodes 에 push, raw 반환. (= UAnimInstance::MakeNode 패턴)
    // 트리의 부모-자식 참조는 raw pointer 로, OwnedNodes 가 모든 노드의 단일 소유자.
    template <typename T, typename... Args>
    T* MakeNode(Args&&... InArgs)
    {
        auto NodePtr = std::make_unique<T>(std::forward<Args>(InArgs)...);
        T*   Raw     = NodePtr.get();
        OwnedNodes.push_back(std::move(NodePtr));
        return Raw;
    }

    // 서비스 빌더 — FBTService_Base 는 FBTNode_Base 가 아니므로 별도 소유 배열에 보관.
    template <typename T, typename... Args>
    T* MakeService(Args&&... InArgs)
    {
        auto Ptr = std::make_unique<T>(std::forward<Args>(InArgs)...);
        T*   Raw = Ptr.get();
        OwnedServices.push_back(std::move(Ptr));
        return Raw;
    }

    UFUNCTION(Pure, Category="AI|BehaviorTree")
    bool HasTree() const { return RootNode != nullptr; }

    // 디스크 UBehaviorTreeAsset 경로. 지정 시 그 자산을 컴파일해 트리를 구동하고,
    // 비우면 Phase 0 데모 트리로 폴백한다(테스트용).
    UFUNCTION(Callable, Category="AI|BehaviorTree")
    void SetBehaviorTreePath(const FString& InPath);
    UFUNCTION(Pure, Category="AI|BehaviorTree")
    const FString& GetBehaviorTreePath() const { return BehaviorTreePath; }

    FBTNode_Base* GetRootNode() const { return RootNode; }

private:
    void BuildContext(FBTContext& OutContext, float StepDelta);
    void ClearTree();
    void EnsureTreeInitialized();
    // BehaviorTreePath 자산을 로드/컴파일. RuntimeVersion 비교로 변경 시에만 재컴파일(핫리로드).
    // 경로가 비어 있으면 코드 데모 트리로 폴백.
    void RecompileTreeIfDirty();
    // 경로 미지정 시 폴백 데모 트리(에디터/에셋 없이 동작 확인용).
    void BuildDemoTree();

    UPROPERTY(Edit, Save, Category="AI|BehaviorTree", DisplayName="Behavior Tree", AssetType="UBehaviorTreeAsset")
    FString BehaviorTreePath;

    // 경로 미지정 시 코드 데모 트리를 돌릴지(테스트용). 기본 false → inert(다른 두뇌 드라이버와
    // 충돌하지 않음). EnemyCharacter 는 BrainBehaviorTreeFile 로만 BT 를 켠다.
    UPROPERTY(Edit, Save, Category="AI|BehaviorTree", DisplayName="Use Demo Tree When No Asset")
    bool bUseDemoTreeWhenNoAsset = false;

    FBTNode_Base*                         RootNode = nullptr;
    TArray<std::unique_ptr<FBTNode_Base>>    OwnedNodes;
    TArray<std::unique_ptr<FBTService_Base>> OwnedServices;

    // 캐시된 자산(매니저가 GC 강참조 소유) + 마지막 컴파일 시점 버전/경로.
    UBehaviorTreeAsset* BehaviorTreeAsset      = nullptr;
    FString             LoadedPath;
    uint32              CompiledRuntimeVersion = 0;

    FCombatClock ThinkClock;
    bool         bTreeInitialized = false;
};
