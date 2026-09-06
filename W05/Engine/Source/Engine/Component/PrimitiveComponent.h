#pragma once

#include "Core/Containers/Array.h"
#include "Core/Geometry/Primitives/AABB.h"
#include "Core/Geometry/Primitives/Triangle.h"
#include "Core/Math/Color.h"
#include "Core/Platform/PlatformTypes.h"
#include "Engine/Component/SceneComponent.h"

class UMaterial;

class UPrimitiveComponent : public USceneComponent
{
    DECLARE_RTTI(UPrimitiveComponent, USceneComponent)

  public:
    virtual ~UPrimitiveComponent() override = default;

    const FColor &GetColor() const;
    void          SetColor(const FColor &NewColor);

    const Geometry::FAABB &GetWorldAABB() const;
    bool                   GetWorldAABB(Geometry::FAABB &OutWorldAABB) const;

    virtual bool            GetLocalTriangles(TArray<Geometry::FTriangle> &OutTriangles) const;
    virtual Geometry::FAABB GetLocalAABB() const;

    virtual UMaterial *GetRenderMaterial(int32 SlotIndex) const;

    void UpdateBounds();

  protected:
    void OnTransformChanged() override;

  protected:
    FColor          Color;
    bool            bBoundsDirty = true;
    Geometry::FAABB WorldAABB;
};
