#include "Render/Submission/Batching/UIBatch.h"

#include "Render/Scene/Proxies/UI/UIProxy.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr float GAsciiAdvanceRatio[95] = {
    0.280f, 0.250f, 0.344f, 0.438f, 0.438f, 0.688f, 0.531f, 0.250f,
    0.281f, 0.281f, 0.375f, 0.438f, 0.250f, 0.312f, 0.250f, 0.375f,
    0.438f, 0.406f, 0.438f, 0.438f, 0.469f, 0.438f, 0.438f, 0.438f,
    0.438f, 0.438f, 0.250f, 0.250f, 0.438f, 0.438f, 0.438f, 0.375f,
    0.656f, 0.500f, 0.469f, 0.500f, 0.469f, 0.406f, 0.406f, 0.500f,
    0.500f, 0.250f, 0.406f, 0.469f, 0.406f, 0.531f, 0.500f, 0.562f,
    0.469f, 0.562f, 0.469f, 0.469f, 0.500f, 0.500f, 0.500f, 0.625f,
    0.469f, 0.500f, 0.438f, 0.281f, 0.375f, 0.281f, 0.438f, 0.500f,
    0.312f, 0.438f, 0.469f, 0.406f, 0.438f, 0.438f, 0.344f, 0.469f,
    0.438f, 0.250f, 0.312f, 0.438f, 0.250f, 0.625f, 0.438f, 0.438f,
    0.469f, 0.438f, 0.344f, 0.406f, 0.375f, 0.438f, 0.438f, 0.625f,
    0.438f, 0.438f, 0.438f, 0.312f, 0.250f, 0.312f, 0.438f
};

bool DecodeUTF8Codepoint(const uint8*& Ptr, const uint8* End, uint32& OutCodepoint)
{
    if (Ptr >= End)
    {
        return false;
    }

    if (Ptr[0] < 0x80)
    {
        OutCodepoint = Ptr[0];
        Ptr += 1;
        return true;
    }

    if ((Ptr[0] & 0xE0) == 0xC0 && Ptr + 1 < End)
    {
        OutCodepoint = ((Ptr[0] & 0x1F) << 6) | (Ptr[1] & 0x3F);
        Ptr += 2;
        return true;
    }

    if ((Ptr[0] & 0xF0) == 0xE0 && Ptr + 2 < End)
    {
        OutCodepoint = ((Ptr[0] & 0x0F) << 12) | ((Ptr[1] & 0x3F) << 6) | (Ptr[2] & 0x3F);
        Ptr += 3;
        return true;
    }

    if ((Ptr[0] & 0xF8) == 0xF0 && Ptr + 3 < End)
    {
        OutCodepoint = ((Ptr[0] & 0x07) << 18) | ((Ptr[1] & 0x3F) << 12) | ((Ptr[2] & 0x3F) << 6) | (Ptr[3] & 0x3F);
        Ptr += 4;
        return true;
    }

    ++Ptr;
    return false;
}

TArray<TArray<uint32>> SplitTextLines(const FString& Text)
{
    TArray<TArray<uint32>> Lines;
    Lines.emplace_back();

    const uint8* Ptr = reinterpret_cast<const uint8*>(Text.c_str());
    const uint8* const End = Ptr + Text.size();

    while (Ptr < End)
    {
        uint32 Codepoint = 0;
        if (!DecodeUTF8Codepoint(Ptr, End, Codepoint))
        {
            continue;
        }

        if (Codepoint == '\r')
        {
            continue;
        }

        if (Codepoint == '\n')
        {
            Lines.emplace_back();
            continue;
        }

        Lines.back().push_back(Codepoint);
    }

    return Lines;
}

bool IsTextWhitespace(uint32 Codepoint)
{
    return Codepoint == ' ' || Codepoint == '\t';
}

