#include "Component/UIButtonComponent.h"

#include "Audio/AudioManager.h"
#include "Input/InputManager.h"
#include "Component/ScriptComponent.h"
#include "Engine/Runtime/Engine.h"
#include "GameFramework/AActor.h"
#include "Object/ObjectFactory.h"
#include "Render/Scene/FScene.h"
#include "Resource/ResourceManager.h"
#include "Serialization/Archive.h"
#include "Texture/Texture2D.h"
#include "Viewport/GameViewportClient.h"

#include <algorithm>
#include <Windows.h>

IMPLEMENT_CLASS(UIButtonComponent, UUIImageComponent)

namespace
{
	constexpr uint32 FallbackQuestionCodepoint = static_cast<uint32>('?');
	constexpr float LabelAnchorPadding = 12.0f;
	const FVector4 SelectedUIOutlineColor(0.10f, 0.54f, 0.96f, -1.0f);
	const char* GButtonLabelAlignmentNames[] =
	{
		"Custom",
		"Top Left",
		"Top Center",
		"Top Right",
		"Center Left",
		"Center",
		"Center Right",
		"Bottom Left",
		"Bottom Center",
		"Bottom Right",
	};
	const char* GButtonBackgroundFitModeNames[] = { "Stretch", "Contain", "Cover" };
	const char* GButtonBackgroundContentAlignmentNames[] = { "Center", "Top", "Bottom", "Left", "Right" };

	float GetResolvedGlyphAdvance(const FFontResource* Font, uint32 Codepoint)
	{
		if (!Font)
		{
			return 0.0f;
		}

		if (const FFontGlyph* Glyph = Font->FindGlyph(Codepoint))
		{
			return Glyph->XAdvance;
		}

		if (Codepoint != FallbackQuestionCodepoint)
		{
			if (const FFontGlyph* FallbackGlyph = Font->FindGlyph(FallbackQuestionCodepoint))
			{
				return FallbackGlyph->XAdvance;
			}
		}

		return 1.0f;
	}

	FVector2 MeasureButtonLabel(const FString& Text, const FFontResource* Font, float Scale)
	{
		if (Text.empty())
		{
			return FVector2(0.0f, 0.0f);
		}

		if (!Font)
		{
			const float CharW = 23.0f * Scale * 0.5f;
			return FVector2(CharW * static_cast<float>(Text.size()), 23.0f * Scale);
		}

		if (!Font->bHasGlyphMetrics)
		{
			const float CharW = 23.0f * Scale;
			const float LetterSpacing = -0.5f * CharW;
			const float Width = CharW * static_cast<float>(Text.size()) + LetterSpacing * static_cast<float>((std::max)(0, static_cast<int32>(Text.size()) - 1));
			return FVector2((std::max)(0.0f, Width), 23.0f * Scale);
		}

		const float SourceLineHeight = Font->LineHeight > 0.0f ? Font->LineHeight : 1.0f;
		const float PixelScale = (23.0f * Scale) / SourceLineHeight;
		const float Height = SourceLineHeight * PixelScale;

		float Width = 0.0f;
		const uint8* Ptr = reinterpret_cast<const uint8*>(Text.c_str());
		const uint8* const End = Ptr + Text.size();
		uint32 PrevCodepoint = 0;
		bool bHasPrevCodepoint = false;
		while (Ptr < End)
		{
			uint32 CP = 0;
			if (Ptr[0] < 0x80) { CP = Ptr[0]; Ptr += 1; }
			else if ((Ptr[0] & 0xE0) == 0xC0 && Ptr + 1 < End) { CP = ((Ptr[0] & 0x1F) << 6) | (Ptr[1] & 0x3F); Ptr += 2; }
			else if ((Ptr[0] & 0xF0) == 0xE0 && Ptr + 2 < End) { CP = ((Ptr[0] & 0x0F) << 12) | ((Ptr[1] & 0x3F) << 6) | (Ptr[2] & 0x3F); Ptr += 3; }
			else if ((Ptr[0] & 0xF8) == 0xF0 && Ptr + 3 < End) { CP = ((Ptr[0] & 0x07) << 18) | ((Ptr[1] & 0x3F) << 12) | ((Ptr[2] & 0x3F) << 6) | (Ptr[3] & 0x3F); Ptr += 4; }
			else { ++Ptr; continue; }

			if (bHasPrevCodepoint)
			{
				Width += Font->GetKerning(PrevCodepoint, CP) * PixelScale;
			}

			Width += GetResolvedGlyphAdvance(Font, CP) * PixelScale;
			PrevCodepoint = CP;
			bHasPrevCodepoint = true;
		}

		return FVector2((std::max)(0.0f, Width), Height);
	}
}

