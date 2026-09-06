#include "Component/UIScreenTextComponent.h"

#include "Component/CanvasRootComponent.h"
#include "Component/UIImageComponent.h"
#include "Editor/EditorEngine.h"
#include "Editor/Viewport/LevelEditorViewportClient.h"
#include "Engine/Runtime/Engine.h"
#include "Engine/Runtime/WindowsWindow.h"
#include "Render/Scene/FScene.h"
#include "Resource/ResourceManager.h"
#include "Serialization/Archive.h"
#include "Viewport/GameViewportClient.h"
#include "Viewport/Viewport.h"

#include <algorithm>

IMPLEMENT_CLASS(UUIScreenTextComponent, UBillboardComponent)

namespace
{
	const FVector4 SelectedUIOutlineColor(0.10f, 0.54f, 0.96f, -1.0f);
	constexpr uint32 FallbackQuestionCodepoint = static_cast<uint32>('?');
	constexpr uint32 NewLineCodepoint = static_cast<uint32>('\n');
	constexpr uint32 CarriageReturnCodepoint = static_cast<uint32>('\r');

	FVector2 GetViewportSize2D()
	{
		FVector2 ViewportSize(1920.0f, 1080.0f);
		if (GEngine)
		{
			if (UGameViewportClient* GameViewportClient = GEngine->GetGameViewportClient())
			{
				if (FViewport* GameViewport = GameViewportClient->GetViewport())
				{
					const float Width = static_cast<float>(GameViewport->GetWidth());
					const float Height = static_cast<float>(GameViewport->GetHeight());
					if (Width > 0.0f && Height > 0.0f)
					{
						ViewportSize.X = Width;
						ViewportSize.Y = Height;
						return ViewportSize;
					}
				}
			}

			if (GIsEditor)
			{
				if (const UEditorEngine* EditorEngine = Cast<UEditorEngine>(GEngine))
				{
					if (const FLevelEditorViewportClient* ActiveViewport = EditorEngine->GetActiveViewport())
					{
						if (const FViewport* EditorViewport = ActiveViewport->GetViewport())
						{
							const float Width = static_cast<float>(EditorViewport->GetWidth());
							const float Height = static_cast<float>(EditorViewport->GetHeight());
							if (Width > 0.0f && Height > 0.0f)
							{
								ViewportSize.X = Width;
								ViewportSize.Y = Height;
								return ViewportSize;
							}
						}
					}
				}
			}

			if (FWindowsWindow* Window = GEngine->GetWindow())
			{
				ViewportSize.X = (std::max)(1.0f, Window->GetWidth());
				ViewportSize.Y = (std::max)(1.0f, Window->GetHeight());
			}
		}

		return ViewportSize;
	}

	bool ResolveParentUILayoutRect(const USceneComponent* StartParent, FVector2& OutPosition, FVector2& OutSize)
	{
		for (const USceneComponent* Current = StartParent; Current != nullptr; Current = Current->GetParent())
		{
			if (const UCanvasRootComponent* CanvasRoot = dynamic_cast<const UCanvasRootComponent*>(Current))
			{
				const FVector& CanvasSize = CanvasRoot->GetCanvasSize();
				OutPosition = CanvasRoot->GetCanvasOrigin();
				OutSize = FVector2((std::max)(1.0f, CanvasSize.X), (std::max)(1.0f, CanvasSize.Y));
				return true;
			}

			if (const UUIImageComponent* ImageParent = dynamic_cast<const UUIImageComponent*>(Current))
			{
				if (ImageParent->ResolveLayoutRect(OutPosition, OutSize))
				{
					return true;
				}
				continue;
			}

			if (const UUIScreenTextComponent* TextParent = dynamic_cast<const UUIScreenTextComponent*>(Current))
			{
				if (TextParent->ResolveLayoutRect(OutPosition, OutSize))
				{
					return true;
				}
				continue;
			}
		}

		OutPosition = FVector2(0.0f, 0.0f);
		OutSize = GetViewportSize2D();
		return true;
	}

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