float GetTextGlyphAdvance(uint32 Codepoint, float CharW)
{
    if (Codepoint >= 32 && Codepoint <= 126)
    {
        return CharW * GAsciiAdvanceRatio[Codepoint - 32];
    }

    if (Codepoint == '\t')
    {
        return CharW * 2.0f;
    }

    return CharW;
}

float GetTextLineWidth(const TArray<uint32>& Line, float CharW, float LetterSpacing)
{
    if (Line.empty())
    {
        return 0.0f;
    }

    float Width = 0.0f;
    for (size_t Index = 0; Index < Line.size(); ++Index)
    {
        Width += GetTextGlyphAdvance(Line[Index], CharW);
        if (Index + 1 < Line.size())
        {
            Width += LetterSpacing;
        }
    }
    return Width;
}

FVector TransformWorldUILocal(const FUIProxy& Proxy, float LocalY, float LocalZ, const FVector& CameraRight, const FVector& CameraUp)
{
    if (!Proxy.bBillboard)
    {
        return Proxy.WorldMatrix.TransformPositionWithW(FVector(0.0f, LocalY, LocalZ));
    }

    const FVector Center = Proxy.WorldMatrix.GetLocation();
    const FVector WorldScale = Proxy.WorldMatrix.GetScale();
    const FVector Right = CameraRight.Normalized() * WorldScale.Y;
    const FVector Up = CameraUp.Normalized() * WorldScale.Z;
    return Center + Right * LocalY + Up * LocalZ;
}
} // namespace

void FUIBatch::Create(ID3D11Device* InDevice)
{
    Buffer.Create(InDevice, 256, 384);
    SolidParamsCB.Create(InDevice, sizeof(FUIParamsCBData));
    TextureParamsCB.Create(InDevice, sizeof(FUIParamsCBData));
    FontParamsCB.Create(InDevice, sizeof(FUIParamsCBData));
    WorldSolidParamsCB.Create(InDevice, sizeof(FUIParamsCBData));
    WorldTextureParamsCB.Create(InDevice, sizeof(FUIParamsCBData));
    WorldFontParamsCB.Create(InDevice, sizeof(FUIParamsCBData));
}

void FUIBatch::Release()
{
    CharInfoMap.clear();
    Clear();
    WorldFontParamsCB.Release();
    WorldTextureParamsCB.Release();
    WorldSolidParamsCB.Release();
    FontParamsCB.Release();
    SolidParamsCB.Release();
    TextureParamsCB.Release();
    Buffer.Release();
}

void FUIBatch::Clear()
{
    Buffer.Clear();
}

