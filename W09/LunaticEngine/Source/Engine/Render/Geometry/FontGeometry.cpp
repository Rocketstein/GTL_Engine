#include "FontGeometry.h"
#include "Resource/ResourceManager.h"

namespace
{
	constexpr uint32 FallbackQuestionCodepoint = static_cast<uint32>('?');
	constexpr uint32 NewLineCodepoint = static_cast<uint32>('\n');
	constexpr uint32 CarriageReturnCodepoint = static_cast<uint32>('\r');
	const FVector4 SolidMagentaColor(1.0f, 0.0f, 1.0f, 1.0f);

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

void FFontGeometry::Create(ID3D11Device* InDevice)
{
	Device = InDevice;
	if (!Device) return;
	Device->AddRef();

	WorldVB.Create(InDevice, 1024, sizeof(FTextureVertex));
	WorldIB.Create(InDevice, 1536);
	ScreenVB.Create(InDevice, 256, sizeof(FTextureVertex));
	ScreenIB.Create(InDevice, 384);

	if (const FFontResource* DefaultFont = FResourceManager::Get().FindFont(FName("Default")))
	{
		if (DefaultFont->Columns > 0 && DefaultFont->Rows > 0)
		{
			BuildCharInfoMap(DefaultFont->Columns, DefaultFont->Rows);
		}
	}
}

void FFontGeometry::Release()
{
	CharInfoMap.clear();
	Clear();
	ClearScreen();

	WorldVB.Release();
	WorldIB.Release();
	ScreenVB.Release();
	ScreenIB.Release();

	if (Device) { Device->Release(); Device = nullptr; }
}

void FFontGeometry::BuildCharInfoMap(uint32 Columns, uint32 Rows)
{
	CharInfoMap.clear();
	CachedColumns = Columns;
	CachedRows = Rows;

	const float CellW = 1.0f / static_cast<float>(Columns);
	const float CellH = 1.0f / static_cast<float>(Rows);

	auto AddChar = [&](uint32 Codepoint, uint32 Slot)
	{
		const uint32 Col = Slot % Columns;
		const uint32 Row = Slot / Columns;
		if (Row >= Rows) return;
		CharInfoMap[Codepoint] = { Col * CellW, Row * CellH, CellW, CellH };
	};

	// ASCII 32(' ') ~ 126('~')
	for (uint32 CP = 32; CP <= 126; ++CP)
		AddChar(CP, CP - 32);

	// 한글 완성형 가(U+AC00) ~ 힣(U+D7A3)
	uint32 Slot = 127;
	for (uint32 CP = 0xAC00; CP <= 0xD7A3; ++CP, ++Slot)
		AddChar(CP, Slot - 32);
}

void FFontGeometry::EnsureCharInfoMap(const FFontResource* Resource)
{
	if (!Resource || Resource->Columns == 0 || Resource->Rows == 0) return;
	if (CachedColumns == Resource->Columns && CachedRows == Resource->Rows) return;
	BuildCharInfoMap(Resource->Columns, Resource->Rows);
}

bool FFontGeometry::HasGlyphForText(const FFontResource* Font, const FString& Text) const
{
	if (!Font)
	{
		return false;
	}

	const uint8* Ptr = reinterpret_cast<const uint8*>(Text.c_str());
	const uint8* const End = Ptr + Text.size();
	while (Ptr < End)
	{
		uint32 CP = 0;
		if      (Ptr[0] < 0x80)                             { CP = Ptr[0];                                                                       Ptr += 1; }
		else if ((Ptr[0] & 0xE0) == 0xC0 && Ptr + 1 < End)  { CP = ((Ptr[0] & 0x1F) << 6)  |  (Ptr[1] & 0x3F);                                   Ptr += 2; }
		else if ((Ptr[0] & 0xF0) == 0xE0 && Ptr + 2 < End)  { CP = ((Ptr[0] & 0x0F) << 12) | ((Ptr[1] & 0x3F) << 6)  |  (Ptr[2] & 0x3F);         Ptr += 3; }
		else if ((Ptr[0] & 0xF8) == 0xF0 && Ptr + 3 < End)  { CP = ((Ptr[0] & 0x07) << 18) | ((Ptr[1] & 0x3F) << 12) | ((Ptr[2] & 0x3F) << 6) | (Ptr[3] & 0x3F); Ptr += 4; }
		else                                                  { ++Ptr; continue; }

		if (Font->bHasGlyphMetrics)
		{
			if (!Font->FindGlyph(CP))
			{
				return false;
			}
		}
		else
		{
			if (CharInfoMap.find(CP) == CharInfoMap.end())
			{
				return false;
			}
		}
	}

	return true;
}

const FFontResource* FFontGeometry::ResolveFontForText(const FFontResource* PreferredFont, const FString& Text) const
{
	if (PreferredFont && PreferredFont->IsLoaded() && HasGlyphForText(PreferredFont, Text))
	{
		return PreferredFont;
	}

	if (const FFontResource* UIFont = FResourceManager::Get().FindFont(FName("Default.Font.UI")))
	{
		if (UIFont->IsLoaded() && HasGlyphForText(UIFont, Text))
		{
			return UIFont;
		}
	}

	const FFontResource* DefaultFont = FResourceManager::Get().FindFont(FName("Default"));
	if (!PreferredFont || !PreferredFont->IsLoaded())
	{
		return (DefaultFont && DefaultFont->IsLoaded()) ? DefaultFont : PreferredFont;
	}

	if (DefaultFont && DefaultFont->IsLoaded() && HasGlyphForText(DefaultFont, Text))
	{
		return DefaultFont;
	}

	return PreferredFont;
}

bool FFontGeometry::GetGlyph(const FFontResource* Resource, uint32 Codepoint, FFontGlyph& OutGlyph) const
{
	if (Resource && Resource->bHasGlyphMetrics)
	{
		if (const FFontGlyph* Glyph = Resource->FindGlyph(Codepoint))
		{
			OutGlyph = *Glyph;
			return true;
		}
	}

	const auto It = CharInfoMap.find(Codepoint);
	if (It == CharInfoMap.end())
	{
		return false;
	}

	const FCharacterInfo& Info = It->second;
	OutGlyph = {};
	OutGlyph.U0 = Info.U;
	OutGlyph.V0 = Info.V;
	OutGlyph.U1 = Info.U + Info.Width;
	OutGlyph.V1 = Info.V + Info.Height;
	OutGlyph.Width = 1.0f;
	OutGlyph.Height = 1.0f;
	OutGlyph.XAdvance = 1.0f;
	return true;
}

bool FFontGeometry::ResolveGlyph(const FFontResource* Resource, uint32 Codepoint, FResolvedGlyph& OutResolvedGlyph) const
{
	OutResolvedGlyph = {};

	if (GetGlyph(Resource, Codepoint, OutResolvedGlyph.Glyph))
	{
		return true;
	}

	if (Codepoint != FallbackQuestionCodepoint && GetGlyph(Resource, FallbackQuestionCodepoint, OutResolvedGlyph.Glyph))
	{
		return true;
	}

	OutResolvedGlyph.bDrawSolidMagenta = true;
	OutResolvedGlyph.Glyph = {};
	OutResolvedGlyph.Glyph.Width = 1.0f;
	OutResolvedGlyph.Glyph.Height = 1.0f;
	OutResolvedGlyph.Glyph.XAdvance = 1.0f;
	return true;
}

void FFontGeometry::Clear()
{
	WorldVertices.clear();
	WorldIndices.clear();
	WorldBatches.clear();
}

void FFontGeometry::ClearScreen()
{
	ScreenVertices.clear();
	ScreenIndices.clear();
	ScreenBatches.clear();
}

void FFontGeometry::AddWorldText(const FString& Text,
	const FVector& WorldPos,
	const FVector& TextRight,
	const FVector& TextUp,
	const FVector& WorldScale,
	const FVector4& Color,
	const FFontResource* Font,
	float Scale)
{
	if (Text.empty()) return;

	Font = ResolveFontForText(Font, Text);
	if (!Font || !Font->IsLoaded()) return;
	EnsureCharInfoMap(Font);
	FVector4 VertexColor = Color;
	VertexColor.A = Font->bHasGlyphMetrics ? std::abs(VertexColor.A) : -std::abs(VertexColor.A);

	const float SourceLineHeight = (Font->bHasGlyphMetrics && Font->LineHeight > 0.0f) ? Font->LineHeight : 1.0f;
	const float HorizontalScale = (0.5f * Scale * WorldScale.Y) / SourceLineHeight;
	const float VerticalScale = (0.5f * Scale * WorldScale.Z) / SourceLineHeight;
	const float LineHeightWorld = SourceLineHeight * VerticalScale;
	const uint32 IdxBase = static_cast<uint32>(WorldIndices.size());
	float CursorX = 0.0f;
	uint32 PrevCodepoint = 0;
	bool bHasPrevCodepoint = false;

	const uint8* Ptr = reinterpret_cast<const uint8*>(Text.c_str());
	const uint8* const End = Ptr + Text.size();

	while (Ptr < End)
	{
		uint32 CP = 0;
		if      (Ptr[0] < 0x80)                             { CP = Ptr[0];                                                                       Ptr += 1; }
		else if ((Ptr[0] & 0xE0) == 0xC0 && Ptr + 1 < End)  { CP = ((Ptr[0] & 0x1F) << 6)  |  (Ptr[1] & 0x3F);                                   Ptr += 2; }
		else if ((Ptr[0] & 0xF0) == 0xE0 && Ptr + 2 < End)  { CP = ((Ptr[0] & 0x0F) << 12) | ((Ptr[1] & 0x3F) << 6)  |  (Ptr[2] & 0x3F);         Ptr += 3; }
		else if ((Ptr[0] & 0xF8) == 0xF0 && Ptr + 3 < End)  { CP = ((Ptr[0] & 0x07) << 18) | ((Ptr[1] & 0x3F) << 12) | ((Ptr[2] & 0x3F) << 6) | (Ptr[3] & 0x3F); Ptr += 4; }
		else                                                  { ++Ptr; continue; }

		if (bHasPrevCodepoint)
		{
			CursorX += Font->GetKerning(PrevCodepoint, CP) * HorizontalScale;
		}

		FResolvedGlyph ResolvedGlyph;
		if (!ResolveGlyph(Font, CP, ResolvedGlyph))
		{
			PrevCodepoint = CP;
			bHasPrevCodepoint = true;
			continue;
		}

		const FFontGlyph& Glyph = ResolvedGlyph.Glyph;
		const FVector4 GlyphColor = ResolvedGlyph.bDrawSolidMagenta ? SolidMagentaColor : VertexColor;

		if (Glyph.Width > 0.0f && Glyph.Height > 0.0f)
		{
			const float LeftOffset = CursorX + Glyph.XOffset * HorizontalScale;
			const float TopOffset = Glyph.YOffset * VerticalScale;
			const float GlyphWidth = Glyph.Width * HorizontalScale;
			const float GlyphHeight = Glyph.Height * VerticalScale;

			const FVector TopLeft = WorldPos + TextRight * LeftOffset + TextUp * (LineHeightWorld * 0.5f - TopOffset);
			const FVector TopRight = TopLeft + TextRight * GlyphWidth;
			const FVector BottomLeft = TopLeft - TextUp * GlyphHeight;
			const FVector BottomRight = BottomLeft + TextRight * GlyphWidth;

			const uint32 Vi = static_cast<uint32>(WorldVertices.size());
			const FVector2 UV0 = ResolvedGlyph.bDrawSolidMagenta ? FVector2(-1.0f, -1.0f) : FVector2(Glyph.U0, Glyph.V0);
			const FVector2 UV1 = ResolvedGlyph.bDrawSolidMagenta ? FVector2(-1.0f, -1.0f) : FVector2(Glyph.U1, Glyph.V1);
			WorldVertices.push_back({ TopLeft, GlyphColor, FVector2(UV0.X, UV0.Y) });
			WorldVertices.push_back({ TopRight, GlyphColor, FVector2(UV1.X, UV0.Y) });
			WorldVertices.push_back({ BottomLeft, GlyphColor, FVector2(UV0.X, UV1.Y) });
			WorldVertices.push_back({ BottomRight, GlyphColor, FVector2(UV1.X, UV1.Y) });

			WorldIndices.push_back(Vi);
			WorldIndices.push_back(Vi + 1);
			WorldIndices.push_back(Vi + 2);
			WorldIndices.push_back(Vi + 1);
			WorldIndices.push_back(Vi + 3);
			WorldIndices.push_back(Vi + 2);
		}

		CursorX += Glyph.XAdvance * HorizontalScale;
		PrevCodepoint = CP;
		bHasPrevCodepoint = true;
	}

	const uint32 AddedIndexCount = static_cast<uint32>(WorldIndices.size()) - IdxBase;
	if (AddedIndexCount > 0)
	{
		WorldBatches.push_back({ IdxBase, AddedIndexCount, Font });
	}
}

void FFontGeometry::AddScreenText(const FString& Text,
	float ScreenX, float ScreenY,
	float ViewportWidth, float ViewportHeight,
	const FVector4& Color,
	const FFontResource* Font,
	float Scale,
	float LineSpacing,
	float LetterSpacing)
{
	if (Text.empty()) return;
	if (ViewportWidth <= 0.0f || ViewportHeight <= 0.0f) return;

	Font = ResolveFontForText(Font, Text);
	if (!Font || !Font->IsLoaded()) return;
	EnsureCharInfoMap(Font);
	FVector4 VertexColor = Color;
	VertexColor.A = Font->bHasGlyphMetrics ? std::abs(VertexColor.A) : -std::abs(VertexColor.A);

	if (!Font->bHasGlyphMetrics)
	{
		const float CharW = 23.0f * Scale;
		const float CharH = 23.0f * Scale;
		const float EffectiveLineSpacing = (std::max)(0.0f, LineSpacing);
		const uint32 IdxBase = static_cast<uint32>(ScreenIndices.size());
		float CursorX = ScreenX;
		float CursorY = ScreenY;
		bool bHasGlyphInLine = false;

		auto PixelToClipX = [ViewportWidth](float X) -> float
			{
				return (X / ViewportWidth) * 2.0f - 1.0f;
			};

		auto PixelToClipY = [ViewportHeight](float Y) -> float
			{
				return 1.0f - (Y / ViewportHeight) * 2.0f;
			};

		const uint8* LegacyPtr = reinterpret_cast<const uint8*>(Text.c_str());
		const uint8* const LegacyEnd = LegacyPtr + Text.size();
		while (LegacyPtr < LegacyEnd)
		{
			uint32 CP = 0;
			if (!DecodeNextUtf8Codepoint(LegacyPtr, LegacyEnd, CP))
			{
				continue;
			}

			if (CP == CarriageReturnCodepoint)
			{
				continue;
			}

			if (CP == NewLineCodepoint)
			{
				CursorX = ScreenX;
				CursorY += CharH * EffectiveLineSpacing;
				bHasGlyphInLine = false;
				continue;
			}

			FResolvedGlyph ResolvedGlyph;
			if (!ResolveGlyph(Font, CP, ResolvedGlyph))
			{
				continue;
			}

			const FFontGlyph& Glyph = ResolvedGlyph.Glyph;
			const FVector4 GlyphColor = ResolvedGlyph.bDrawSolidMagenta ? SolidMagentaColor : VertexColor;

			if (bHasGlyphInLine)
			{
				CursorX += LetterSpacing;
			}

			const float Left = PixelToClipX(CursorX);
			const float Right = PixelToClipX(CursorX + CharW);
			const float Top = PixelToClipY(CursorY);
			const float Bottom = PixelToClipY(CursorY + CharH);
			const uint32 Vi = static_cast<uint32>(ScreenVertices.size());
			const FVector2 UV0 = ResolvedGlyph.bDrawSolidMagenta ? FVector2(-1.0f, -1.0f) : FVector2(Glyph.U0, Glyph.V0);
			const FVector2 UV1 = ResolvedGlyph.bDrawSolidMagenta ? FVector2(-1.0f, -1.0f) : FVector2(Glyph.U1, Glyph.V1);
			ScreenVertices.push_back({ FVector(Left,  Top,    0.0f), GlyphColor, FVector2(UV0.X, UV0.Y) });
			ScreenVertices.push_back({ FVector(Right, Top,    0.0f), GlyphColor, FVector2(UV1.X, UV0.Y) });
			ScreenVertices.push_back({ FVector(Left,  Bottom, 0.0f), GlyphColor, FVector2(UV0.X, UV1.Y) });
			ScreenVertices.push_back({ FVector(Right, Bottom, 0.0f), GlyphColor, FVector2(UV1.X, UV1.Y) });

			ScreenIndices.push_back(Vi);
			ScreenIndices.push_back(Vi + 1);
			ScreenIndices.push_back(Vi + 2);
			ScreenIndices.push_back(Vi + 1);
			ScreenIndices.push_back(Vi + 3);
			ScreenIndices.push_back(Vi + 2);
			CursorX += CharW;
			bHasGlyphInLine = true;
		}

		const uint32 AddedIndexCount = static_cast<uint32>(ScreenIndices.size()) - IdxBase;
		if (AddedIndexCount > 0)
		{
			ScreenBatches.push_back({ IdxBase, AddedIndexCount, Font });
		}
		return;
	}

	const float SourceLineHeight = (Font->bHasGlyphMetrics && Font->LineHeight > 0.0f) ? Font->LineHeight : 1.0f;
	const float PixelScale = (23.0f * Scale) / SourceLineHeight;
	const float EffectiveLineSpacing = (std::max)(0.0f, LineSpacing);
	const uint32 IdxBase = static_cast<uint32>(ScreenIndices.size());

	const uint8* Ptr = reinterpret_cast<const uint8*>(Text.c_str());
	const uint8* const End = Ptr + Text.size();

	float CursorX = ScreenX;
	float CursorY = ScreenY;
	uint32 PrevCodepoint = 0;
	bool bHasPrevCodepoint = false;

	auto PixelToClipX = [ViewportWidth](float X) -> float
		{
			return (X / ViewportWidth) * 2.0f - 1.0f;
		};

	auto PixelToClipY = [ViewportHeight](float Y) -> float
		{
			return 1.0f - (Y / ViewportHeight) * 2.0f;
		};

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
			CursorX = ScreenX;
			CursorY += SourceLineHeight * PixelScale * EffectiveLineSpacing;
			PrevCodepoint = 0;
			bHasPrevCodepoint = false;
			continue;
		}

