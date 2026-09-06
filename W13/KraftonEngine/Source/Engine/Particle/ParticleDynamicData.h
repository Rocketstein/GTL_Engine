#pragma once

#include "Math/MathUtils.h"
#include "Particle/ParticleTypes.h"

class UParticleModuleRequired;

enum class EDynamicEmitterType : uint8
{
	Unknown,
	Sprite,
	Mesh,
	Beam,
	Ribbon,
};

// 데이터 배치도 [ParticleData][ParticleIndices]
struct FParticleDataContainer
{
	int32 MemBlockSize = 0;
	int32 ParticleDataNumBytes = 0;
	int32 ParticleIndicesNumShorts = 0;
	TArray<uint8> MemBlock;
	uint8* ParticleData = nullptr;
	uint16* ParticleIndices = nullptr;

	void Alloc(int32 InParticleDataNumBytes, int32 InParticleIndicesNumShorts);
	void Free();
	void Reset()
	{
		Free();
	}
	void CopyFrom(const uint8* InParticleData, int32 InParticleDataNumBytes, const uint16* InParticleIndices, int32 InParticleIndicesNumShorts);

	int32 GetParticleDataNumBytes() const { return ParticleDataNumBytes; }
	int32 GetParticleIndicesNumShorts() const { return ParticleIndicesNumShorts; }
};

struct FParticleSpriteVertex
{
	FVector Location = FVector::ZeroVector;
	FVector Size = FVector::OneVector;
	FLinearColor Color = FLinearColor::White();
	float Rotation = 0.0f;
};

struct FMeshParticleInstanceVertex
{
	FVector Location = FVector::ZeroVector;
	FVector Size = FVector::OneVector;
	FLinearColor Color = FLinearColor::White();
	float Rotation = 0.0f;
};

struct FDynamicEmitterReplayDataBase
{
	virtual ~FDynamicEmitterReplayDataBase() = default;

	EDynamicEmitterType EmitterType = EDynamicEmitterType::Unknown;
	int32 ActiveParticleCount = 0;
	int32 ParticleStride = sizeof(FBaseParticle);
	float EmitterTime = 0.0f;
	FVector Scale = FVector::OneVector;
	EParticleSortMode SortMode = EParticleSortMode::None;
	int32 TranslucencySortPriority = 0;
	bool bUseLocalSpace = false;
	FParticleDataContainer DataContainer;
};

struct FDynamicSpriteEmitterReplayDataBase : public FDynamicEmitterReplayDataBase
{
	explicit FDynamicSpriteEmitterReplayDataBase(const UParticleModuleRequired* RequiredModule = nullptr);

	const UParticleModuleRequired* RequiredModule = nullptr;
	FString MaterialPath;
	FString MeshPath;
	EParticleBlendMode BlendMode = EParticleBlendMode::AlphaBlend;
	EParticleScreenAlignment ScreenAlignment = EParticleScreenAlignment::PSA_FacingCameraPosition;
	bool bUseSubUV = false;
	FString SubUVResourceName;
	int32 SubImagesX = 1;
	int32 SubImagesY = 1;
	float SubUVFrameRate = 16.0f;
	bool bLoopSubUV = false;
	int32 SubUVPayloadOffset = -1;
};

struct FDynamicBeamEmitterReplayData : public FDynamicSpriteEmitterReplayDataBase
{
	explicit FDynamicBeamEmitterReplayData(const UParticleModuleRequired* RequiredModule = nullptr);

	int32 MaxBeamCount = 1;
	int32 Sheets = 1;
	int32 InterpolationPoints = 1;
	EParticleBeamEndpointMethod SourceMethod = EParticleBeamEndpointMethod::Emitter;
	EParticleBeamEndpointMethod TargetMethod = EParticleBeamEndpointMethod::Particle;
	FVector SourcePoint = FVector::ZeroVector;
	FVector TargetPoint = FVector(0.0f, 0.0f, 100.0f);
	FVector SourceTangent = FVector(100.0f, 0.0f, 0.0f);
	FVector TargetTangent = FVector(100.0f, 0.0f, 0.0f);
	bool bUseSourceTangent = false;
	bool bUseTargetTangent = false;
	int32 NoiseFrequency = 0;
	float NoiseStrength = 0.0f;
	float NoiseSpeed = 0.0f;
	int32 NoisePayloadOffset = -1;
	int32 NoisePayloadSize = 0;
};

struct FDynamicTrailsEmitterReplayData : public FDynamicSpriteEmitterReplayDataBase
{
	explicit FDynamicTrailsEmitterReplayData(const UParticleModuleRequired* RequiredModule = nullptr);

