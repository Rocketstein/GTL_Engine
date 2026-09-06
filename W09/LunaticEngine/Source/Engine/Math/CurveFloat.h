#pragma once
#include "Object/ObjectFactory.h"
#include "Vector.h"

class UCurveFloat :public UObject {
public:
	DECLARE_CLASS(UCurveFloat, UObject)
	UCurveFloat();
	TArray<FVector2> Curve;

public:
	float Evaluate(float NormalizedT) const;
	void AddKey(float Time, float Value);
	void SortKeys();
	void ResetLinear();
};