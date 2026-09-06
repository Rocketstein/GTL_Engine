#pragma once

#include "GameFramework/AActor.h"

class UTextUIComponent;

class ATextUIActor : public AActor
{
public:
    DECLARE_CLASS(ATextUIActor, AActor)

    ATextUIActor() = default;

    void InitDefaultComponents() override;

    UTextUIComponent* GetTextUIComponent() const { return TextUIComponent; }
    void SetText(const FString& InText);

private:
    UTextUIComponent* TextUIComponent = nullptr;
};
