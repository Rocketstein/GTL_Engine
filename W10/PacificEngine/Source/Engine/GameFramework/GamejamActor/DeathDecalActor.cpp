#include "DeathDecalActor.h"
#include "Component/DecalComponent.h"
#include "Materials/MaterialManager.h"

IMPLEMENT_CLASS(ADeathDecalActor, AActor)

ADeathDecalActor::~ADeathDecalActor()
{
    if (DecalComponent)
    {
        DecalComponent->OnEndedFadeOut.RemoveDynamic(this);
    }
}

void ADeathDecalActor::InitDefaultComponents()
{
    DecalComponent = AddComponent<UDecalComponent>();
    auto DecalMaterial = FMaterialManager::Get().GetOrCreateMaterial(AssetPath);
    DecalComponent->SetMaterial(0, DecalMaterial);
    DecalComponent->SetFade(0.0f, 0.08f, 2.5f, 1.0f);
    DecalComponent->SetRelativeRotation(FRotator(-90.0f, 0, 0));
    DecalComponent->SetRelativeScale(FVector(2.0f, 2.0f, 2.0f));
    SetRootComponent(DecalComponent);
    DecalComponent->OnEndedFadeOut.AddDynamic(this, &ADeathDecalActor::HandleFadeOutEnded);
}

void ADeathDecalActor::Activate()
{
    Super::Activate();
    if (DecalComponent)
    {
        DecalComponent->ResetFade();
    }
}

void ADeathDecalActor::HandleFadeOutEnded()
{
    RequestReturnToPool();
}
