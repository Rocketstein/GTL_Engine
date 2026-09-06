#pragma once

#include "Core/Containers/Array.h"
#include "Core/Containers/String.h"
#include "Core/Geometry/Primitives/AABB.h"
#include "Core/Math/Matrix.h"
#include "Core/Math/Vector2.h"
#include "Engine/Component/PrimitiveComponent.h"
#include <memory>

class UStaticMesh;
class UMaterial;

class UStaticMeshComponent : public UPrimitiveComponent
{
    DECLARE_RTTI(UStaticMeshComponent, UPrimitiveComponent)

  public:
    virtual ~UStaticMeshComponent() override = default;

    const FString &GetStaticMeshPath() const;
    void           SetStaticMeshPath(const FString &InPath);

    void         SetStaticMeshAsset(UStaticMesh *InStaticMesh);
    UStaticMesh *GetStaticMesh() const { return StaticMesh; }

    void       SyncMaterialOverridesWithStaticMesh();
    UMaterial *GetRenderMaterial(int32 SlotIndex) const override;

    void Update(float DeltaTime) override;

    bool            GetLocalTriangles(TArray<Geometry::FTriangle> &OutTriangles) const override;
    Geometry::FAABB GetLocalAABB() const override;

    int32   GetMaterialSlotCount() const;
    bool    IsValidMaterialSlotIndex(int32 SlotIndex) const;
    FString GetMaterialSlotName(int32 SlotIndex) const;

    UMaterial *GetMaterial(int32 SlotIndex) const;
    UMaterial *GetOverrideMaterial(int32 SlotIndex) const;
    bool       HasMaterialOverride(int32 SlotIndex) const;
    void       SetMaterial(int32 SlotIndex, UMaterial *InMaterial);
    void       ClearMaterialOverride(int32 SlotIndex);
    void       ClearAllMaterialOverrides();

    const FMatrix         &GetCachedWorldMatrix() const;
    const FMatrix         &GetCachedInverseWorldMatrix() const;
    const Geometry::FAABB &GetCachedWorldAABB() const;

    void MarkRaycastTransformDirty();

  private:
    void UpdateCachedRaycastTransformData() const;

  private:
    FString             MeshPath;
    UStaticMesh        *StaticMesh = nullptr;
    TArray<UMaterial *> OverrideMaterials;

    mutable bool            bBoundsDirty = true;
    mutable bool            bRaycastTransformDirty = true;
    mutable FMatrix         CachedWorldMatrix;
    mutable FMatrix         CachedInverseWorldMatrix;
    mutable Geometry::FAABB CachedWorldAABB;
};
