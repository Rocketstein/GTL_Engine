#pragma once
#include "PrimitiveComponent.h"
#include "Render/Resource/MeshBufferManager.h"
#include "Core/ResourceTypes.h"
#include "Object/FName.h"

class FPrimitiveSceneProxy;

class UBillboardComponent : public UPrimitiveComponent
{
public:
	DECLARE_CLASS(UBillboardComponent, UPrimitiveComponent)

	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;
	FPrimitiveSceneProxy* CreateSceneProxy() override;
	void UpdateWorldAABB() const override;
	bool LineTraceComponent(const FRay& Ray, FRayHitResult& OutHitResult) override;

	void Serialize(FArchive& Ar) override;
	void PostDuplicate() override;

	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void PostEditProperty(const char* PropertyName) override;

	void SetBillboardEnabled(bool bEnable) { bIsBillboard = bEnable; }
	bool IsBillboardEnabled() const { return bIsBillboard; }
	bool ParticipatesInRenderSpatialStructure() const override { return false; }
	bool ParticipatesInPickingSpatialStructure() const override { return true; }

	// --- Texture ---
	void SetTexture(class UTexture2D* InTexture);
	void SetMaterial(class UMaterial* InMaterial);
	class UTexture2D* GetTexture() const { return Texture; }
	const FString& GetTexturePath() const { return TextureSlot.Path; }

	// --- Sprite Size (월드 공간) ---
	void SetSpriteSize(float InWidth, float InHeight) { Width = InWidth; Height = InHeight; }
	float GetWidth()  const { return Width; }
	float GetHeight() const { return Height; }

	// 주어진 카메라 방향으로 빌보드 월드 행렬을 계산 (per-view 렌더링용)
	FMatrix ComputeBillboardMatrix(const FVector& CameraForward) const;

	FMeshBuffer* GetMeshBuffer() const override { return &FMeshBufferManager::Get().GetMeshBuffer(EMeshShape::Quad); }
	FMeshDataView GetMeshDataView() const override { return FMeshDataView::FromMeshData(FMeshBufferManager::Get().GetMeshData(EMeshShape::Quad)); }

protected:
	bool ResolveTextureFromPath(const FString& InPath);
	bool IntersectBillboard(const FRay& Ray, FRayHitResult& OutHitResult, bool bRespectTextureAlpha) const;

	bool bIsBillboard = true;

	FTextureSlot TextureSlot;
	UTexture2D* Texture = nullptr;

	float Width  = 1.0f;
	float Height = 1.0f;
};

