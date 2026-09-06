#include "AmbientLightActor.h"
#include "Component/BillboardComponent.h"
#include "Component/Light/LightComponentBase.h"
#include "Component/Light/AmbientLightComponent.h"
#include "Materials/MaterialManager.h"

IMPLEMENT_CLASS(AAmbientLightActor, AActor)

void AAmbientLightActor::InitDefaultComponents()
{
	LightComponent = AddComponent<UAmbientLightComponent>();
	LightComponent->SetCanDeleteFromDetails(false);
	SetRootComponent(LightComponent);

	BillboardComponent = LightComponent->EnsureEditorBillboard();
	if (BillboardComponent)
	{
		BillboardComponent->SetCanDeleteFromDetails(false);
	}
}
