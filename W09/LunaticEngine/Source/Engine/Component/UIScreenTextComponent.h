#pragma once

#include "Component/BillboardComponent.h"
#include "Core/ResourceTypes.h"
#include "Object/FName.h"

class FScene;
struct FPropertyDescriptor;

class UUIScreenTextComponent : public UBillboardComponent
{
public:
	DECLARE_CLASS(UUIScreenTextComponent, UBillboardComponent)

	UUIScreenTextComponent();

	FPrimitiveSceneProxy* CreateSceneProxy() override { return nullptr; }
	void Serialize(FArchive& Ar) override;
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void PostEditProperty(const char* PropertyName) override;
	void ContributeVisuals(FScene& Scene) const override;
	void ContributeSelectedVisuals(FScene& Scene) const override;
	bool SupportsUIScreenPicking() const override { return true; }
	bool HitTestUIScreenPoint(float X, float Y) const override;
	int32 GetUIScreenPickingZOrder() const override { return 0; }
	bool SupportsWorldGizmo() const override { return false; }
	bool ParticipatesInSpatialStructure() const override { return false; }

	void SetText(const FString& InText) { Text = InText; }
	const FString& GetText() const { return Text; }

	void SetFont(const FName& InFontName);
	const FName& GetFontName() const { return FontName; }
	const FFontResource* GetFont() const { return CachedFont; }

	void SetScreenPosition(const FVector& InScreenPosition) { ScreenPosition = InScreenPosition; }
	const FVector& GetScreenPosition() const { return ScreenPosition; }

	bool IsAnchoredLayoutEnabled() const { return bUseAnchoredLayout; }
	void SetAnchoredLayoutEnabled(bool bEnabled) { bUseAnchoredLayout = bEnabled; }

	const FVector& GetAnchor() const { return Anchor; }
	void SetAnchor(const FVector& InAnchor) { Anchor = InAnchor; }

	const FVector& GetAlignment() const { return Alignment; }
	void SetAlignment(const FVector& InAlignment) { Alignment = InAlignment; }

	const FVector& GetAnchorOffset() const { return AnchorOffset; }
	void SetAnchorOffset(const FVector& InAnchorOffset) { AnchorOffset = InAnchorOffset; }

	void SetColor(const FVector4& InColor) { Color = InColor; }
	const FVector4& GetColor() const { return Color; }

	void SetFontSize(float InSize) { FontSize = InSize; }
	float GetFontSize() const { return FontSize; }
	void SetLineSpacing(float InLineSpacing) { LineSpacing = InLineSpacing; }
	float GetLineSpacing() const { return LineSpacing; }
	void SetLetterSpacing(float InLetterSpacing) { LetterSpacing = InLetterSpacing; }
	float GetLetterSpacing() const { return LetterSpacing; }
	void SetBottomBorderThickness(float InThickness) { BottomBorderThickness = InThickness; }
	float GetBottomBorderThickness() const { return BottomBorderThickness; }
	void SetBottomBorderOffset(float InOffset) { BottomBorderOffset = InOffset; }
	float GetBottomBorderOffset() const { return BottomBorderOffset; }
	void SetBottomBorderColor(const FVector4& InColor) { BottomBorderColor = InColor; }
	const FVector4& GetBottomBorderColor() const { return BottomBorderColor; }
	bool ResolveLayoutRect(FVector2& OutPosition, FVector2& OutSize) const;
	bool GetResolvedScreenBounds(float& OutX, float& OutY, float& OutWidth, float& OutHeight) const;

private:
	bool ComputeTextContentSize(float& OutWidth, float& OutHeight) const;
	bool ComputeScreenBounds(float& OutX, float& OutY, float& OutWidth, float& OutHeight) const;
	FVector2 ResolveScreenPosition(const FVector2& ElementSize) const;

	FString Text = FString("Screen Text");
	FName FontName = FName("Default");
	FFontResource* CachedFont = nullptr;
	FVector ScreenPosition = FVector(32.0f, 32.0f, 0.0f);
	bool bUseAnchoredLayout = false;
	FVector Anchor = FVector(0.0f, 0.0f, 0.0f);
	FVector Alignment = FVector(0.0f, 0.0f, 0.0f);
	FVector AnchorOffset = FVector(0.0f, 0.0f, 0.0f);
	FVector4 Color = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	float FontSize = 1.0f;
	float LineSpacing = 1.14f;
	float LetterSpacing = 0.0f;
	float BottomBorderThickness = 0.0f;
	float BottomBorderOffset = 4.0f;
	FVector4 BottomBorderColor = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
};
