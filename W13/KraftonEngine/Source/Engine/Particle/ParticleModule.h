#pragma once

#include "Object/Object.h"
#include "Object/Ptr/SoftObjectPtr.h"
#include "Object/FName.h"
#include "Distributions/DistributionFloat.h"
#include "Distributions/DistributionVector.h"
#include "Particle/ParticleTypes.h"
#include "Engine/Math/Transform.h"
#include "Source/Engine/Particle/ParticleModule.generated.h"

class FArchive;
struct FParticleEmitterInstance;

namespace ParticleDefaults
{
	constexpr const char* DefaultSpriteMaterialPath = "Content/Material/Editor/DefaultParticleSprite.mat";
}

UCLASS()
class UParticleModule : public UObject
{
public:
	GENERATED_BODY()
	UParticleModule() = default;
	~UParticleModule() override = default;

	virtual bool IsSpawnModule() const { return false; }
	virtual bool IsUpdateModule() const { return false; }
	virtual bool IsTypeDataModule() const { return false; }
	virtual EParticleModuleType GetModuleType() const { return EParticleModuleType::General; }

	virtual int32 GetParticlePayloadSize() const { return 0; }
	virtual int32 GetInstancePayloadSize() const { return 0; }

public:
	struct FContext
	{
		FParticleEmitterInstance& Owner;
		const FTransform& GetTransform() const;
		UObject* GetDistributionData() const;
		FString GetTemplateName() const;
		FString GetInstanceName() const;
		FContext(FParticleEmitterInstance& Ow) : Owner(Ow) {}
	};

public:
	

	/**
	 *	Called on a particle that is freshly spawned by the emitter.
	 *
	 *	@param	Owner		The FParticleEmitterInstance that spawned the particle.
	 *	@param	Offset		The modules offset into the data payload of the particle.
	 *	@param	SpawnTime	The time of the spawn.
	 */
	struct FSpawnContext : FContext
	{
		int32 Offset;
		float SpawnTime;
		FBaseParticle* ParticleBase;
		bool bLockInitialLocation = false;
		FSpawnContext(FParticleEmitterInstance& Ow, int32 Of, float St, FBaseParticle* Pb, bool bInLockInitialLocation = false)
			: FContext(Ow)
			, Offset(Of)
			, SpawnTime(St)
			, ParticleBase(Pb)
			, bLockInitialLocation(bInLockInitialLocation)
		{
		}
	};
	virtual void Spawn(const FSpawnContext& Context) {}

	/**
	 *	Called on a particle that is being updated by its emitter.
	 *
	 *	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
	 *	@param	Offset		The modules offset into the data payload of the particle.
	 *	@param	DeltaTime	The time since the last update.
	 */
	struct FUpdateContext : FContext
	{
		int32 Offset;
		float DeltaTime;
		FUpdateContext(FParticleEmitterInstance& Ow, int32 Of, float Dt)
			: FContext(Ow), Offset(Of), DeltaTime(Dt) {}
	};
	virtual void Update(const FUpdateContext& Context) {}


	bool IsEnabled() const { return bEnabled; }
	void SetEnabled(bool bInEnabled) { bEnabled = bInEnabled; }

	void Serialize(FArchive& Ar) override;

protected:
	UPROPERTY(Edit, Save, Category="Module", DisplayName="Enabled")
	bool bEnabled = true;
};

UCLASS()
class UParticleModuleTypeDataBase : public UParticleModule
{
public:
	GENERATED_BODY()
	bool IsTypeDataModule() const override { return true; }
	EParticleModuleType GetModuleType() const override { return EParticleModuleType::TypeData; }
	virtual EParticleEmitterType GetEmitterType() const { return EParticleEmitterType::Sprite; }
};

UCLASS()
class UParticleModuleTypeDataSprite : public UParticleModuleTypeDataBase
{
public:
	GENERATED_BODY()
	UParticleModuleTypeDataSprite();
	EParticleEmitterType GetEmitterType() const override { return EParticleEmitterType::Sprite; }

	// Deprecated storage for assets saved before Screen Alignment moved to Required.
	// Intentionally not editable, so the particle editor exposes only Required::ScreenAlignment.
	UPROPERTY(Save, Category="Sprite", DisplayName="Screen Alignment", Enum=EParticleScreenAlignment)
	EParticleScreenAlignment ScreenAlignment = EParticleScreenAlignment::PSA_FacingCameraPosition;

