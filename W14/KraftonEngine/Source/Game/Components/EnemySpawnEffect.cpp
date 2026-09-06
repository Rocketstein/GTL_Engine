#include "EnemySpawnEffect.h"
#include "Engine/GameFramework/AActor.h"
#include "Engine/Component/Particle/ParticleSystemComponent.h"
#include "Particle/ParticleSystemManager.h"
#include "Engine/Core/Logging/Log.h"

UEnemySpawnEffectComponent::UEnemySpawnEffectComponent()
{
	// 자동재생 금지 — 디렉터가 PlayEffect 로 인보크할 때까지 대기한다.
	bAutoActivate = false;
}

void UEnemySpawnEffectComponent::BeginPlay()
{
	EnsureParticleSystemAsset();

	// 인보크 전까지 소유 액터를 숨긴다(컴포넌트가 직접 책임 — 에디터 플래그/코루틴에 의존 안 함).
	if (AActor* Owner = GetOwner())
	{
		Owner->SetVisible(false);
	}

	State = ESpawnState::Idle;
	Super::BeginPlay();
}

void UEnemySpawnEffectComponent::EnsureParticleSystemAsset()
{
	AActor* Owner = GetOwner();
	if (!Owner) return;

	if (!EffectPSC.IsValid())
	{
		EffectPSC = Owner->AddComponent<UParticleSystemComponent>();
	}

	UParticleSystemComponent* PSC = EffectPSC.Get();
	if (!PSC) return;

	// 연출 전용 — 충돌/네비 영향 없음. 활성화는 우리가 직접 구동한다.
	PSC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PSC->SetCanEverAffectNavigation(false);
	PSC->SetAutoActivate(false);

	if (UParticleSystem* Template = FParticleSystemManager::Get().Load(ParticleSystemAssetPath))
	{
		PSC->SetTemplate(Template);
	}
	else
	{
		UE_LOG("[EnemySpawnEffect] could not load particle system: %s", ParticleSystemAssetPath.c_str());
	}

	// 적 루트에 붙여 이동을 따라가게 하고, 원점에 둔다.
	if (USceneComponent* Root = Owner->GetRootComponent())
	{
		PSC->AttachToComponent(Root, FName());
	}
	PSC->SetRelativeLocation(FVector(0.0f, 0.0f, 0.0f));
	PSC->Deactivate();
}

void UEnemySpawnEffectComponent::PlayEffect()
{
	// 이미 진행 중이면 무시(중복 인보크 방지).
	if (State == ESpawnState::Delaying || State == ESpawnState::Emitting) return;

	if (!EffectPSC.IsValid())
	{
		EnsureParticleSystemAsset();
	}

	Elapsed = 0.f;
	State = ESpawnState::Delaying;
	SetComponentTickEnabled(true);

	// 지연이 없으면 즉시 스폰.
	if (SpawnDelay <= 0.f)
	{
		DoSpawn();
	}
}

void UEnemySpawnEffectComponent::Activate()
{
	Super::Activate();
	PlayEffect();
}

void UEnemySpawnEffectComponent::DoSpawn()
{
	Elapsed = 0.f;
	State = ESpawnState::Emitting;

	if (AActor* Owner = GetOwner())
	{
		Owner->SetVisible(true);
	}

	if (UParticleSystemComponent* PSC = EffectPSC.Get())
	{
		PSC->Activate(true); // reset + 버스트 재생
	}
}

bool UEnemySpawnEffectComponent::IsPlaying() const
{
	return State == ESpawnState::Delaying || State == ESpawnState::Emitting;
}

void UEnemySpawnEffectComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (State == ESpawnState::Delaying)
	{
		Elapsed += DeltaTime;
		if (Elapsed >= SpawnDelay)
		{
			DoSpawn();
		}
		return;
	}

	if (State == ESpawnState::Emitting)
	{
		Elapsed += DeltaTime;
		if (Elapsed >= Duration)
		{
			if (UParticleSystemComponent* PSC = EffectPSC.Get())
			{
				PSC->Deactivate(); // graceful — 기존 입자는 수명대로 소멸
			}
			State = ESpawnState::Done;
			SetComponentTickEnabled(false);
		}
	}
}

void UEnemySpawnEffectComponent::EndPlay()
{
	if (UParticleSystemComponent* PSC = EffectPSC.Get())
	{
		PSC->Deactivate();
		if (Owner)
		{
			Owner->RemoveComponent(PSC);
		}
	}
	Super::EndPlay();
}