FUIBatchRange FUIBatch::AddScreenQuad(const FUIProxy& Proxy, float ViewportWidth, float ViewportHeight)
{
    FUIBatchRange Range = {};
    const FVector2 LayoutSize = Proxy.GetScreenLayoutSize(ViewportWidth, ViewportHeight);
    if (ViewportWidth <= 0.0f || ViewportHeight <= 0.0f ||
        LayoutSize.X == 0.0f || LayoutSize.Y == 0.0f)
    {
        return Range;
    }

    const FVector2 PivotPosition = Proxy.GetScreenPivotPosition(ViewportWidth, ViewportHeight);
    const float Left   = -Proxy.Pivot.X * LayoutSize.X;
    const float Top    = -Proxy.Pivot.Y * LayoutSize.Y;
    const float Right  = Left + LayoutSize.X;
    const float Bottom = Top + LayoutSize.Y;

    const float Radians = Proxy.RotationDegrees * 3.14159265358979323846f / 180.0f;
    const float CosTheta = std::cos(Radians);
    const float SinTheta = std::sin(Radians);

    auto TransformCorner = [&](float X, float Y) -> FVector2
    {
        return FVector2(
            PivotPosition.X + X * CosTheta - Y * SinTheta,
            PivotPosition.Y + X * SinTheta + Y * CosTheta);
    };

    const FVector2 TopLeft = TransformCorner(Left, Top);
    const FVector2 TopRight = TransformCorner(Right, Top);
    const FVector2 BottomLeft = TransformCorner(Left, Bottom);
    const FVector2 BottomRight = TransformCorner(Right, Bottom);

    auto PixelToClipX = [ViewportWidth](float X) -> float
    {
        return (X / ViewportWidth) * 2.0f - 1.0f;
    };

    auto PixelToClipY = [ViewportHeight](float Y) -> float
    {
        return 1.0f - (Y / ViewportHeight) * 2.0f;
    };

    const uint32 BaseVertex = static_cast<uint32>(Buffer.Vertices.size());
    const uint32 BaseIndex  = static_cast<uint32>(Buffer.Indices.size());

    const float U0 = Proxy.SubUVRect.X;
    const float V0 = Proxy.SubUVRect.Y;
    const float U1 = Proxy.SubUVRect.X + Proxy.SubUVRect.Z;
    const float V1 = Proxy.SubUVRect.Y + Proxy.SubUVRect.W;

    Buffer.Vertices.push_back({ FVector(PixelToClipX(TopLeft.X), PixelToClipY(TopLeft.Y), 0.0f), FVector2(U0, V0), Proxy.TintColor });
    Buffer.Vertices.push_back({ FVector(PixelToClipX(TopRight.X), PixelToClipY(TopRight.Y), 0.0f), FVector2(U1, V0), Proxy.TintColor });
    Buffer.Vertices.push_back({ FVector(PixelToClipX(BottomLeft.X), PixelToClipY(BottomLeft.Y), 0.0f), FVector2(U0, V1), Proxy.TintColor });
    Buffer.Vertices.push_back({ FVector(PixelToClipX(BottomRight.X), PixelToClipY(BottomRight.Y), 0.0f), FVector2(U1, V1), Proxy.TintColor });

    Buffer.Indices.push_back(BaseVertex);
    Buffer.Indices.push_back(BaseVertex + 1);
    Buffer.Indices.push_back(BaseVertex + 2);
    Buffer.Indices.push_back(BaseVertex + 1);
    Buffer.Indices.push_back(BaseVertex + 3);
    Buffer.Indices.push_back(BaseVertex + 2);

    Range.FirstIndex = BaseIndex;
    Range.IndexCount = 6;
    return Range;
}

FUIBatchRange FUIBatch::AddWorldQuad(const FUIProxy& Proxy, const FVector& CameraRight, const FVector& CameraUp)
{
    FUIBatchRange Range = {};
    const FVector2 LayoutSize = Proxy.GetWorldLayoutSize();
    if (LayoutSize.X == 0.0f || LayoutSize.Y == 0.0f)
    {
        return Range;
    }

    const float Left = -Proxy.Pivot.X * LayoutSize.X;
    const float Right = Left + LayoutSize.X;
    const float Top = (1.0f - Proxy.Pivot.Y) * LayoutSize.Y;
    const float Bottom = Top - LayoutSize.Y;

    const float Radians = Proxy.RotationDegrees * 3.14159265358979323846f / 180.0f;
    const float CosTheta = std::cos(Radians);
    const float SinTheta = std::sin(Radians);

    auto TransformCorner = [&](float Y, float Z) -> FVector
    {
        const float RotY = Y * CosTheta - Z * SinTheta;
        const float RotZ = Y * SinTheta + Z * CosTheta;
        return TransformWorldUILocal(Proxy, RotY, RotZ, CameraRight, CameraUp);
    };

    const FVector TopLeft = TransformCorner(Left, Top);
    const FVector TopRight = TransformCorner(Right, Top);
    const FVector BottomLeft = TransformCorner(Left, Bottom);
    const FVector BottomRight = TransformCorner(Right, Bottom);

    const uint32 BaseVertex = static_cast<uint32>(Buffer.Vertices.size());
    const uint32 BaseIndex = static_cast<uint32>(Buffer.Indices.size());

    const float U0 = Proxy.SubUVRect.X;
    const float V0 = Proxy.SubUVRect.Y;
    const float U1 = Proxy.SubUVRect.X + Proxy.SubUVRect.Z;
    const float V1 = Proxy.SubUVRect.Y + Proxy.SubUVRect.W;

    Buffer.Vertices.push_back({ TopLeft, FVector2(U0, V0), Proxy.TintColor });
    Buffer.Vertices.push_back({ TopRight, FVector2(U1, V0), Proxy.TintColor });
    Buffer.Vertices.push_back({ BottomLeft, FVector2(U0, V1), Proxy.TintColor });
    Buffer.Vertices.push_back({ BottomRight, FVector2(U1, V1), Proxy.TintColor });

    Buffer.Indices.push_back(BaseVertex);
    Buffer.Indices.push_back(BaseVertex + 1);
    Buffer.Indices.push_back(BaseVertex + 2);
    Buffer.Indices.push_back(BaseVertex + 1);
    Buffer.Indices.push_back(BaseVertex + 3);
    Buffer.Indices.push_back(BaseVertex + 2);

    Range.FirstIndex = BaseIndex;
    Range.IndexCount = 6;
    return Range;
}

