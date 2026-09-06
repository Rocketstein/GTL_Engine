#include "PointLightActor.h"
#include "Component/BillboardComponent.h"
#include "Component/Light/PointLightComponent.h"
#include "Materials/MaterialManager.h"

IMPLEMENT_CLASS(APointLightActor, AActor)

void APointLightActor::InitDefaultComponents()
{
	LightComponent = AddComponent<UPointLightComponent>();
	LightComponent->SetCanDeleteFromDetails(false);
	SetRootComponent(LightComponent);

	BillboardComponent = LightComponent->EnsureEditorBillboard();
	if (BillboardComponent)
	{
		BillboardComponent->SetCanDeleteFromDetails(false);
	}
}
