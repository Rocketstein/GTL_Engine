#include "GameFramework/ButtonActor.h"

#include "Object/ObjectFactory.h"
#include "UI/ButtonComponent.h"

IMPLEMENT_CLASS(AButtonActor, AActor)

void AButtonActor::InitDefaultComponents()
{
    ButtonComponent = AddComponent<UUIButtonComponent>();
    if (!ButtonComponent)
    {
        return;
    }

    ButtonComponent->SetRenderSpace(EUIRenderSpace::ScreenSpace);
    ButtonComponent->SetGeometryType(EUIGeometryType::Quad);
    ButtonComponent->SetAnchorMin(FVector2(0.0f, 0.0f));
    ButtonComponent->SetAnchorMax(FVector2(0.0f, 0.0f));
    ButtonComponent->SetAnchoredPosition(FVector2(100.0f, 100.0f));
    ButtonComponent->SetSizeDelta(FVector2(200.0f, 80.0f));
    ButtonComponent->SetPivot(FVector2(0.0f, 0.0f));
    ButtonComponent->SetNormalTint(FVector4(0.22f, 0.45f, 0.95f, 1.0f));
    ButtonComponent->SetHoveredTint(FVector4(0.30f, 0.55f, 1.0f, 1.0f));
    ButtonComponent->SetPressedTint(FVector4(0.16f, 0.34f, 0.78f, 1.0f));
    ButtonComponent->SetDisabledTint(FVector4(0.25f, 0.28f, 0.34f, 0.65f));
    ButtonComponent->SetInteractable(true);

    SetRootComponent(ButtonComponent);
}
