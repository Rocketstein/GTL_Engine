#pragma once
#include "GameFramework/AActor.h"
#include "Object/Ptr/WeakObjectPtr.h"
#include "Source/Engine/GameFramework/Actor/EmptyActor.generated.h"
class UBillboardComponent;

UCLASS()
class AEmptyActor : public AActor
{
public:
	GENERATED_BODY()
	void InitDefaultComponents();
	void PostDuplicate() override;

protected:
	void OnPostLoad(FArchive& Ar) override;

private:
	void RestoreBillboardComponent();

	TWeakObjectPtr<UBillboardComponent> BillboardComponent = nullptr;
};

