#pragma once

#include "Distributions/Distribution.h"
#include "Math/FloatCurve.h"

#include "Source/Engine/Distributions/DistributionVector.generated.h"

UENUM()
enum class EDistributionVectorLockFlags : uint8
{
	None,
	XY,
	XZ,
	YZ,
	XYZ,
};

UENUM()
enum class EDistributionVectorMirrorFlags : uint8
{
	Same,
	Different,
	Mirror,
};

UCLASS()
class UDistributionVector : public UDistribution
{
public:
	GENERATED_BODY()

	virtual FVector GetValue(float Time = 0.0f, UObject* Data = nullptr, FDistributionRandomStream* RandomStream = nullptr) const;
	virtual void GetValueRange(float Time, FVector& OutMin, FVector& OutMax) const;
	virtual void GetOutRange(FVector& OutMin, FVector& OutMax) const;
};

USTRUCT()
struct FRawDistributionVector : public FRawDistribution
{
	GENERATED_BODY()

	UDistributionVector* Distribution = nullptr;

	FVector GetValue(float Time = 0.0f, UObject* Data = nullptr, FDistributionRandomStream* RandomStream = nullptr) const;
	void GetOutRange(FVector& OutMin, FVector& OutMax) const;
	bool IsUniform() const;
	bool IsCreated() const { return Distribution != nullptr; }
	void Initialize();
	void SetDistribution(UDistributionVector* InDistribution) { Distribution = InDistribution; ResetLookupTable(); }
};

UCLASS()
class UDistributionVectorConstant : public UDistributionVector
{
public:
	GENERATED_BODY()

	FVector GetValue(float Time = 0.0f, UObject* Data = nullptr, FDistributionRandomStream* RandomStream = nullptr) const override;
	void GetValueRange(float Time, FVector& OutMin, FVector& OutMax) const override;
	void GetOutRange(FVector& OutMin, FVector& OutMax) const override;

	UPROPERTY(Edit, Save, Category="Distribution", DisplayName="Constant")
	FVector Constant = FVector::ZeroVector;
};

UCLASS()
class UDistributionVectorUniform : public UDistributionVector
{
public:
	GENERATED_BODY()

	bool IsUniform() const override { return true; }
	FVector GetValue(float Time = 0.0f, UObject* Data = nullptr, FDistributionRandomStream* RandomStream = nullptr) const override;
	void GetValueRange(float Time, FVector& OutMin, FVector& OutMax) const override;
	void GetOutRange(FVector& OutMin, FVector& OutMax) const override;

	UPROPERTY(Edit, Save, Category="Distribution", DisplayName="Min")
	FVector Min = FVector::ZeroVector;

	UPROPERTY(Edit, Save, Category="Distribution", DisplayName="Max")
	FVector Max = FVector::ZeroVector;

	UPROPERTY(Edit, Save, Category="Distribution", DisplayName="Use Extremes")
	bool bUseExtremes = false;

	UPROPERTY(Edit, Save, Category="Distribution", DisplayName="Locked Axes", Enum=EDistributionVectorLockFlags)
	EDistributionVectorLockFlags LockedAxes = EDistributionVectorLockFlags::None;

	UPROPERTY(Edit, Save, Category="Distribution", DisplayName="Mirror X", Enum=EDistributionVectorMirrorFlags)
	EDistributionVectorMirrorFlags MirrorX = EDistributionVectorMirrorFlags::Same;

	UPROPERTY(Edit, Save, Category="Distribution", DisplayName="Mirror Y", Enum=EDistributionVectorMirrorFlags)
	EDistributionVectorMirrorFlags MirrorY = EDistributionVectorMirrorFlags::Same;

	UPROPERTY(Edit, Save, Category="Distribution", DisplayName="Mirror Z", Enum=EDistributionVectorMirrorFlags)
	EDistributionVectorMirrorFlags MirrorZ = EDistributionVectorMirrorFlags::Same;
};

UCLASS()
class UDistributionVectorConstantCurve : public UDistributionVector
{
public:
	GENERATED_BODY()

	FVector GetValue(float Time = 0.0f, UObject* Data = nullptr, FDistributionRandomStream* RandomStream = nullptr) const override;
	void GetValueRange(float Time, FVector& OutMin, FVector& OutMax) const override;
	void GetOutRange(FVector& OutMin, FVector& OutMax) const override;
	void GetTimeRange(float& OutMinTime, float& OutMaxTime) const override;