FUIBatchRange FUIBatch::AddScreenText(const FUIProxy& Proxy, const FFontResource* Font, float ViewportWidth, float ViewportHeight)
{
    FUIBatchRange Range = {};
    if (Proxy.Text.empty() || !Font || !Font->IsLoaded() ||
        ViewportWidth <= 0.0f || ViewportHeight <= 0.0f)
    {
        return Range;
    }

    EnsureCharInfoMap(Font);

    const FVector2 LayoutSize = Proxy.GetScreenLayoutSize(ViewportWidth, ViewportHeight);
    if (LayoutSize.X == 0.0f || LayoutSize.Y == 0.0f)
    {
        return Range;
    }

    const float CharW = 23.0f * std::max(Proxy.FontSize, 0.01f);
    const float CharH = 23.0f * std::max(Proxy.FontSize, 0.01f);
    const float LetterSpacing = Proxy.TextLetterSpacing;
    const float LineSpacing = Proxy.TextLineSpacing;

    TArray<TArray<uint32>> Lines = SplitTextLines(Proxy.Text);
    if (Lines.empty())
    {
        return Range;
    }

    const float BlockHeight =
        static_cast<float>(Lines.size()) * CharH +
        static_cast<float>(Lines.size() > 0 ? Lines.size() - 1 : 0) * LineSpacing;

    const float LocalLeft = -Proxy.Pivot.X * LayoutSize.X;
    const float LocalTop = -Proxy.Pivot.Y * LayoutSize.Y;

    float BaseY = LocalTop;
    if (Proxy.TextVAlign == EUITextVAlign::Center)
    {
        BaseY += (LayoutSize.Y - BlockHeight) * 0.5f;
    }
    else if (Proxy.TextVAlign == EUITextVAlign::Bottom)
    {
        BaseY += LayoutSize.Y - BlockHeight;
    }

    const FVector2 PivotPosition = Proxy.GetScreenPivotPosition(ViewportWidth, ViewportHeight);
    const float Radians = Proxy.RotationDegrees * 3.14159265358979323846f / 180.0f;
    const float CosTheta = std::cos(Radians);
    const float SinTheta = std::sin(Radians);

    auto TransformLocal = [&](float X, float Y) -> FVector2
    {
        return FVector2(
            PivotPosition.X + X * CosTheta - Y * SinTheta,
            PivotPosition.Y + X * SinTheta + Y * CosTheta);
    };

    auto PixelToClipX = [ViewportWidth](float X) -> float
    {
        return (X / ViewportWidth) * 2.0f - 1.0f;
    };

    auto PixelToClipY = [ViewportHeight](float Y) -> float
    {
        return 1.0f - (Y / ViewportHeight) * 2.0f;
    };

    const uint32 BaseIndex = static_cast<uint32>(Buffer.Indices.size());

    float CursorY = BaseY;
    for (const TArray<uint32>& Line : Lines)
    {
        const float LineWidth = GetTextLineWidth(Line, CharW, LetterSpacing);
        float CursorX = LocalLeft;
        if (Proxy.TextHAlign == EUITextHAlign::Center)
        {
            CursorX += (LayoutSize.X - LineWidth) * 0.5f;
        }
        else if (Proxy.TextHAlign == EUITextHAlign::Right)
        {
            CursorX += LayoutSize.X - LineWidth;
        }

        for (uint32 Codepoint : Line)
        {
            const float GlyphAdvance = GetTextGlyphAdvance(Codepoint, CharW);
            if (IsTextWhitespace(Codepoint))
            {
                CursorX += GlyphAdvance + LetterSpacing;
                continue;
            }

            FVector2 UVMin;
            FVector2 UVMax;
            if (GetCharUV(Codepoint, UVMin, UVMax))
            {
                const FVector2 TopLeft = TransformLocal(CursorX, CursorY);
                const FVector2 TopRight = TransformLocal(CursorX + CharW, CursorY);
                const FVector2 BottomLeft = TransformLocal(CursorX, CursorY + CharH);
                const FVector2 BottomRight = TransformLocal(CursorX + CharW, CursorY + CharH);

                const uint32 BaseVertex = static_cast<uint32>(Buffer.Vertices.size());
                Buffer.Vertices.push_back({ FVector(PixelToClipX(TopLeft.X), PixelToClipY(TopLeft.Y), 0.0f), FVector2(UVMin.X, UVMin.Y), Proxy.TintColor });
                Buffer.Vertices.push_back({ FVector(PixelToClipX(TopRight.X), PixelToClipY(TopRight.Y), 0.0f), FVector2(UVMax.X, UVMin.Y), Proxy.TintColor });
                Buffer.Vertices.push_back({ FVector(PixelToClipX(BottomLeft.X), PixelToClipY(BottomLeft.Y), 0.0f), FVector2(UVMin.X, UVMax.Y), Proxy.TintColor });
                Buffer.Vertices.push_back({ FVector(PixelToClipX(BottomRight.X), PixelToClipY(BottomRight.Y), 0.0f), FVector2(UVMax.X, UVMax.Y), Proxy.TintColor });

                Buffer.Indices.push_back(BaseVertex);
                Buffer.Indices.push_back(BaseVertex + 1);
                Buffer.Indices.push_back(BaseVertex + 2);
                Buffer.Indices.push_back(BaseVertex + 1);
                Buffer.Indices.push_back(BaseVertex + 3);
                Buffer.Indices.push_back(BaseVertex + 2);
            }

            CursorX += GlyphAdvance + LetterSpacing;
        }

        CursorY += CharH + LineSpacing;
    }

    Range.FirstIndex = BaseIndex;
    Range.IndexCount = static_cast<uint32>(Buffer.Indices.size()) - BaseIndex;
    return Range;
}

