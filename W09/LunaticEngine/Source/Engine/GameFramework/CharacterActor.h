#pragma once

#include "GameFramework/AActor.h"

class USceneComponent;
class UBillboardComponent;

class ACharacterActor : public AActor
{
public:
	DECLARE_CLASS(ACharacterActor, AActor)

	ACharacterActor();

	void InitDefaultComponents();

private:
	USceneComponent* RootSceneComponent = nullptr;
	UBillboardComponent* BillboardComponent = nullptr;
};