UIButtonComponent::UIButtonComponent()
{
	bTickEnable = true;
	PrimaryComponentTick.SetTickEnabled(true);
	SetFont(FontName);
}

void UIButtonComponent::Serialize(FArchive& Ar)
{
	UUIImageComponent::Serialize(Ar);
	Ar << Label;
	Ar << FontName;
	Ar << LabelColor;
	Ar << LabelAlignment;
	Ar << LabelOffset;
	Ar << LabelScale;
	Ar << bDrawBackground;
	Ar << BackgroundTextureSlot.Path;
	Ar << BackgroundFitMode;
	Ar << BackgroundContentAlignment;
	Ar << BackgroundNormalTint;
	Ar << BackgroundHoverTint;
	Ar << BackgroundPressedTint;
	Ar << NormalTint;
	Ar << HoverTint;
	Ar << PressedTint;
	Ar << PressedOffset;
	Ar << ClickSound;
	Ar << OnClickAction;
	Ar << OnPressAction;
	Ar << OnReleaseAction;
	Ar << OnHoverEnterAction;
	Ar << OnHoverExitAction;
}

void UIButtonComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	UUIImageComponent::GetEditableProperties(OutProps);
	OutProps.erase(
		std::remove_if(
			OutProps.begin(),
			OutProps.end(),
			[](const FPropertyDescriptor& Prop)
			{
				return Prop.Name == "Tint"
					|| Prop.Name == "Draw Background"
					|| Prop.Name == "Background Texture"
					|| Prop.Name == "Background Fit Mode"
					|| Prop.Name == "Background Content Alignment"
					|| Prop.Name == "Background Tint";
			}),
		OutProps.end());
	OutProps.push_back({ "Label", EPropertyType::String, &Label });
	OutProps.push_back({ "Font", EPropertyType::Name, &FontName });
	OutProps.push_back({ "Label Color", EPropertyType::Color4, &LabelColor });
	OutProps.push_back({ "Label Alignment", EPropertyType::Enum, &LabelAlignment, 0.0f, 0.0f, 0.1f, GButtonLabelAlignmentNames, static_cast<uint32>(std::size(GButtonLabelAlignmentNames)) });
	OutProps.push_back({ "Label Offset", EPropertyType::Vec3, &LabelOffset, -4096.0f, 4096.0f, 1.0f });
	OutProps.push_back({ "Label Scale", EPropertyType::Float, &LabelScale, 0.1f, 10.0f, 0.05f });
	OutProps.push_back({ "Draw Background", EPropertyType::Bool, &bDrawBackground });
	OutProps.push_back({ "Background Texture", EPropertyType::TextureSlot, &BackgroundTextureSlot });
	OutProps.push_back({ "Background Fit Mode", EPropertyType::Enum, &BackgroundFitMode, 0.0f, 0.0f, 0.1f, GButtonBackgroundFitModeNames, static_cast<uint32>(std::size(GButtonBackgroundFitModeNames)) });
	OutProps.push_back({ "Background Content Alignment", EPropertyType::Enum, &BackgroundContentAlignment, 0.0f, 0.0f, 0.1f, GButtonBackgroundContentAlignmentNames, static_cast<uint32>(std::size(GButtonBackgroundContentAlignmentNames)) });
	OutProps.push_back({ "Normal Fill", EPropertyType::Color4, &BackgroundNormalTint });
	OutProps.push_back({ "Hover Fill", EPropertyType::Color4, &BackgroundHoverTint });
	OutProps.push_back({ "Pressed Fill", EPropertyType::Color4, &BackgroundPressedTint });
	OutProps.push_back({ "Normal Tint", EPropertyType::Color4, &NormalTint });
	OutProps.push_back({ "Hover Tint", EPropertyType::Color4, &HoverTint });
	OutProps.push_back({ "Pressed Tint", EPropertyType::Color4, &PressedTint });
	OutProps.push_back({ "Pressed Offset", EPropertyType::Vec3, &PressedOffset, -64.0f, 64.0f, 1.0f });
	OutProps.push_back({ "Click Sound", EPropertyType::Name, &ClickSound });
	OutProps.push_back({ "On Click Action", EPropertyType::String, &OnClickAction });
	OutProps.push_back({ "On Press Action", EPropertyType::String, &OnPressAction });
	OutProps.push_back({ "On Release Action", EPropertyType::String, &OnReleaseAction });
	OutProps.push_back({ "On Hover Enter Action", EPropertyType::String, &OnHoverEnterAction });
	OutProps.push_back({ "On Hover Exit Action", EPropertyType::String, &OnHoverExitAction });
}