FUIBatchRange FUIBatch::AddWorldText(const FUIProxy& Proxy, const FFontResource* Font, const FVector& CameraRight, const FVector& CameraUp)
{
    FUIBatchRange Range = {};
    if (Proxy.Text.empty() || !Font || !Font->IsLoaded())
    {
        return Range;
    }

    EnsureCharInfoMap(Font);

    const FVector2 LayoutSize = Proxy.GetWorldLayoutSize();
    if (LayoutSize.X == 0.0f || LayoutSize.Y == 0.0f)
    {
        return Range;
    }

    const float CharW = 0.23f * std::max(Proxy.FontSize, 0.01f);
    const float CharH = 0.23f * std::max(Proxy.FontSize, 0.01f);
    const float LetterSpacing = Proxy.TextLetterSpacing;
    const float LineSpacing = Proxy.TextLineSpacing;

    TArray<TArray<uint32>> Lines = SplitTextLines(Proxy.Text);
    if (Lines.empty())
    {
        return Range;
    }

    const float BlockHeight =
        static_cast<float>(Lines.size()) * CharH +
        static_cast<float>(Lines.size() > 0 ? Lines.size() - 1 : 0) * LineSpacing;

    const float LocalLeft = -Proxy.Pivot.X * LayoutSize.X;
    const float LocalTop = (1.0f - Proxy.Pivot.Y) * LayoutSize.Y;

    float BaseZ = LocalTop;
    if (Proxy.TextVAlign == EUITextVAlign::Center)
    {
        BaseZ -= (LayoutSize.Y - BlockHeight) * 0.5f;
    }
    else if (Proxy.TextVAlign == EUITextVAlign::Bottom)
    {
        BaseZ -= LayoutSize.Y - BlockHeight;
    }

    const float Radians = Proxy.RotationDegrees * 3.14159265358979323846f / 180.0f;
    const float CosTheta = std::cos(Radians);
    const float SinTheta = std::sin(Radians);

    auto TransformLocal = [&](float Y, float Z) -> FVector
    {
        const float RotY = Y * CosTheta - Z * SinTheta;
        const float RotZ = Y * SinTheta + Z * CosTheta;
        return TransformWorldUILocal(Proxy, RotY, RotZ, CameraRight, CameraUp);
    };

    const uint32 BaseIndex = static_cast<uint32>(Buffer.Indices.size());

    float CursorZ = BaseZ;
    for (const TArray<uint32>& Line : Lines)
    {
        const float LineWidth = GetTextLineWidth(Line, CharW, LetterSpacing);
        float CursorY = LocalLeft;
        if (Proxy.TextHAlign == EUITextHAlign::Center)
        {
            CursorY += (LayoutSize.X - LineWidth) * 0.5f;
        }
        else if (Proxy.TextHAlign == EUITextHAlign::Right)
        {
            CursorY += LayoutSize.X - LineWidth;
        }

        for (uint32 Codepoint : Line)
        {
            const float GlyphAdvance = GetTextGlyphAdvance(Codepoint, CharW);
            if (IsTextWhitespace(Codepoint))
            {
                CursorY += GlyphAdvance + LetterSpacing;
                continue;
            }

            FVector2 UVMin;
            FVector2 UVMax;
            if (GetCharUV(Codepoint, UVMin, UVMax))
            {
                const FVector TopLeft = TransformLocal(CursorY, CursorZ);
                const FVector TopRight = TransformLocal(CursorY + CharW, CursorZ);
                const FVector BottomLeft = TransformLocal(CursorY, CursorZ - CharH);
                const FVector BottomRight = TransformLocal(CursorY + CharW, CursorZ - CharH);

                const uint32 BaseVertex = static_cast<uint32>(Buffer.Vertices.size());
                Buffer.Vertices.push_back({ TopLeft, FVector2(UVMin.X, UVMin.Y), Proxy.TintColor });
                Buffer.Vertices.push_back({ TopRight, FVector2(UVMax.X, UVMin.Y), Proxy.TintColor });
                Buffer.Vertices.push_back({ BottomLeft, FVector2(UVMin.X, UVMax.Y), Proxy.TintColor });
                Buffer.Vertices.push_back({ BottomRight, FVector2(UVMax.X, UVMax.Y), Proxy.TintColor });

                Buffer.Indices.push_back(BaseVertex);
                Buffer.Indices.push_back(BaseVertex + 1);
                Buffer.Indices.push_back(BaseVertex + 2);
                Buffer.Indices.push_back(BaseVertex + 1);
                Buffer.Indices.push_back(BaseVertex + 3);
                Buffer.Indices.push_back(BaseVertex + 2);
            }

            CursorY += GlyphAdvance + LetterSpacing;
        }

        CursorZ -= CharH + LineSpacing;
    }

    Range.FirstIndex = BaseIndex;
    Range.IndexCount = static_cast<uint32>(Buffer.Indices.size()) - BaseIndex;
    return Range;
}