	UPROPERTY(Save, Category="Sprite", DisplayName="Use SubUV")
	bool bUseSubUV = false;

	UPROPERTY(Save, Category="Sprite", DisplayName="SubUV Resource", AssetType="SubUVResource")
	FName SubUVResourceName;

	UPROPERTY(Save, Category="Sprite", DisplayName="Sub Images X", Min=1.0f, Max=64.0f, Speed=1.0f)
	int32 SubImagesX = 1;

	UPROPERTY(Save, Category="Sprite", DisplayName="Sub Images Y", Min=1.0f, Max=64.0f, Speed=1.0f)
	int32 SubImagesY = 1;

	UPROPERTY(Save, Category="Sprite", DisplayName="SubUV Frame Rate (FPS)", Min=0.0f, Max=120.0f, Speed=1.0f)
	float SubUVFrameRate = 16.0f;

	UPROPERTY(Save, Category="Sprite", DisplayName="Loop SubUV")
	bool bLoopSubUV = false;

	bool ShouldExposeProperty(const FProperty& Property) const override;
};

UCLASS()
class UParticleModuleSubUVBase : public UParticleModule
{
public:
	GENERATED_BODY()
	EParticleModuleType GetModuleType() const override { return EParticleModuleType::SubUV; }
};

UCLASS()
class UParticleModuleSubUV : public UParticleModuleSubUVBase
{
public:
	GENERATED_BODY()
	UParticleModuleSubUV();

	bool IsSpawnModule() const override { return true; }
	bool IsUpdateModule() const override { return true; }
	int32 GetParticlePayloadSize() const override;
	void Spawn(const FSpawnContext& Context) override;
	void Update(const FUpdateContext& Context) override;

	float DetermineImageIndex(const FContext& Context, const FBaseParticle* Particle) const;

	UPROPERTY(Edit, Save, Category="SubUV", DisplayName="SubUV Resource", AssetType="SubUVResource")
	FName SubUVResourceName;

	UPROPERTY(Edit, Save, Category="SubUV", DisplayName="Sub Images X", Min=1.0f, Max=64.0f, Speed=1.0f)
	int32 SubImagesX = 1;

	UPROPERTY(Edit, Save, Category="SubUV", DisplayName="Sub Images Y", Min=1.0f, Max=64.0f, Speed=1.0f)
	int32 SubImagesY = 1;

	UPROPERTY(Edit, Save, Instanced, Category="SubUV", DisplayName="SubImage Index", Type=ObjectRef, AllowedClass=UDistributionFloat, Member=SubImageIndex.Distribution, CppType=UDistributionFloat*)
	;
	FRawDistributionFloat SubImageIndex;

	UPROPERTY(Edit, Save, Category="SubUV", DisplayName="Lock SubImage On Spawn")
	bool bLockSubImageOnSpawn = false;
};

UCLASS()
class UParticleModuleTypeDataMesh : public UParticleModuleTypeDataBase
{
public:
	GENERATED_BODY()
	UParticleModuleTypeDataMesh();
	EParticleEmitterType GetEmitterType() const override { return EParticleEmitterType::Mesh; }

	UPROPERTY(Edit, Save, Category="Mesh", DisplayName="Mesh", AssetType="StaticMesh")
	FSoftObjectPtr MeshPath = "None";
};

UCLASS()
class UParticleModuleTypeDataBeam : public UParticleModuleTypeDataBase
{
public:
	GENERATED_BODY()
	UParticleModuleTypeDataBeam();
	EParticleEmitterType GetEmitterType() const override { return EParticleEmitterType::Beam; }

	UPROPERTY(Edit, Save, Category="Beam", DisplayName="Max Beam Count", Min=1.0f, Max=1024.0f, Speed=1.0f)
	int32 MaxBeamCount = 1;

	UPROPERTY(Edit, Save, Category="Beam", DisplayName="Sheets", Min=1.0f, Max=16.0f, Speed=1.0f)
	int32 Sheets = 1;

	UPROPERTY(Edit, Save, Category="Beam", DisplayName="Interpolation Points", Min=1.0f, Max=64.0f, Speed=1.0f)
	int32 InterpolationPoints = 1;
};

UCLASS()
class UParticleModuleTypeDataRibbon : public UParticleModuleTypeDataBase
{
public:
	GENERATED_BODY()
	UParticleModuleTypeDataRibbon();
	EParticleEmitterType GetEmitterType() const override { return EParticleEmitterType::Ribbon; }