		if (bHasPrevCodepoint)
		{
			CursorX += Font->GetKerning(PrevCodepoint, CP) * PixelScale;
			CursorX += LetterSpacing;
		}

		FResolvedGlyph ResolvedGlyph;
		if (!ResolveGlyph(Font, CP, ResolvedGlyph))
		{
			PrevCodepoint = CP;
			bHasPrevCodepoint = true;
			continue;
		}

		const FFontGlyph& Glyph = ResolvedGlyph.Glyph;
		const FVector4 GlyphColor = ResolvedGlyph.bDrawSolidMagenta ? SolidMagentaColor : VertexColor;

		if (Glyph.Width > 0.0f && Glyph.Height > 0.0f)
		{
			const float LeftPx = CursorX + Glyph.XOffset * PixelScale;
			const float TopPx = CursorY + Glyph.YOffset * PixelScale;
			const float RightPx = LeftPx + Glyph.Width * PixelScale;
			const float BottomPx = TopPx + Glyph.Height * PixelScale;

			const float Left = PixelToClipX(LeftPx);
			const float Right = PixelToClipX(RightPx);
			const float Top = PixelToClipY(TopPx);
			const float Bottom = PixelToClipY(BottomPx);

			const uint32 Vi = static_cast<uint32>(ScreenVertices.size());
			const FVector2 UV0 = ResolvedGlyph.bDrawSolidMagenta ? FVector2(-1.0f, -1.0f) : FVector2(Glyph.U0, Glyph.V0);
			const FVector2 UV1 = ResolvedGlyph.bDrawSolidMagenta ? FVector2(-1.0f, -1.0f) : FVector2(Glyph.U1, Glyph.V1);
			ScreenVertices.push_back({ FVector(Left,  Top,    0.0f), GlyphColor, FVector2(UV0.X, UV0.Y) });
			ScreenVertices.push_back({ FVector(Right, Top,    0.0f), GlyphColor, FVector2(UV1.X, UV0.Y) });
			ScreenVertices.push_back({ FVector(Left,  Bottom, 0.0f), GlyphColor, FVector2(UV0.X, UV1.Y) });
			ScreenVertices.push_back({ FVector(Right, Bottom, 0.0f), GlyphColor, FVector2(UV1.X, UV1.Y) });

			ScreenIndices.push_back(Vi);
			ScreenIndices.push_back(Vi + 1);
			ScreenIndices.push_back(Vi + 2);
			ScreenIndices.push_back(Vi + 1);
			ScreenIndices.push_back(Vi + 3);
			ScreenIndices.push_back(Vi + 2);
		}

