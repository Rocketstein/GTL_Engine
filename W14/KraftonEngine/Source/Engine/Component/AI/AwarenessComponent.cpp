#include "Component/AI/AwarenessComponent.h"

#include "AI/CombatTargetRegistry.h"
#include "Component/AI/AIBlackboardComponent.h"
#include "Component/Combat/CombatStateComponent.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Math/MathUtils.h"

namespace
{
	const FName Key_AwarenessLevel   = FName("AwarenessLevel");
	const FName Key_Suspicion        = FName("Suspicion");
	const FName Key_AwarenessState   = FName("AwarenessState");
	const FName Key_IsUnawareTarget  = FName("IsUnawareTarget");
	const FName Key_InCombat         = FName("AwareInCombat");
	const FName Key_InvestigateLoc   = FName("InvestigateLocation");

	const FName Key_CanSee      = FName("CanSee");
	const FName Key_InProximity = FName("InProximity");
	const FName Key_HasTarget   = FName("HasTarget");
}

void UAwarenessComponent::BeginPlay()
{
	UActorComponent::BeginPlay();
	State = EAwarenessState::Unaware;
	Suspicion = 0.0f;
	LostTimer = SearchTimer = ReturnTimer = 0.0f;
	if (AActor* OwnerActor = GetOwner())
	{
		HomeAnchor = OwnerActor->GetActorLocation();
		bHomeAnchorSet = true;
	}
	PublishToBlackboard();
}

void UAwarenessComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
	UActorComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UAIBlackboardComponent* BB = GetBlackboard();
	if (!BB)
	{
		return;
	}

	const bool bCanSee   = BB->GetBool(Key_CanSee);
	const bool bInProx   = BB->GetBool(Key_InProximity);
	const bool bPerceived = bCanSee || bInProx;

	if (bPerceived)
	{
		float Gain = 0.0f;
		if (bCanSee) Gain += SightGainPerSecond;
		if (bInProx) Gain += ProximityGainPerSecond;
		Suspicion += Gain * DeltaTime;
		InvestigateLocation = BB->GetLastSeenLocation();
	}
	else
	{
		Suspicion -= SuspicionDecayPerSecond * DeltaTime;
	}
	Suspicion = FMath::Clamp(Suspicion, 0.0f, 1.0f);

	switch (State)
	{
	case EAwarenessState::Unaware:
		if (Suspicion >= ConfirmThreshold)        SetState(EAwarenessState::Alert);
		else if (Suspicion >= SuspiciousThreshold) SetState(EAwarenessState::Suspicious);
		break;

	case EAwarenessState::Suspicious:
		if (Suspicion >= ConfirmThreshold)   SetState(EAwarenessState::Alert);
		else if (Suspicion <= 0.001f)        SetState(EAwarenessState::Unaware);
		else if (!bPerceived)                SetState(EAwarenessState::Investigating);
		break;

	case EAwarenessState::Investigating:
		if (Suspicion >= ConfirmThreshold) SetState(EAwarenessState::Alert);
		else if (Suspicion <= 0.001f)      SetState(EAwarenessState::Returning);
		break;

	case EAwarenessState::Alert:
		if (bPerceived)
		{
			LostTimer = 0.0f;
		}
		else
		{
			LostTimer += DeltaTime;
			if (LostTimer >= LostGraceSeconds) SetState(EAwarenessState::Searching);
		}
		break;

	case EAwarenessState::Searching:
		if (bPerceived)
		{
			SetState(EAwarenessState::Alert);
		}
		else
		{
			SearchTimer += DeltaTime;
			if (SearchTimer >= SearchSeconds) SetState(EAwarenessState::Returning);
		}
		break;

	case EAwarenessState::Returning:
		if (bPerceived)
		{
			SetState(EAwarenessState::Alert);
		}
		else
		{
			ReturnTimer += DeltaTime;
			if (ReturnTimer >= ReturnSeconds)
			{
				Suspicion = 0.0f;
				SetState(EAwarenessState::Unaware);
			}
		}
		break;
	}

	PublishToBlackboard();
}

void UAwarenessComponent::ReportNoise(const FVector& Location, float Strength)
{
	ReportNoiseClassified(Location, Strength, static_cast<int32>(EAISoundClass::Normal));
}

