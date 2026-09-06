#pragma once

#include "Component/SceneComponent.h"

class FUIProxy;
struct FRay;
struct FViewportPointerEvent;

enum class EUIRenderSpace : int32
{
    ScreenSpace,
    WorldSpace
};

enum class EUIGeometryType : int32
{
    Quad,
    Mesh
};

enum class EUIElementType : int32
{
    None,
    Image,
    Text
};

enum class EUITextHAlign : int32
{
    Left,
    Center,
    Right
};

enum class EUITextVAlign : int32
{
    Top,
    Center,
    Bottom
};

class UUIComponent : public USceneComponent
{
public:
    DECLARE_CLASS(UUIComponent, USceneComponent)

    ~UUIComponent() override;

    void CreateRenderState() override;
    void DestroyRenderState() override;

    void Serialize(FArchive& Ar) override;
    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
    void PostEditProperty(const char* PropertyName) override;

    virtual FUIProxy* CreateUIProxy();
    FUIProxy* GetUIProxy() const { return UIProxy; }

    void SetRenderSpace(EUIRenderSpace InSpace);
    EUIRenderSpace GetRenderSpace() const { return RenderSpace; }

    void SetGeometryType(EUIGeometryType InType);
    EUIGeometryType GetGeometryType() const { return GeometryType; }
    virtual EUIElementType GetUIElementType() const { return EUIElementType::None; }

    void SetAnchorMin(const FVector2& InAnchorMin);
    const FVector2& GetAnchorMin() const { return AnchorMin; }

    void SetAnchorMax(const FVector2& InAnchorMax);
    const FVector2& GetAnchorMax() const { return AnchorMax; }

    void SetAnchoredPosition(const FVector2& InPosition);
    const FVector2& GetAnchoredPosition() const { return AnchoredPosition; }

    void SetSizeDelta(const FVector2& InSizeDelta);
    const FVector2& GetSizeDelta() const { return SizeDelta; }

    void SetWorldSize(const FVector2& InWorldSize);
    const FVector2& GetWorldSize() const { return WorldSize; }

    void SetBillboard(bool bInBillboard);
    bool IsBillboard() const { return bBillboard; }

    void SetPivot(const FVector2& InPivot);
    const FVector2& GetPivot() const { return Pivot; }

    void SetRotationDegrees(float InDegrees);
    float GetRotationDegrees() const { return RotationDegrees; }

    void SetLayer(int32 InLayer);
    int32 GetLayer() const { return Layer; }

    void SetZOrder(int32 InZOrder);
    int32 GetZOrder() const { return ZOrder; }

    void SetVisibility(bool bNewVisible);
    bool IsVisible() const { return bVisible; }

    void SetHitTestVisible(bool bNewHitTestVisible);
    bool IsHitTestVisible() const { return bHitTestVisible; }

    void SetTintColor(const FVector4& InColor);
    const FVector4& GetTintColor() const { return TintColor; }

    FVector2 GetScreenLayoutSize(float ViewportWidth, float ViewportHeight) const;
    FVector2 GetScreenPivotPosition(float ViewportWidth, float ViewportHeight) const;
    bool HitTestScreenPoint(const FVector2& ScreenPoint, float ViewportWidth, float ViewportHeight) const;
    bool HitTestWorldRay(const FRay& Ray, const FVector& CameraRight, const FVector& CameraUp, const FVector& CameraForward, float& OutDistance) const;
    virtual bool HandleUIPointerEvent(const FViewportPointerEvent& Event);
    virtual void OnUIPointerEnter(const FViewportPointerEvent& Event);
    virtual void OnUIPointerLeave(const FViewportPointerEvent& Event);

    void MarkUIRenderStateDirty();
    void MarkUILayoutDirty();
    void MarkUIStyleDirty();
    void MarkUIVisibilityDirty();

protected:
    void OnTransformDirty() override;

    FUIProxy* UIProxy = nullptr;

    EUIRenderSpace RenderSpace = EUIRenderSpace::ScreenSpace;
    EUIGeometryType GeometryType = EUIGeometryType::Quad;
    FVector2 AnchorMin = { 0.0f, 0.0f };
    FVector2 AnchorMax = { 0.0f, 0.0f };
    FVector2 AnchoredPosition = { 0.0f, 0.0f };
    FVector2 SizeDelta = { 100.0f, 100.0f };
    FVector2 WorldSize = { 1.0f, 1.0f };
    bool bBillboard = false;
    FVector2 Pivot = { 0.5f, 0.5f };
    float RotationDegrees = 0.0f;
    int32 Layer = 0;
    int32 ZOrder = 0;
    bool bVisible = true;
    bool bHitTestVisible = true;
    FVector4 TintColor = { 1.0f, 1.0f, 1.0f, 1.0f };
};
