#include "UIComponent.h"

#include "Core/RayTypes.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Object/ObjectFactory.h"
#include "Render/Scene/Proxies/UI/UIProxy.h"
#include "Render/Scene/Scene.h"
#include "Serialization/Archive.h"

#include <algorithm>
#include <cmath>
#include <cstring>

IMPLEMENT_CLASS(UUIComponent, USceneComponent)

namespace
{
constexpr FEnumPropertyOption GUIRenderSpaceOptions[] = {
    { "ScreenSpace", static_cast<int32_t>(EUIRenderSpace::ScreenSpace) },
    { "WorldSpace", static_cast<int32_t>(EUIRenderSpace::WorldSpace) },
};

constexpr FEnumPropertyMeta GUIRenderSpaceMeta = {
    "EUIRenderSpace",
    GUIRenderSpaceOptions,
    static_cast<int32_t>(sizeof(GUIRenderSpaceOptions) / sizeof(GUIRenderSpaceOptions[0])),
};

constexpr FEnumPropertyOption GUIGeometryTypeOptions[] = {
    { "Quad", static_cast<int32_t>(EUIGeometryType::Quad) },
    { "Mesh", static_cast<int32_t>(EUIGeometryType::Mesh) },
};

constexpr FEnumPropertyMeta GUIGeometryTypeMeta = {
    "EUIGeometryType",
    GUIGeometryTypeOptions,
    static_cast<int32_t>(sizeof(GUIGeometryTypeOptions) / sizeof(GUIGeometryTypeOptions[0])),
};
} // namespace

UUIComponent::~UUIComponent()
{
    DestroyRenderState();
}

void UUIComponent::CreateRenderState()
{
    if (UIProxy || !Owner || !Owner->GetWorld())
    {
        return;
    }

    UIProxy = Owner->GetWorld()->GetScene().AddUI(this);
}

void UUIComponent::DestroyRenderState()
{
    if (!UIProxy)
    {
        return;
    }

    if (Owner)
    {
        if (UWorld* World = Owner->GetWorld())
        {
            World->GetScene().RemoveUI(UIProxy);
        }
    }

    UIProxy = nullptr;
}

void UUIComponent::Serialize(FArchive& Ar)
{
    USceneComponent::Serialize(Ar);
    Ar << RenderSpace;
    Ar << GeometryType;
    Ar << AnchorMin;
    Ar << AnchorMax;
    Ar << AnchoredPosition;
    Ar << SizeDelta;
    Ar << WorldSize;
    Ar << bBillboard;
    Ar << Pivot;
    Ar << RotationDegrees;
    Ar << Layer;
    Ar << ZOrder;
    Ar << bVisible;
    Ar << bHitTestVisible;
    Ar << TintColor;
}

void UUIComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    USceneComponent::GetEditableProperties(OutProps);
    OutProps.push_back({ "Render Space", EPropertyType::Enum, &RenderSpace, 0.0f, 0.0f, 0.1f, &GUIRenderSpaceMeta });
    OutProps.push_back({ "Geometry Type", EPropertyType::Enum, &GeometryType, 0.0f, 0.0f, 0.1f, &GUIGeometryTypeMeta });
    OutProps.push_back({ "Anchor Min", EPropertyType::Vec2, &AnchorMin, 0.0f, 1.0f, 0.01f });
    OutProps.push_back({ "Anchor Max", EPropertyType::Vec2, &AnchorMax, 0.0f, 1.0f, 0.01f });
    OutProps.push_back({ "Anchored Position", EPropertyType::Vec2, &AnchoredPosition });
    OutProps.push_back({ "Size Delta", EPropertyType::Vec2, &SizeDelta });
    OutProps.push_back({ "World Size", EPropertyType::Vec2, &WorldSize });
    OutProps.push_back({ "Billboard", EPropertyType::Bool, &bBillboard });
    OutProps.push_back({ "Pivot", EPropertyType::Vec2, &Pivot });
    OutProps.push_back({ "Rotation Degrees", EPropertyType::Float, &RotationDegrees, -360.0f, 360.0f, 1.0f });
    OutProps.push_back({ "Layer", EPropertyType::Int, &Layer });
    OutProps.push_back({ "Z Order", EPropertyType::Int, &ZOrder });
    OutProps.push_back({ "Visible", EPropertyType::Bool, &bVisible });
    OutProps.push_back({ "Hit Test Visible", EPropertyType::Bool, &bHitTestVisible });
    OutProps.push_back({ "Tint Color", EPropertyType::Color4, &TintColor });
}

