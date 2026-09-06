#pragma once
#include "AActor.h"

class USkeletalMeshComponent;

class ASkeletalMeshActor : public AActor
{
public:
    DECLARE_CLASS(ASkeletalMeshActor, AActor)
    ASkeletalMeshActor() {}

    void InitDefaultComponents() override;
    void InitDefaultComponents(const FString& UStaticMeshFileName);

	USkeletalMeshComponent* GetSkeletalMeshComponent() const { return SkeletalMeshComponent; }

private:
    USkeletalMeshComponent* SkeletalMeshComponent = nullptr;
};
