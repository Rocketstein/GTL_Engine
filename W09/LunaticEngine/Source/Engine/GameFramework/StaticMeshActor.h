#pragma once

#include "GameFramework/AActor.h"

class UStaticMeshComponent;
class UTextRenderComponent;
class USubUVComponent;

class AStaticMeshActor : public AActor
{
public:
	DECLARE_CLASS(AStaticMeshActor, AActor)
	AStaticMeshActor() {}

	virtual void InitDefaultComponents(const FString& UStaticMeshFileName = "None");

protected:
	bool IsBasicShapeAssetPath(const FString& Path);

protected:
	UStaticMeshComponent* StaticMeshComponent = nullptr;
	//UTextRenderComponent* TextRenderComponent = nullptr;
	//USubUVComponent* SubUVComponent = nullptr;
};
