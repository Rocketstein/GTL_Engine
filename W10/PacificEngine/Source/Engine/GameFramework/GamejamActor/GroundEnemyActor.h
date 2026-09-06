#pragma once
#include "EnemyBaseActor.h"

class AGroundEnemyActor : public AEnemyBaseActor
{
public:
    DECLARE_CLASS(AGroundEnemyActor, AEnemyBaseActor)
protected:
    UStaticMeshComponent* GetDefaultMeshComponent() override;
    UCollider2DComponent* GetDefaultColliderComponent() override;
};
