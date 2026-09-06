#include "PickupActor.h"
#include "Component/BillboardComponent.h"
#include "Component/Collision/CircleCollider2DComponent.h"
#include "Materials/MaterialManager.h"
#include "Object/ObjectFactory.h"


IMPLEMENT_CLASS(APickupActor, AActor)


APickupActor::APickupActor()
{
    bNeedsTick = false;
    SetActorTag(FName("Pickup"));
}

void APickupActor::InitDefaultComponents()
{
    ColliderComponent = AddComponent<UCircleCollider2DComponent>();
    ColliderComponent->SetFName("PickupCollider");
    ColliderComponent->SetCollisionChannel(ECollisionChannel::Pickup);
    ColliderComponent->SetGenerateOverlapEvents(true);
    SetRootComponent(ColliderComponent);

    BillboardComponent = AddComponent<UBillboardComponent>();
    auto Icon = FMaterialManager::Get().GetOrCreateMaterial(IconPath);
    BillboardComponent->SetVisibleInGame(true);
    BillboardComponent->SetEditorHelper(false);
    BillboardComponent->SetMaterial(Icon);
    BillboardComponent->AttachToComponent(ColliderComponent);

}