bool FUIBatch::UploadBuffers(ID3D11DeviceContext* Context)
{
    return Buffer.Upload(Context);
}

void FUIBatch::UpdateParams(ID3D11DeviceContext* Context)
{
    if (!Context)
    {
        return;
    }

    FUIParamsCBData SolidParams = {};
    SolidParams.Flags = FVector4(0.0f, 0.0f, 0.0f, 0.0f);
    SolidParamsCB.Update(Context, &SolidParams, sizeof(SolidParams));

    FUIParamsCBData TextureParams = {};
    TextureParams.Flags = FVector4(1.0f, 0.0f, 0.0f, 0.0f);
    TextureParamsCB.Update(Context, &TextureParams, sizeof(TextureParams));

    FUIParamsCBData FontParams = {};
    FontParams.Flags = FVector4(1.0f, 1.0f, 0.0f, 0.0f);
    FontParamsCB.Update(Context, &FontParams, sizeof(FontParams));

    FUIParamsCBData WorldSolidParams = {};
    WorldSolidParams.Flags = FVector4(0.0f, 0.0f, 1.0f, 0.0f);
    WorldSolidParamsCB.Update(Context, &WorldSolidParams, sizeof(WorldSolidParams));

    FUIParamsCBData WorldTextureParams = {};
    WorldTextureParams.Flags = FVector4(1.0f, 0.0f, 1.0f, 0.0f);
    WorldTextureParamsCB.Update(Context, &WorldTextureParams, sizeof(WorldTextureParams));

    FUIParamsCBData WorldFontParams = {};
    WorldFontParams.Flags = FVector4(1.0f, 1.0f, 1.0f, 0.0f);
    WorldFontParamsCB.Update(Context, &WorldFontParams, sizeof(WorldFontParams));
}

