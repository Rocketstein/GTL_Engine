#include "GameFramework/UIRootActor.h"

#include "Component/CanvasRootComponent.h"
#include "Object/ObjectFactory.h"

IMPLEMENT_CLASS(AUIRootActor, AActor)

void AUIRootActor::InitDefaultComponents()
{
	CanvasRootComponent = AddComponent<UCanvasRootComponent>();
	if (!CanvasRootComponent)
	{
		return;
	}

	CanvasRootComponent->SetCanDeleteFromDetails(false);
	SetRootComponent(CanvasRootComponent);
}
