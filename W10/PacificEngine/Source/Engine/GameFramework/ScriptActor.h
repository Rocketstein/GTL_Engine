#pragma once

#include "GameFramework/AActor.h"

class USceneComponent;
class UScriptComponent;

class AScriptActor : public AActor
{
public:
    DECLARE_CLASS(AScriptActor, AActor)

    AScriptActor() = default;

    void InitDefaultComponents() override;
    void InitDefaultComponents(const FString& InScriptPath);

    void SetScriptPath(const FString& InScriptPath);
    UScriptComponent* GetScriptComponent() const;

private:
    USceneComponent* SceneRootComponent = nullptr;
    UScriptComponent* ScriptComponent = nullptr;
};