	UPROPERTY(Edit, Save, Category="Ribbon", DisplayName="Max Trail Count", Min=1.0f, Max=1024.0f, Speed=1.0f)
	int32 MaxTrailCount = 1;

	UPROPERTY(Edit, Save, Category="Ribbon", DisplayName="Max Particles In Trail", Min=2.0f, Max=4096.0f, Speed=1.0f)
	int32 MaxParticlesInTrail = 64;

	UPROPERTY(Edit, Save, Category="Ribbon", DisplayName="Sheets Per Trail", Min=1.0f, Max=16.0f, Speed=1.0f)
	int32 SheetsPerTrail = 1;

	UPROPERTY(Edit, Save, Category="Ribbon", DisplayName="Tiling Distance", Min=0.0f, Max=100000.0f, Speed=1.0f)
	float TilingDistance = 0.0f;

	UPROPERTY(Edit, Save, Category="Ribbon", DisplayName="Render Axis", Enum=EParticleTrailRenderAxis)
	EParticleTrailRenderAxis RenderAxis = EParticleTrailRenderAxis::CameraFacing;

	UPROPERTY(Edit, Save, Category="Ribbon", DisplayName="Spawn Initial Particle")
	bool bSpawnInitialParticle = true;
};

UCLASS()
class UParticleModuleBeamBase : public UParticleModule
{
public:
	GENERATED_BODY()
	EParticleModuleType GetModuleType() const override { return EParticleModuleType::Beam; }
};

UCLASS()
class UParticleModuleTrailBase : public UParticleModule
{
public:
	GENERATED_BODY()
	EParticleModuleType GetModuleType() const override { return EParticleModuleType::Trail; }
};

UCLASS()
class UParticleModuleTrailSource : public UParticleModuleTrailBase
{
public:
	GENERATED_BODY()

	UPROPERTY(Edit, Save, Category="Trail Source", DisplayName="Source Method", Enum=ETrail2SourceMethod)
	ETrail2SourceMethod SourceMethod = ETrail2SourceMethod::Default;

	UPROPERTY(Edit, Save, Category="Trail Source", DisplayName="Source Name")
	FName SourceName;

	UPROPERTY(Edit, Save, Category="Trail Source", DisplayName="Source Offset")
	FVector SourceOffset = FVector::ZeroVector;
};

UCLASS()
class UParticleModuleBeamSource : public UParticleModuleBeamBase
{
public:
	GENERATED_BODY()

	UPROPERTY(Edit, Save, Category="Beam Source", DisplayName="Source Method", Enum=EParticleBeamEndpointMethod)
	EParticleBeamEndpointMethod SourceMethod = EParticleBeamEndpointMethod::Emitter;

	UPROPERTY(Edit, Save, Category="Beam Source", DisplayName="Source Point")
	FVector SourcePoint = FVector(0.0f, 0.0f, 0.0f);

	UPROPERTY(Edit, Save, Category="Beam Source", DisplayName="Source Absolute")
	bool bSourceAbsolute = false;

	UPROPERTY(Edit, Save, Category="Beam Source", DisplayName="Use Source Tangent")
	bool bUseSourceTangent = false;

	UPROPERTY(Edit, Save, Category="Beam Source", DisplayName="Source Tangent")
	FVector SourceTangent = FVector(100.0f, 0.0f, 0.0f);
};

UCLASS()
class UParticleModuleBeamTarget : public UParticleModuleBeamBase
{
public:
	GENERATED_BODY()

	UPROPERTY(Edit, Save, Category="Beam Target", DisplayName="Target Method", Enum=EParticleBeamEndpointMethod)
	EParticleBeamEndpointMethod TargetMethod = EParticleBeamEndpointMethod::Particle;

	UPROPERTY(Edit, Save, Category="Beam Target", DisplayName="Target Point")
	FVector TargetPoint = FVector(0.0f, 0.0f, 100.0f);

	UPROPERTY(Edit, Save, Category="Beam Target", DisplayName="Target Absolute")
	bool bTargetAbsolute = false;

	UPROPERTY(Edit, Save, Category="Beam Target", DisplayName="Use Target Tangent")
	bool bUseTargetTangent = false;

	UPROPERTY(Edit, Save, Category="Beam Target", DisplayName="Target Tangent")
	FVector TargetTangent = FVector(100.0f, 0.0f, 0.0f);
};

