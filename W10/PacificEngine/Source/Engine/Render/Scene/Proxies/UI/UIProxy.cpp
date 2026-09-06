#include "Render/Scene/Proxies/UI/UIProxy.h"

#include "Engine/Runtime/Engine.h"
#include "GameFramework/AActor.h"
#include "Resource/FontManager.h"
#include "Texture/Texture2D.h"
#include "UI/TextUIComponent.h"
#include "UI/TextureUIComponent.h"

FUIProxy::FUIProxy(UUIComponent* InOwner)
    : Owner(InOwner)
{
    UpdateLayout();
    UpdateStyle();
    UpdateVisibility();
}

void FUIProxy::UpdateLayout()
{
    if (!Owner)
    {
        return;
    }

    RenderSpace = Owner->GetRenderSpace();
    GeometryType = Owner->GetGeometryType();
    ElementType = Owner->GetUIElementType();
    AnchorMin = Owner->GetAnchorMin();
    AnchorMax = Owner->GetAnchorMax();
    AnchoredPosition = Owner->GetAnchoredPosition();
    SizeDelta = Owner->GetSizeDelta();
    WorldSize = Owner->GetWorldSize();
    bBillboard = Owner->IsBillboard();
    Pivot = Owner->GetPivot();
    RotationDegrees = Owner->GetRotationDegrees();
    SubUVRect = FVector4(0.0f, 0.0f, 1.0f, 1.0f);
    if (const UTextureUIComponent* TextureOwner = Cast<UTextureUIComponent>(Owner))
    {
        SubUVRect = TextureOwner->GetSubUVRect();
    }
    WorldMatrix = Owner->GetWorldMatrix();
    Layer = Owner->GetLayer();
    ZOrder = Owner->GetZOrder();
}

FVector2 FUIProxy::GetScreenLayoutSize(float ViewportWidth, float ViewportHeight) const
{
    const FVector2 AnchorSize(
        (AnchorMax.X - AnchorMin.X) * ViewportWidth,
        (AnchorMax.Y - AnchorMin.Y) * ViewportHeight);

    return FVector2(
        AnchorSize.X + SizeDelta.X,
        AnchorSize.Y + SizeDelta.Y);
}

FVector2 FUIProxy::GetScreenPivotPosition(float ViewportWidth, float ViewportHeight) const
{
    const FVector2 AnchorMinPixels(
        AnchorMin.X * ViewportWidth,
        AnchorMin.Y * ViewportHeight);
    const FVector2 AnchorSize(
        (AnchorMax.X - AnchorMin.X) * ViewportWidth,
        (AnchorMax.Y - AnchorMin.Y) * ViewportHeight);

    return FVector2(
        AnchorMinPixels.X + AnchorSize.X * Pivot.X + AnchoredPosition.X,
        AnchorMinPixels.Y + AnchorSize.Y * Pivot.Y + AnchoredPosition.Y);
}

void FUIProxy::UpdateStyle()
{
    if (!Owner)
    {
        TintColor = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
        TexturePath.clear();
        Texture = nullptr;
        TextureSRV = nullptr;
        bUseTexture = false;
        Text.clear();
        FontName = FName("Default");
        FontResource = nullptr;
        FontSize = 1.0f;
        TextLetterSpacing = 0.0f;
        TextLineSpacing = 0.0f;
        TextHAlign = EUITextHAlign::Left;
        TextVAlign = EUITextVAlign::Top;
        return;
    }

    TintColor = Owner->GetTintColor();
    ElementType = Owner->GetUIElementType();
    TexturePath.clear();
    Texture = nullptr;
    TextureSRV = nullptr;
    bUseTexture = false;
    Text.clear();
    FontName = FName("Default");
    FontResource = nullptr;
    FontSize = 1.0f;
    TextLetterSpacing = 0.0f;
    TextLineSpacing = 0.0f;
    TextHAlign = EUITextHAlign::Left;
    TextVAlign = EUITextVAlign::Top;

    if (const UTextUIComponent* TextOwner = Cast<UTextUIComponent>(Owner))
    {
        Text = TextOwner->GetText();
        FontName = TextOwner->GetFontName();
        FontResource = FFontManager::Get().FindFont(FontName);
        if (!FontResource || !FontResource->IsLoaded())
        {
            FontResource = TextOwner->GetFont();
        }
        if (!FontResource || !FontResource->IsLoaded())
        {
            FontResource = FFontManager::Get().FindFont(FName("Default"));
        }

        FontSize = TextOwner->GetFontSize();
        TextLetterSpacing = TextOwner->GetLetterSpacing();
        TextLineSpacing = TextOwner->GetLineSpacing();
        TextHAlign = TextOwner->GetHorizontalAlignment();
        TextVAlign = TextOwner->GetVerticalAlignment();
        TextureSRV = (FontResource && FontResource->IsLoaded()) ? FontResource->SRV : nullptr;
        bUseTexture = TextureSRV != nullptr;
        return;
    }

    if (const UTextureUIComponent* TextureOwner = Cast<UTextureUIComponent>(Owner))
    {
        TexturePath = TextureOwner->GetTexturePath();
    }

    if (!TexturePath.empty() && GEngine)
    {
        ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
        Texture = UTexture2D::LoadFromFile(TexturePath, Device);
    }

    TextureSRV = (Texture && Texture->IsLoaded()) ? Texture->GetSRV() : nullptr;
    bUseTexture = TextureSRV != nullptr;
}

void FUIProxy::UpdateVisibility()
{
    if (!Owner)
    {
        bVisible = false;
        bHitTestVisible = false;
        return;
    }

    const AActor* OwnerActor = Owner->GetOwner();
    const bool bActorVisible = !OwnerActor || OwnerActor->IsVisible();
    bVisible = bActorVisible && Owner->IsVisible();
    bHitTestVisible = bVisible && Owner->IsHitTestVisible();
}
