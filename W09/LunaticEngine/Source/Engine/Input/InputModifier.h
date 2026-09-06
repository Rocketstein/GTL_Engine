#pragma once
#include "Input/InputAction.h"
#include <cmath>

// Class : Input Modifier 
// Modify Raw Input Value before Trigger Check and Callback
class FInputModifier
{
public:
	virtual ~FInputModifier() = default;
	virtual FInputActionValue ModifyRaw(const FInputActionValue& Value) = 0;
};

// Class : Modifier Negative
// Negative Input Value x, y, z axis can be set independently
class FModifierNegative : public FInputModifier
{
public:
	bool bX = true, bY = true, bZ = true;

	FModifierNegative() = default;
	FModifierNegative(bool InX, bool InY, bool InZ) : bX(InX), bY(InY), bZ(InZ) {}

	FInputActionValue ModifyRaw(const FInputActionValue& V) override
	{
		return V * FVector{ bX ? -1.0f : 1.0f, bY ? -1.0f : 1.0f, bZ ? -1.0f : 1.0f };
	}

};

//Class : Modifier Scale
//Fuction : Scale Input Value xyz axis can be set independently
class FModifierScale : public FInputModifier
{
public:
	FVector Scale = { 1.0f, 1.0f, 1.0f };
	FModifierScale() = default;
	explicit FModifierScale(const FVector& InScale) : Scale(InScale) {}
	FInputActionValue ModifyRaw(const FInputActionValue& V) override
	{
		return V * Scale;
	}
};
// Class : Modifier Deadzone
// Fuction : If Magnitude of Input Value is less than Lowerthreshold return 0
// else Normalize to range between Lowerthreshold and Uppertyreshold
class FModifierDeadzone : public FInputModifier
{
public:
	float LowerThreshold = 0.2f;
	float UpperThreshold = 1.0f;

	FModifierDeadzone() = default;
	FModifierDeadzone(float InLower, float InUpper) : LowerThreshold(InLower), UpperThreshold(InUpper) {}

	FInputActionValue ModifyRaw(const FInputActionValue& V) override
	{
		float Magnitude = std::fabsf(V.Get());
		if (Magnitude < LowerThreshold)
			return FInputActionValue(0.0f);// DeadZone input ignore
		float Range = UpperThreshold - LowerThreshold;
		if (Range <= 0.0f)// Setting value error 
			return V; //prevent devide 0
		float Normalized = (Magnitude - LowerThreshold) / Range;
		if (Normalized > 1.0f) //limit value
		Normalized = 1.0f;
		float Sign = V.Get() >= 0.0f ? 1.0f : -1.0f; // Dirction recovery

		return FInputActionValue(Sign * Normalized);
	}
};


// will be test
// Class :Modifier Swizzle Axis
// Fuction : Swizzle Axis Order of Input Value
class FModifierSwizzleAxis : public FInputModifier
{
public:
	enum class ESwizzleOrder { YXZ, ZYX, XZY, YZX, ZXY };
	ESwizzleOrder Order = ESwizzleOrder::YXZ;

	FModifierSwizzleAxis() = default;
	explicit FModifierSwizzleAxis(ESwizzleOrder InOrder) : Order(InOrder) {}

	FInputActionValue ModifyRaw(const FInputActionValue& V) override
	{
		FVector In = V.GetVector();
		FVector Out;
		switch (Order)
		{
		case ESwizzleOrder::YXZ: Out = { In.Y, In.X, In.Z }; break;
		case ESwizzleOrder::ZYX: Out = { In.Z, In.Y, In.X }; break;
		case ESwizzleOrder::XZY: Out = { In.X, In.Z, In.Y }; break;
		case ESwizzleOrder::YZX: Out = { In.Y, In.Z, In.X }; break;
		case ESwizzleOrder::ZXY: Out = { In.Z, In.X, In.Y }; break;
		}
		return FInputActionValue(Out);
	}
};