	UPROPERTY(Edit, Save, Category="Distribution", DisplayName="X Curve", Type=Struct)
	FFloatCurve XCurve;

	UPROPERTY(Edit, Save, Category="Distribution", DisplayName="Y Curve", Type=Struct)
	FFloatCurve YCurve;

	UPROPERTY(Edit, Save, Category="Distribution", DisplayName="Z Curve", Type=Struct)
	FFloatCurve ZCurve;
};

UCLASS()
class UDistributionVectorUniformCurve : public UDistributionVector
{
public:
	GENERATED_BODY()

	bool IsUniform() const override { return true; }
	FVector GetValue(float Time = 0.0f, UObject* Data = nullptr, FDistributionRandomStream* RandomStream = nullptr) const override;
	void GetValueRange(float Time, FVector& OutMin, FVector& OutMax) const override;
	void GetOutRange(FVector& OutMin, FVector& OutMax) const override;
	void GetTimeRange(float& OutMinTime, float& OutMaxTime) const override;

	UPROPERTY(Edit, Save, Category="Distribution", DisplayName="Min X Curve", Type=Struct)
	FFloatCurve MinXCurve;

	UPROPERTY(Edit, Save, Category="Distribution", DisplayName="Min Y Curve", Type=Struct)
	FFloatCurve MinYCurve;

	UPROPERTY(Edit, Save, Category="Distribution", DisplayName="Min Z Curve", Type=Struct)
	FFloatCurve MinZCurve;

	UPROPERTY(Edit, Save, Category="Distribution", DisplayName="Max X Curve", Type=Struct)
	FFloatCurve MaxXCurve;

	UPROPERTY(Edit, Save, Category="Distribution", DisplayName="Max Y Curve", Type=Struct)
	FFloatCurve MaxYCurve;

	UPROPERTY(Edit, Save, Category="Distribution", DisplayName="Max Z Curve", Type=Struct)
	FFloatCurve MaxZCurve;

	UPROPERTY(Edit, Save, Category="Distribution", DisplayName="Use Extremes")
	bool bUseExtremes = false;

	UPROPERTY(Edit, Save, Category="Distribution", DisplayName="Locked Axes", Enum=EDistributionVectorLockFlags)
	EDistributionVectorLockFlags LockedAxes = EDistributionVectorLockFlags::None;
};

UCLASS()
class UDistributionVectorParameterBase : public UDistributionVector
{
public:
	GENERATED_BODY()

	bool CanBeBaked() const override { return false; }
	FVector GetValue(float Time = 0.0f, UObject* Data = nullptr, FDistributionRandomStream* RandomStream = nullptr) const override;
	void GetValueRange(float Time, FVector& OutMin, FVector& OutMax) const override;
	void GetOutRange(FVector& OutMin, FVector& OutMax) const override;

	virtual bool GetParamValue(UObject* Data, FName ParamName, FVector& OutVector) const;

	UPROPERTY(Edit, Save, Category="Parameter", DisplayName="Parameter Name")
	FName ParameterName;

	UPROPERTY(Edit, Save, Category="Parameter", DisplayName="Constant")
	FVector Constant = FVector::ZeroVector;

	UPROPERTY(Edit, Save, Category="Parameter", DisplayName="Min Input")
	FVector MinInput = FVector(0.0f, 0.0f, 0.0f);

	UPROPERTY(Edit, Save, Category="Parameter", DisplayName="Max Input")
	FVector MaxInput = FVector(1.0f, 1.0f, 1.0f);

	UPROPERTY(Edit, Save, Category="Parameter", DisplayName="Min Output")
	FVector MinOutput = FVector::ZeroVector;

	UPROPERTY(Edit, Save, Category="Parameter", DisplayName="Max Output")
	FVector MaxOutput = FVector::OneVector;

	UPROPERTY(Edit, Save, Category="Parameter", DisplayName="Param Mode", Enum=EDistributionParamMode)
	EDistributionParamMode ParamMode = EDistributionParamMode::Normal;
};

UCLASS()
class UDistributionVectorParticleParameter : public UDistributionVectorParameterBase
{
public:
	GENERATED_BODY()

	bool GetParamValue(UObject* Data, FName ParamName, FVector& OutVector) const override;
};
