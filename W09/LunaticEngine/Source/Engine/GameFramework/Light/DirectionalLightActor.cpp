#include "DirectionalLightActor.h"
#include "Component/BillboardComponent.h"
#include "Component/Light/DirectionalLightComponent.h"
#include "Materials/MaterialManager.h"
IMPLEMENT_CLASS(ADirectionalLightActor, AActor)

void ADirectionalLightActor::InitDefaultComponents()
{
	LightComponent = AddComponent<UDirectionalLightComponent>();
	LightComponent->SetCanDeleteFromDetails(false);
	SetRootComponent(LightComponent);

	BillboardComponent = LightComponent->EnsureEditorBillboard();
	if (BillboardComponent)
	{
		BillboardComponent->SetCanDeleteFromDetails(false);
	}
}
