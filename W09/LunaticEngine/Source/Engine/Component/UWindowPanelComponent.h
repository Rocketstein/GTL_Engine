#pragma once

#include "Component/UIImageComponent.h"

class UNineSlicePanelComponent : public UUIImageComponent
{
public:
	DECLARE_CLASS(UNineSlicePanelComponent, UUIImageComponent)

	UNineSlicePanelComponent();

	void Serialize(FArchive& Ar) override;
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void PostEditProperty(const char* PropertyName) override;
	void ContributeVisuals(FScene& Scene) const override;

private:
	bool LoadStyleFromJson();
	void ClampSlice();
	bool ResolveOptionalTextureSlot(FTextureSlot& Slot, class UTexture2D*& LoadedTexture);
	ID3D11ShaderResourceView* GetOptionalTextureSRV(const FTextureSlot& Slot, class UTexture2D* LoadedTexture) const;
	void AddPanelQuad(FScene& Scene, ID3D11ShaderResourceView* SRV, float X, float Y, float Width, float Height,
		float U0, float V0, float U1, float V1) const;

private:
	FString StyleJsonPath;
	FVector4 AtlasRegion = FVector4(0.0f, 0.0f, 0.0f, 0.0f); // x, y, width, height in source texture pixels
	FVector4 Slice = FVector4(8.0f, 8.0f, 8.0f, 8.0f);
	bool bDrawBorder = true;
	bool bDrawCenter = true;
	FTextureSlot TopLeftTextureSlot;
	FTextureSlot TopTextureSlot;
	FTextureSlot TopRightTextureSlot;
	FTextureSlot LeftTextureSlot;
	FTextureSlot CenterTextureSlot;
	FTextureSlot RightTextureSlot;
	FTextureSlot BottomLeftTextureSlot;
	FTextureSlot BottomTextureSlot;
	FTextureSlot BottomRightTextureSlot;
	UTexture2D* TopLeftTexture = nullptr;
	UTexture2D* TopTexture = nullptr;
	UTexture2D* TopRightTexture = nullptr;
	UTexture2D* LeftTexture = nullptr;
	UTexture2D* CenterTexture = nullptr;
	UTexture2D* RightTexture = nullptr;
	UTexture2D* BottomLeftTexture = nullptr;
	UTexture2D* BottomTexture = nullptr;
	UTexture2D* BottomRightTexture = nullptr;
};

// Legacy compatibility class kept so older scenes that serialized
// UWindowPanelComponent can still be loaded without migration.
class UWindowPanelComponent : public UNineSlicePanelComponent
{
public:
	DECLARE_CLASS(UWindowPanelComponent, UNineSlicePanelComponent)

	UWindowPanelComponent() = default;
};
