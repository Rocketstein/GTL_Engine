#include "PrimitiveComponent.h"
#include "ComponentProperty.h"
#include "Core/Geometry/Primitives/AABBUtility.h"
#include "Core/Geometry/Primitives/Triangle.h"
#include "Core/Logging/LogMacros.h"
#include "Core/Math/Color.h"
#include "Core/Math/Vector3.h"
#include "Core/Misc/BitMaskEnum.h"
#include "Core/Platform/PlatformTypes.h"
#include <cfloat>

const FColor &UPrimitiveComponent::GetColor() const { return Color; }

void UPrimitiveComponent::SetColor(const FColor &NewColor)
{
    if (Color == NewColor)
    {
        return;
    }

    Color = NewColor;
}

const Geometry::FAABB &UPrimitiveComponent::GetWorldAABB() const
{
    if (bBoundsDirty)
    {
        const_cast<UPrimitiveComponent *>(this)->UpdateBounds();
        const_cast<UPrimitiveComponent *>(this)->bBoundsDirty = false;
    }

    return WorldAABB;
}

bool UPrimitiveComponent::GetWorldAABB(Geometry::FAABB &OutWorldAABB) const
{
    OutWorldAABB = GetWorldAABB();
    return true;
}

bool UPrimitiveComponent::GetLocalTriangles(TArray<Geometry::FTriangle> &OutTriangles) const
{
    //
    return false;
}

Geometry::FAABB UPrimitiveComponent::GetLocalAABB() const { return Geometry::FAABB(); }

void UPrimitiveComponent::UpdateBounds()
{
    const FMatrix               WorldMatrix = GetRelativeMatrix();
    TArray<Geometry::FTriangle> LocalTriangles;

    if (GetLocalTriangles(LocalTriangles) && !LocalTriangles.empty())
    {
        FVector3 Min(FLT_MAX, FLT_MAX, FLT_MAX);
        FVector3 Max(-FLT_MAX, -FLT_MAX, -FLT_MAX);

        auto Expand = [&Min, &Max](const FVector3 &P)
        {
            Min.X = std::min(Min.X, P.X);
            Min.Y = std::min(Min.Y, P.Y);
            Min.Z = std::min(Min.Z, P.Z);

            Max.X = std::max(Max.X, P.X);
            Max.Y = std::max(Max.Y, P.Y);
            Max.Z = std::max(Max.Z, P.Z);
        };

        for (const Geometry::FTriangle &Triangle : LocalTriangles)
        {
            Expand(WorldMatrix.TransformPosition(Triangle.V0));
            Expand(WorldMatrix.TransformPosition(Triangle.V1));
            Expand(WorldMatrix.TransformPosition(Triangle.V2));
        }

        WorldAABB = Geometry::FAABB(Min, Max);
        UE_LOG(PrimitiveComponent, ELogLevel::Verbose,
               "AABB updated from triangles for %s: min=(%.3f, %.3f, %.3f) max=(%.3f, %.3f, %.3f)",
               GetTypeName(), WorldAABB.Min.X, WorldAABB.Min.Y, WorldAABB.Min.Z, WorldAABB.Max.X,
               WorldAABB.Max.Y, WorldAABB.Max.Z);
        return;
    }

    WorldAABB = Geometry::TransformAABB(GetLocalAABB(), WorldMatrix);
    UE_LOG(PrimitiveComponent, ELogLevel::Verbose,
           "AABB updated from local bounds for %s: min=(%.3f, %.3f, %.3f) max=(%.3f, %.3f, %.3f)",
           GetTypeName(), WorldAABB.Min.X, WorldAABB.Min.Y, WorldAABB.Min.Z, WorldAABB.Max.X,
           WorldAABB.Max.Y, WorldAABB.Max.Z);
}

void UPrimitiveComponent::OnTransformChanged()
{
    bBoundsDirty = true;
    UE_LOG(PrimitiveComponent, ELogLevel::Verbose,
           "Transform changed -> bounds marked dirty for %s", GetTypeName());
}

UMaterial *UPrimitiveComponent::GetRenderMaterial(int32) const { return nullptr; }
