#include "GameFramework/Actor/NavMeshBoundsVolume.h"
#include "Component/Shape/BoxComponent.h"
#include "GameFramework/World.h"
#include "Navigation/NavigationSystem.h"

void ANavMeshBoundsVolume::InitDefaultComponents(const FVector& Extent)
{
	BoundsComponent = AddComponent<UBoxComponent>();
	if (BoundsComponent)
	{
		BoundsComponent->SetBoxExtent(Extent);
		BoundsComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		SetRootComponent(BoundsComponent);
	}
}

void ANavMeshBoundsVolume::PostDuplicate()
{
	Super::PostDuplicate();
	BoundsComponent = GetComponentByClass<UBoxComponent>();
}

UBoxComponent* ANavMeshBoundsVolume::GetBoundsComponent() const
{
	// BoundsComponent는 직렬화되지 않는 런타임 캐시다. PostDuplicate(PIE 복제)에서는 재바인딩되지만
	// 씬 로드(역직렬화) 경로에는 대응 훅이 없어 에디터 월드에서 null로 남는다. 그 경우 CollectBuildBounds가
	// 이 볼륨을 건너뛰고 fallback(전체 지오메트리 AABB)으로 빠져, 에디터/PIE의 navmesh 범위가 어긋난다.
	// 모든 경로(로드/복제/스폰)를 덮도록 null이면 지연 resolve 한다.
	if (!BoundsComponent.IsValid())
	{
		const_cast<ANavMeshBoundsVolume*>(this)->BoundsComponent = GetComponentByClass<UBoxComponent>();
	}
	return BoundsComponent.Get();
}

bool ANavMeshBoundsVolume::ContainsPoint(const FVector& Point) const
{
	UBoxComponent* Box = BoundsComponent.Get();
	if (!Box)
	{
		return false;
	}
	const FVector Center = Box->GetWorldLocation();
	const FVector Extent = Box->GetScaledBoxExtent();
	return Point.X >= Center.X - Extent.X && Point.X <= Center.X + Extent.X
		&& Point.Y >= Center.Y - Extent.Y && Point.Y <= Center.Y + Extent.Y
		&& Point.Z >= Center.Z - Extent.Z && Point.Z <= Center.Z + Extent.Z;
}

void ANavMeshBoundsVolume::PostEditProperty(const char* PropertyName)
{
	Super::PostEditProperty(PropertyName);
	if (UWorld* World = GetWorld())
	{
		if (UNavigationSystem* NavSys = World->GetNavigationSystem())
		{
			NavSys->InvalidateNavigationData();
		}
	}
}
