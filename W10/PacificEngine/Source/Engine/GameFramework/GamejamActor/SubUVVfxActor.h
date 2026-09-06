#pragma once
#include "GameFramework/AActor.h"

class ASubUVVfxActor : public AActor
{
public:
    DECLARE_CLASS(ASubUVVfxActor, AActor);
    void InitDefaultComponents() override;

    void SetVfxPreset(const FString& PresetName);
    class USubUVComponent* GetSubUV() const { return SubUV; }

private:
    class USubUVComponent* SubUV = nullptr;
};
