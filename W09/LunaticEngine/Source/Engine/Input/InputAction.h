#pragma once

#include "Core/CoreTypes.h"
#include "Math/Vector.h"
#include <cmath>
// InputAction store action information of name and type of input value
enum class EInputKey : int32
{
	MouseX = 256,
	MouseY = 257,
	MouseWheel = 258,
	MouseDragL_X = 259,
	MouseDragL_Y = 260,
	MouseDragR_X = 261,
	MouseDragR_Y = 262,
	MouseDragM_X = 263,
	MouseDragM_Y = 264,
};
// InputActionValue store input value of action and provide interface to get value in different type
enum class EInputActionValueType : uint32
{
	Bool,
	Float,
	Axis2D,
	Axis3D,
};

//Struct : Input action value
//Function : Store input value of action and provide interface to get value in different type
struct FInputActionValue
{
	EInputActionValueType ValueType = EInputActionValueType::Bool; // Input type
	FVector Value = { 0.0f, 0.0f, 0.0f }; //Input Value Container float, bool : x, 2D :x,y  3D x,y,z
	FInputActionValue() = default;

	// bool generator : true -> 1.f, flase -> 1.f
	// float generator : x-> fvalue , y ,z ignored
	// vector generator : x,y,z -> vector value

	explicit FInputActionValue(bool bValue)
		: ValueType(EInputActionValueType::Bool)
	{
		if (bValue)
			Value = { 1.0f, 1.0f, 1.0f };
		else
			Value = { 0.0f, 0.0f, 0.0f };
	}

	explicit FInputActionValue(float fValue)
		: ValueType(EInputActionValueType::Float)
	{
		Value = { fValue, 0.0f, 0.0f };
	}
	explicit FInputActionValue(const FVector& vValue)
		: ValueType(EInputActionValueType::Axis3D)
		, Value(vValue) {}

	bool IsNonZero() const
	{
		return std::fabsf(Value.X) > 1e-6f
			|| std::fabsf(Value.Y) > 1e-6f
			|| std::fabsf(Value.Z) > 1e-6f;
	}
	float Get() const { return Value.X; }
	FVector GetVector() const { return Value; }

	FInputActionValue operator - () const
	{
		FInputActionValue result;
		result.ValueType = ValueType;
		result.Value = { -Value.X, -Value.Y, -Value.Z };
		return result;
	}
	FInputActionValue operator*(const FVector& Scale) const
	{
		FInputActionValue result;
		result.ValueType = ValueType;
		result.Value = { Value.X * Scale.X, Value.Y * Scale.Y, Value.Z * Scale.Z };
		return result;
	}
	FInputActionValue operator+(const FInputActionValue& Other) const
	{
		FInputActionValue result;
		result.ValueType = ValueType;
		result.Value = { Value.X + Other.Value.X, Value.Y + Other.Value.Y, Value.Z + Other.Value.Z };
		return result;
	}
};
// Struct : Input action 
// Function : Store action information of name and type of input value
struct FInputAction
{
	FString ActionName;
	EInputActionValueType ValueType = EInputActionValueType::Float;

	FInputAction() = default;
	FInputAction(const FString& InName, EInputActionValueType InType)
		: ActionName(InName), ValueType(InType) {
	}
};
