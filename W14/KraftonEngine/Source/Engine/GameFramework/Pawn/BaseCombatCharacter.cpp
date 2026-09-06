#include "GameFramework/Pawn/BaseCombatCharacter.h"

#include "Animation/AnimInstance.h"
#include "Animation/Graph/AnimGraphInstance.h"
#include "Animation/Montage/AnimMontage.h"
#include "Component/Combat/CombatStateComponent.h"
#include "Component/Combat/HealthComponent.h"
#include "Component/Input/ActionComponent.h"
#include "Component/Primitive/SkeletalMeshComponent.h"

void ABaseCombatCharacter::InitDefaultComponents(const FString& SkeletalMeshFileName)
{
	Super::InitDefaultComponents(SkeletalMeshFileName);
	HealthComponent = AddComponent<UHealthComponent>();
	CombatStateComponent = AddComponent<UCombatStateComponent>();
	ActionComponent = AddComponent<UActionComponent>();

	if (HealthComponent)
	{
		HealthComponent->OnDamaged.AddUObject(this, &ABaseCombatCharacter::HandleDamaged);
		HealthComponent->OnDeath.AddUObject(this, &ABaseCombatCharacter::HandleDeath);
	}
}

void ABaseCombatCharacter::PostDuplicate()
{
	Super::PostDuplicate();
	RebindCombatComponents();
}

void ABaseCombatCharacter::OnPostLoad(FArchive& Ar)
{
	Super::OnPostLoad(Ar);
	RebindCombatComponents();
}

void ABaseCombatCharacter::RebindCombatComponents()
{
	HealthComponent = GetComponentByClass<UHealthComponent>();
	CombatStateComponent = GetComponentByClass<UCombatStateComponent>();
	ActionComponent = GetComponentByClass<UActionComponent>();
	if (HealthComponent)
	{
		HealthComponent->OnDamaged.AddUObject(this, &ABaseCombatCharacter::HandleDamaged);
		HealthComponent->OnDeath.AddUObject(this, &ABaseCombatCharacter::HandleDeath);
	}
}

void ABaseCombatCharacter::ApplyCombatDamage(float Damage, AActor* DamageCauser, AActor* InstigatorActor)
{
	if (HealthComponent)
	{
		HealthComponent->ApplyDamage(Damage, DamageCauser, InstigatorActor);
	}
}

bool ABaseCombatCharacter::IsDead() const
{
	return HealthComponent && HealthComponent->IsDead();
}

bool ABaseCombatCharacter::IsAlive() const
{
	return !HealthComponent || HealthComponent->IsAlive();
}

bool ABaseCombatCharacter::PlayCombatMontage(UAnimMontage* Montage)
{
	return PlayCombatMontageRated(Montage, 1.0f, -1.0f);
}

bool ABaseCombatCharacter::PlayCombatMontageRated(UAnimMontage* Montage, float PlayRate, float BlendInTime)
{
	UAnimInstance* AnimInstance = GetAnimInstance();
	if (!AnimInstance || !Montage)
	{
		return false;
	}
	AnimInstance->PlayMontage(Montage, FName::None, PlayRate > 0.0f ? PlayRate : 1.0f, BlendInTime);
	return true;
}

void ABaseCombatCharacter::SetAnimGraphBool(const FName& Name, bool Value)
{
	if (UAnimGraphInstance* Graph = Cast<UAnimGraphInstance>(GetAnimInstance()))
	{
		Graph->SetGraphVariableBool(Name, Value);
	}
}

void ABaseCombatCharacter::SetAnimGraphFloat(const FName& Name, float Value)
{
	if (UAnimGraphInstance* Graph = Cast<UAnimGraphInstance>(GetAnimInstance()))
	{
		Graph->SetGraphVariableFloat(Name, Value);
	}
}

void ABaseCombatCharacter::SetAnimGraphInt(const FName& Name, int32 Value)
{
	if (UAnimGraphInstance* Graph = Cast<UAnimGraphInstance>(GetAnimInstance()))
	{
		Graph->SetGraphVariableInt(Name, Value);
	}
}

void ABaseCombatCharacter::HandleDamaged(UHealthComponent* /*Component*/, float /*Damage*/, float /*NewHealth*/, AActor* /*DamageCauser*/, AActor* /*InstigatorActor*/)
{
	SetAnimGraphBool(FName("IsDamaged"), true);
}

void ABaseCombatCharacter::HandleDeath(UHealthComponent* /*Component*/, AActor* /*DamageCauser*/, AActor* /*InstigatorActor*/)
{
	SetAnimGraphBool(FName("IsDead"), true);
}

UAnimInstance* ABaseCombatCharacter::GetAnimInstance() const
{
	USkeletalMeshComponent* MeshComponent = GetMesh();
	return MeshComponent ? MeshComponent->GetAnimInstance() : nullptr;
}
