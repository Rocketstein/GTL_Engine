#pragma once

#include "GameFramework/AActor.h"

class UTextRenderComponent;

class AWorldTextActor : public AActor
{
public:
	DECLARE_CLASS(AWorldTextActor, AActor)

	AWorldTextActor() = default;

	void InitDefaultComponents();

private:
	UTextRenderComponent* TextRenderComponent = nullptr;
};
