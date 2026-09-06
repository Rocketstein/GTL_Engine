#include "UI/TextureUIComponent.h"

#include "Object/ObjectFactory.h"
#include "Serialization/Archive.h"

#include <algorithm>
#include <cstring>

IMPLEMENT_CLASS(UTextureUIComponent, UUIComponent)

void UTextureUIComponent::Serialize(FArchive& Ar)
{
    UUIComponent::Serialize(Ar);
    Ar << TexturePath;
    Ar << SubUVRect;
    Ar << SpriteColumns;
    Ar << SpriteRows;
    Ar << SpriteFrameIndex;
    Ar << SpriteFPS;
    Ar << bSpriteAnimation;
    Ar << bSpriteLoop;
    Ar << bSpritePlaying;
    Ar << bSpriteFinished;
    Ar << SpriteTimeAccumulator;
}

void UTextureUIComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    UUIComponent::GetEditableProperties(OutProps);
    OutProps.push_back({ "Texture Path", EPropertyType::String, &TexturePath });
    OutProps.push_back({ "SubUV Rect", EPropertyType::Vec4, &SubUVRect, 0.0f, 1.0f, 0.01f });
    OutProps.push_back({ "Sprite Columns", EPropertyType::Int, &SpriteColumns, 1.0f, 64.0f, 1.0f });
    OutProps.push_back({ "Sprite Rows", EPropertyType::Int, &SpriteRows, 1.0f, 64.0f, 1.0f });
    OutProps.push_back({ "Sprite Frame", EPropertyType::Int, &SpriteFrameIndex, 0.0f, 4096.0f, 1.0f });
    OutProps.push_back({ "Sprite FPS", EPropertyType::Float, &SpriteFPS, 0.0f, 120.0f, 1.0f });
    OutProps.push_back({ "Sprite Animation", EPropertyType::Bool, &bSpriteAnimation });
    OutProps.push_back({ "Sprite Loop", EPropertyType::Bool, &bSpriteLoop });
    OutProps.push_back({ "Sprite Playing", EPropertyType::Bool, &bSpritePlaying });
}

void UTextureUIComponent::PostEditProperty(const char* PropertyName)
{
    UUIComponent::PostEditProperty(PropertyName);

    if (std::strcmp(PropertyName, "SubUV Rect") == 0)
    {
        MarkUILayoutDirty();
        return;
    }

    if (std::strcmp(PropertyName, "Sprite Columns") == 0 ||
        std::strcmp(PropertyName, "Sprite Rows") == 0 ||
        std::strcmp(PropertyName, "Sprite Frame") == 0 ||
        std::strcmp(PropertyName, "Sprite Animation") == 0)
    {
        ClampSpriteSheetState();
        ApplySpriteFrameToSubUVRect();
        MarkUILayoutDirty();
        return;
    }

    if (std::strcmp(PropertyName, "Sprite FPS") == 0)
    {
        SpriteFPS = std::max(SpriteFPS, 0.0f);
        return;
    }

    if (std::strcmp(PropertyName, "Sprite Playing") == 0)
    {
        if (bSpritePlaying)
        {
            bSpriteFinished = false;
        }
        return;
    }

    if (std::strcmp(PropertyName, "Texture Path") == 0)
    {
        MarkUIStyleDirty();
    }
}

void UTextureUIComponent::SetTexturePath(const FString& InTexturePath)
{
    if (TexturePath == InTexturePath)
    {
        return;
    }

    TexturePath = InTexturePath;
    MarkUIStyleDirty();
}

void UTextureUIComponent::SetSubUVRect(const FVector4& InRect)
{
    if (SubUVRect.X == InRect.X &&
        SubUVRect.Y == InRect.Y &&
        SubUVRect.Z == InRect.Z &&
        SubUVRect.W == InRect.W)
    {
        return;
    }

    SubUVRect = InRect;
    MarkUILayoutDirty();
}

void UTextureUIComponent::SetSubUVFrame(int32 Columns, int32 Rows, int32 Index)
{
    SpriteColumns = Columns;
    SpriteRows = Rows;
    SpriteFrameIndex = Index;
    ClampSpriteSheetState();
    ApplySpriteFrameToSubUVRect();
    MarkUILayoutDirty();
}

void UTextureUIComponent::ResetSubUVRect()
{
    SetSubUVRect(FVector4(0.0f, 0.0f, 1.0f, 1.0f));
}

