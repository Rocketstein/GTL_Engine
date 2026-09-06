#include "SpotLightActor.h"
#include "Component/BillboardComponent.h"
#include "Component/Light/SpotLightComponent.h"
#include "Materials/MaterialManager.h"

IMPLEMENT_CLASS(ASpotLightActor, AActor)

void ASpotLightActor::InitDefaultComponents()
{
	LightComponent = AddComponent<USpotLightComponent>();
	LightComponent->SetCanDeleteFromDetails(false);
	SetRootComponent(LightComponent);

	BillboardComponent = LightComponent->EnsureEditorBillboard();
	if (BillboardComponent)
	{
		BillboardComponent->SetCanDeleteFromDetails(false);
	}
}
