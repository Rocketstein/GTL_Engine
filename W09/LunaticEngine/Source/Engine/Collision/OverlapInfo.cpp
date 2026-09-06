#include "OverlapInfo.h"
#include "GameFramework/AActor.h"
#include "Component/PrimitiveComponent.h"

AActor* FHitResult::GetActor() {
	if (!Component || !Component->GetOwner()) return nullptr;
	return Component->GetOwner();
}