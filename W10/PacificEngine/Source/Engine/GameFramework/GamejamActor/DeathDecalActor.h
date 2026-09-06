#pragma once
#include "GameFramework/AActor.h"
#include "Platform/Paths.h"

class UDecalComponent;

class ADeathDecalActor : public AActor
{
public:
    DECLARE_CLASS(ADeathDecalActor, AActor)
    ADeathDecalActor() = default;
    ~ADeathDecalActor() override;
    void InitDefaultComponents() override;
    void Activate() override;

private:
    void HandleFadeOutEnded();

    UDecalComponent* DecalComponent = nullptr;
    FString AssetPath = FPaths::ContentRelativePath("Materials/greenblood.json");
};