	bool DecodeNextUtf8Codepoint(const uint8*& Ptr, const uint8* End, uint32& OutCodepoint)
	{
		if (Ptr >= End)
		{
			return false;
		}

		if (Ptr[0] < 0x80) { OutCodepoint = Ptr[0]; Ptr += 1; return true; }
		if ((Ptr[0] & 0xE0) == 0xC0 && Ptr + 1 < End) { OutCodepoint = ((Ptr[0] & 0x1F) << 6) | (Ptr[1] & 0x3F); Ptr += 2; return true; }
		if ((Ptr[0] & 0xF0) == 0xE0 && Ptr + 2 < End) { OutCodepoint = ((Ptr[0] & 0x0F) << 12) | ((Ptr[1] & 0x3F) << 6) | (Ptr[2] & 0x3F); Ptr += 3; return true; }
		if ((Ptr[0] & 0xF8) == 0xF0 && Ptr + 3 < End) { OutCodepoint = ((Ptr[0] & 0x07) << 18) | ((Ptr[1] & 0x3F) << 12) | ((Ptr[2] & 0x3F) << 6) | (Ptr[3] & 0x3F); Ptr += 4; return true; }

		++Ptr;
		return false;
	}
}

UUIScreenTextComponent::UUIScreenTextComponent()
{
	SetFont(FontName);
	SetCanDeleteFromDetails(true);
}

void UUIScreenTextComponent::Serialize(FArchive& Ar)
{
	UBillboardComponent::Serialize(Ar);
	Ar << Text;
	Ar << FontName;
	Ar << ScreenPosition;
	Ar << bUseAnchoredLayout;
	Ar << Anchor;
	Ar << Alignment;
	Ar << AnchorOffset;
	Ar << Color;
	Ar << FontSize;
	Ar << LineSpacing;
	Ar << LetterSpacing;
	Ar << BottomBorderThickness;
	Ar << BottomBorderOffset;
	Ar << BottomBorderColor;
}

void UUIScreenTextComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	UActorComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Text", EPropertyType::String, &Text });
	OutProps.push_back({ "Font", EPropertyType::Name, &FontName });
	OutProps.push_back({ "Screen Position", EPropertyType::Vec3, &ScreenPosition, 0.0f, 4096.0f, 1.0f });
	OutProps.push_back({ "Use Anchored Layout", EPropertyType::Bool, &bUseAnchoredLayout });
	OutProps.push_back({ "Anchor", EPropertyType::Vec3, &Anchor, 0.0f, 1.0f, 0.01f });
	OutProps.push_back({ "Alignment", EPropertyType::Vec3, &Alignment, 0.0f, 1.0f, 0.01f });
	OutProps.push_back({ "Anchor Offset", EPropertyType::Vec3, &AnchorOffset, -4096.0f, 4096.0f, 1.0f });
	OutProps.push_back({ "Color", EPropertyType::Color4, &Color });
	OutProps.push_back({ "Font Size", EPropertyType::Float, &FontSize, 0.1f, 100.0f, 0.1f });
	OutProps.push_back({ "Line Spacing", EPropertyType::Float, &LineSpacing, 0.0f, 4.0f, 0.01f });
	OutProps.push_back({ "Letter Spacing", EPropertyType::Float, &LetterSpacing, -32.0f, 64.0f, 0.1f });
	OutProps.push_back({ "Bottom Border Thickness", EPropertyType::Float, &BottomBorderThickness, 0.0f, 64.0f, 0.1f });
	OutProps.push_back({ "Bottom Border Offset", EPropertyType::Float, &BottomBorderOffset, -64.0f, 64.0f, 0.1f });
	OutProps.push_back({ "Bottom Border Color", EPropertyType::Color4, &BottomBorderColor });
	OutProps.push_back({ "Visible", EPropertyType::Bool, &bIsVisible });
}

void UUIScreenTextComponent::PostEditProperty(const char* PropertyName)
{
	UBillboardComponent::PostEditProperty(PropertyName);

	if (strcmp(PropertyName, "Font") == 0)
	{
		SetFont(FontName);
	}
}

