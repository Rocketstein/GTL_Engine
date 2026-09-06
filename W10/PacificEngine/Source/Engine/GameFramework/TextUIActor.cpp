#include "GameFramework/TextUIActor.h"

#include "Object/ObjectFactory.h"
#include "UI/TextUIComponent.h"

IMPLEMENT_CLASS(ATextUIActor, AActor)

void ATextUIActor::InitDefaultComponents()
{
    TextUIComponent = AddComponent<UTextUIComponent>();
    if (!TextUIComponent)
    {
        return;
    }

    TextUIComponent->SetRenderSpace(EUIRenderSpace::ScreenSpace);
    TextUIComponent->SetGeometryType(EUIGeometryType::Quad);
    TextUIComponent->SetAnchorMin(FVector2(0.0f, 0.0f));
    TextUIComponent->SetAnchorMax(FVector2(0.0f, 0.0f));
    TextUIComponent->SetAnchoredPosition(FVector2(100.0f, 100.0f));
    TextUIComponent->SetSizeDelta(FVector2(240.0f, 40.0f));
    TextUIComponent->SetPivot(FVector2(0.0f, 0.0f));
    TextUIComponent->SetTintColor(FVector4(1.0f, 1.0f, 1.0f, 1.0f));
    TextUIComponent->SetText("Text");
    TextUIComponent->SetFont(FName("Default"));
    TextUIComponent->SetFontSize(1.0f);
    TextUIComponent->SetHorizontalAlignment(EUITextHAlign::Left);
    TextUIComponent->SetVerticalAlignment(EUITextVAlign::Top);

    SetRootComponent(TextUIComponent);
}

void ATextUIActor::SetText(const FString& InText)
{
    if (TextUIComponent)
    {
        TextUIComponent->SetText(InText);
    }
}
