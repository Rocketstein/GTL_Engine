// 컴포넌트 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "Component/MeshComponent.h"
#include "Core/PropertyTypes.h"
#include "Mesh/ObjManager.h"
#include "Mesh/StaticMesh.h"

class UMaterial;
class FPrimitiveProxy;

namespace json
{
class JSON;
}

// UStaticMeshComponent 컴포넌트이다.
class UStaticMeshComponent : public UMeshComponent
{
public:
    DECLARE_CLASS(UStaticMeshComponent, UMeshComponent)

    UStaticMeshComponent() = default;
    ~UStaticMeshComponent() override = default;

    FStaticMeshBuffer* GetMeshBuffer() const override;
    FMeshDataView GetMeshDataView() const override;
    bool LineTraceComponent(const FRay& Ray, FHitResult& OutHitResult) override;
    bool LineTraceStaticMeshFast(const FRay& Ray, const FMatrix& WorldMatrix, const FMatrix& WorldInverse, FHitResult& OutHitResult);

    // 구체 프록시 생성 (FStaticMeshSceneProxy)
    FPrimitiveProxy* CreateSceneProxy() override;

    void SetStaticMesh(UStaticMesh* InMesh);
    UStaticMesh* GetStaticMesh() const;

    void Serialize(FArchive& Ar) override;
    void PostDuplicate() override;

    // Property Editor 지원
    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
    void PostEditProperty(const char* PropertyName) override;

    const FString& GetStaticMeshPath() const { return StaticMeshPath; }

private:
    void CacheLocalBounds() override;

    UStaticMesh* StaticMesh = nullptr;
    FString StaticMeshPath = "None";
};