void UIButtonComponent::PostEditProperty(const char* PropertyName)
{
	UUIImageComponent::PostEditProperty(PropertyName);

	if (strcmp(PropertyName, "Font") == 0)
	{
		SetFont(FontName);
	}
	else if (strcmp(PropertyName, "Background Texture") == 0)
	{
		if (BackgroundTextureSlot.Path == "None" || BackgroundTextureSlot.Path.empty())
		{
			BackgroundTexture = nullptr;
		}
		else
		{
			EnsureBackgroundTextureLoaded();
		}
	}
	else if (strcmp(PropertyName, "Background Fit Mode") == 0)
	{
		BackgroundFitMode = static_cast<int32>(SanitizeFitModeValue(BackgroundFitMode));
	}
	else if (strcmp(PropertyName, "Background Content Alignment") == 0)
	{
		BackgroundContentAlignment = static_cast<int32>(SanitizeContentAlignmentValue(BackgroundContentAlignment));
	}
}

void UIButtonComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
	UUIImageComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);

	bClickedThisFrame = false;
	const bool bWasHovered = bHovered;
	const bool bWasPressed = bPressed;

	if (FInputManager::Get().IsGuiUsingMouse())
	{
		bHovered = false;
		bPressed = false;

		if (bWasHovered)
		{
			TriggerAction(OnHoverExitAction);
		}

		return;
	}

	float CursorX = 0.0f;
	float CursorY = 0.0f;
	bool bHasViewportCursor = false;
	if (GEngine)
	{
		if (UGameViewportClient* GameViewportClient = GEngine->GetGameViewportClient())
		{
			bHasViewportCursor = GameViewportClient->TryGetCursorViewportPosition(CursorX, CursorY);
		}
	}

	if (!bHasViewportCursor)
	{
		POINT CursorPoint = FInputManager::Get().GetMousePos();
		HWND ForegroundWindow = ::GetForegroundWindow();
		if (ForegroundWindow)
		{
			::ScreenToClient(ForegroundWindow, &CursorPoint);
		}

		CursorX = static_cast<float>(CursorPoint.x);
		CursorY = static_cast<float>(CursorPoint.y);
	}

	bHovered = IsPointInsideButton(CursorX, CursorY);
	const bool bMouseDown = FInputManager::Get().IsMouseButtonDown(FInputManager::MOUSE_LEFT);
	const bool bMousePressedThisFrame = FInputManager::Get().IsMouseButtonPressed(FInputManager::MOUSE_LEFT);
	const bool bMouseReleasedThisFrame = FInputManager::Get().IsMouseButtonReleased(FInputManager::MOUSE_LEFT);

	if (bMousePressedThisFrame && bHovered)
	{
		bPressed = true;
		bClickedThisFrame = true;
	}
	else if (!bMouseDown)
	{
		bPressed = false;
	}

	if (!bWasHovered && bHovered)
	{
		TriggerAction(OnHoverEnterAction);
	}
	else if (bWasHovered && !bHovered)
	{
		TriggerAction(OnHoverExitAction);
	}

	if (!bWasPressed && bPressed)
	{
		TriggerAction(OnPressAction);
	}

	if (bWasPressed && bMouseReleasedThisFrame)
	{
		TriggerAction(OnReleaseAction);
		if (bHovered)
		{
			PlayClickSound();
			TriggerAction(OnClickAction);
		}
	}
}

