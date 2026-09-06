#pragma once

#include "Component/UIImageComponent.h"
#include "Core/ResourceTypes.h"
#include "Object/FName.h"

class UTexture2D;
struct ID3D11ShaderResourceView;
struct FSoundResource;

class UIButtonComponent : public UUIImageComponent
{
public:
	DECLARE_CLASS(UIButtonComponent, UUIImageComponent)

	UIButtonComponent();

	void Serialize(FArchive& Ar) override;
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void PostEditProperty(const char* PropertyName) override;
	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;
	void ContributeVisuals(FScene& Scene) const override;
	void ContributeSelectedVisuals(FScene& Scene) const override;
	bool HitTestUIScreenPoint(float X, float Y) const override;

	const FString& GetLabel() const { return Label; }
	void SetLabel(const FString& InLabel) { Label = InLabel; }
	void SetFont(const FName& InFontName);
	const FName& GetFontName() const { return FontName; }
	const FFontResource* GetFont() const { return CachedFont; }

	bool IsHovered() const { return bHovered; }
	bool IsPressed() const { return bPressed; }
	bool WasClicked() const { return bClickedThisFrame; }

private:
	enum EButtonLabelAlignment : int32
	{
		LabelAlign_Custom = 0,
		LabelAlign_TopLeft,
		LabelAlign_TopCenter,
		LabelAlign_TopRight,
		LabelAlign_CenterLeft,
		LabelAlign_Center,
		LabelAlign_CenterRight,
		LabelAlign_BottomLeft,
		LabelAlign_BottomCenter,
		LabelAlign_BottomRight,
	};

	FVector4 GetCurrentTint() const;
	FVector4 GetCurrentBackgroundTint() const;
	FVector2 GetCurrentPressedOffset() const;
	FVector2 GetCurrentShadowOffset() const;
	FVector2 GetResolvedLabelPosition(const FVector2& ButtonPosition, const FVector2& ButtonSize, const FFontResource* Font) const;
	void GetInteractiveBounds(FVector2& OutPosition, FVector2& OutSize) const;
	bool IsPointInsideButton(float X, float Y) const;
	bool EnsureBackgroundTextureLoaded() const;
	ID3D11ShaderResourceView* GetBackgroundTextureSRV() const;
	void PlayClickSound() const;
	void TriggerAction(const FString& FunctionName) const;

private:
	FString Label = "Button";
	FName FontName = FName("Default");
	FFontResource* CachedFont = nullptr;
	FVector4 LabelColor = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	int32 LabelAlignment = LabelAlign_Custom;
	FVector LabelOffset = FVector(24.0f, 18.0f, 0.0f);
	float LabelScale = 1.0f;
	bool bDrawBackground = false;
	FTextureSlot BackgroundTextureSlot;
	mutable UTexture2D* BackgroundTexture = nullptr;
	int32 BackgroundFitMode = static_cast<int32>(EUIImageFitMode::Stretch);
	int32 BackgroundContentAlignment = static_cast<int32>(EUIImageContentAlignment::Center);
	FVector4 BackgroundNormalTint = FVector4(0.18f, 0.20f, 0.24f, 0.90f);
	FVector4 BackgroundHoverTint = FVector4(0.25f, 0.28f, 0.33f, 0.96f);
	FVector4 BackgroundPressedTint = FVector4(0.12f, 0.14f, 0.18f, 1.0f);
	FVector4 NormalTint = FVector4(1.0f, 1.0f, 1.0f, 0.95f);
	FVector4 HoverTint = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	FVector4 PressedTint = FVector4(0.85f, 0.85f, 0.85f, 1.0f);
	FVector PressedOffset = FVector(0.0f, 3.0f, 0.0f);
	FName ClickSound = FName("None");
	FString OnClickAction;
	FString OnPressAction;
	FString OnReleaseAction;
	FString OnHoverEnterAction;
	FString OnHoverExitAction;
	bool bHovered = false;
	bool bPressed = false;
	bool bClickedThisFrame = false;
};
