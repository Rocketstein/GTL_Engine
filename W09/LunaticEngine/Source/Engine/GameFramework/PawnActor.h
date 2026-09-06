#pragma once

#include "GameFramework/AActor.h"

class USceneComponent;
class UBillboardComponent;

class APawnActor : public AActor
{
public:
	DECLARE_CLASS(APawnActor, AActor)

	APawnActor();

	void BeginPlay() override;

	void InitDefaultComponents();

private:
	USceneComponent* RootSceneComponent = nullptr;
	UBillboardComponent* BillboardComponent = nullptr;
};
