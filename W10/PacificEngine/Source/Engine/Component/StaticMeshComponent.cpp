// 컴포넌트 영역의 세부 동작을 구현합니다.
#include "Component/StaticMeshComponent.h"
#include <algorithm>
#include <cmath>
#include "Object/ObjectFactory.h"
#include "Core/PropertyTypes.h"
#include "Collision/RayUtils.h"
#include "Mesh/StaticMeshAsset.h"
#include "Engine/Runtime/Engine.h"
#include "Render/Resources/Shaders/ShaderManager.h"
#include "Texture/Texture2D.h"
#include "Render/Scene/Proxies/Primitive/StaticMeshSceneProxy.h"
#include "Render/Scene/Proxies/Primitive/PrimitiveProxy.h"
#include "Serialization/Archive.h"

IMPLEMENT_CLASS(UStaticMeshComponent, UMeshComponent)

FPrimitiveProxy* UStaticMeshComponent::CreateSceneProxy()
{
    return new FStaticMeshSceneProxy(this);
}

void UStaticMeshComponent::SetStaticMesh(UStaticMesh* InMesh)
{
    StaticMesh = InMesh;
    if (InMesh)
    {
        StaticMeshPath = InMesh->GetAssetPathFileName();
        const TArray<FStaticMaterial>& DefaultMaterials = StaticMesh->GetStaticMaterials();

        OverrideMaterials.resize(DefaultMaterials.size());
        MaterialSlots.resize(DefaultMaterials.size());

        for (int32 i = 0; i < (int32)DefaultMaterials.size(); ++i)
        {
            OverrideMaterials[i] = DefaultMaterials[i].MaterialInterface;

            if (OverrideMaterials[i])
                MaterialSlots[i].Path = OverrideMaterials[i]->GetAssetPathFileName();
            else
                MaterialSlots[i].Path = "None";
        }
    }
    else
    {
        StaticMeshPath = "None";
        OverrideMaterials.clear();
        MaterialSlots.clear();
    }
    CacheLocalBounds();
    MarkRenderStateDirty();
    MarkWorldBoundsDirty();
}

void UStaticMeshComponent::CacheLocalBounds()
{
    bHasValidBounds = false;
    if (!StaticMesh)
        return;
    FStaticMesh* Asset = StaticMesh->GetStaticMeshAsset();
    if (!Asset || Asset->Vertices.empty())
        return;

    // Reuse cached asset bounds when available.
    if (!Asset->bBoundsValid)
    {
        Asset->CacheBounds();
    }

    CachedLocalCenter = Asset->BoundsCenter;
    CachedLocalExtent = Asset->BoundsExtent;
    bHasValidBounds = Asset->bBoundsValid;
}

UStaticMesh* UStaticMeshComponent::GetStaticMesh() const
{
    return StaticMesh;
}

FStaticMeshBuffer* UStaticMeshComponent::GetMeshBuffer() const
{
    if (!StaticMesh)
        return nullptr;
    FStaticMesh* Asset = StaticMesh->GetStaticMeshAsset();
    if (!Asset || !Asset->RenderBuffer)
        return nullptr;
    return Asset->RenderBuffer.get();
}

FMeshDataView UStaticMeshComponent::GetMeshDataView() const
{
    if (!StaticMesh)
        return {};
    FStaticMesh* Asset = StaticMesh->GetStaticMeshAsset();
    if (!Asset || Asset->Vertices.empty())
        return {};

    FMeshDataView View;
    View.VertexData = Asset->Vertices.data();
    View.VertexCount = (uint32)Asset->Vertices.size();
    View.Stride = sizeof(FVertexPNCT_T);
    View.IndexData = Asset->Indices.data();
    View.IndexCount = (uint32)Asset->Indices.size();
    return View;
}

bool UStaticMeshComponent::LineTraceComponent(const FRay& Ray, FHitResult& OutHitResult)
{
    const FMatrix& WorldMatrix = GetWorldMatrix();
    const FMatrix& WorldInverse = GetWorldInverseMatrix();
    return LineTraceStaticMeshFast(Ray, WorldMatrix, WorldInverse, OutHitResult);
}

