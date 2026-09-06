#pragma once
#include "World/PrimitiveComponent.h"

class UBillBoardComponent : public UPrimitiveComponent {
private:
	const FUVMeshData* UVMeshData = nullptr;
	int32 TextureID = -1;
	float Size = 1.0f;
	std::wstring TexturePath;
public:
	DECLARE_CLASS(UBillBoardComponent, UPrimitiveComponent)
	UBillBoardComponent();
	EPrimitiveType GetPrimitiveType() const override { return EPrimitiveType::EPT_SubUV; }
	void UpdateWorldAABB() override;
	bool GetRenderCommand(const FMatrix& viewMatrix, const FMatrix& projMatrix, FRenderCommand& OutCommand) override;
	const FBoundingBox& GetBoundingBox() const override;
	bool RaycastMesh(const FRay& Ray, FHitResult& OutHitResult) override;

	void LoadTexture(const std::wstring& InTexturePath);

	void Serialize(json::JSON& j) const override;
	void Deserialize(const json::JSON& j) override;
};