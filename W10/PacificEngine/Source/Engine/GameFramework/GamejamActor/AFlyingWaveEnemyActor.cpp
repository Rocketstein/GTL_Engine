#include "AFlyingWaveEnemyActor.h"
#include "Component/Collision/CircleCollider2DComponent.h"
#include "GameFramework/World.h"
#include "Component/StaticMeshComponent.h"
#include "Core/Random.h"
#include "Engine/Platform/Paths.h"
#include "Engine/Runtime/Engine.h"
#include "Math/MathUtils.h"
#include "Mesh/ObjManager.h"

#include <cmath>

IMPLEMENT_CLASS(AFlyingWaveEnemyActor, AEnemyBaseActor)

namespace
{
	constexpr const char* HelicopterBodyMeshPath = "Models/Heli/helibody.obj";
	constexpr const char* HelicopterPropellerMeshPath = "Models/Heli/heliwing.obj";
	constexpr float PropellerSpinSpeedDegreesPerSecond = 1440.0f;

	UStaticMesh* LoadHelicopterMesh(const char* RelativePath)
	{
		ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
		return FObjManager::Get().Load(FPaths::ContentRelativePath(RelativePath));
	}

	FVector NormalizeDirectionOrForward(const FVector& Direction)
	{
		if (Direction.LengthSquared() <= EPSILON)
		{
			return FVector(1.0f, 0.0f, 0.0f);
		}

		return Direction.Normalized();
	}

	FRotator YawRotationFromDirection(const FVector& Direction)
	{
		const float Yaw = std::atan2(Direction.Y, Direction.X) * RAD_TO_DEG;
		return FRotator(0.0f, Yaw, 0.0f);
	}
}

void AFlyingWaveEnemyActor::Tick(float DeltaTime)
{
    if (UWorld* World = GetWorld())
    {
        if (World->IsGameplayPaused())
        {
            return;
        }
    }

    AEnemyBaseActor::Tick(DeltaTime);

	if (PropellerMeshComponent)
	{
		PropellerMeshComponent->AddLocalRotation(FRotator(0.0f, PropellerSpinSpeedDegreesPerSecond * DeltaTime, 0.0f));
	}

    AddActorWorldOffset(MoveDirection * MoveSpeed * DeltaTime);

    ElapsedTime += DeltaTime;
    if (ElapsedTime >= LifeTime)
    {
        RequestReturnToPool();
    }
}

void AFlyingWaveEnemyActor::InitDefaultComponents()
{
    Super::InitDefaultComponents();
    ColliderComponent->SetCollisionChannel(ECollisionChannel::FlyingEnemy);
    ColliderComponent->SetGenerateOverlapEvents(true);
    ColliderComponent->SetGenerateHitEvents(false);

	PropellerMeshComponent = AddComponent<UStaticMeshComponent>();
	PropellerMeshComponent->SetFName(FName("HelicopterPropellerMesh"));
	PropellerMeshComponent->SetStaticMesh(LoadHelicopterMesh(HelicopterPropellerMeshPath));
	PropellerMeshComponent->SetReceivesDecals(false);

	MeshComponent->SetRelativeScale(FVector(0.5f, 0.5f, 0.5f));

	if (MeshComponent)
	{
		MeshComponent->AddChild(PropellerMeshComponent);
	}
	else if (ColliderComponent)
	{
		ColliderComponent->AddChild(PropellerMeshComponent);
	}
}

UStaticMeshComponent* AFlyingWaveEnemyActor::GetDefaultMeshComponent()
{
	UStaticMeshComponent* MeshComp = AddComponent<UStaticMeshComponent>();
	MeshComp->SetStaticMesh(LoadHelicopterMesh(HelicopterBodyMeshPath));
	MeshComp->SetReceivesDecals(false);
	return MeshComp;
}

void AFlyingWaveEnemyActor::InitWave(const FVector& NewDirection, float NewSpeed, float InLifeTime)
{
    MoveDirection = NormalizeDirectionOrForward(NewDirection);
    MoveSpeed = NewSpeed;
    LifeTime = InLifeTime;
    ElapsedTime = 0.0f;

	SetActorRotation(YawRotationFromDirection(MoveDirection));

	if (PropellerMeshComponent)
	{
		PropellerMeshComponent->SetRelativeRotation(FRotator(0.0f, FRandom::Range(0.0f, 360.0f), 0.0f));
	}
}