void UAwarenessComponent::ReportNoiseClassified(const FVector& Location, float Strength, int32 SoundClass)
{
	if (Strength <= 0.0f)
	{
		return;
	}
	const bool bImportant = (SoundClass == static_cast<int32>(EAISoundClass::Important));
	const float Scale = bImportant ? (ImportantNoiseScale > 1.0f ? ImportantNoiseScale : 1.0f) : 1.0f;

	Suspicion = FMath::Clamp(Suspicion + Strength * NoiseGainScale * Scale, 0.0f, 1.0f);
	InvestigateLocation = Location;
	LastSoundClass = SoundClass;
	if (UAIBlackboardComponent* BB = GetBlackboard())
	{
		BB->SetLastHeardLocation(Location);
		BB->SetFloat(FName("LastSoundClass"), static_cast<float>(SoundClass));
	}

	if (bImportant)
	{
		// 중요 소리(사기그릇/휘슬): 무지각/수상함이라도 즉시 그 위치로 조사하러 간다(세키로식 유인).
		if (State == EAwarenessState::Unaware || State == EAwarenessState::Suspicious)
		{
			SetState(EAwarenessState::Investigating);
			PublishToBlackboard();
		}
	}
	else if (State == EAwarenessState::Unaware && Suspicion >= SuspiciousThreshold)
	{
		// 일반 소리: 임계를 넘겨야 수상함으로 승격(다음 Tick 이 마저 처리).
		SetState(EAwarenessState::Suspicious);
		PublishToBlackboard();
	}
}

void UAwarenessComponent::ForceState(EAwarenessState NewState)
{
	if (NewState == EAwarenessState::Alert)   Suspicion = 1.0f;
	if (NewState == EAwarenessState::Unaware) Suspicion = 0.0f;
	SetState(NewState);
	PublishToBlackboard();
}

void UAwarenessComponent::ResetAwareness()
{
	Suspicion = 0.0f;
	LostTimer = SearchTimer = ReturnTimer = 0.0f;
	LastSoundClass = -1;
	SetState(EAwarenessState::Unaware);
	PublishToBlackboard();
}

void UAwarenessComponent::SetState(EAwarenessState NewState)
{
	if (State == NewState)
	{
		return;
	}
	State = NewState;
	LostTimer = SearchTimer = ReturnTimer = 0.0f;
	if (NewState == EAwarenessState::Returning && bHomeAnchorSet)
	{
		InvestigateLocation = HomeAnchor;
	}
	// 전투 확정으로 전환하는 순간 주변 아군에게 경계를 전파한다.
	if (NewState == EAwarenessState::Alert)
	{
		PropagateAlertToAllies();
	}
}

void UAwarenessComponent::PropagateAlertToAllies()
{
	if (AlertPropagationRadius <= 0.0f || AlertPropagationStrength <= 0.0f)
	{
		return;
	}
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return;
	}
	UCombatStateComponent* Combat = OwnerActor->GetComponentByClass<UCombatStateComponent>();
	if (!Combat)
	{
		return;
	}
	const FVector Origin = OwnerActor->GetActorLocation();
	TArray<AActor*> Allies;
	FCombatTargetRegistry::Get().GatherAllies(Combat, Origin, AlertPropagationRadius, Allies);
	for (AActor* Ally : Allies)
	{
		if (!Ally)
		{
			continue;
		}
		// ReportNoise 는 무지각→수상함까지만 끌어올린다(직접 Alert 설정 아님) → 재귀 전파 없음.
		if (UAwarenessComponent* AllyAwareness = Ally->GetComponentByClass<UAwarenessComponent>())
		{
			AllyAwareness->ReportNoise(Origin, AlertPropagationStrength);
		}
	}
}

void UAwarenessComponent::PublishToBlackboard()
{
	UAIBlackboardComponent* BB = GetBlackboard();
	if (!BB)
	{
		return;
	}
	BB->SetFloat(Key_AwarenessLevel, Suspicion);
	BB->SetFloat(Key_Suspicion, Suspicion);
	BB->SetFloat(Key_AwarenessState, static_cast<float>(static_cast<int32>(State)));
	BB->SetBool(Key_IsUnawareTarget, IsUnaware());
	BB->SetBool(Key_InCombat, IsInCombat());
	BB->SetVector(Key_InvestigateLoc, InvestigateLocation);
}

UAIBlackboardComponent* UAwarenessComponent::GetBlackboard() const
{
	AActor* OwnerActor = GetOwner();
	return OwnerActor ? OwnerActor->GetComponentByClass<UAIBlackboardComponent>() : nullptr;
}