		CursorX += Glyph.XAdvance * PixelScale;
		PrevCodepoint = CP;
		bHasPrevCodepoint = true;
	}

	const uint32 AddedIndexCount = static_cast<uint32>(ScreenIndices.size()) - IdxBase;
	if (AddedIndexCount > 0)
	{
		ScreenBatches.push_back({ IdxBase, AddedIndexCount, Font });
	}
}

bool FFontGeometry::UploadWorldBuffers(ID3D11DeviceContext* Context)
{
	if (WorldVertices.empty()) return false;

	const uint32 VertCount = static_cast<uint32>(WorldVertices.size());
	const uint32 IdxCount  = static_cast<uint32>(WorldIndices.size());

	WorldVB.EnsureCapacity(Device, VertCount);
	WorldIB.EnsureCapacity(Device, IdxCount);
	if (!WorldVB.Update(Context, WorldVertices.data(), VertCount)) return false;
	if (!WorldIB.Update(Context, WorldIndices.data(), IdxCount)) return false;
	return true;
}

bool FFontGeometry::UploadScreenBuffers(ID3D11DeviceContext* Context)
{
	if (ScreenVertices.empty()) return false;

	const uint32 VertCount = static_cast<uint32>(ScreenVertices.size());
	const uint32 IdxCount  = static_cast<uint32>(ScreenIndices.size());

	ScreenVB.EnsureCapacity(Device, VertCount);
	ScreenIB.EnsureCapacity(Device, IdxCount);
	if (!ScreenVB.Update(Context, ScreenVertices.data(), VertCount)) return false;
	if (!ScreenIB.Update(Context, ScreenIndices.data(), IdxCount)) return false;
	return true;
}
