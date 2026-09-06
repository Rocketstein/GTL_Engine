#include "LogFragmentItemActor.h"
#include "Component/Shape/BoxComponent.h"

IMPLEMENT_CLASS(ALogFragmentItemActor, AItemActorBase)

ALogFragmentItemActor::ALogFragmentItemActor()
{
	SetTag("Item.LogFragment");

	SetItemScript("Scripts/Game/Items/LogItem.lua");

	// 생성자에서는 texture 경로만 지정한다.
	SetItemTexturePath("Asset/Content/Texture/item_log.png");

	InteractionConfig.ScoreValue = 10;
	InteractionConfig.RequiredInteractorTag = "Player";
	InteractionConfig.bStartsEnabled = true;

	SetFeatureFlags(
		static_cast<uint32>(EItemFeatureFlags::PickupOnOverlap)
		| static_cast<uint32>(EItemFeatureFlags::ConsumeOnPickup)
		| static_cast<uint32>(EItemFeatureFlags::SingleUse)
		| static_cast<uint32>(EItemFeatureFlags::ScoreReward)
	);

	if (UBoxComponent* Trigger = GetItemTriggerBox())
	{
		Trigger->SetBoxExtent(FVector(0.75f, 0.75f, 0.75f));
		Trigger->SetCollisionEnabled(true);
		Trigger->SetGenerateOverlapEvents(true);
	}
}
