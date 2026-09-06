#include "GameFramework/ScreenTextActor.h"

#include "Component/UIScreenTextComponent.h"

IMPLEMENT_CLASS(AScreenTextActor, AActor)

void AScreenTextActor::InitDefaultComponents()
{
	TextRenderComponent = AddComponent<UUIScreenTextComponent>();
	if (!TextRenderComponent)
	{
		return;
	}

	TextRenderComponent->SetCanDeleteFromDetails(false);
	SetRootComponent(TextRenderComponent);
}
