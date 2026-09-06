#pragma once

#include "GameFramework/AActor.h"

class UUIComponent;

class AUIActor : public AActor
{
public:
    DECLARE_CLASS(AUIActor, AActor)

    AUIActor() = default;

    void InitDefaultComponents();

    UUIComponent* GetUIComponent() const { return UIComponent; }

private:
    UUIComponent* UIComponent = nullptr;
};