void FUIBatch::BuildCharInfoMap(uint32 Columns, uint32 Rows)
{
    CharInfoMap.clear();
    CachedFontColumns = Columns;
    CachedFontRows = Rows;

    if (Columns == 0 || Rows == 0)
    {
        return;
    }

    const float CellW = 1.0f / static_cast<float>(Columns);
    const float CellH = 1.0f / static_cast<float>(Rows);

    auto AddChar = [&](uint32 Codepoint, uint32 Slot)
    {
        const uint32 Col = Slot % Columns;
        const uint32 Row = Slot / Columns;
        if (Row >= Rows)
        {
            return;
        }
        CharInfoMap[Codepoint] = { Col * CellW, Row * CellH, CellW, CellH };
    };

    for (uint32 Codepoint = 32; Codepoint <= 126; ++Codepoint)
    {
        AddChar(Codepoint, Codepoint - 32);
    }

    uint32 Slot = 127;
    for (uint32 Codepoint = 0xAC00; Codepoint <= 0xD7A3; ++Codepoint, ++Slot)
    {
        AddChar(Codepoint, Slot - 32);
    }
}

void FUIBatch::EnsureCharInfoMap(const FFontResource* Font)
{
    if (!Font || Font->Columns == 0 || Font->Rows == 0)
    {
        return;
    }

    if (CachedFontColumns == Font->Columns && CachedFontRows == Font->Rows)
    {
        return;
    }

    BuildCharInfoMap(Font->Columns, Font->Rows);
}

bool FUIBatch::GetCharUV(uint32 Codepoint, FVector2& OutUVMin, FVector2& OutUVMax) const
{
    const auto It = CharInfoMap.find(Codepoint);
    if (It == CharInfoMap.end())
    {
        return false;
    }

    const FUICharacterInfo& Info = It->second;
    OutUVMin = FVector2(Info.U, Info.V);
    OutUVMax = FVector2(Info.U + Info.Width, Info.V + Info.Height);
    return true;
}
