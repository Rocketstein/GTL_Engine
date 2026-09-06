#include "StaticMeshComponent.h"
#include "Core/Logging/LogMacros.h"
#include "Core/Math/Vector3.h"
#include "Engine/Asset/StaticMesh.h"
#include "Engine/Component/ComponentProperty.h"
#include <cmath>
#include <cstring>

REGISTER_CLASS(, UStaticMeshComponent)

namespace
{
    static bool ReadVertexPosition(const TArray<uint8> &VertexData, uint32 VertexStride,
                                   uint32 VertexIndex, FVector3 &OutPosition)
    {
        if (VertexStride < sizeof(FVector3))
        {
            return false;
        }

        const size_t Offset = static_cast<size_t>(VertexIndex) * VertexStride;
        if (Offset + sizeof(FVector3) > VertexData.size())
        {
            return false;
        }

        std::memcpy(&OutPosition, VertexData.data() + Offset, sizeof(FVector3));
        return true;
    }

    static Geometry::FAABB TransformAABB(const Geometry::FAABB &LocalBounds, const FMatrix &World)
    {
        const FVector3 Min = LocalBounds.Min;
        const FVector3 Max = LocalBounds.Max;

        const FVector3 Corners[8] = {
            FVector3(Min.X, Min.Y, Min.Z), FVector3(Max.X, Min.Y, Min.Z),
            FVector3(Min.X, Max.Y, Min.Z), FVector3(Max.X, Max.Y, Min.Z),
            FVector3(Min.X, Min.Y, Max.Z), FVector3(Max.X, Min.Y, Max.Z),
            FVector3(Min.X, Max.Y, Max.Z), FVector3(Max.X, Max.Y, Max.Z),
        };

        FVector3 OutMin = World.TransformPosition(Corners[0]);
        FVector3 OutMax = OutMin;

        for (int32 i = 1; i < 8; ++i)
        {
            const FVector3 P = World.TransformPosition(Corners[i]);

            OutMin.X = (std::min)(OutMin.X, P.X);
            OutMin.Y = (std::min)(OutMin.Y, P.Y);
            OutMin.Z = (std::min)(OutMin.Z, P.Z);

            OutMax.X = (std::max)(OutMax.X, P.X);
            OutMax.Y = (std::max)(OutMax.Y, P.Y);
            OutMax.Z = (std::max)(OutMax.Z, P.Z);
        }

        return Geometry::FAABB(OutMin, OutMax);
    }
} // namespace

const FString &UStaticMeshComponent::GetStaticMeshPath() const { return MeshPath; }

void UStaticMeshComponent::SetStaticMeshPath(const FString &InPath)
{
    if (MeshPath == InPath)
    {
        return;
    }

    MeshPath = InPath;
    StaticMesh = nullptr;
    OverrideMaterials.clear();
    bBoundsDirty = true;
    bRaycastTransformDirty = true;

    UE_LOG(StaticMeshComponent, ELogLevel::Verbose, "Static mesh path changed: %s",
           MeshPath.c_str());
}

void UStaticMeshComponent::SetStaticMeshAsset(UStaticMesh *InStaticMesh)
{
    bBoundsDirty = true;
    bRaycastTransformDirty = true;
    StaticMesh = InStaticMesh;

    UE_LOG(StaticMeshComponent, ELogLevel::Verbose, "Static mesh asset assigned: %s",
           StaticMesh ? StaticMesh->GetAssetPath().c_str() : "<null>");

    SyncMaterialOverridesWithStaticMesh();
}

void UStaticMeshComponent::SyncMaterialOverridesWithStaticMesh()
{
    OverrideMaterials.clear();

    if (StaticMesh == nullptr)
    {
        return;
    }

    OverrideMaterials.resize(StaticMesh->GetMaterialSlots().size(), nullptr);
    UE_LOG(StaticMeshComponent, ELogLevel::Verbose, "Material override slots synced: %zu",
           OverrideMaterials.size());
}

UMaterial *UStaticMeshComponent::GetRenderMaterial(int32 SlotIndex) const
{
    return GetMaterial(SlotIndex);
}

void UStaticMeshComponent::Update(float DeltaTime) { UpdateCachedRaycastTransformData(); }

bool UStaticMeshComponent::GetLocalTriangles(TArray<Geometry::FTriangle> &OutTriangles) const
{
    OutTriangles.clear();

    if (StaticMesh == nullptr || !StaticMesh->IsValidLowLevel())
    {
        return false;
    }

    const TArray<uint8>  &VertexData = StaticMesh->GetVerticesData();
    const TArray<uint32> &Indices = StaticMesh->GetIndicesData();
    const uint32          VertexStride = StaticMesh->GetVertexStride();
    const uint32          VertexCount = StaticMesh->GetVerticesCount();

    if (VertexStride < sizeof(FVector3) || VertexCount == 0)
    {
        return false;
    }

    OutTriangles.reserve(Indices.size() / 3);

    for (size_t i = 0; i + 2 < Indices.size(); i += 3)
    {
        const uint32 I0 = Indices[i + 0];
        const uint32 I1 = Indices[i + 1];
        const uint32 I2 = Indices[i + 2];

        if (I0 >= VertexCount || I1 >= VertexCount || I2 >= VertexCount)
        {
            continue;
        }

        FVector3 P0, P1, P2;
        if (!ReadVertexPosition(VertexData, VertexStride, I0, P0) ||
            !ReadVertexPosition(VertexData, VertexStride, I1, P1) ||
            !ReadVertexPosition(VertexData, VertexStride, I2, P2))
        {
            continue;
        }

        Geometry::FTriangle Triangle;
        Triangle.V0 = P0;
        Triangle.V1 = P1;
        Triangle.V2 = P2;
        OutTriangles.push_back(Triangle);
    }

    return !OutTriangles.empty();
}

