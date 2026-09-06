#pragma once

#include "Component/MeshComponent.h"
#include "Core/Types/PropertyTypes.h"
#include "Mesh/MeshManager.h"
#include "Mesh/Static/StaticMesh.h"
#include "Object/Ptr/ObjectPtr.h"
#include "Object/Ptr/SoftObjectPtr.h"

class UMaterial;
class FPrimitiveSceneProxy;

namespace json { class JSON; }

// UStaticMeshComponent — 월드 배치 컴포넌트

#include "Source/Engine/Component/Primitive/StaticMeshComponent.generated.h"

UCLASS()
class UStaticMeshComponent : public UMeshComponent
{
public:
	GENERATED_BODY()
	UStaticMeshComponent() = default;
	~UStaticMeshComponent() override = default;

	FMeshBuffer* GetMeshBuffer() const override;
	FMeshDataView GetMeshDataView() const override;
	bool LineTraceComponent(const FRay& Ray, FHitResult& OutHitResult) override;
	bool LineTraceStaticMeshFast(const FRay& Ray, const FMatrix& WorldMatrix, const FMatrix& WorldInverse, FHitResult& OutHitResult);
	void UpdateWorldAABB() const override;

	// 구체 프록시 생성 (FStaticMeshSceneProxy)
	FPrimitiveSceneProxy* CreateSceneProxy() override;

	void SetStaticMesh(UStaticMesh* InMesh);
	UStaticMesh* GetStaticMesh() const;

	// 물리 바디 생성용 콜리전 셋업 — 메시 에셋의 BodySetup(AggGeom convex / TriMesh)을 그대로 사용.
	UBodySetup* GetBodySetup() override;

	void SetMaterial(int32 ElementIndex, UMaterial* InMaterial);
	UMaterial* GetMaterial(int32 ElementIndex) const;
	const TArray<UMaterial*>& GetOverrideMaterials() const { return OverrideMaterials; }

	void PostDuplicate() override;

	// Property Editor 지원
	void PostEditProperty(const char* PropertyName) override;

	const FString& GetStaticMeshPath() const { return StaticMeshPath.ToString(); }

	// 로컬(메시-에셋) 바운드 — center 와 half-extent. 컴포넌트 스케일은 미적용이므로
	// 월드 치수가 필요하면 호출측에서 GetWorldScale() 을 곱해야 한다. 유효 바운드가 있을 때만 true.
	bool GetLocalBounds(FVector& OutCenter, FVector& OutExtent) const
	{
		OutCenter = CachedLocalCenter;
		OutExtent = CachedLocalExtent;
		return bHasValidBounds;
	}

private:
	void CacheLocalBounds();

	TObjectPtr<UStaticMesh> StaticMesh;
	UPROPERTY(Edit, Save, Category="Mesh", DisplayName="Static Mesh", AssetType="StaticMesh")
	FSoftObjectPtr StaticMeshPath = "None";
	TArray<UMaterial*> OverrideMaterials;
	UPROPERTY(Edit, Save, EditFixedSize, Category="Materials", DisplayName="Materials", AssetType="Material")
	TArray<FSoftObjectPtr> MaterialSlots;

	FVector CachedLocalCenter = { 0, 0, 0 };
	FVector CachedLocalExtent = { 0.5f, 0.5f, 0.5f };
	bool bHasValidBounds = false;
};
