// 컴포넌트 영역의 세부 동작을 구현합니다.
#include "Component/MeshComponent.h"
#include "Materials/Material.h"
#include "Object/ObjectFactory.h"
#include "Serialization/Archive.h"
#include <cmath>
#include <cstring>

IMPLEMENT_CLASS(UMeshComponent, UPrimitiveComponent)

void UMeshComponent::Serialize(FArchive& Ar)
{
    UPrimitiveComponent::Serialize(Ar);
    Ar << bCastShadow;
}

void UMeshComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    UPrimitiveComponent::GetEditableProperties(OutProps);
    OutProps.push_back({ "Cast Shadow", EPropertyType::Bool, &bCastShadow });
}

void UMeshComponent::PostEditProperty(const char* PropertyName)
{
    UPrimitiveComponent::PostEditProperty(PropertyName);

    if (strcmp(PropertyName, "Cast Shadow") == 0)
    {
        MarkProxyDirty(ESceneProxyDirtyFlag::Shadow);
    }
}

void UMeshComponent::SetCastShadow(bool bNewCastShadow)
{
    if (bCastShadow == bNewCastShadow)
    {
        return;
    }

    bCastShadow = bNewCastShadow;
    MarkProxyDirty(ESceneProxyDirtyFlag::Shadow);
}

void UMeshComponent::SetMaterial(int32 ElementIndex, UMaterial* InMaterial)
{
    if (ElementIndex >= 0 && ElementIndex < static_cast<int32>(OverrideMaterials.size()))
    {
        OverrideMaterials[ElementIndex] = InMaterial;

        if (ElementIndex < static_cast<int32>(MaterialSlots.size()))
        {
            MaterialSlots[ElementIndex].Path = InMaterial
                                                   ? InMaterial->GetAssetPathFileName()
                                                   : "None";
        }

        MarkProxyDirty(ESceneProxyDirtyFlag::Material);
    }
}

UMaterial* UMeshComponent::GetMaterial(int32 ElementIndex) const
{
    if (ElementIndex >= 0 && ElementIndex < static_cast<int32>(OverrideMaterials.size()))
    {
        return OverrideMaterials[ElementIndex];
    }

    return nullptr;
}

void UMeshComponent::UpdateWorldAABB() const
{
    if (!bHasValidBounds)
    {
        UPrimitiveComponent::UpdateWorldAABB();
        return;
    }

    FVector WorldCenter = CachedWorldMatrix.TransformPositionWithW(CachedLocalCenter);

    float Ex = std::abs(CachedWorldMatrix.M[0][0]) * CachedLocalExtent.X + std::abs(CachedWorldMatrix.M[1][0]) * CachedLocalExtent.Y + std::abs(CachedWorldMatrix.M[2][0]) * CachedLocalExtent.Z;
    float Ey = std::abs(CachedWorldMatrix.M[0][1]) * CachedLocalExtent.X + std::abs(CachedWorldMatrix.M[1][1]) * CachedLocalExtent.Y + std::abs(CachedWorldMatrix.M[2][1]) * CachedLocalExtent.Z;
    float Ez = std::abs(CachedWorldMatrix.M[0][2]) * CachedLocalExtent.X + std::abs(CachedWorldMatrix.M[1][2]) * CachedLocalExtent.Y + std::abs(CachedWorldMatrix.M[2][2]) * CachedLocalExtent.Z;

    WorldAABBMinLocation = WorldCenter - FVector(Ex, Ey, Ez);
    WorldAABBMaxLocation = WorldCenter + FVector(Ex, Ey, Ez);
    bWorldAABBDirty = false;
    bHasValidWorldAABB = true;
}

void UMeshComponent::CacheLocalBounds()
{
    bHasValidBounds = false;
}