Geometry::FAABB UStaticMeshComponent::GetLocalAABB() const
{
    if (StaticMesh)
    {
        return StaticMesh->GetAABB();
    }

    return {};
}

int32 UStaticMeshComponent::GetMaterialSlotCount() const
{
    if (StaticMesh == nullptr)
    {
        return 0;
    }
    return static_cast<int32>(StaticMesh->GetMaterialSlots().size());
}

bool UStaticMeshComponent::IsValidMaterialSlotIndex(int32 SlotIndex) const
{
    return SlotIndex >= 0 && SlotIndex < GetMaterialSlotCount();
}

FString UStaticMeshComponent::GetMaterialSlotName(int32 SlotIndex) const
{
    if (!IsValidMaterialSlotIndex(SlotIndex) || StaticMesh == nullptr)
    {
        return {};
    }

    const auto &CookedData = StaticMesh->GetCookedData();
    if (CookedData != nullptr && static_cast<size_t>(SlotIndex) < CookedData->Materials.size())
    {
        const Asset::FObjCookedMaterialRef &MaterialRef = CookedData->Materials[SlotIndex];
        if (!MaterialRef.Name.empty())
        {
            return MaterialRef.Name;
        }
    }

    return "Material Slot " + std::to_string(SlotIndex);
}

UMaterial *UStaticMeshComponent::GetMaterial(int32 SlotIndex) const
{
    if (!IsValidMaterialSlotIndex(SlotIndex))
    {
        return nullptr;
    }

    if (UMaterial *OverrideMaterial = GetOverrideMaterial(SlotIndex))
    {
        return OverrideMaterial;
    }

    const auto &Slots = StaticMesh->GetMaterialSlots();
    return Slots[SlotIndex];
}

UMaterial *UStaticMeshComponent::GetOverrideMaterial(int32 SlotIndex) const
{
    if (!IsValidMaterialSlotIndex(SlotIndex))
    {
        return nullptr;
    }

    if (static_cast<size_t>(SlotIndex) >= OverrideMaterials.size())
    {
        return nullptr;
    }

    return OverrideMaterials[SlotIndex];
}

bool UStaticMeshComponent::HasMaterialOverride(int32 SlotIndex) const
{
    return GetOverrideMaterial(SlotIndex) != nullptr;
}

void UStaticMeshComponent::SetMaterial(int32 SlotIndex, UMaterial *InMaterial)
{
    if (!IsValidMaterialSlotIndex(SlotIndex))
    {
        return;
    }

    if (OverrideMaterials.size() < StaticMesh->GetMaterialSlots().size())
    {
        OverrideMaterials.resize(StaticMesh->GetMaterialSlots().size(), nullptr);
        UE_LOG(StaticMeshComponent, ELogLevel::Verbose, "Material override slots synced: %zu",
               OverrideMaterials.size());
    }

    OverrideMaterials[SlotIndex] = InMaterial;
}

void UStaticMeshComponent::ClearMaterialOverride(int32 SlotIndex)
{
    if (!IsValidMaterialSlotIndex(SlotIndex))
    {
        return;
    }

    if (static_cast<size_t>(SlotIndex) >= OverrideMaterials.size())
    {
        return;
    }

    OverrideMaterials[SlotIndex] = nullptr;
}

void UStaticMeshComponent::ClearAllMaterialOverrides()
{
    for (UMaterial *&OverrideMaterial : OverrideMaterials)
    {
        OverrideMaterial = nullptr;
    }
}

const FMatrix &UStaticMeshComponent::GetCachedWorldMatrix() const
{
    UpdateCachedRaycastTransformData();
    return CachedWorldMatrix;
}

const FMatrix &UStaticMeshComponent::GetCachedInverseWorldMatrix() const
{
    UpdateCachedRaycastTransformData();
    return CachedInverseWorldMatrix;
}

const Geometry::FAABB &UStaticMeshComponent::GetCachedWorldAABB() const
{
    UpdateCachedRaycastTransformData();
    return CachedWorldAABB;
}

void UStaticMeshComponent::MarkRaycastTransformDirty()
{
    bRaycastTransformDirty = true;
    bBoundsDirty = true;
}

void UStaticMeshComponent::UpdateCachedRaycastTransformData() const
{
    if (!bRaycastTransformDirty && !bBoundsDirty)
    {
        return;
    }

    CachedWorldMatrix = GetRelativeMatrix();
    CachedInverseWorldMatrix = CachedWorldMatrix.GetInverse();

    if (StaticMesh != nullptr && StaticMesh->IsValidLowLevel())
    {
        CachedWorldAABB = TransformAABB(StaticMesh->GetAABB(), CachedWorldMatrix);
    }
    else
    {
        CachedWorldAABB = Geometry::FAABB();
    }

    bRaycastTransformDirty = false;
    bBoundsDirty = false;
}