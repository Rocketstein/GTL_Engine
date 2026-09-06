#pragma once

#include "GameFramework/AActor.h"
#include "Platform/Paths.h"

class UBillboardComponent;
class UCircleCollider2DComponent;


class APickupActor : public AActor
{
public:
    DECLARE_CLASS(APickupActor, AActor)
    APickupActor();
    void InitDefaultComponents() override;


private:
    UCircleCollider2DComponent* ColliderComponent = nullptr;
    UBillboardComponent* BillboardComponent = nullptr;
    FString IconPath = FPaths::ContentRelativePath("Materials/gem.json");
};
