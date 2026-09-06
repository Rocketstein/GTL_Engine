#pragma once
#include "Math/CurveFloat.h"
#include "Object/ObjectFactory.h"

class UCameraShakePattern : public UObject
{
public:
	DECLARE_CLASS(UCameraShakePattern, UObject)
	virtual ~UCameraShakePattern() = default;

	virtual float EvalTransitionX(float Alpha) const;
	virtual float EvalTransitionY(float Alpha) const;
	virtual float EvalTransitionZ(float Alpha) const;

	virtual float EvalRotationX(float Alpha) const;
	virtual float EvalRotationY(float Alpha) const;
	virtual float EvalRotationZ(float Alpha) const;
};

class USinWaveCameraShakePattern : public UCameraShakePattern
{
public:
	DECLARE_CLASS(USinWaveCameraShakePattern, UCameraShakePattern)

	float EvalTransitionX(float Alpha) const override;
	float EvalTransitionY(float Alpha) const override;
	float EvalTransitionZ(float Alpha) const override;

	float EvalRotationX(float Alpha) const override;
	float EvalRotationY(float Alpha) const override;
	float EvalRotationZ(float Alpha) const override;

public:
	float Frequency = 25.0f;
	float TransitionAmplitudeX = 0.0f;
	float TransitionAmplitudeY = 1.0f;
	float TransitionAmplitudeZ = 0.5f;
	float RotationAmplitudeX = 0.0f;
	float RotationAmplitudeY = 0.1f;
	float RotationAmplitudeZ = 0.2f;
};

class UCurveCameraShakePattern : public UCameraShakePattern
{
public:
	DECLARE_CLASS(UCurveCameraShakePattern, UCameraShakePattern)
	~UCurveCameraShakePattern() override;

	UCurveFloat* GetTransitionCurveX() const { return TransitionCurveX; }
	virtual void SetTransitionCurveX(UCurveFloat* InCurve) { TransitionCurveX = InCurve; }
	UCurveFloat* GetTransitionCurveY() const { return TransitionCurveY; }
	virtual void SetTransitionCurveY(UCurveFloat* InCurve) { TransitionCurveY = InCurve; }
	UCurveFloat* GetTransitionCurveZ() const { return TransitionCurveZ; }
	virtual void SetTransitionCurveZ(UCurveFloat* InCurve) { TransitionCurveZ = InCurve; }

	UCurveFloat* GetRotationCurveX() const { return RotationCurveX; }
	virtual void SetRotationCurveX(UCurveFloat* InCurve) { RotationCurveX = InCurve; }
	UCurveFloat* GetRotationCurveY() const { return RotationCurveY; }
	virtual void SetRotationCurveY(UCurveFloat* InCurve) { RotationCurveY = InCurve; }
	UCurveFloat* GetRotationCurveZ() const { return RotationCurveZ; }
	virtual void SetRotationCurveZ(UCurveFloat* InCurve) { RotationCurveZ = InCurve; }

	float EvalTransitionX(float Alpha) const override;
	float EvalTransitionY(float Alpha) const override;
	float EvalTransitionZ(float Alpha) const override;

	float EvalRotationX(float Alpha) const override;
	float EvalRotationY(float Alpha) const override;
	float EvalRotationZ(float Alpha) const override;

private:
	UCurveFloat* TransitionCurveX = nullptr;
	UCurveFloat* TransitionCurveY = nullptr;
	UCurveFloat* TransitionCurveZ = nullptr;
	UCurveFloat* RotationCurveX = nullptr;
	UCurveFloat* RotationCurveY = nullptr;
	UCurveFloat* RotationCurveZ = nullptr;
};
