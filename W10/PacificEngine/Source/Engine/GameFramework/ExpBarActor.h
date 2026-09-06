#pragma once

#include "GameFramework/AActor.h"

class UScriptComponent;
class UTextureUIComponent;

class AExpBarActor : public AActor
{
public:
    DECLARE_CLASS(AExpBarActor, AActor)

    AExpBarActor() = default;

    void BeginPlay() override;
    void InitDefaultComponents() override;

    UTextureUIComponent* GetBackComponent() const { return BackComponent; }
    UTextureUIComponent* GetFillComponent() const { return FillComponent; }

private:
    void EnsureExpBarComponents();
    UTextureUIComponent* GetOrCreateTextureUIComponent(const FString& Name);
    UScriptComponent* GetOrCreateScriptComponent();
    void ConfigureBackComponent(UTextureUIComponent* Component);
    void ConfigureFillComponent(UTextureUIComponent* Component);

private:
    UTextureUIComponent* BackComponent = nullptr;
    UTextureUIComponent* FillComponent = nullptr;
    UScriptComponent* ScriptComponent = nullptr;
};
