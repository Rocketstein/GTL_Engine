#pragma once
#include "AActor.h"
class UCameraComponent;
class ACameraActor : public AActor
{
public:
	UCameraComponent* CameraComponent = nullptr;

};