UCLASS()
class UParticleModuleBeamNoise : public UParticleModuleBeamBase
{
public:
	GENERATED_BODY()
	bool IsSpawnModule() const override { return true; }
	bool IsUpdateModule() const override { return true; }
	int32 GetParticlePayloadSize() const override;
	void Spawn(const FSpawnContext& Context) override;
	void Update(const FUpdateContext& Context) override;

	UPROPERTY(Edit, Save, Category="Beam Noise", DisplayName="Frequency", Min=0.0f, Max=64.0f, Speed=1.0f)
	int32 Frequency = 0;

	UPROPERTY(Edit, Save, Category="Beam Noise", DisplayName="Frequency Low Range", Min=0.0f, Max=64.0f, Speed=1.0f)
	int32 FrequencyLowRange = 0;

	UPROPERTY(Edit, Save, Category="Beam Noise", DisplayName="Strength", Min=0.0f, Max=10000.0f, Speed=1.0f)
	float Strength = 0.0f;

	UPROPERTY(Edit, Save, Category="Beam Noise", DisplayName="Speed", Min=0.0f, Max=1000.0f, Speed=0.1f)
	float Speed = 0.0f;

	UPROPERTY(Edit, Save, Category="Beam Noise", DisplayName="Noise Lock Time", Min=0.0f, Max=1000.0f, Speed=0.1f)
	float NoiseLockTime = 0.0f;
};

UCLASS()
class UParticleModuleRequired : public UParticleModule
{
public:
	GENERATED_BODY()
	EParticleModuleType GetModuleType() const override { return EParticleModuleType::Required; }

	UPROPERTY(Edit, Save, Category="Required", DisplayName="Material", AssetType="Material")
	FSoftObjectPtr MaterialPath = ParticleDefaults::DefaultSpriteMaterialPath;

	UPROPERTY(Edit, Save, Category="Required", DisplayName="Max Particles", Min=1.0f, Max=100000.0f, Speed=10.0f)
	int32 MaxParticles = 1000;

	UPROPERTY(Edit, Save, Category="Required", DisplayName="Use Local Space")
	bool bUseLocalSpace = false;

	UPROPERTY(Edit, Save, Category="Required", DisplayName="Screen Alignment", Enum=EParticleScreenAlignment)
	EParticleScreenAlignment ScreenAlignment = EParticleScreenAlignment::PSA_FacingCameraPosition;

	UPROPERTY(Edit, Save, Category="Required", DisplayName="Sort Mode", Enum=EParticleSortMode)
	EParticleSortMode SortMode = EParticleSortMode::None;

	UPROPERTY(Edit, Save, Category="Required", DisplayName="Translucency Sort Priority", Min=-10000.0f, Max=10000.0f, Speed=1.0f)
	int32 TranslucencySortPriority = 0;

	UPROPERTY(Edit, Save, Category="Required", DisplayName="Blend Mode", Enum=EParticleBlendMode)
	EParticleBlendMode BlendMode = EParticleBlendMode::AlphaBlend;

	UPROPERTY(Edit, Save, Category="Required", DisplayName="Emitter Duration (sec)", Min=0.0f, Max=1000.0f, Speed=0.1f)
	float EmitterDuration = 1.0f;

	UPROPERTY(Edit, Save, Category="Required", DisplayName="Looping")
	bool bLooping = true;
};

UCLASS()
class UParticleModuleSpawn : public UParticleModule
{
public:
	GENERATED_BODY()
	UParticleModuleSpawn();
	bool IsSpawnModule() const override { return true; }
	EParticleModuleType GetModuleType() const override { return EParticleModuleType::Spawn; }

	UPROPERTY(Edit, Save, Instanced, Category="Spawn", DisplayName="Rate (per sec)", Type=ObjectRef, AllowedClass=UDistributionFloat, Member=Rate.Distribution, CppType=UDistributionFloat*)
	;
	FRawDistributionFloat Rate;

	UPROPERTY(Edit, Save, Instanced, Category="Spawn", DisplayName="Rate Scale", Type=ObjectRef, AllowedClass=UDistributionFloat, Member=RateScale.Distribution, CppType=UDistributionFloat*)
	;
	FRawDistributionFloat RateScale;

	UPROPERTY(Edit, Save, Instanced, Category="Spawn", DisplayName="Burst Count", Type=ObjectRef, AllowedClass=UDistributionFloat, Member=BurstCount.Distribution, CppType=UDistributionFloat*)
	;
	FRawDistributionFloat BurstCount;

