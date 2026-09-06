#pragma once

#include "Component/BillboardComponent.h"
#include "Core/ResourceTypes.h"

#include <algorithm>

class FScene;
class UTexture2D;
struct ID3D11ShaderResourceView;

enum class EUIImageFitMode : int32
{
	Stretch = 0,
	Contain,
	Cover,
};

enum class EUIImageContentAlignment : int32
{
	Center = 0,
	Top,
	Bottom,
	Left,
	Right,
};

class UUIImageComponent : public UBillboardComponent
{
public:
	DECLARE_CLASS(UUIImageComponent, UBillboardComponent)

	UUIImageComponent();

	FPrimitiveSceneProxy* CreateSceneProxy() override { return nullptr; }
	void Serialize(FArchive& Ar) override;
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void PostEditProperty(const char* PropertyName) override;
	void ContributeVisuals(FScene& Scene) const override;
	void ContributeSelectedVisuals(FScene& Scene) const override;
	bool SupportsUIScreenPicking() const override { return true; }
	bool HitTestUIScreenPoint(float X, float Y) const override;
	int32 GetUIScreenPickingZOrder() const override { return ZOrder; }
	bool SupportsWorldGizmo() const override { return false; }
	bool ParticipatesInSpatialStructure() const override { return false; }

	const FVector& GetScreenPosition() const { return ScreenPosition; }
	void SetScreenPosition(const FVector& InScreenPosition) { ScreenPosition = InScreenPosition; }

	const FVector4& GetTint() const { return Tint; }
	void SetTint(const FVector4& InTint) { Tint = InTint; }

	float GetBorderThickness() const { return BorderThickness; }
	void SetBorderThickness(float InBorderThickness) { BorderThickness = (std::max)(0.0f, InBorderThickness); }
	bool IsBottomBorderOnly() const { return bBottomBorderOnly; }
	void SetBottomBorderOnly(bool bInBottomBorderOnly) { bBottomBorderOnly = bInBottomBorderOnly; }

	const FVector4& GetBorderColor() const { return BorderColor; }
	void SetBorderColor(const FVector4& InBorderColor) { BorderColor = InBorderColor; }

	int32 GetZOrder() const { return ZOrder; }
	void SetZOrder(int32 InZOrder) { ZOrder = InZOrder; }

	const FVector& GetScreenSize() const { return ScreenSize; }
	void SetScreenSize(const FVector& InScreenSize);

	bool IsAnchoredLayoutEnabled() const { return bUseAnchoredLayout; }
	void SetAnchoredLayoutEnabled(bool bEnabled) { bUseAnchoredLayout = bEnabled; }

	const FVector& GetAnchor() const { return Anchor; }
	void SetAnchor(const FVector& InAnchor) { Anchor = InAnchor; }

	const FVector& GetAlignment() const { return Alignment; }
	void SetAlignment(const FVector& InAlignment) { Alignment = InAlignment; }

	const FVector& GetAnchorOffset() const { return AnchorOffset; }
	void SetAnchorOffset(const FVector& InAnchorOffset) { AnchorOffset = InAnchorOffset; }

	const FString& GetTexturePath() const { return TextureSlot.Path; }
	bool SetTexturePath(const FString& InTexturePath);
	bool ResolveLayoutRect(FVector2& OutPosition, FVector2& OutSize) const;
	FVector2 GetResolvedScreenPosition() const;
	FVector2 GetResolvedScreenSize() const;
	EUIImageFitMode GetFitMode() const { return static_cast<EUIImageFitMode>(FitMode); }
	void SetFitMode(EUIImageFitMode InFitMode) { FitMode = static_cast<int32>(InFitMode); }
	EUIImageContentAlignment GetContentAlignment() const { return static_cast<EUIImageContentAlignment>(ContentAlignment); }
	void SetContentAlignment(EUIImageContentAlignment InAlignment) { ContentAlignment = static_cast<int32>(InAlignment); }
	bool IsShadowEnabled() const { return bDrawShadow; }

protected:
	struct FResolvedImageDrawParams
	{
		FVector2 Position;
		FVector2 Size;
		FVector2 UVMin = FVector2(0.0f, 0.0f);
		FVector2 UVMax = FVector2(1.0f, 1.0f);
	};

	bool EnsureTextureLoaded();
	ID3D11ShaderResourceView* GetResolvedTextureSRV() const;
	bool EnsureBackgroundTextureLoaded() const;
	ID3D11ShaderResourceView* GetBackgroundTextureSRV() const;
	bool ShouldDrawShadow() const;
	FVector2 GetShadowOffset2D() const;
	float GetShadowBlurRadius() const;
	FVector4 GetShadowMaskTopColor() const;
	FVector4 GetShadowMaskBottomColor() const;
	void AddShadowScreenQuad(FScene& Scene, ID3D11ShaderResourceView* TextureSRV, const FVector2& Position, const FVector2& Size, int32 InZOrder,
		const FVector2& UVMin = FVector2(0.0f, 0.0f), const FVector2& UVMax = FVector2(1.0f, 1.0f)) const;
	FVector2 ResolveScreenPosition(const FVector2& ElementSize) const;
	FVector2 ResolveScreenSize2D() const;
	static EUIImageFitMode SanitizeFitModeValue(int32 InFitMode);
	static EUIImageContentAlignment SanitizeContentAlignmentValue(int32 InAlignment);
	static FResolvedImageDrawParams ResolveImageDrawParams(
		const FVector2& ContainerPosition,
		const FVector2& ContainerSize,
		const UTexture2D* Texture,
		EUIImageFitMode InFitMode,
		EUIImageContentAlignment InAlignment);

protected:
	FVector ScreenPosition = FVector(0.0f, 0.0f, 0.0f);
	FVector ScreenSize = FVector(256.0f, 128.0f, 0.0f);
	bool bUseAnchoredLayout = false;
	FVector Anchor = FVector(0.0f, 0.0f, 0.0f);
	FVector Alignment = FVector(0.0f, 0.0f, 0.0f);
	FVector AnchorOffset = FVector(0.0f, 0.0f, 0.0f);
	FVector4 Tint = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	float BorderThickness = 0.0f;
	bool bBottomBorderOnly = false;
	FVector4 BorderColor = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	int32 ZOrder = 0;
	int32 FitMode = static_cast<int32>(EUIImageFitMode::Stretch);
	int32 ContentAlignment = static_cast<int32>(EUIImageContentAlignment::Center);
	bool bDrawBackground = false;
	FTextureSlot BackgroundTextureSlot;
	mutable UTexture2D* BackgroundTexture = nullptr;
	int32 BackgroundFitMode = static_cast<int32>(EUIImageFitMode::Stretch);
	int32 BackgroundContentAlignment = static_cast<int32>(EUIImageContentAlignment::Center);
	FVector4 BackgroundTint = FVector4(0.18f, 0.20f, 0.24f, 0.90f);
	bool bDrawShadow = false;
	FVector ShadowOffset = FVector(0.0f, 4.0f, 0.0f);
	FVector4 ShadowTint = FVector4(1.0f, 1.0f, 1.0f, 0.35f);
	FVector4 ShadowTopTint = FVector4(1.0f, 1.0f, 1.0f, 0.50f);
	FVector4 ShadowBottomTint = FVector4(1.0f, 1.0f, 1.0f, 0.10f);
};