void UIButtonComponent::ContributeVisuals(FScene& Scene) const
{
	if (!IsVisible())
	{
		return;
	}

	const FVector2 ResolvedSize = ResolveScreenSize2D();
	const FVector2 BaseResolvedPosition = ResolveScreenPosition(ResolvedSize);
	const FVector2 ResolvedPosition = BaseResolvedPosition + GetCurrentPressedOffset();

	if (bDrawBackground)
	{
		ID3D11ShaderResourceView* BackgroundSRV = GetBackgroundTextureSRV();
		FVector4 BackgroundColor = GetCurrentBackgroundTint();
		const bool bSolidBackground = (BackgroundSRV == nullptr);
		if (bSolidBackground)
		{
			BackgroundColor.W = (BackgroundColor.W > 0.0f) ? -BackgroundColor.W : BackgroundColor.W;
		}

		const FResolvedImageDrawParams BackgroundDrawParams = ResolveImageDrawParams(
			ResolvedPosition,
			ResolvedSize,
			BackgroundTexture,
			SanitizeFitModeValue(BackgroundFitMode),
			SanitizeContentAlignmentValue(BackgroundContentAlignment));

		Scene.AddScreenQuad(
			BackgroundSRV,
			BackgroundDrawParams.Position,
			BackgroundDrawParams.Size,
			BackgroundColor,
			ZOrder - 1,
			BackgroundDrawParams.UVMin,
			BackgroundDrawParams.UVMax,
			bSolidBackground);
	}

	ID3D11ShaderResourceView* SRV = GetResolvedTextureSRV();
	const bool bSolidImage = (SRV == nullptr);
	FVector4 DrawTint = GetCurrentTint();
	if (bSolidImage)
	{
		DrawTint.W = (DrawTint.W > 0.0f) ? -DrawTint.W : DrawTint.W;
	}

	const FResolvedImageDrawParams DrawParams = ResolveImageDrawParams(
		ResolvedPosition,
		ResolvedSize,
		GetTexture(),
		SanitizeFitModeValue(FitMode),
		SanitizeContentAlignmentValue(ContentAlignment));

	if (!bSolidImage && ShouldDrawShadow())
	{
		AddShadowScreenQuad(
			Scene,
			SRV,
			DrawParams.Position + GetCurrentPressedOffset(),
			DrawParams.Size,
			ZOrder - 1,
			DrawParams.UVMin,
			DrawParams.UVMax);
	}

	Scene.AddScreenQuad(
		SRV,
		DrawParams.Position,
		DrawParams.Size,
		DrawTint,
		ZOrder,
		DrawParams.UVMin,
		DrawParams.UVMax,
		bSolidImage);

	if (!Label.empty())
	{
		const FFontResource* ResolvedFont = FResourceManager::Get().FindFont(FontName);
		Scene.AddScreenText(
			Label,
			GetResolvedLabelPosition(ResolvedPosition, ResolvedSize, ResolvedFont ? ResolvedFont : CachedFont),
			LabelScale,
			LabelColor,
			ResolvedFont ? ResolvedFont : CachedFont);
	}
}