	UPROPERTY(Edit, Save, Category="Spawn", DisplayName="Burst Time", Min=0.0f, Max=1000.0f, Speed=0.01f)
	float BurstTime = 0.0f;
};

UCLASS()
class UParticleModuleSpawnPerUnit : public UParticleModule
{
public:
	GENERATED_BODY()
	UParticleModuleSpawnPerUnit();
	bool IsUpdateModule() const override { return true; }
	EParticleModuleType GetModuleType() const override { return EParticleModuleType::Spawn; }
	int32 GetInstancePayloadSize() const override;
	void Update(const FUpdateContext& Context) override;

	UPROPERTY(Edit, Save, Category="Spawn Per Unit", DisplayName="Unit Scalar", Min=0.001f, Max=100000.0f, Speed=1.0f)
	float UnitScalar = 50.0f;

	UPROPERTY(Edit, Save, Instanced, Category="Spawn Per Unit", DisplayName="Spawn Per Unit", Type=ObjectRef, AllowedClass=UDistributionFloat, Member=SpawnPerUnit.Distribution, CppType=UDistributionFloat*)
	;
	FRawDistributionFloat SpawnPerUnit;

	UPROPERTY(Edit, Save, Category="Spawn Per Unit", DisplayName="Movement Tolerance", Min=0.0f, Max=10000.0f, Speed=0.1f)
	float MovementTolerance = 0.1f;

	UPROPERTY(Edit, Save, Category="Spawn Per Unit", DisplayName="Max Frame Distance", Min=0.0f, Max=100000.0f, Speed=1.0f)
	float MaxFrameDistance = 1000.0f;

	UPROPERTY(Edit, Save, Category="Spawn Per Unit", DisplayName="Ignore Movement Along Z")
	bool bIgnoreMovementAlongZ = false;
};

UCLASS()
class UParticleModuleLifetime : public UParticleModule
{
public:
	GENERATED_BODY()
	UParticleModuleLifetime();
	bool IsSpawnModule() const override { return true; }
	void Spawn(const FSpawnContext& Context) override;

	UPROPERTY(Edit, Save, Instanced, Category="Lifetime", DisplayName="Lifetime (sec)", Type=ObjectRef, AllowedClass=UDistributionFloat, Member=Lifetime.Distribution, CppType=UDistributionFloat*)
	;
	FRawDistributionFloat Lifetime;
};

UCLASS()
class UParticleModuleLocation : public UParticleModule
{
public:
	GENERATED_BODY()
	UParticleModuleLocation();
	bool IsSpawnModule() const override { return true; }
	void Spawn(const FSpawnContext& Context) override;

	UPROPERTY(Edit, Save, Instanced, Category="Location", DisplayName="Start Location", Type=ObjectRef, AllowedClass=UDistributionVector, Member=StartLocation.Distribution, CppType=UDistributionVector*)
	;
	FRawDistributionVector StartLocation;
};

UCLASS()
class UParticleModuleVelocity : public UParticleModule
{
public:
	GENERATED_BODY()
	UParticleModuleVelocity();
	bool IsSpawnModule() const override { return true; }
	void Spawn(const FSpawnContext& Context) override;

	UPROPERTY(Edit, Save, Instanced, Category="Velocity", DisplayName="Start Velocity", Type=ObjectRef, AllowedClass=UDistributionVector, Member=StartVelocity.Distribution, CppType=UDistributionVector*)
	;
	FRawDistributionVector StartVelocity;

	UPROPERTY(Edit, Save, Category="Velocity", DisplayName="Inherit Owner Velocity")
	bool bInheritOwnerVelocity = false;

	UPROPERTY(Edit, Save, Category="Velocity", DisplayName="Inherit Velocity Scale", Min=0.0f, Max=10.0f, Speed=0.1f)
	float InheritVelocityScale = 1.0f;
};

UCLASS()
class UParticleModuleAccelerationBase : public UParticleModule
{
public:
	GENERATED_BODY()
	bool IsUpdateModule() const override { return true; }
};

UCLASS()
class UParticleModuleAccelerationConstant : public UParticleModuleAccelerationBase
{
public:
	GENERATED_BODY()
	void Update(const FUpdateContext& Context) override;

