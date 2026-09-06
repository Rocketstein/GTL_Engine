#include "GameFramework/TextureUIActor.h"

#include "Object/ObjectFactory.h"
#include "UI/TextureUIComponent.h"

IMPLEMENT_CLASS(ATextureUIActor, AActor)

void ATextureUIActor::InitDefaultComponents()
{
    TextureUIComponent = AddComponent<UTextureUIComponent>();
    if (!TextureUIComponent)
    {
        return;
    }

    TextureUIComponent->SetRenderSpace(EUIRenderSpace::ScreenSpace);
    TextureUIComponent->SetGeometryType(EUIGeometryType::Quad);
    TextureUIComponent->SetAnchorMin(FVector2(0.0f, 0.0f));
    TextureUIComponent->SetAnchorMax(FVector2(0.0f, 0.0f));
    TextureUIComponent->SetAnchoredPosition(FVector2(100.0f, 100.0f));
    TextureUIComponent->SetSizeDelta(FVector2(128.0f, 128.0f));
    TextureUIComponent->SetPivot(FVector2(0.0f, 0.0f));
    TextureUIComponent->SetTintColor(FVector4(1.0f, 1.0f, 1.0f, 1.0f));
    TextureUIComponent->ResetSubUVRect();

    SetRootComponent(TextureUIComponent);
}

void ATextureUIActor::SetTexturePath(const FString& InTexturePath)
{
    if (TextureUIComponent)
    {
        TextureUIComponent->SetTexturePath(InTexturePath);
    }
}