bool UStaticMeshComponent::LineTraceStaticMeshFast(
    const FRay& Ray,
    const FMatrix& WorldMatrix,
    const FMatrix& WorldInverse,
    FHitResult& OutHitResult)
{
    if (!StaticMesh)
        return false;

    FVector LocalOrigin = WorldInverse.TransformPositionWithW(Ray.Origin);
    FVector LocalDirection = WorldInverse.TransformVector(Ray.Direction);
    LocalDirection.Normalize();

    // Use the mesh BVH as the fast component picking path.
    if (StaticMesh->RaycastMeshTrianglesWithBVHLocal(LocalOrigin, LocalDirection, OutHitResult))
    {
        const FVector LocalHitPoint = LocalOrigin + LocalDirection * OutHitResult.Distance;
        const FVector WorldHitPoint = WorldMatrix.TransformPositionWithW(LocalHitPoint);
        OutHitResult.Distance = FVector::Distance(Ray.Origin, WorldHitPoint);
        OutHitResult.HitComponent = this;
        return true;
    }

    /*
    BVH misses no longer fall back to a full triangle scan. The fallback was useful
    for validation, but it is too expensive for regular component picking.
    */
    return false; // bHit;
}

void UStaticMeshComponent::Serialize(FArchive& Ar)
{
    UMeshComponent::Serialize(Ar);
    Ar << StaticMeshPath;
    Ar << MaterialSlots;
}

void UStaticMeshComponent::PostDuplicate()
{
    UMeshComponent::PostDuplicate();

    // Load the referenced mesh asset after duplication.
    if (!StaticMeshPath.empty() && StaticMeshPath != "None")
    {
        ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
        UStaticMesh* Loaded = FObjManager::Get().Load(StaticMeshPath);
        if (Loaded)
        {
            // Preserve serialized material slot data across SetStaticMesh.
            TArray<FMaterialSlot> SavedSlots = MaterialSlots;
            SetStaticMesh(Loaded);

            // Reload override materials from serialized slot paths.
            for (int32 i = 0; i < (int32)MaterialSlots.size() && i < (int32)SavedSlots.size(); ++i)
            {
                MaterialSlots[i] = SavedSlots[i];
                const FString& MatPath = MaterialSlots[i].Path;
                if (MatPath.empty() || MatPath == "None")
                {
                    OverrideMaterials[i] = nullptr;
                }
                else
                {
                    UMaterial* LoadedMat = FMaterialManager::Get().GetOrCreateStaticMeshMaterial(MatPath);
                    OverrideMaterials[i] = LoadedMat;
                }
            }
        }
    }

    CacheLocalBounds();
    MarkRenderStateDirty();
    MarkWorldBoundsDirty();
}

void UStaticMeshComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    UMeshComponent::GetEditableProperties(OutProps);
    OutProps.push_back({ "Static Mesh", EPropertyType::StaticMeshRef, &StaticMeshPath });

    for (int32 i = 0; i < (int32)MaterialSlots.size(); ++i)
    {
        FPropertyDescriptor Desc;
        Desc.Name = "Element " + std::to_string(i);
        Desc.Type = EPropertyType::MaterialSlot;
        Desc.ValuePtr = &MaterialSlots[i];
        OutProps.push_back(Desc);
    }
}

void UStaticMeshComponent::PostEditProperty(const char* PropertyName)
{
    UMeshComponent::PostEditProperty(PropertyName);

    if (strcmp(PropertyName, "Static Mesh") == 0)
    {
        if (StaticMeshPath.empty() || StaticMeshPath == "None")
        {
            StaticMesh = nullptr;
        }
        else
        {
            ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
            UStaticMesh* Loaded = FObjManager::Get().Load(StaticMeshPath);
            SetStaticMesh(Loaded);
        }
        CacheLocalBounds();
        MarkWorldBoundsDirty();
    }

    if (strncmp(PropertyName, "Element ", 8) == 0)
    {
        // Parse the numeric suffix after the 'Element ' prefix.
        int32 Index = atoi(&PropertyName[8]);

        // Validate the material slot index before applying the edit.
        if (Index >= 0 && Index < (int32)MaterialSlots.size())
        {
            FString NewMatPath = MaterialSlots[Index].Path;

            if (NewMatPath == "None" || NewMatPath.empty())
            {
                SetMaterial(Index, nullptr);
            }
            else
            {
                UMaterial* LoadedMat = FMaterialManager::Get().GetOrCreateStaticMeshMaterial(NewMatPath);
                if (LoadedMat)
                {
                    SetMaterial(Index, LoadedMat);
                }
            }
        }
    }
}