	int32 TrailCount = 1;
	int32 Sheets = 1;
	int32 MaxActiveParticleCount = 0;
};

struct FDynamicRibbonEmitterReplayData : public FDynamicTrailsEmitterReplayData
{
	explicit FDynamicRibbonEmitterReplayData(const UParticleModuleRequired* RequiredModule = nullptr);

	int32 MaxParticlesInTrail = 64;
	int32 RibbonPayloadOffset = -1;
	float TilingDistance = 0.0f;
	EParticleTrailRenderAxis RenderAxis = EParticleTrailRenderAxis::CameraFacing;
};

struct FDynamicEmitterDataBase
{
	virtual ~FDynamicEmitterDataBase() = default;

	int32 EmitterIndex = -1;

	virtual const FDynamicEmitterReplayDataBase& GetSource() const = 0;
	virtual FDynamicEmitterReplayDataBase& GetSource() = 0;
};

struct FDynamicSpriteEmitterDataBase : public FDynamicEmitterDataBase
{
	explicit FDynamicSpriteEmitterDataBase(const UParticleModuleRequired* RequiredModule = nullptr);

	const FDynamicEmitterReplayDataBase& GetSource() const override { return Source; }
	FDynamicEmitterReplayDataBase& GetSource() override { return Source; }
	virtual const FDynamicSpriteEmitterReplayDataBase& GetSpriteSource() const { return Source; }
	virtual FDynamicSpriteEmitterReplayDataBase& GetSpriteSource() { return Source; }

	virtual int32 GetDynamicVertexStride() const = 0;

protected:
	FDynamicSpriteEmitterReplayDataBase Source;
};

struct FDynamicSpriteEmitterData : public FDynamicSpriteEmitterDataBase
{
	explicit FDynamicSpriteEmitterData(const UParticleModuleRequired* RequiredModule = nullptr);
	int32 GetDynamicVertexStride() const override { return sizeof(FParticleSpriteVertex); }
};

struct FDynamicMeshEmitterData : public FDynamicSpriteEmitterData
{
	explicit FDynamicMeshEmitterData(const UParticleModuleRequired* RequiredModule = nullptr);
	int32 GetDynamicVertexStride() const override { return sizeof(FMeshParticleInstanceVertex); }
};

struct FDynamicBeamEmitterData : public FDynamicSpriteEmitterDataBase
{
	explicit FDynamicBeamEmitterData(const UParticleModuleRequired* RequiredModule = nullptr);

	const FDynamicEmitterReplayDataBase& GetSource() const override { return Source; }
	FDynamicEmitterReplayDataBase& GetSource() override { return Source; }
	const FDynamicSpriteEmitterReplayDataBase& GetSpriteSource() const override { return Source; }
	FDynamicSpriteEmitterReplayDataBase& GetSpriteSource() override { return Source; }
	int32 GetDynamicVertexStride() const override { return sizeof(FParticleSpriteVertex); }
	const FDynamicBeamEmitterReplayData& GetBeamSource() const { return Source; }
	FDynamicBeamEmitterReplayData& GetBeamSource() { return Source; }

	FDynamicBeamEmitterReplayData Source;
};

struct FDynamicTrailsEmitterData : public FDynamicSpriteEmitterDataBase
{
	explicit FDynamicTrailsEmitterData(const UParticleModuleRequired* RequiredModule = nullptr);

	int32 GetDynamicVertexStride() const override { return sizeof(FParticleSpriteVertex); }
	virtual const FDynamicTrailsEmitterReplayData& GetTrailsSource() const = 0;
	virtual FDynamicTrailsEmitterReplayData& GetTrailsSource() = 0;
};

struct FDynamicRibbonEmitterData : public FDynamicTrailsEmitterData
{
	explicit FDynamicRibbonEmitterData(const UParticleModuleRequired* RequiredModule = nullptr);

	const FDynamicEmitterReplayDataBase& GetSource() const override { return Source; }
	FDynamicEmitterReplayDataBase& GetSource() override { return Source; }
	const FDynamicSpriteEmitterReplayDataBase& GetSpriteSource() const override { return Source; }
	FDynamicSpriteEmitterReplayDataBase& GetSpriteSource() override { return Source; }
	const FDynamicTrailsEmitterReplayData& GetTrailsSource() const override { return Source; }
	FDynamicTrailsEmitterReplayData& GetTrailsSource() override { return Source; }
	const FDynamicRibbonEmitterReplayData& GetRibbonSource() const { return Source; }
	FDynamicRibbonEmitterReplayData& GetRibbonSource() { return Source; }

	FDynamicRibbonEmitterReplayData Source;
};