void UTextureUIComponent::SetSpriteSheet(int32 Columns, int32 Rows)
{
    if (SpriteColumns == Columns && SpriteRows == Rows)
    {
        return;
    }

    SpriteColumns = Columns;
    SpriteRows = Rows;
    ClampSpriteSheetState();
    ApplySpriteFrameToSubUVRect();
    MarkUILayoutDirty();
}

void UTextureUIComponent::SetSpriteFrame(int32 Index)
{
    if (SpriteFrameIndex == Index)
    {
        return;
    }

    SpriteFrameIndex = Index;
    ClampSpriteSheetState();
    ApplySpriteFrameToSubUVRect();
    MarkUILayoutDirty();
}

void UTextureUIComponent::SetSpriteFPS(float InFPS)
{
    SpriteFPS = std::max(InFPS, 0.0f);
}

void UTextureUIComponent::SetSpriteLoop(bool bInLoop)
{
    bSpriteLoop = bInLoop;
}

void UTextureUIComponent::SetSpriteAnimationEnabled(bool bEnabled)
{
    if (bSpriteAnimation == bEnabled)
    {
        return;
    }

    bSpriteAnimation = bEnabled;
    bSpriteFinished = false;
    SpriteTimeAccumulator = 0.0f;
    ClampSpriteSheetState();
    ApplySpriteFrameToSubUVRect();
    MarkUILayoutDirty();
}

void UTextureUIComponent::PlaySpriteAnimation(bool bRestart)
{
    bSpriteAnimation = true;
    bSpritePlaying = true;
    bSpriteFinished = false;
    SpriteTimeAccumulator = 0.0f;

    if (bRestart)
    {
        SpriteFrameIndex = 0;
    }

    ClampSpriteSheetState();
    ApplySpriteFrameToSubUVRect();
    MarkUILayoutDirty();
}

void UTextureUIComponent::PauseSpriteAnimation()
{
    bSpritePlaying = false;
}

void UTextureUIComponent::StopSpriteAnimation()
{
    bSpritePlaying = false;
    bSpriteFinished = false;
    SpriteTimeAccumulator = 0.0f;
    SpriteFrameIndex = 0;
    ApplySpriteFrameToSubUVRect();
    MarkUILayoutDirty();
}

void UTextureUIComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
    (void)TickType;
    (void)ThisTickFunction;

    if (!bSpriteAnimation || !bSpritePlaying || bSpriteFinished || SpriteFPS <= 0.0f)
    {
        return;
    }

    const int32 FrameCount = GetSpriteFrameCount();
    if (FrameCount <= 1)
    {
        return;
    }

    SpriteTimeAccumulator += DeltaTime;
    const float FrameDuration = 1.0f / SpriteFPS;
    bool bFrameChanged = false;

    while (SpriteTimeAccumulator >= FrameDuration)
    {
        SpriteTimeAccumulator -= FrameDuration;

        if (SpriteFrameIndex < FrameCount - 1)
        {
            ++SpriteFrameIndex;
            bFrameChanged = true;
            continue;
        }

        if (bSpriteLoop)
        {
            SpriteFrameIndex = 0;
            bFrameChanged = true;
        }
        else
        {
            bSpritePlaying = false;
            bSpriteFinished = true;
            SpriteTimeAccumulator = 0.0f;
            break;
        }
    }

    if (bFrameChanged)
    {
        ApplySpriteFrameToSubUVRect();
        MarkUILayoutDirty();
    }
}

int32 UTextureUIComponent::GetSpriteFrameCount() const
{
    return std::max(SpriteColumns, 1) * std::max(SpriteRows, 1);
}

void UTextureUIComponent::ClampSpriteSheetState()
{
    SpriteColumns = std::max(SpriteColumns, 1);
    SpriteRows = std::max(SpriteRows, 1);

    const int32 FrameCount = GetSpriteFrameCount();
    SpriteFrameIndex = std::max(0, std::min(SpriteFrameIndex, FrameCount - 1));
    SpriteFPS = std::max(SpriteFPS, 0.0f);
}

void UTextureUIComponent::ApplySpriteFrameToSubUVRect()
{
    ClampSpriteSheetState();

    const float CellWidth = 1.0f / static_cast<float>(SpriteColumns);
    const float CellHeight = 1.0f / static_cast<float>(SpriteRows);
    const int32 Column = SpriteFrameIndex % SpriteColumns;
    const int32 Row = SpriteFrameIndex / SpriteColumns;

    SubUVRect = FVector4(
        static_cast<float>(Column) * CellWidth,
        static_cast<float>(Row) * CellHeight,
        CellWidth,
        CellHeight);
}
