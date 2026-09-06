#pragma once

#include "GameFramework/AActor.h"

class UTextureUIComponent;

class ATextureUIActor : public AActor
{
public:
    DECLARE_CLASS(ATextureUIActor, AActor)

    ATextureUIActor() = default;

    void InitDefaultComponents() override;

    UTextureUIComponent* GetTextureUIComponent() const { return TextureUIComponent; }
    void SetTexturePath(const FString& InTexturePath);

private:
    UTextureUIComponent* TextureUIComponent = nullptr;
};
