#pragma once

#include "GameFramework/AActor.h"

class UCanvasRootComponent;

class AUIRootActor : public AActor
{
public:
	DECLARE_CLASS(AUIRootActor, AActor)

	AUIRootActor() = default;

	void InitDefaultComponents();

private:
	UCanvasRootComponent* CanvasRootComponent = nullptr;
};
