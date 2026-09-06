#include "CrashDumpItemActor.h"
#include "Component/Shape/BoxComponent.h"

IMPLEMENT_CLASS(ACrashDumpItemActor, AItemActorBase)

ACrashDumpItemActor::ACrashDumpItemActor()
{
	SetTag("Item.CrashDump");

	SetItemScript("Scripts/Game/Items/CrashDumpItem.lua");

	// 생성자에서는 texture 경로만 지정
	SetItemTexturePath("Asset/Content/Texture/item_crash_dump.png");

	InteractionConfig.ScoreValue = 0;
	InteractionConfig.RequiredInteractorTag = "Player";
	InteractionConfig.bStartsEnabled = true;

	SetFeatureFlags(
		static_cast<uint32>(EItemFeatureFlags::PickupOnOverlap)
		| static_cast<uint32>(EItemFeatureFlags::ConsumeOnPickup)
		| static_cast<uint32>(EItemFeatureFlags::SingleUse)
	);

	if (UBoxComponent* Trigger = GetItemTriggerBox())
	{
		Trigger->SetBoxExtent(FVector(0.9f, 0.9f, 0.9f));
		Trigger->SetCollisionEnabled(true);
		Trigger->SetGenerateOverlapEvents(true);
	}
}