void UIButtonComponent::ContributeSelectedVisuals(FScene& Scene) const
{
	if (!IsVisible() || Scene.GetSelectedComponent() != this)
	{
		return;
	}

	constexpr float OutlineThickness = 3.0f;
	FVector2 BoundsPosition;
	FVector2 BoundsSize;
	GetInteractiveBounds(BoundsPosition, BoundsSize);

	const float X = BoundsPosition.X;
	const float Y = BoundsPosition.Y;
	const float W = BoundsSize.X;
	const float H = BoundsSize.Y;
	const int32 OutlineZ = ZOrder + 1000;

	Scene.AddScreenQuad(nullptr, FVector2(X - OutlineThickness, Y - OutlineThickness), FVector2(W + OutlineThickness * 2.0f, OutlineThickness), SelectedUIOutlineColor, OutlineZ);
	Scene.AddScreenQuad(nullptr, FVector2(X - OutlineThickness, Y + H), FVector2(W + OutlineThickness * 2.0f, OutlineThickness), SelectedUIOutlineColor, OutlineZ);
	Scene.AddScreenQuad(nullptr, FVector2(X - OutlineThickness, Y), FVector2(OutlineThickness, H), SelectedUIOutlineColor, OutlineZ);
	Scene.AddScreenQuad(nullptr, FVector2(X + W, Y), FVector2(OutlineThickness, H), SelectedUIOutlineColor, OutlineZ);
}

void UIButtonComponent::SetFont(const FName& InFontName)
{
	FontName = InFontName;
	CachedFont = FResourceManager::Get().FindFont(FontName);
}

FVector4 UIButtonComponent::GetCurrentTint() const
{
	if (bPressed)
	{
		return PressedTint;
	}

	if (bHovered)
	{
		return HoverTint;
	}

	return NormalTint;
}

FVector4 UIButtonComponent::GetCurrentBackgroundTint() const
{
	if (bPressed)
	{
		return BackgroundPressedTint;
	}

	if (bHovered)
	{
		return BackgroundHoverTint;
	}

	return BackgroundNormalTint;
}

FVector2 UIButtonComponent::GetCurrentPressedOffset() const
{
	if (!bPressed)
	{
		return FVector2(0.0f, 0.0f);
	}

	return FVector2(PressedOffset.X, PressedOffset.Y);
}

FVector2 UIButtonComponent::GetCurrentShadowOffset() const
{
	return FVector2(ShadowOffset.X, ShadowOffset.Y) + GetCurrentPressedOffset();
}

FVector2 UIButtonComponent::GetResolvedLabelPosition(const FVector2& ButtonPosition, const FVector2& ButtonSize, const FFontResource* Font) const
{
	if (LabelAlignment == LabelAlign_Custom)
	{
		return FVector2(ButtonPosition.X + LabelOffset.X, ButtonPosition.Y + LabelOffset.Y);
	}

	const FVector2 TextSize = MeasureButtonLabel(Label, Font, LabelScale);
	float X = ButtonPosition.X;
	float Y = ButtonPosition.Y;

	switch (LabelAlignment)
	{
	case LabelAlign_TopLeft:
		X += LabelAnchorPadding;
		Y += LabelAnchorPadding;
		break;
	case LabelAlign_TopCenter:
		X += (ButtonSize.X - TextSize.X) * 0.5f;
		Y += LabelAnchorPadding;
		break;
	case LabelAlign_TopRight:
		X += ButtonSize.X - TextSize.X - LabelAnchorPadding;
		Y += LabelAnchorPadding;
		break;
	case LabelAlign_CenterLeft:
		X += LabelAnchorPadding;
		Y += (ButtonSize.Y - TextSize.Y) * 0.5f;
		break;
	case LabelAlign_Center:
		X += (ButtonSize.X - TextSize.X) * 0.5f;
		Y += (ButtonSize.Y - TextSize.Y) * 0.5f;
		break;
	case LabelAlign_CenterRight:
		X += ButtonSize.X - TextSize.X - LabelAnchorPadding;
		Y += (ButtonSize.Y - TextSize.Y) * 0.5f;
		break;
	case LabelAlign_BottomLeft:
		X += LabelAnchorPadding;
		Y += ButtonSize.Y - TextSize.Y - LabelAnchorPadding;
		break;
	case LabelAlign_BottomCenter:
		X += (ButtonSize.X - TextSize.X) * 0.5f;
		Y += ButtonSize.Y - TextSize.Y - LabelAnchorPadding;
		break;
	case LabelAlign_BottomRight:
		X += ButtonSize.X - TextSize.X - LabelAnchorPadding;
		Y += ButtonSize.Y - TextSize.Y - LabelAnchorPadding;
		break;
	default:
		break;
	}

	return FVector2(X + LabelOffset.X, Y + LabelOffset.Y);
}

