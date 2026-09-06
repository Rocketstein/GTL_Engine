#include "HeightFogActor.h"
#include "Component/HeightFogComponent.h"
#include "Component/BillboardComponent.h"
#include "Materials/MaterialManager.h"

IMPLEMENT_CLASS(AHeightFogActor, AActor)

AHeightFogActor::AHeightFogActor()
{
}

void AHeightFogActor::InitDefaultComponents()
{
	FogComponent = AddComponent<UHeightFogComponent>();
	FogComponent->SetCanDeleteFromDetails(false);
	SetRootComponent(FogComponent);

	BillboardComponent = FogComponent->EnsureEditorBillboard();
	if (BillboardComponent)
	{
		BillboardComponent->SetCanDeleteFromDetails(false);
	}
}
