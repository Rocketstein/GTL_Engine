#pragma once

#include "EnemyBaseActor.h"

class UStaticMeshComponent;

class AFlyingWaveEnemyActor : public AEnemyBaseActor
{
public:
    DECLARE_CLASS(AFlyingWaveEnemyActor, AEnemyBaseActor)
    AFlyingWaveEnemyActor() = default;
    void Tick(float DeltaTime) override;
    void InitDefaultComponents() override;

	void InitWave(const FVector& NewDirection, float NewSpeed, float InLifeTime);

protected:
	UStaticMeshComponent* GetDefaultMeshComponent() override;

private:
	UStaticMeshComponent* PropellerMeshComponent = nullptr;
    FVector MoveDirection = FVector::ZeroVector;
    float MoveSpeed = 1.0f;
    float LifeTime = 10.0f;
    float ElapsedTime = 0.0f;
};
