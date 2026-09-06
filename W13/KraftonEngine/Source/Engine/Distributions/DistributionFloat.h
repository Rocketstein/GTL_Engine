#pragma once

#include "Distributions/Distribution.h"
#include "Math/FloatCurve.h"

#include "Source/Engine/Distributions/DistributionFloat.generated.h"

UCLASS()
class UDistributionFloat : public UDistribution
{
public:
	GENERATED_BODY()

	virtual float GetValue(float Time = 0.0f, UObject* Data = nullptr, FDistributionRandomStream* RandomStream = nullptr) const;
	virtual void GetValueRange(float Time, float& OutMin, float& OutMax) const;
	virtual void GetOutRange(float& OutMin, float& OutMax) const;
};

USTRUCT()
struct FRawDistributionFloat : public FRawDistribution
{
	GENERATED_BODY()

	UDistributionFloat* Distribution = nullptr;

	float GetValue(float Time = 0.0f, UObject* Data = nullptr, FDistributionRandomStream* RandomStream = nullptr) const;
	void GetOutRange(float& OutMin, float& OutMax) const;
	bool IsUniform() const;
	bool IsCreated() const { return Distribution != nullptr; }
	void Initialize();
	void SetDistribution(UDistributionFloat* InDistribution) { Distribution = InDistribution; ResetLookupTable(); }
};

UCLASS()
class UDistributionFloatConstant : public UDistributionFloat
{
public:
	GENERATED_BODY()

	float GetValue(float Time = 0.0f, UObject* Data = nullptr, FDistributionRandomStream* RandomStream = nullptr) const override;
	void GetValueRange(float Time, float& OutMin, float& OutMax) const override;
	void GetOutRange(float& OutMin, float& OutMax) const override;

	UPROPERTY(Edit, Save, Category="Distribution", DisplayName="Constant")
	float Constant = 0.0f;
};

UCLASS()
class UDistributionFloatUniform : public UDistributionFloat
{
public:
	GENERATED_BODY()

	bool IsUniform() const override { return true; }
	float GetValue(float Time = 0.0f, UObject* Data = nullptr, FDistributionRandomStream* RandomStream = nullptr) const override;
	void GetValueRange(float Time, float& OutMin, float& OutMax) const override;
	void GetOutRange(float& OutMin, float& OutMax) const override;

	UPROPERTY(Edit, Save, Category="Distribution", DisplayName="Min")
	float Min = 0.0f;

	UPROPERTY(Edit, Save, Category="Distribution", DisplayName="Max")
	float Max = 0.0f;

	UPROPERTY(Edit, Save, Category="Distribution", DisplayName="Use Extremes")
	bool bUseExtremes = false;
};

UCLASS()
class UDistributionFloatConstantCurve : public UDistributionFloat
{
public:
	GENERATED_BODY()

	float GetValue(float Time = 0.0f, UObject* Data = nullptr, FDistributionRandomStream* RandomStream = nullptr) const override;
	void GetValueRange(float Time, float& OutMin, float& OutMax) const override;
	void GetOutRange(float& OutMin, float& OutMax) const override;
	void GetTimeRange(float& OutMinTime, float& OutMaxTime) const override;

	UPROPERTY(Edit, Save, Category="Distribution", DisplayName="Constant Curve", Type=Struct)
	FFloatCurve ConstantCurve;
};

UCLASS()
class UDistributionFloatUniformCurve : public UDistributionFloat
{
public:
	GENERATED_BODY()

	bool IsUniform() const override { return true; }
	float GetValue(float Time = 0.0f, UObject* Data = nullptr, FDistributionRandomStream* RandomStream = nullptr) const override;
	void GetValueRange(float Time, float& OutMin, float& OutMax) const override;
	void GetOutRange(float& OutMin, float& OutMax) const override;
	void GetTimeRange(float& OutMinTime, float& OutMaxTime) const override;

	UPROPERTY(Edit, Save, Category="Distribution", DisplayName="Min Curve", Type=Struct)
	FFloatCurve MinCurve;

	UPROPERTY(Edit, Save, Category="Distribution", DisplayName="Max Curve", Type=Struct)
	FFloatCurve MaxCurve;

	UPROPERTY(Edit, Save, Category="Distribution", DisplayName="Use Extremes")
	bool bUseExtremes = false;
};

UCLASS()
class UDistributionFloatParameterBase : public UDistributionFloat
{
public:
	GENERATED_BODY()

	bool CanBeBaked() const override { return false; }
	float GetValue(float Time = 0.0f, UObject* Data = nullptr, FDistributionRandomStream* RandomStream = nullptr) const override;
	void GetValueRange(float Time, float& OutMin, float& OutMax) const override;
	void GetOutRange(float& OutMin, float& OutMax) const override;

	virtual bool GetParamValue(UObject* Data, FName ParamName, float& OutFloat) const;

	UPROPERTY(Edit, Save, Category="Parameter", DisplayName="Parameter Name")
	FName ParameterName;

	UPROPERTY(Edit, Save, Category="Parameter", DisplayName="Constant")
	float Constant = 0.0f;

	UPROPERTY(Edit, Save, Category="Parameter", DisplayName="Min Input")
	float MinInput = 0.0f;

	UPROPERTY(Edit, Save, Category="Parameter", DisplayName="Max Input")
	float MaxInput = 1.0f;

	UPROPERTY(Edit, Save, Category="Parameter", DisplayName="Min Output")
	float MinOutput = 0.0f;

	UPROPERTY(Edit, Save, Category="Parameter", DisplayName="Max Output")
	float MaxOutput = 1.0f;

	UPROPERTY(Edit, Save, Category="Parameter", DisplayName="Param Mode", Enum=EDistributionParamMode)
	EDistributionParamMode ParamMode = EDistributionParamMode::Normal;
};

UCLASS()
class UDistributionFloatParticleParameter : public UDistributionFloatParameterBase
{
public:
	GENERATED_BODY()

	bool GetParamValue(UObject* Data, FName ParamName, float& OutFloat) const override;
};
