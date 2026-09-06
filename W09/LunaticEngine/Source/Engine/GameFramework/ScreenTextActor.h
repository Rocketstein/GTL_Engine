#pragma once

#include "GameFramework/AActor.h"

class UUIScreenTextComponent;

class AScreenTextActor : public AActor
{
public:
	DECLARE_CLASS(AScreenTextActor, AActor)

	AScreenTextActor() = default;

	void InitDefaultComponents();

private:
	UUIScreenTextComponent* TextRenderComponent = nullptr;
};
