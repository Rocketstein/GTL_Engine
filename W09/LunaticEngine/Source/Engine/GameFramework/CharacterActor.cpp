#include "CharacterActor.h"

#include "Component/BillboardComponent.h"
#include "Component/SceneComponent.h"
#include "Engine/Runtime/Engine.h"
#include "Resource/ResourceManager.h"
#include "Texture/Texture2D.h"

IMPLEMENT_CLASS(ACharacterActor, AActor)

ACharacterActor::ACharacterActor()
{
	bNeedsTick = true;
	bTickInEditor = true;
}

void ACharacterActor::InitDefaultComponents()
{
	RootSceneComponent = AddComponent<USceneComponent>();
	RootSceneComponent->SetCanDeleteFromDetails(false);
	SetRootComponent(RootSceneComponent);

	BillboardComponent = AddComponent<UBillboardComponent>();
	BillboardComponent->SetCanDeleteFromDetails(false);
	BillboardComponent->AttachToComponent(RootSceneComponent);
	BillboardComponent->SetAbsoluteScale(true);
	BillboardComponent->SetEditorOnlyComponent(true);
	BillboardComponent->SetHiddenInComponentTree(true);
	BillboardComponent->SetSpriteSize(1.0f, 1.0f);

	const FString IconPath = FResourceManager::Get().ResolvePath(FName("Editor.Icon.Character"));
	ID3D11Device* Device = GEngine ? GEngine->GetRenderer().GetFD3DDevice().GetDevice() : nullptr;
	if (UTexture2D* Texture = UTexture2D::LoadFromFile(IconPath, Device))
	{
		BillboardComponent->SetTexture(Texture);
	}
}
