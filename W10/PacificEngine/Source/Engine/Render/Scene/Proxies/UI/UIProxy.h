#pragma once

#include "Core/ResourceTypes.h"
#include "Render/Scene/Proxies/SceneProxy.h"
#include "UI/UIComponent.h"

struct ID3D11ShaderResourceView;
class UTexture2D;

class FUIProxy : public FSceneProxy
{
public:
    explicit FUIProxy(UUIComponent* InOwner);

    virtual void UpdateLayout();
    virtual void UpdateStyle();
    virtual void UpdateVisibility();

    FVector2 GetScreenLayoutSize(float ViewportWidth, float ViewportHeight) const;
    FVector2 GetScreenPivotPosition(float ViewportWidth, float ViewportHeight) const;
    const FVector2& GetWorldLayoutSize() const { return WorldSize; }

    UUIComponent* Owner = nullptr;

    EUIRenderSpace RenderSpace = EUIRenderSpace::ScreenSpace;
    EUIGeometryType GeometryType = EUIGeometryType::Quad;
    EUIElementType ElementType = EUIElementType::Image;

    FVector2 AnchorMin = { 0.0f, 0.0f };
    FVector2 AnchorMax = { 0.0f, 0.0f };
    FVector2 AnchoredPosition = { 0.0f, 0.0f };
    FVector2 SizeDelta = { 100.0f, 100.0f };
    FVector2 WorldSize = { 1.0f, 1.0f };
    bool bBillboard = false;
    FVector2 Pivot = { 0.5f, 0.5f };
    float RotationDegrees = 0.0f;

    FMatrix WorldMatrix;
    int32 Layer = 0;
    int32 ZOrder = 0;

    bool bVisible = true;
    bool bHitTestVisible = true;

    FVector4 TintColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    FString TexturePath;
    FVector4 SubUVRect = { 0.0f, 0.0f, 1.0f, 1.0f };
    UTexture2D* Texture = nullptr;
    ID3D11ShaderResourceView* TextureSRV = nullptr;
    bool bUseTexture = false;

    FString Text;
    FName FontName = FName("Default");
    const FFontResource* FontResource = nullptr;
    float FontSize = 1.0f;
    float TextLetterSpacing = 0.0f;
    float TextLineSpacing = 0.0f;
    EUITextHAlign TextHAlign = EUITextHAlign::Left;
    EUITextVAlign TextVAlign = EUITextVAlign::Top;
};
