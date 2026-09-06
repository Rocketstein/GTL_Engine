#include "ItemActorBase.h"

#include "Component/BillboardComponent.h"
#include "Component/ScriptComponent.h"
#include "Component/Shape/BoxComponent.h"
#include "Engine/Runtime/Engine.h"
#include "Serialization/Archive.h"
#include "Texture/Texture2D.h"

IMPLEMENT_CLASS(AItemActorBase, AActor)

AItemActorBase::AItemActorBase()
{
	// BoxComponent
	ItemTrigger = AddComponent<UBoxComponent>();
	ItemTrigger->SetCanDeleteFromDetails(false);
	ItemTrigger->SetBoxExtent(FVector(1.0f, 1.0f, 1.0f));
	ItemTrigger->SetCollisionEnabled(InteractionConfig.bStartsEnabled);
	ItemTrigger->SetGenerateOverlapEvents(true);
	SetRootComponent(ItemTrigger);

	// Billboard
	ItemImage = AddComponent<UBillboardComponent>();
	ItemImage->SetCollisionEnabled(false);
	ItemImage->SetGenerateOverlapEvents(false);
	ItemImage->AttachToComponent(GetRootComponent());

	// 기본 script 부착
	// item별 동작은 SetItemScript("Scripts/Game/Items/LogItem.lua")로 확장
	ItemScript = AddComponent<UScriptComponent>();
	ItemScript->SetScriptPath("Scripts/Game/Items/ItemBase.lua");
}

void AItemActorBase::BeginPlay()
{
	bPicked = false;

	// BeginPlay 직전에 다시 보정.
	// Details 창 수정, deserialize, spawn 이후 값 변경이 섞여도 최종 상태를 맞춘다.
	if (ItemTrigger)
	{
		ItemTrigger->SetCollisionEnabled(InteractionConfig.bStartsEnabled);
		ItemTrigger->SetGenerateOverlapEvents(InteractionConfig.bStartsEnabled);
	}

	if (ItemImage)
	{
		ItemImage->SetCollisionEnabled(false);
		ItemImage->SetGenerateOverlapEvents(false);

		if (GetRootComponent())
		{
			ItemImage->AttachToComponent(GetRootComponent());
		}
	}

	// Texture는 반드시 BeginPlay에서 적용.
	// 생성자에서 넣으면 renderer/resource 준비 시점 문제로 적용이 안 되는 경우가 있었음.
	if (ItemImage && !ItemTexturePath.empty() && ItemTexturePath != "None" && GEngine)
	{
		ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
		if (Device)
		{
			if (UTexture2D* Texture = UTexture2D::LoadFromFile(ItemTexturePath, Device))
			{
				ItemImage->SetTexture(Texture);
				// 생성자에서 만든 컴포넌트는 World 연결 전이라 렌더 프록시가 없을 수 있다.
				ItemImage->MarkRenderStateDirty();
			}
		}
	}

	Super::BeginPlay();

	if (ItemTrigger)
	{
		ItemTrigger->OnComponentBeginOverlap.AddDynamic(this, &AItemActorBase::OnItemBeginOverlap);
	}
}

void AItemActorBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AItemActorBase::EndPlay()
{
	Super::EndPlay();
}

void AItemActorBase::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	// C++ config는 future-proof 저장 데이터입니다.
	// 현재 gameplay 튜닝은 Lua property가 담당하지만, scene 저장/로드 시 기본값이 사라지지 않게 보존합니다.
	Ar << ItemFeatureFlags;
	Ar << InteractionConfig.ScoreValue;
	Ar << InteractionConfig.RequiredInteractorTag;
	Ar << InteractionConfig.RespawnDelay;
	Ar << InteractionConfig.Cooldown;
	Ar << InteractionConfig.bStartsEnabled;

	// BeginPlay에서 texture를 다시 적용할 수 있게 경로도 저장.
	Ar << ItemTexturePath;
}

UPrimitiveComponent* AItemActorBase::GetItemTrigger() const
{
	return ItemTrigger;
}

bool AItemActorBase::IsValidInteractor(AActor* OtherActor) const
{
	if (!OtherActor)
	{
		return false;
	}

	if (InteractionConfig.RequiredInteractorTag.empty())
	{
		return true;
	}

	return OtherActor->HasTag(InteractionConfig.RequiredInteractorTag);
}

void AItemActorBase::OnItemBeginOverlap(const FComponentOverlapEvent& Event)
{
	if (!HasFeature(EItemFeatureFlags::PickupOnOverlap))
	{
		return;
	}

	if (HasFeature(EItemFeatureFlags::SingleUse) && bPicked)
	{
		return;
	}

	if (!IsValidInteractor(Event.OtherActor))
	{
		return;
	}

	bPicked = true;

	if (HasFeature(EItemFeatureFlags::ConsumeOnPickup))
	{
		ConsumeItem();
	}
}

void AItemActorBase::ConsumeItem()
{
	// 안보이게 처리만 한다 Chunk가 소멸될 때 같이 처리
	SetTriggerEnabled(false);
	ItemImage->SetTexture(nullptr);

	// overlap dispatch 중 즉시 Destroy하면 dispatcher가 들고 있는 component/owner 포인터가 무효화된다.
	// 실제 해제는 AMapChunk::EndPlay에서 처리한다.
}

void AItemActorBase::SetItemScript(const FString& ScriptPath)
{
	if (!ItemScript)
	{
		ItemScript = AddComponent<UScriptComponent>();
	}

	if (ItemScript)
	{
		ItemScript->SetScriptPath(ScriptPath);
	}
}

void AItemActorBase::SetItemTexturePath(const FString& TexturePath)
{
	if (TexturePath.empty())
	{
		ItemTexturePath = "None";
		return;
	}

	ItemTexturePath = TexturePath;
}

bool AItemActorBase::HasFeature(EItemFeatureFlags Feature) const
{
	return (ItemFeatureFlags & static_cast<uint32>(Feature)) != 0;
}

void AItemActorBase::SetFeatureEnabled(EItemFeatureFlags Feature, bool bEnabled)
{
	if (bEnabled)
	{
		AddFeature(Feature);
	}
	else
	{
		RemoveFeature(Feature);
	}
}

void AItemActorBase::AddFeature(EItemFeatureFlags Feature)
{
	ItemFeatureFlags |= static_cast<uint32>(Feature);
}

void AItemActorBase::RemoveFeature(EItemFeatureFlags Feature)
{
	ItemFeatureFlags &= ~static_cast<uint32>(Feature);
}

void AItemActorBase::SetTriggerEnabled(bool bEnabled)
{
	InteractionConfig.bStartsEnabled = bEnabled;

	if (ItemTrigger)
	{
		ItemTrigger->SetCollisionEnabled(bEnabled);
		ItemTrigger->SetGenerateOverlapEvents(bEnabled);
	}
}

bool AItemActorBase::IsTriggerEnabled() const
{
	return ItemTrigger && ItemTrigger->IsCollisionEnabled();
}
