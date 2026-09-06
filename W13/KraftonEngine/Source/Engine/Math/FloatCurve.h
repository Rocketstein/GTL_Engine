#pragma once

#include "Core/Types/CoreTypes.h"
#include "Object/Reflection/ObjectMacros.h"
#include "Object/Reflection/UStruct.h"

#include "Source/Engine/Math/FloatCurve.generated.h"

UENUM()
enum class ECurveInterpMode : uint8
{
	Constant,
	Linear,
	Cubic,
};

UENUM()
enum class ECurveExtrapMode : uint8
{
	Clamp,
	Linear,
	Loop,
};

UENUM()
enum class ECurveTangentMode : uint8
{
	Auto,
	User,
	Break,
};

USTRUCT()
struct FCurveKey
{
	GENERATED_BODY()

	UPROPERTY(Edit, Save, Category="Curve", DisplayName="Time")
	float Time = 0.0f;

	UPROPERTY(Edit, Save, Category="Curve", DisplayName="Value")
	float Value = 0.0f;

	UPROPERTY(Edit, Save, Category="Curve", DisplayName="Arrive Tangent")
	float ArriveTangent = 0.0f;

	UPROPERTY(Edit, Save, Category="Curve", DisplayName="Leave Tangent")
	float LeaveTangent = 0.0f;

	UPROPERTY(Edit, Save, Category="Curve", DisplayName="Interp Mode", Enum=ECurveInterpMode)
	ECurveInterpMode InterpMode = ECurveInterpMode::Linear;

	UPROPERTY(Edit, Save, Category="Curve", DisplayName="Tangent Mode", Enum=ECurveTangentMode)
	ECurveTangentMode TangentMode = ECurveTangentMode::Auto;
};

USTRUCT()
struct FFloatCurve
{
	GENERATED_BODY()

	UPROPERTY(Edit, Save, Category="Curve", DisplayName="Keys", Type=Array, Struct=FCurveKey)
	TArray<FCurveKey> Keys;

	UPROPERTY(Edit, Save, Category="Curve", DisplayName="Pre Extrap Mode", Enum=ECurveExtrapMode)
	ECurveExtrapMode PreExtrapMode = ECurveExtrapMode::Clamp;

	UPROPERTY(Edit, Save, Category="Curve", DisplayName="Post Extrap Mode", Enum=ECurveExtrapMode)
	ECurveExtrapMode PostExtrapMode = ECurveExtrapMode::Clamp;

	UPROPERTY(Edit, Save, Category="Curve", DisplayName="Default Value")
	float DefaultValue = 0.0f;

	bool IsEmpty() const;
	void Reset();

	void AddKey(float Time, float Value, ECurveInterpMode InterpMode = ECurveInterpMode::Linear);
	void SortKeys();
	void AutoSetTangents();

	float Evaluate(float Time) const;

private:
	int32 FindKeyIndexBefore(float Time) const;
	float EvaluateSegment(const FCurveKey& A, const FCurveKey& B, float Time) const;
};