void UUIComponent::PostEditProperty(const char* PropertyName)
{
    USceneComponent::PostEditProperty(PropertyName);

    if (std::strcmp(PropertyName, "Visible") == 0 ||
        std::strcmp(PropertyName, "Hit Test Visible") == 0)
    {
        MarkUIVisibilityDirty();
        return;
    }

    if (std::strcmp(PropertyName, "Render Space") == 0 ||
        std::strcmp(PropertyName, "Geometry Type") == 0 ||
        std::strcmp(PropertyName, "Anchor Min") == 0 ||
        std::strcmp(PropertyName, "Anchor Max") == 0 ||
        std::strcmp(PropertyName, "Anchored Position") == 0 ||
        std::strcmp(PropertyName, "Size Delta") == 0 ||
        std::strcmp(PropertyName, "World Size") == 0 ||
        std::strcmp(PropertyName, "Billboard") == 0 ||
        std::strcmp(PropertyName, "Pivot") == 0 ||
        std::strcmp(PropertyName, "Rotation Degrees") == 0 ||
        std::strcmp(PropertyName, "Layer") == 0 ||
        std::strcmp(PropertyName, "Z Order") == 0)
    {
        MarkUILayoutDirty();
        return;
    }

    if (std::strcmp(PropertyName, "Tint Color") == 0)
    {
        MarkUIStyleDirty();
    }
}

FUIProxy* UUIComponent::CreateUIProxy()
{
    return new FUIProxy(this);
}

void UUIComponent::SetRenderSpace(EUIRenderSpace InSpace)
{
    if (RenderSpace == InSpace)
    {
        return;
    }

    RenderSpace = InSpace;
    MarkUILayoutDirty();
}

void UUIComponent::SetGeometryType(EUIGeometryType InType)
{
    if (GeometryType == InType)
    {
        return;
    }

    GeometryType = InType;
    MarkUILayoutDirty();
}

void UUIComponent::SetAnchorMin(const FVector2& InAnchorMin)
{
    if (AnchorMin.X == InAnchorMin.X && AnchorMin.Y == InAnchorMin.Y)
    {
        return;
    }

    AnchorMin = InAnchorMin;
    MarkUILayoutDirty();
}

void UUIComponent::SetAnchorMax(const FVector2& InAnchorMax)
{
    if (AnchorMax.X == InAnchorMax.X && AnchorMax.Y == InAnchorMax.Y)
    {
        return;
    }

    AnchorMax = InAnchorMax;
    MarkUILayoutDirty();
}

void UUIComponent::SetAnchoredPosition(const FVector2& InPosition)
{
    if (AnchoredPosition.X == InPosition.X && AnchoredPosition.Y == InPosition.Y)
    {
        return;
    }

    AnchoredPosition = InPosition;
    MarkUILayoutDirty();
}

void UUIComponent::SetSizeDelta(const FVector2& InSizeDelta)
{
    if (SizeDelta.X == InSizeDelta.X && SizeDelta.Y == InSizeDelta.Y)
    {
        return;
    }

    SizeDelta = InSizeDelta;
    MarkUILayoutDirty();
}

void UUIComponent::SetWorldSize(const FVector2& InWorldSize)
{
    if (WorldSize.X == InWorldSize.X && WorldSize.Y == InWorldSize.Y)
    {
        return;
    }

    WorldSize = InWorldSize;
    MarkUILayoutDirty();
}

void UUIComponent::SetBillboard(bool bInBillboard)
{
    if (bBillboard == bInBillboard)
    {
        return;
    }

    bBillboard = bInBillboard;
    MarkUILayoutDirty();
}

void UUIComponent::SetPivot(const FVector2& InPivot)
{
    if (Pivot.X == InPivot.X && Pivot.Y == InPivot.Y)
    {
        return;
    }

    Pivot = InPivot;
    MarkUILayoutDirty();
}

void UUIComponent::SetRotationDegrees(float InDegrees)
{
    if (RotationDegrees == InDegrees)
    {
        return;
    }

    RotationDegrees = InDegrees;
    MarkUILayoutDirty();
}