void UUIScreenTextComponent::ContributeVisuals(FScene& Scene) const
{
	if (!IsVisible())
	{
		return;
	}

	const FFontResource* ResolvedFont = FResourceManager::Get().FindFont(FontName);
	float X = 0.0f;
	float Y = 0.0f;
	float Width = 0.0f;
	float Height = 0.0f;
	if (!ComputeScreenBounds(X, Y, Width, Height))
	{
		return;
	}

	float TextWidth = 0.0f;
	float TextHeight = 0.0f;
	if (!ComputeTextContentSize(TextWidth, TextHeight))
	{
		return;
	}

	Scene.AddScreenText(Text, FVector2(X, Y), FontSize, Color, ResolvedFont ? ResolvedFont : CachedFont, LineSpacing, LetterSpacing);
	if (BottomBorderThickness > 0.0f)
	{
		Scene.AddScreenQuad(nullptr,
			FVector2(X, Y + TextHeight + BottomBorderOffset),
			FVector2(TextWidth, BottomBorderThickness),
			BottomBorderColor,
			1,
			FVector2(0.0f, 0.0f),
			FVector2(1.0f, 1.0f),
			true);
	}
}

void UUIScreenTextComponent::ContributeSelectedVisuals(FScene& Scene) const
{
	if (!IsVisible() || Scene.GetSelectedComponent() != this)
	{
		return;
	}

	float X = 0.0f;
	float Y = 0.0f;
	float Width = 0.0f;
	float Height = 0.0f;
	if (!ComputeScreenBounds(X, Y, Width, Height))
	{
		return;
	}

	constexpr float OutlineThickness = 3.0f;
	const int32 OutlineZ = 1000;

	Scene.AddScreenQuad(nullptr, FVector2(X - OutlineThickness, Y - OutlineThickness), FVector2(Width + OutlineThickness * 2.0f, OutlineThickness), SelectedUIOutlineColor, OutlineZ);
	Scene.AddScreenQuad(nullptr, FVector2(X - OutlineThickness, Y + Height), FVector2(Width + OutlineThickness * 2.0f, OutlineThickness), SelectedUIOutlineColor, OutlineZ);
	Scene.AddScreenQuad(nullptr, FVector2(X - OutlineThickness, Y), FVector2(OutlineThickness, Height), SelectedUIOutlineColor, OutlineZ);
	Scene.AddScreenQuad(nullptr, FVector2(X + Width, Y), FVector2(OutlineThickness, Height), SelectedUIOutlineColor, OutlineZ);
}

bool UUIScreenTextComponent::HitTestUIScreenPoint(float X, float Y) const
{
	float RectX = 0.0f;
	float RectY = 0.0f;
	float RectW = 0.0f;
	float RectH = 0.0f;
	if (!ComputeScreenBounds(RectX, RectY, RectW, RectH))
	{
		return false;
	}

	return X >= RectX && X <= RectX + RectW && Y >= RectY && Y <= RectY + RectH;
}

bool UUIScreenTextComponent::ComputeTextContentSize(float& OutWidth, float& OutHeight) const
{
	if (!IsVisible() || Text.empty())
	{
		return false;
	}

	const FFontResource* Font = FResourceManager::Get().FindFont(FontName);
	const float Scale = FontSize;
	float MaxWidth = 0.0f;
	float CurrentLineWidth = 0.0f;
	float LineHeight = 23.0f * Scale;
	int32 LineCount = 1;
	const float EffectiveLineSpacing = (std::max)(0.0f, LineSpacing);

	if (Font && Font->IsLoaded() && Font->bHasGlyphMetrics && Font->LineHeight > 0.0f)
	{
		const float PixelScale = (23.0f * Scale) / Font->LineHeight;
		LineHeight = Font->LineHeight * PixelScale;

		const uint8* Ptr = reinterpret_cast<const uint8*>(Text.c_str());
		const uint8* const End = Ptr + Text.size();
		uint32 PrevCodepoint = 0;
		bool bHasPrevCodepoint = false;

		while (Ptr < End)
		{
			uint32 CP = 0;
			if (!DecodeNextUtf8Codepoint(Ptr, End, CP))
			{
				continue;
			}

			if (CP == CarriageReturnCodepoint)
			{
				continue;
			}

			if (CP == NewLineCodepoint)
			{
				MaxWidth = (std::max)(MaxWidth, CurrentLineWidth);
				CurrentLineWidth = 0.0f;
				PrevCodepoint = 0;
				bHasPrevCodepoint = false;
				++LineCount;
				continue;
			}

			if (bHasPrevCodepoint)
			{
				CurrentLineWidth += Font->GetKerning(PrevCodepoint, CP) * PixelScale;
				CurrentLineWidth += LetterSpacing;
			}

			CurrentLineWidth += GetResolvedGlyphAdvance(Font, CP) * PixelScale;

			PrevCodepoint = CP;
			bHasPrevCodepoint = true;
		}

		MaxWidth = (std::max)(MaxWidth, CurrentLineWidth);
	}
	else
	{
		const float FallbackAdvance = 23.0f * Scale * 0.5f;
		const uint8* Ptr = reinterpret_cast<const uint8*>(Text.c_str());
		const uint8* const End = Ptr + Text.size();

		while (Ptr < End)
		{
			uint32 CP = 0;
			if (!DecodeNextUtf8Codepoint(Ptr, End, CP))
			{
				continue;
			}

			if (CP == CarriageReturnCodepoint)
			{
				continue;
			}

			if (CP == NewLineCodepoint)
			{
				MaxWidth = (std::max)(MaxWidth, CurrentLineWidth);
				CurrentLineWidth = 0.0f;
				++LineCount;
				continue;
			}

			if (CurrentLineWidth > 0.0f)
			{
				CurrentLineWidth += LetterSpacing;
			}
			CurrentLineWidth += FallbackAdvance;
		}

		MaxWidth = (std::max)(MaxWidth, CurrentLineWidth);
	}

	OutWidth = (std::max)(1.0f, MaxWidth);
	const float TotalHeight = LineHeight + (std::max)(0, LineCount - 1) * (LineHeight * EffectiveLineSpacing);
	OutHeight = (std::max)(1.0f, TotalHeight);
	return true;
}

