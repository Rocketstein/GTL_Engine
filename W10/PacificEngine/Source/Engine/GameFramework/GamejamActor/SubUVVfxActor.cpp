#include "SubUVVfxActor.h"
#include "Component/SubUVComponent.h"

IMPLEMENT_CLASS(ASubUVVfxActor, AActor)

void ASubUVVfxActor::InitDefaultComponents()
{
    SubUV = AddComponent<USubUVComponent>();
    SubUV->SetFName(FName("SubUV"));
    SetRootComponent(SubUV);
    SubUV->SetSpriteVfx("Explode");
    SubUV->SetFrameRate(240.0f);
    SubUV->SetLoop(false);
}

void ASubUVVfxActor::SetVfxPreset(const FString& PresetName)
{
    if (SubUV)
    {
        SubUV->SetSpriteVfx(FName(PresetName));
        SubUV->Play();
    }
}