void UUIComponent::SetLayer(int32 InLayer)
{
    if (Layer == InLayer)
    {
        return;
    }

    Layer = InLayer;
    MarkUILayoutDirty();
}

void UUIComponent::SetZOrder(int32 InZOrder)
{
    if (ZOrder == InZOrder)
    {
        return;
    }

    ZOrder = InZOrder;
    MarkUILayoutDirty();
}

void UUIComponent::SetVisibility(bool bNewVisible)
{
    if (bVisible == bNewVisible)
    {
        return;
    }

    bVisible = bNewVisible;
    MarkUIVisibilityDirty();
}

void UUIComponent::SetHitTestVisible(bool bNewHitTestVisible)
{
    if (bHitTestVisible == bNewHitTestVisible)
    {
        return;
    }

    bHitTestVisible = bNewHitTestVisible;
    MarkUIVisibilityDirty();
}

void UUIComponent::SetTintColor(const FVector4& InColor)
{
    if (TintColor.X == InColor.X &&
        TintColor.Y == InColor.Y &&
        TintColor.Z == InColor.Z &&
        TintColor.W == InColor.W)
    {
        return;
    }

    TintColor = InColor;
    MarkUIStyleDirty();
}

FVector2 UUIComponent::GetScreenLayoutSize(float ViewportWidth, float ViewportHeight) const
{
    const FVector2 AnchorSize(
        (AnchorMax.X - AnchorMin.X) * ViewportWidth,
        (AnchorMax.Y - AnchorMin.Y) * ViewportHeight);

    return FVector2(
        AnchorSize.X + SizeDelta.X,
        AnchorSize.Y + SizeDelta.Y);
}

FVector2 UUIComponent::GetScreenPivotPosition(float ViewportWidth, float ViewportHeight) const
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

bool UUIComponent::HitTestScreenPoint(const FVector2& ScreenPoint, float ViewportWidth, float ViewportHeight) const
{
    const FVector2 LayoutSize = GetScreenLayoutSize(ViewportWidth, ViewportHeight);
    const bool bActorVisible = !Owner || Owner->IsVisible();
    if (!bActorVisible || !bVisible || !bHitTestVisible ||
        RenderSpace != EUIRenderSpace::ScreenSpace ||
        GeometryType != EUIGeometryType::Quad ||
        LayoutSize.X == 0.0f || LayoutSize.Y == 0.0f)
    {
        return false;
    }

    const FVector2 PivotPosition = GetScreenPivotPosition(ViewportWidth, ViewportHeight);
    const float LocalLeft   = -Pivot.X * LayoutSize.X;
    const float LocalTop    = -Pivot.Y * LayoutSize.Y;
    const float LocalRight  = LocalLeft + LayoutSize.X;
    const float LocalBottom = LocalTop + LayoutSize.Y;

    const float DeltaX = ScreenPoint.X - PivotPosition.X;
    const float DeltaY = ScreenPoint.Y - PivotPosition.Y;
    const float Radians = RotationDegrees * 3.14159265358979323846f / 180.0f;
    const float CosTheta = std::cos(Radians);
    const float SinTheta = std::sin(Radians);

    const float LocalX = DeltaX * CosTheta + DeltaY * SinTheta;
    const float LocalY = -DeltaX * SinTheta + DeltaY * CosTheta;

    return LocalX >= LocalLeft && LocalX < LocalRight &&
           LocalY >= LocalTop && LocalY < LocalBottom;
}