bool UUIScreenTextComponent::ComputeScreenBounds(float& OutX, float& OutY, float& OutWidth, float& OutHeight) const
{
	float TextWidth = 0.0f;
	float TextHeight = 0.0f;
	if (!ComputeTextContentSize(TextWidth, TextHeight))
	{
		return false;
	}

	OutWidth = TextWidth;
	OutHeight = TextHeight;
	if (BottomBorderThickness > 0.0f)
	{
		OutHeight += (std::max)(0.0f, BottomBorderOffset) + BottomBorderThickness;
	}
	const FVector2 ResolvedPosition = ResolveScreenPosition(FVector2(OutWidth, OutHeight));
	OutX = ResolvedPosition.X;
	OutY = ResolvedPosition.Y;
	return true;
}

bool UUIScreenTextComponent::ResolveLayoutRect(FVector2& OutPosition, FVector2& OutSize) const
{
	float X = 0.0f;
	float Y = 0.0f;
	float Width = 0.0f;
	float Height = 0.0f;
	if (!ComputeScreenBounds(X, Y, Width, Height))
	{
		return false;
	}

	OutPosition = FVector2(X, Y);
	OutSize = FVector2(Width, Height);
	return true;
}

bool UUIScreenTextComponent::GetResolvedScreenBounds(float& OutX, float& OutY, float& OutWidth, float& OutHeight) const
{
	return ComputeScreenBounds(OutX, OutY, OutWidth, OutHeight);
}

void UUIScreenTextComponent::SetFont(const FName& InFontName)
{
	FontName = InFontName;
	CachedFont = FResourceManager::Get().FindFont(FontName);
}

FVector2 UUIScreenTextComponent::ResolveScreenPosition(const FVector2& ElementSize) const
{
	if (!bUseAnchoredLayout)
	{
		return FVector2(ScreenPosition.X, ScreenPosition.Y);
	}

	FVector2 ParentPosition(0.0f, 0.0f);
	FVector2 ParentSize = GetViewportSize2D();
	ResolveParentUILayoutRect(GetParent(), ParentPosition, ParentSize);

	const float ClampedAnchorX = (std::clamp)(Anchor.X, 0.0f, 1.0f);
	const float ClampedAnchorY = (std::clamp)(Anchor.Y, 0.0f, 1.0f);
	const float ClampedAlignmentX = (std::clamp)(Alignment.X, 0.0f, 1.0f);
	const float ClampedAlignmentY = (std::clamp)(Alignment.Y, 0.0f, 1.0f);

	return FVector2(
		ParentPosition.X + ParentSize.X * ClampedAnchorX + AnchorOffset.X - ElementSize.X * ClampedAlignmentX,
		ParentPosition.Y + ParentSize.Y * ClampedAnchorY + AnchorOffset.Y - ElementSize.Y * ClampedAlignmentY);
}
