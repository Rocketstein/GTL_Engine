#include "UI/TextUIComponent.h"

#include "Object/ObjectFactory.h"
#include "Resource/FontManager.h"
#include "Serialization/Archive.h"

#include <algorithm>
#include <cstring>

IMPLEMENT_CLASS(UTextUIComponent, UUIComponent)

namespace
{
constexpr FEnumPropertyOption GUITextHAlignOptions[] = {
    { "Left", static_cast<int32_t>(EUITextHAlign::Left) },
    { "Center", static_cast<int32_t>(EUITextHAlign::Center) },
    { "Right", static_cast<int32_t>(EUITextHAlign::Right) },
};

constexpr FEnumPropertyMeta GUITextHAlignMeta = {
    "EUITextHAlign",
    GUITextHAlignOptions,
    static_cast<int32_t>(sizeof(GUITextHAlignOptions) / sizeof(GUITextHAlignOptions[0])),
};

constexpr FEnumPropertyOption GUITextVAlignOptions[] = {
    { "Top", static_cast<int32_t>(EUITextVAlign::Top) },
    { "Center", static_cast<int32_t>(EUITextVAlign::Center) },
    { "Bottom", static_cast<int32_t>(EUITextVAlign::Bottom) },
};

constexpr FEnumPropertyMeta GUITextVAlignMeta = {
    "EUITextVAlign",
    GUITextVAlignOptions,
    static_cast<int32_t>(sizeof(GUITextVAlignOptions) / sizeof(GUITextVAlignOptions[0])),
};
} // namespace

UTextUIComponent::UTextUIComponent()
{
    SetSizeDelta(FVector2(200.0f, 32.0f));
    SetPivot(FVector2(0.0f, 0.0f));
    ReacquireFont();
}

void UTextUIComponent::Serialize(FArchive& Ar)
{
    UUIComponent::Serialize(Ar);

    Ar << Text;
    Ar << FontName;
    Ar << FontSize;
    Ar << LetterSpacing;
    Ar << LineSpacing;
    Ar << HAlign;
    Ar << VAlign;

    if (Ar.IsLoading())
    {
        FontSize = std::max(FontSize, 0.01f);
        ReacquireFont();
    }
}

void UTextUIComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    UUIComponent::GetEditableProperties(OutProps);
    OutProps.push_back({ "Text", EPropertyType::String, &Text });
    OutProps.push_back({ "Font", EPropertyType::Name, &FontName });
    OutProps.push_back({ "Font Size", EPropertyType::Float, &FontSize, 0.01f, 100.0f, 0.1f });
    OutProps.push_back({ "Text Letter Spacing", EPropertyType::Float, &LetterSpacing, -64.0f, 256.0f, 0.25f });
    OutProps.push_back({ "Text Line Spacing", EPropertyType::Float, &LineSpacing, -64.0f, 256.0f, 0.25f });
    OutProps.push_back({ "Text Horizontal Align", EPropertyType::Enum, &HAlign, 0.0f, 0.0f, 0.1f, &GUITextHAlignMeta });
    OutProps.push_back({ "Text Vertical Align", EPropertyType::Enum, &VAlign, 0.0f, 0.0f, 0.1f, &GUITextVAlignMeta });
}

void UTextUIComponent::PostEditProperty(const char* PropertyName)
{
    UUIComponent::PostEditProperty(PropertyName);

    if (std::strcmp(PropertyName, "Font") == 0)
    {
        ReacquireFont();
        MarkUIStyleDirty();
        return;
    }

    if (std::strcmp(PropertyName, "Font Size") == 0)
    {
        FontSize = std::max(FontSize, 0.01f);
        MarkUIStyleDirty();
        return;
    }

    if (std::strcmp(PropertyName, "Text") == 0 ||
        std::strcmp(PropertyName, "Text Letter Spacing") == 0 ||
        std::strcmp(PropertyName, "Text Line Spacing") == 0 ||
        std::strcmp(PropertyName, "Text Horizontal Align") == 0 ||
        std::strcmp(PropertyName, "Text Vertical Align") == 0)
    {
        MarkUIStyleDirty();
    }
}

void UTextUIComponent::SetText(const FString& InText)
{
    if (Text == InText)
    {
        return;
    }

    Text = InText;
    MarkUIStyleDirty();
}

void UTextUIComponent::SetFont(const FName& InFontName)
{
    if (FontName == InFontName && CachedFont && CachedFont->IsLoaded())
    {
        return;
    }

    FontName = InFontName;
    ReacquireFont();
    MarkUIStyleDirty();
}

void UTextUIComponent::SetFontSize(float InFontSize)
{
    InFontSize = std::max(InFontSize, 0.01f);
    if (FontSize == InFontSize)
    {
        return;
    }

    FontSize = InFontSize;
    MarkUIStyleDirty();
}

void UTextUIComponent::SetLetterSpacing(float InLetterSpacing)
{
    if (LetterSpacing == InLetterSpacing)
    {
        return;
    }

    LetterSpacing = InLetterSpacing;
    MarkUIStyleDirty();
}

void UTextUIComponent::SetLineSpacing(float InLineSpacing)
{
    if (LineSpacing == InLineSpacing)
    {
        return;
    }

    LineSpacing = InLineSpacing;
    MarkUIStyleDirty();
}

void UTextUIComponent::SetHorizontalAlignment(EUITextHAlign InAlign)
{
    if (HAlign == InAlign)
    {
        return;
    }

    HAlign = InAlign;
    MarkUIStyleDirty();
}

void UTextUIComponent::SetVerticalAlignment(EUITextVAlign InAlign)
{
    if (VAlign == InAlign)
    {
        return;
    }

    VAlign = InAlign;
    MarkUIStyleDirty();
}

bool UTextUIComponent::ReacquireFont()
{
    CachedFont = FFontManager::Get().FindFont(FontName);
    if (CachedFont && CachedFont->IsLoaded())
    {
        return true;
    }

    FontName = FName("Default");
    CachedFont = FFontManager::Get().FindFont(FontName);
    return CachedFont && CachedFont->IsLoaded();
}
