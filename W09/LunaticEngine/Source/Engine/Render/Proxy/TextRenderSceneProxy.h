#pragma once

#include "Component/TextRenderComponent.h"
#include "Core/ResourceTypes.h"
#include "Render/Proxy/BillboardSceneProxy.h"

class UMaterial;
class UTextRenderComponent;

class FTextRenderSceneProxy : public FBillboardSceneProxy
{
public:
	FTextRenderSceneProxy(UTextRenderComponent* InComponent);
	~FTextRenderSceneProxy() override;

	void UpdateTransform() override;
	void UpdateMesh() override;
	void UpdatePerViewport(const FFrameContext& Frame) override;

	FString CachedText;
	float CachedFontScale = 1.0f;
	FMatrix CachedTextWorldMatrix;
	FVector CachedTextRight = FVector(0.0f, 1.0f, 0.0f);
	FVector CachedTextUp = FVector(0.0f, 0.0f, 1.0f);
	FVector4 CachedColor = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	const FFontResource* CachedFont = nullptr;

private:
	UTextRenderComponent* GetTextRenderComponent() const;

	UMaterial* TextMaterial = nullptr;
	float CachedCharWidth = 0.5f;
	float CachedCharHeight = 0.5f;
};
