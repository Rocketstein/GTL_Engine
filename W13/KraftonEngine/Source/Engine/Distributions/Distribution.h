#pragma once

#include "Distributions/Distributions.h"
#include "Object/Object.h"
#include "Object/FName.h"

#include "Source/Engine/Distributions/Distribution.generated.h"

class FArchive;

UCLASS()
class UDistribution : public UObject
{
public:
	GENERATED_BODY()

	void Serialize(FArchive& Ar) override;

	virtual bool CanBeBaked() const { return true; }
	virtual bool IsUniform() const { return false; }
	virtual void GetTimeRange(float& OutMinTime, float& OutMaxTime) const;
};
