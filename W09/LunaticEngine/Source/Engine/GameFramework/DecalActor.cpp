#include "DecalActor.h"
#include "Component/DecalComponent.h"
#include "Component/BillboardComponent.h"
#include "Component/TextRenderComponent.h"
#include "Materials/MaterialManager.h"
#include "Resource/ResourceManager.h"

IMPLEMENT_CLASS(ADecalActor, AActor)

ADecalActor::ADecalActor()
	: DecalComponent(nullptr)
{
	bNeedsTick = true;
	bTickInEditor = true;
}

void ADecalActor::InitDefaultComponents()
{
	DecalComponent = AddComponent<UDecalComponent>();
	DecalComponent->SetCanDeleteFromDetails(false);
	const FString DefaultDecalMaterialPath = FResourceManager::Get().ResolvePath(FName("Default.Material.Decal"));
	auto Material = FMaterialManager::Get().GetOrCreateMaterial(DefaultDecalMaterialPath);
	DecalComponent->SetMaterial(Material);
	SetRootComponent(DecalComponent);

	BillboardComponent = DecalComponent->EnsureEditorBillboard();
	if (BillboardComponent)
	{
		BillboardComponent->SetCanDeleteFromDetails(false);
	}
	
	// UUID 텍스트 표시
	TextRenderComponent = AddComponent<UTextRenderComponent>();
	TextRenderComponent->SetCanDeleteFromDetails(false);
	TextRenderComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 1.3f));
	TextRenderComponent->SetText("UUID : " + TextRenderComponent->GetOwnerUUIDToString());
	TextRenderComponent->AttachToComponent(DecalComponent);
	TextRenderComponent->SetFont(FName("Default"));
}
