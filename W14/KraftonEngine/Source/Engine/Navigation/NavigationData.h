#pragma once

#include "GameFramework/AActor.h"
#include "Navigation/NavTypes.h"

class UWorld;

#include "Source/Engine/Navigation/NavigationData.generated.h"

// ============================================================
// ANavigationData
// 월드가 미리 인식한 길 데이터를 보관하는 추상 베이스.
// UNavigationSystem은 MoveTo 요청 시 이 데이터 위에서 Project/FindPath를 수행한다.
// ============================================================
UCLASS()
class ANavigationData : public AActor
{
public:
	GENERATED_BODY()
	ANavigationData();
	~ANavigationData() override = default;

	UFUNCTION(Callable, Category="Navigation")
	virtual bool RebuildNavigationData();
	UFUNCTION(Callable, Category="Navigation")
	virtual void ClearNavigationData();

	UFUNCTION(Pure, Category="Navigation")
	virtual bool IsNavigationDataBuilt() const { return bNavigationDataBuilt; }
	UFUNCTION(Pure, Category="Navigation")
	virtual int32 GetNavigationNodeCount() const { return 0; }
	UFUNCTION(Pure, Category="Navigation")
	virtual int32 GetBlockedNodeCount() const { return 0; }

	UFUNCTION(Callable, Category="Navigation")
	virtual bool ProjectPointToNavigation(const FVector& Point, FVector& OutProjectedPoint, const FNavAgentProperties& AgentProps) const;
	UFUNCTION(Callable, Category="Navigation")
	virtual bool FindPath(const FVector& Start, const FVector& End, const FNavAgentProperties& AgentProps, FNavigationPath& OutPath) const;
	UFUNCTION(Callable, Category="Navigation")
	virtual bool NavigationRaycast(const FVector& Start, const FVector& End, const FNavAgentProperties& AgentProps) const;
	UFUNCTION(Callable, Category="Navigation")
	virtual bool GetRandomReachablePointInRadius(const FVector& Origin, float Radius, FVector& OutPoint, const FNavAgentProperties& AgentProps) const;

	UFUNCTION(Callable, Category="Navigation|Debug")
	virtual void DebugDrawNavigationData(float Duration = 0.0f) const;

	UFUNCTION(Pure, Category="Navigation")
	FNavAgentProperties GetSupportedAgent() const { return SupportedAgent; }
	UFUNCTION(Callable, Category="Navigation")
	void SetSupportedAgent(const FNavAgentProperties& InAgent) { SupportedAgent = InAgent; bNavigationDataBuilt = false; }

protected:
	UPROPERTY(Edit, Save, Category="Navigation|Agent", DisplayName="Supported Agent")
	FNavAgentProperties SupportedAgent;

	bool bNavigationDataBuilt = false;
};
