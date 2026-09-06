#include "Cinematic/CinematicWaypointComponent.h"

#include "GameFramework/AActor.h"
#include "Object/GarbageCollection.h"

namespace
{
	// 핀 마커용 작은 스피어. 배치 메뉴의 Sphere 와 동일 에셋 재사용.
	constexpr const char* WaypointMeshPath = "Content/Data/BasicShape/Sphere.OBJ";
	constexpr float WaypointVisualScale = 0.15f;
}

void UCinematicWaypointComponent::InitWaypointVisual()
{
	SetStaticMeshByPath(WaypointMeshPath);
	// 핀은 저작용 마커 — 게임/PIE 월드에서는 렌더하지 않는다.
	SetEditorOnly(true);
	// 핀 스케일은 카메라 트랜스폼과 무관(메시 크기 표시용)하게 작게.
	SetRelativeScale(FVector(WaypointVisualScale, WaypointVisualScale, WaypointVisualScale));
}

void UCinematicWaypointComponent::AddReferencedObjects(FReferenceCollector& Collector)
{
	UStaticMeshComponent::AddReferencedObjects(Collector);
	Collector.AddReferencedObject(LookAtActor, "UCinematicWaypointComponent.LookAtActor");
}
