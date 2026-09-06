#include "EmptyActor.h"
#include "Component/Primitive/BillboardComponent.h"
#include "Materials/MaterialManager.h"

namespace
{
	constexpr const char* EmptyActorMaterialPath = "Content/Material/Editor/EmptyActor.uasset";
}

void AEmptyActor::InitDefaultComponents()
{
	BillboardComponent = AddComponent<UBillboardComponent>();
	SetRootComponent(BillboardComponent);
	RestoreBillboardComponent();
}

void AEmptyActor::PostDuplicate()
{
	AActor::PostDuplicate();
	RestoreBillboardComponent();
}

void AEmptyActor::OnPostLoad(FArchive& Ar)
{
	Super::OnPostLoad(Ar);
	RestoreBillboardComponent();
}

void AEmptyActor::RestoreBillboardComponent()
{
	BillboardComponent = Cast<UBillboardComponent>(GetRootComponent());
	if (!BillboardComponent)
	{
		if (!GetRootComponent())
		{
			BillboardComponent = AddComponent<UBillboardComponent>();
			SetRootComponent(BillboardComponent);
		}
	}

	if (!BillboardComponent)
	{
		return;
	}

	BillboardComponent->SetAbsoluteScale(true);
	BillboardComponent->SetEditorOnlyComponent(true);
	BillboardComponent->SetHiddenInComponentTree(true);

	if (!BillboardComponent->GetMaterial())
	{
		auto Material = FMaterialManager::Get().GetOrCreateMaterial(EmptyActorMaterialPath);
		BillboardComponent->SetMaterial(Material);
	}
}