bool UUIComponent::HitTestWorldRay(const FRay& Ray, const FVector& CameraRight, const FVector& CameraUp, const FVector& CameraForward, float& OutDistance) const
{
    OutDistance = 0.0f;

    const bool bActorVisible = !Owner || Owner->IsVisible();
    if (!bActorVisible || !bVisible || !bHitTestVisible ||
        RenderSpace != EUIRenderSpace::WorldSpace ||
        GeometryType != EUIGeometryType::Quad ||
        WorldSize.X == 0.0f || WorldSize.Y == 0.0f)
    {
        return false;
    }

    FVector LocalHit;
    if (bBillboard)
    {
        const FVector PlaneNormal = CameraForward.Normalized();
        const float Denom = Ray.Direction.Dot(PlaneNormal);
        if (std::abs(Denom) < 0.0001f)
        {
            return false;
        }

        const FVector Center = GetWorldLocation();
        const float RayT = (Center - Ray.Origin).Dot(PlaneNormal) / Denom;
        if (RayT < 0.0f)
        {
            return false;
        }

        const FVector WorldHit = Ray.Origin + Ray.Direction * RayT;
        const FVector ToHit = WorldHit - Center;
        const FVector RightAxis = CameraRight.Normalized();
        const FVector UpAxis = CameraUp.Normalized();
        const FVector WorldScale = GetWorldScale();
        const float ScaleY = std::max(std::abs(WorldScale.Y), 0.0001f);
        const float ScaleZ = std::max(std::abs(WorldScale.Z), 0.0001f);
        LocalHit = FVector(0.0f, ToHit.Dot(RightAxis) / ScaleY, ToHit.Dot(UpAxis) / ScaleZ);
        OutDistance = (WorldHit - Ray.Origin).Length();
    }
    else
    {
        const FMatrix InvWorldMatrix = GetWorldMatrix().GetInverse();
        const FVector LocalOrigin = InvWorldMatrix.TransformPositionWithW(Ray.Origin);
        const FVector LocalDirection = InvWorldMatrix.TransformVector(Ray.Direction).Normalized();

        if (std::abs(LocalDirection.X) < 0.0001f)
        {
            return false;
        }

        const float LocalT = -LocalOrigin.X / LocalDirection.X;
        if (LocalT < 0.0f)
        {
            return false;
        }

        LocalHit = LocalOrigin + LocalDirection * LocalT;
    }

    const float Radians = RotationDegrees * 3.14159265358979323846f / 180.0f;
    const float CosTheta = std::cos(Radians);
    const float SinTheta = std::sin(Radians);

    const float UnrotatedY = LocalHit.Y * CosTheta + LocalHit.Z * SinTheta;
    const float UnrotatedZ = -LocalHit.Y * SinTheta + LocalHit.Z * CosTheta;

    const float LocalLeft = -Pivot.X * WorldSize.X;
    const float LocalRight = LocalLeft + WorldSize.X;
    const float LocalTop = (1.0f - Pivot.Y) * WorldSize.Y;
    const float LocalBottom = LocalTop - WorldSize.Y;

    if (UnrotatedY < LocalLeft || UnrotatedY >= LocalRight ||
        UnrotatedZ > LocalTop || UnrotatedZ <= LocalBottom)
    {
        return false;
    }

    if (!bBillboard)
    {
        const FVector WorldHit = GetWorldMatrix().TransformPositionWithW(LocalHit);
        OutDistance = (WorldHit - Ray.Origin).Length();
    }
    return true;
}

bool UUIComponent::HandleUIPointerEvent(const FViewportPointerEvent& Event)
{
    (void)Event;
    return false;
}

void UUIComponent::OnUIPointerEnter(const FViewportPointerEvent& Event)
{
    (void)Event;
}

void UUIComponent::OnUIPointerLeave(const FViewportPointerEvent& Event)
{
    (void)Event;
}

void UUIComponent::MarkUIRenderStateDirty()
{
    DestroyRenderState();
    CreateRenderState();
}

void UUIComponent::MarkUILayoutDirty()
{
    if (!UIProxy || !Owner || !Owner->GetWorld())
    {
        return;
    }

    Owner->GetWorld()->GetScene().MarkUIProxyDirty(UIProxy, ESceneProxyDirtyFlag::Transform);
}

void UUIComponent::MarkUIStyleDirty()
{
    if (!UIProxy || !Owner || !Owner->GetWorld())
    {
        return;
    }

    Owner->GetWorld()->GetScene().MarkUIProxyDirty(UIProxy, ESceneProxyDirtyFlag::Material);
}

void UUIComponent::MarkUIVisibilityDirty()
{
    if (!UIProxy || !Owner || !Owner->GetWorld())
    {
        return;
    }

    Owner->GetWorld()->GetScene().MarkUIProxyDirty(UIProxy, ESceneProxyDirtyFlag::Visibility);
}

void UUIComponent::OnTransformDirty()
{
    USceneComponent::OnTransformDirty();
    MarkUILayoutDirty();
}