void UIButtonComponent::GetInteractiveBounds(FVector2& OutPosition, FVector2& OutSize) const
{
	OutSize = ResolveScreenSize2D();
	OutPosition = ResolveScreenPosition(OutSize);
}

bool UIButtonComponent::IsPointInsideButton(float X, float Y) const
{
	FVector2 BoundsPosition;
	FVector2 BoundsSize;
	GetInteractiveBounds(BoundsPosition, BoundsSize);

	return X >= BoundsPosition.X
		&& X <= BoundsPosition.X + BoundsSize.X
		&& Y >= BoundsPosition.Y
		&& Y <= BoundsPosition.Y + BoundsSize.Y;
}

bool UIButtonComponent::HitTestUIScreenPoint(float X, float Y) const
{
	if (!IsVisible())
	{
		return false;
	}

	return IsPointInsideButton(X, Y);
}

bool UIButtonComponent::EnsureBackgroundTextureLoaded() const
{
	if (BackgroundTextureSlot.Path.empty() || BackgroundTextureSlot.Path == "None")
	{
		BackgroundTexture = nullptr;
		return false;
	}

	if (BackgroundTexture && BackgroundTexture->IsLoaded() && BackgroundTexture->GetSourcePath() == BackgroundTextureSlot.Path)
	{
		return true;
	}

	ID3D11Device* Device = GEngine ? GEngine->GetRenderer().GetFD3DDevice().GetDevice() : nullptr;
	if (!Device)
	{
		return false;
	}

	BackgroundTexture = UTexture2D::LoadFromFile(BackgroundTextureSlot.Path, Device);
	return BackgroundTexture && BackgroundTexture->IsLoaded();
}

ID3D11ShaderResourceView* UIButtonComponent::GetBackgroundTextureSRV() const
{
	if (!BackgroundTexture && !BackgroundTextureSlot.Path.empty() && BackgroundTextureSlot.Path != "None")
	{
		EnsureBackgroundTextureLoaded();
	}

	if (BackgroundTexture && BackgroundTexture->IsLoaded())
	{
		return BackgroundTexture->GetSRV();
	}

	return nullptr;
}

void UIButtonComponent::PlayClickSound() const
{
	if (ClickSound.ToString().empty() || ClickSound == FName("None"))
	{
		return;
	}

	const FSoundResource* SoundResource = FResourceManager::Get().FindSound(ClickSound);
	if (!SoundResource)
	{
		return;
	}

	FAudioManager::Get().PlaySFX(SoundResource->Path, false);
}

void UIButtonComponent::TriggerAction(const FString& FunctionName) const
{
	if (FunctionName.empty())
	{
		return;
	}

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return;
	}

	for (UActorComponent* Component : OwnerActor->GetComponents())
	{
		UScriptComponent* ScriptComponent = Cast<UScriptComponent>(Component);
		if (ScriptComponent)
		{
			ScriptComponent->CallScriptFunction(FunctionName);
		}
	}
}
