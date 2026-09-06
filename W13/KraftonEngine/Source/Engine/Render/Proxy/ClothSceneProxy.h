#pragma once

#include "Render/Proxy/PrimitiveSceneProxy.h"
#include "Render/Resource/Buffer.h"

class UClothComponent;
struct FDrawCommandBuffer;

class FClothSceneProxy : public FPrimitiveSceneProxy
{
public:
	explicit FClothSceneProxy(UClothComponent* InComponent);
	~FClothSceneProxy() override = default;

	void UpdateMaterial() override;
	void UpdateMesh() override;

	bool PrepareDrawBuffer(ID3D11Device* Device, ID3D11DeviceContext* Context, FDrawCommandBuffer& OutBuffer) const override;

private:
	UClothComponent* GetClothComponent() const;
	void RebuildSectionDraws();

private:
	mutable FDynamicVertexBuffer DynamicVertexBuffer;
	mutable FDynamicIndexBuffer DynamicIndexBuffer;
	mutable uint64 UploadedRevision = 0;
	uint32 CachedVertexCount = 0;
	uint32 CachedIndexCount = 0;
	mutable bool bDynamicBuffersNeedCreate = true;
};
