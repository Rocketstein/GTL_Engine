#pragma once

#include "GameFramework/AActor.h"

class UStaticMesh;

class AHomingMissileActor : public AActor
{
public:
    DECLARE_CLASS(AHomingMissileActor, AActor)

    void InitDefaultComponents() override;
    void Tick(float DeltaTime) override;

    void SetTargetActor(AActor* InTargetActor) { TargetActor = InTargetActor; }
    void SetDamage(float InDamage) { Damage = InDamage; }
    void SetImpactRadius(float InRadius) { ImpactRadius = InRadius; }
    void SetTurnSpeed(float InTurnSpeed) { TurnSpeed = InTurnSpeed; }
    void SetSpeed(float InSpeed) { Speed = InSpeed; }

    void Fire();

private:
    UStaticMesh* LoadDefaultMesh() const;
    void Explode();
    void ReturnOrDeactivate();

private:
    AActor* TargetActor = nullptr;

    float Damage = 10.0f;
    float ImpactRadius = 0.0f;
    float TurnSpeed = 120.0f;
    float Speed = 10.0f;
    float ColliderSize = 0.8f;

    FVector CurrentDirection = FVector(1.0f, 0.0f, 0.0f);
    bool bActive = false;
};
