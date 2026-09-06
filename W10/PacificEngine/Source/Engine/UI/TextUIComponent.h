#pragma once

#include "Core/ResourceTypes.h"
#include "Object/FName.h"
#include "UI/UIComponent.h"

class UTextUIComponent : public UUIComponent
{
public:
    DECLARE_CLASS(UTextUIComponent, UUIComponent)

    UTextUIComponent();

    void Serialize(FArchive& Ar) override;
    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
    void PostEditProperty(const char* PropertyName) override;

    EUIElementType GetUIElementType() const override { return EUIElementType::Text; }

    void SetText(const FString& InText);
    const FString& GetText() const { return Text; }

    void SetFont(const FName& InFontName);
    const FName& GetFontName() const { return FontName; }
    const FFontResource* GetFont() const { return CachedFont; }

    void SetFontSize(float InFontSize);
    float GetFontSize() const { return FontSize; }

    void SetLetterSpacing(float InLetterSpacing);
    float GetLetterSpacing() const { return LetterSpacing; }

    void SetLineSpacing(float InLineSpacing);
    float GetLineSpacing() const { return LineSpacing; }

    void SetHorizontalAlignment(EUITextHAlign InAlign);
    EUITextHAlign GetHorizontalAlignment() const { return HAlign; }

    void SetVerticalAlignment(EUITextVAlign InAlign);
    EUITextVAlign GetVerticalAlignment() const { return VAlign; }

private:
    bool ReacquireFont();

private:
    FString Text = "Text";
    FName FontName = FName("Default");
    FFontResource* CachedFont = nullptr;

    float FontSize = 1.0f;
    float LetterSpacing = 0.0f;
    float LineSpacing = 0.0f;
    EUITextHAlign HAlign = EUITextHAlign::Left;
    EUITextVAlign VAlign = EUITextVAlign::Top;
};
