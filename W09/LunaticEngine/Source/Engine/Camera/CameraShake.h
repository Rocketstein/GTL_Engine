#pragma once
#include "CameraShakePattern.h"
#include "Math/Rotator.h"
#include "Math/Vector.h"
#include "Object/Object.h"
#include "Object/ObjectFactory.h"

class UCameraShakeBase : public UObject
{
public:
	DECLARE_CLASS(UCameraShakeBase, UObject)

	UCameraShakeBase() = default;
	virtual ~UCameraShakeBase();

	void SetRootShakePattern(UCameraShakePattern* InPattern) { RootShakePattern = InPattern; }
	UCameraShakePattern* GetRootShakePattern() const { return RootShakePattern; }

	virtual void UpdateShake(float DeltaTime, FVector& OutLoc, FRotator& OutRot);
	bool IsFinished() const { return ElapsedTime >= Duration; }

public:
	float Duration = 0.5f;
	float Intensity = 1.0f;
	float ElapsedTime = 0.0f;

protected:
	UCameraShakePattern* RootShakePattern = nullptr;
};
