#pragma once

#include "UI/UIComponent.h"

class UTextureUIComponent : public UUIComponent
{
public:
    DECLARE_CLASS(UTextureUIComponent, UUIComponent)

    UTextureUIComponent() = default;

    void Serialize(FArchive& Ar) override;
    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
    void PostEditProperty(const char* PropertyName) override;

    EUIElementType GetUIElementType() const override { return EUIElementType::Image; }

    void SetTexturePath(const FString& InTexturePath);
    void SetTexture(const FString& InTexturePath) { SetTexturePath(InTexturePath); }
    const FString& GetTexturePath() const { return TexturePath; }

    void SetSubUVRect(const FVector4& InRect);
    void SetSubUVFrame(int32 Columns, int32 Rows, int32 Index);
    void ResetSubUVRect();
    const FVector4& GetSubUVRect() const { return SubUVRect; }

    void SetSpriteSheet(int32 Columns, int32 Rows);
    void SetSpriteFrame(int32 Index);
    void SetSpriteFPS(float InFPS);
    void SetSpriteLoop(bool bInLoop);
    void SetSpriteAnimationEnabled(bool bEnabled);
    void PlaySpriteAnimation(bool bRestart = true);
    void PauseSpriteAnimation();
    void StopSpriteAnimation();

    int32 GetSpriteColumns() const { return SpriteColumns; }
    int32 GetSpriteRows() const { return SpriteRows; }
    int32 GetSpriteFrame() const { return SpriteFrameIndex; }
    float GetSpriteFPS() const { return SpriteFPS; }
    bool IsSpriteLoop() const { return bSpriteLoop; }
    bool IsSpriteAnimationEnabled() const { return bSpriteAnimation; }
    bool IsSpritePlaying() const { return bSpritePlaying; }
    bool IsSpriteAnimationFinished() const { return bSpriteFinished; }

protected:
    void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;

private:
    int32 GetSpriteFrameCount() const;
    void ClampSpriteSheetState();
    void ApplySpriteFrameToSubUVRect();

private:
    FString TexturePath;
    FVector4 SubUVRect = { 0.0f, 0.0f, 1.0f, 1.0f };

    int32 SpriteColumns = 1;
    int32 SpriteRows = 1;
    int32 SpriteFrameIndex = 0;
    float SpriteFPS = 12.0f;
    bool bSpriteAnimation = false;
    bool bSpriteLoop = true;
    bool bSpritePlaying = true;
    bool bSpriteFinished = false;
    float SpriteTimeAccumulator = 0.0f;
};
