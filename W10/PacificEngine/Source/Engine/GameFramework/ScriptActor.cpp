#include "GameFramework/ScriptActor.h"

#include "Component/SceneComponent.h"
#include "Component/ScriptComponent.h"

IMPLEMENT_CLASS(AScriptActor, AActor)

void AScriptActor::InitDefaultComponents()
{
    SceneRootComponent = AddComponent<USceneComponent>();
    if (SceneRootComponent)
    {
        SceneRootComponent->SetFName(FName("Root"));
        SetRootComponent(SceneRootComponent);
    }

    ScriptComponent = AddComponent<UScriptComponent>();
    if (ScriptComponent)
    {
        ScriptComponent->SetFName(FName("Script"));
    }
}

void AScriptActor::InitDefaultComponents(const FString& InScriptPath)
{
    InitDefaultComponents();
    SetScriptPath(InScriptPath);
}

void AScriptActor::SetScriptPath(const FString& InScriptPath)
{
    if (UScriptComponent* Component = GetScriptComponent())
    {
        Component->SetScriptPath(InScriptPath);
    }
}

UScriptComponent* AScriptActor::GetScriptComponent() const
{
    if (ScriptComponent)
    {
        return ScriptComponent;
    }

    return FindComponentByClass<UScriptComponent>();
}
