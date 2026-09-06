// 컴포넌트 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "Component/PrimitiveComponent.h"

// UMeshComponent 컴포넌트이다.
class UMeshComponent : public UPrimitiveComponent
{
public:
    DECLARE_CLASS(UMeshComponent, UPrimitiveComponent)

    UMeshComponent() = default;
    ~UMeshComponent() override = default;

    void Serialize(FArchive& Ar) override;
    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
    void PostEditProperty(const char* PropertyName) override;

	void SetCastShadow(bool bNewCastShadow);
    bool ShouldCastShadow() const { return bCastShadow; }

    void UpdateWorldAABB() const override;

	void SetMaterial(int32 ElementIndex, UMaterial* InMaterial) override;
    UMaterial* GetMaterial(int32 ElementIndex) const override;
    const TArray<UMaterial*>& GetOverrideMaterials() const { return OverrideMaterials; }

protected:
    virtual void CacheLocalBounds();

    bool bCastShadow = true;
    bool bHasValidBounds = false;
    FVector CachedLocalCenter = { 0, 0, 0 };
    FVector CachedLocalExtent = { 0.5f, 0.5f, 0.5f };
    TArray<UMaterial*> OverrideMaterials;
    TArray<FMaterialSlot> MaterialSlots; // 경로 + UVScroll 묶음
};