	UPROPERTY(Edit, Save, Category="Acceleration", DisplayName="Acceleration")
	FVector Acceleration = FVector(0.0f, 0.0f, -9.8f);
};

UCLASS()
class UParticleModuleDrag : public UParticleModule
{
public:
	GENERATED_BODY()
	UParticleModuleDrag();
	bool IsUpdateModule() const override { return true; }
	void Update(const FUpdateContext& Context) override;

	UPROPERTY(Edit, Save, Instanced, Category="Drag", DisplayName="Drag Coefficient", Type=ObjectRef, AllowedClass=UDistributionFloat, Member=DragCoefficient.Distribution, CppType=UDistributionFloat*)
	;
	FRawDistributionFloat DragCoefficient;
};

UCLASS()
class UParticleModulePointAttractor : public UParticleModule
{
public:
	GENERATED_BODY()
	UParticleModulePointAttractor();
	bool IsUpdateModule() const override { return true; }
	void Update(const FUpdateContext& Context) override;

	UPROPERTY(Edit, Save, Category="Point Attractor", DisplayName="Attractor Position", Type=Vec3)
	FVector AttractorPosition = FVector::ZeroVector;

	UPROPERTY(Edit, Save, Instanced, Category="Point Attractor", DisplayName="Strength", Type=ObjectRef, AllowedClass=UDistributionFloat, Member=Strength.Distribution, CppType=UDistributionFloat*)
	;
	FRawDistributionFloat Strength;
};

UCLASS()
class UParticleModuleColor : public UParticleModule
{
public:
	GENERATED_BODY()
	UParticleModuleColor();
	bool IsSpawnModule() const override { return true; }
	bool IsUpdateModule() const override { return bColorOverLife; }
	void Serialize(FArchive& Ar) override;
	void Spawn(const FSpawnContext& Context) override;
	void Update(const FUpdateContext& Context) override;

	UPROPERTY(Edit, Save, Instanced, Category="Color", DisplayName="Start Color", Type=ObjectRef, AllowedClass=UDistributionVector, Member=StartColor.Distribution, CppType=UDistributionVector*)
	;
	FRawDistributionVector StartColor;

	UPROPERTY(Edit, Save, Instanced, Category="Color", DisplayName="Start Alpha", Type=ObjectRef, AllowedClass=UDistributionFloat, Member=StartAlpha.Distribution, CppType=UDistributionFloat*)
	;
	FRawDistributionFloat StartAlpha;

	UPROPERTY(Edit, Save, Instanced, Category="Color", DisplayName="End Color", Type=ObjectRef, AllowedClass=UDistributionVector, Member=EndColor.Distribution, CppType=UDistributionVector*)
	;
	FRawDistributionVector EndColor;

	UPROPERTY(Edit, Save, Instanced, Category="Color", DisplayName="End Alpha", Type=ObjectRef, AllowedClass=UDistributionFloat, Member=EndAlpha.Distribution, CppType=UDistributionFloat*)
	;
	FRawDistributionFloat EndAlpha;

	// Saved manually by Serialize so older binary particle assets keep their original layout.
	UPROPERTY(Edit, Category="Color", DisplayName="Emissive Intensity", Min=0.0f, Max=50.0f, Speed=0.05f)
	float EmissiveIntensity = 1.0f;

	UPROPERTY(Edit, Save, Category="Color", DisplayName="Color Over Life")
	bool bColorOverLife = true;
};

UCLASS()
class UParticleModuleSize : public UParticleModule
{
public:
	GENERATED_BODY()
	UParticleModuleSize();
	bool IsSpawnModule() const override { return true; }
	bool IsUpdateModule() const override { return bSizeOverLife; }
	void Spawn(const FSpawnContext& Context) override;
	void Update(const FUpdateContext& Context) override;

	UPROPERTY(Edit, Save, Instanced, Category="Size", DisplayName="Start Size", Type=ObjectRef, AllowedClass=UDistributionVector, Member=StartSize.Distribution, CppType=UDistributionVector*)
	;
	FRawDistributionVector StartSize;

	UPROPERTY(Edit, Save, Instanced, Category="Size", DisplayName="End Size", Type=ObjectRef, AllowedClass=UDistributionVector, Member=EndSize.Distribution, CppType=UDistributionVector*)
	;
	FRawDistributionVector EndSize;

	UPROPERTY(Edit, Save, Category="Size", DisplayName="Size Over Life")
	bool bSizeOverLife = false;
};
