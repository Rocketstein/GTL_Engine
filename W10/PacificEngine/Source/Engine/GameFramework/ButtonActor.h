#pragma once

#include "GameFramework/AActor.h"

class UUIButtonComponent;

class AButtonActor : public AActor
{
public:
    DECLARE_CLASS(AButtonActor, AActor)

    AButtonActor() = default;

    void InitDefaultComponents();

    UUIButtonComponent* GetButtonComponent() const { return ButtonComponent; }

private:
    UUIButtonComponent* ButtonComponent = nullptr;
};
