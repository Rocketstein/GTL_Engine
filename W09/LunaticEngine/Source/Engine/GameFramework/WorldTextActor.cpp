#include "GameFramework/WorldTextActor.h"

#include "Component/TextRenderComponent.h"

IMPLEMENT_CLASS(AWorldTextActor, AActor)

void AWorldTextActor::InitDefaultComponents()
{
	TextRenderComponent = AddComponent<UTextRenderComponent>();
	if (!TextRenderComponent)
	{
		return;
	}

	TextRenderComponent->SetCanDeleteFromDetails(false);
	TextRenderComponent->SetText("World Text");
	TextRenderComponent->SetFont(FName("Default"));
	TextRenderComponent->SetFontSize(1.0f);
	SetRootComponent(TextRenderComponent);
}
