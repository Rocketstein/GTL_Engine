#include "Particle/ParticleModuleVectorField.h"

#include "Component/Primitive/ParticleSystemComponent.h"
#include "GameFramework/World.h"
#include "Math/Matrix.h"
#include "Particle/ParticleHelper.h"
#include "Particle/ParticleLODLevel.h"
#include "Particle/VectorField/VectorFieldAsset.h"
#include "Particle/VectorField/VectorFieldManager.h"
#include "Render/Scene/FScene.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace
{
	constexpr float VectorFieldBoundsScaleEpsilon = 1.0e-4f;

	float MakeSafeScaleComponent(float Value)
	{
		return std::max(std::abs(Value), VectorFieldBoundsScaleEpsilon);
	}

	float ComputeGridSampleCoordinate(float Min, float Max, int32 Index, int32 Size)
	{
		if (Size <= 1)
		{
			return (Min + Max) * 0.5f;
		}

		const float Alpha = static_cast<float>(Index) / static_cast<float>(Size - 1);
		return Min + (Max - Min) * Alpha;
	}
}

FQuat UParticleModuleVectorFieldRotation::GetRotation(float EmitterTime) const
{
	const FRotator EffectiveRotation = Rotation + RotationRate * EmitterTime;
	return EffectiveRotation.ToQuaternion().GetNormalized();
}

UVectorFieldAsset* UParticleModuleVectorFieldLocal::ResolveVectorField()
{
	const FString& Path = VectorFieldPath.ToString();
	if (VectorFieldPath.IsNull())
	{
		CachedVectorFieldPath.clear();
		CachedVectorField = nullptr;
		VectorFieldPath.SetCachedObject(nullptr);
		return nullptr;
	}

	if (CachedVectorField && CachedVectorFieldPath == Path)
	{
		return CachedVectorField;
	}

	CachedVectorFieldPath = Path;
	CachedVectorField = FVectorFieldManager::Get().Load(Path);
	VectorFieldPath.SetCachedObject(CachedVectorField);
	return CachedVectorField;
}

FVector UParticleModuleVectorFieldLocal::GetSafeFieldBoundsScale() const
{
	return FVector(
		MakeSafeScaleComponent(FieldBoundsScale.X),
		MakeSafeScaleComponent(FieldBoundsScale.Y),
		MakeSafeScaleComponent(FieldBoundsScale.Z));
}

FQuat UParticleModuleVectorFieldLocal::ResolveFieldRotation(const UParticleLODLevel* LODLevel, float EmitterTime) const
{
	if (!LODLevel)
	{
		return FQuat::Identity;
	}

	FQuat FieldRotation = FQuat::Identity;
	for (UParticleModule* Module : LODLevel->GetModules())
	{
		if (!Module || !Module->IsEnabled())
		{
			continue;
		}

		if (const UParticleModuleVectorFieldRotation* RotationModule = Cast<UParticleModuleVectorFieldRotation>(Module))
		{
			FieldRotation *= RotationModule->GetRotation(EmitterTime);
		}
	}
	return FieldRotation.GetNormalized();
}

FVector UParticleModuleVectorFieldLocal::TransformComponentLocalToFieldLocal(const FVector& ComponentLocalPosition, const FQuat& FieldRotation) const
{
	const FVector SafeScale = GetSafeFieldBoundsScale();
	const FVector Relative = FieldRotation.Inverse().RotateVector(ComponentLocalPosition - FieldBoundsOffset);
	return FVector(
		Relative.X / SafeScale.X,
		Relative.Y / SafeScale.Y,
		Relative.Z / SafeScale.Z);
}

FVector UParticleModuleVectorFieldLocal::TransformFieldLocalToComponentLocal(const FVector& FieldLocalPosition, const FQuat& FieldRotation) const
{
	const FVector SafeScale = GetSafeFieldBoundsScale();
	const FVector Scaled = FVector(
		FieldLocalPosition.X * SafeScale.X,
		FieldLocalPosition.Y * SafeScale.Y,
		FieldLocalPosition.Z * SafeScale.Z);
	return FieldRotation.RotateVector(Scaled) + FieldBoundsOffset;
}

FVector UParticleModuleVectorFieldLocal::TransformFieldVectorToComponentLocal(const FVector& FieldVector, const FQuat& FieldRotation) const
{
	return FieldRotation.RotateVector(FieldVector);
}

bool UParticleModuleVectorFieldLocal::ShouldExposeProperty(const FProperty& Property) const
{
	if (Property.Name
		&& (std::strcmp(Property.Name, "bDrawBounds") == 0
			|| std::strcmp(Property.Name, "bDrawVectors") == 0))
	{
		return false;
	}

	return UParticleModule::ShouldExposeProperty(Property);
}

void UParticleModuleVectorFieldLocal::AppendFieldDebugLines(FScene& Scene, const FMatrix& ComponentToWorld, const UParticleLODLevel* LODLevel, float EmitterTime)
{
	UVectorFieldAsset* VectorField = ResolveVectorField();
	if (!VectorField || !VectorField->IsValidGrid())
	{
		return;
	}

	const FVector& Min = VectorField->GetBoundsMin();
	const FVector& Max = VectorField->GetBoundsMax();
	const FQuat FieldRotation = ResolveFieldRotation(LODLevel, EmitterTime);

	const FVector C000 = ComponentToWorld.TransformPositionWithW(TransformFieldLocalToComponentLocal(FVector(Min.X, Min.Y, Min.Z), FieldRotation));
	const FVector C100 = ComponentToWorld.TransformPositionWithW(TransformFieldLocalToComponentLocal(FVector(Max.X, Min.Y, Min.Z), FieldRotation));
	const FVector C110 = ComponentToWorld.TransformPositionWithW(TransformFieldLocalToComponentLocal(FVector(Max.X, Max.Y, Min.Z), FieldRotation));
	const FVector C010 = ComponentToWorld.TransformPositionWithW(TransformFieldLocalToComponentLocal(FVector(Min.X, Max.Y, Min.Z), FieldRotation));
	const FVector C001 = ComponentToWorld.TransformPositionWithW(TransformFieldLocalToComponentLocal(FVector(Min.X, Min.Y, Max.Z), FieldRotation));
	const FVector C101 = ComponentToWorld.TransformPositionWithW(TransformFieldLocalToComponentLocal(FVector(Max.X, Min.Y, Max.Z), FieldRotation));
	const FVector C111 = ComponentToWorld.TransformPositionWithW(TransformFieldLocalToComponentLocal(FVector(Max.X, Max.Y, Max.Z), FieldRotation));
	const FVector C011 = ComponentToWorld.TransformPositionWithW(TransformFieldLocalToComponentLocal(FVector(Min.X, Max.Y, Max.Z), FieldRotation));

	const FColor BoundsColor(0, 200, 255);
	Scene.AddDebugLine(C000, C100, BoundsColor);
	Scene.AddDebugLine(C100, C110, BoundsColor);
	Scene.AddDebugLine(C110, C010, BoundsColor);
	Scene.AddDebugLine(C010, C000, BoundsColor);
	Scene.AddDebugLine(C001, C101, BoundsColor);
	Scene.AddDebugLine(C101, C111, BoundsColor);
	Scene.AddDebugLine(C111, C011, BoundsColor);
	Scene.AddDebugLine(C011, C001, BoundsColor);
	Scene.AddDebugLine(C000, C001, BoundsColor);
	Scene.AddDebugLine(C100, C101, BoundsColor);
	Scene.AddDebugLine(C110, C111, BoundsColor);
	Scene.AddDebugLine(C010, C011, BoundsColor);

	const FVector CenterLocal = TransformFieldLocalToComponentLocal((Min + Max) * 0.5f, FieldRotation);
	const FVector Center = ComponentToWorld.TransformPositionWithW(CenterLocal);
	const FVector SafeScale = GetSafeFieldBoundsScale();
	const FVector AxisX = ComponentToWorld.TransformVector(TransformFieldVectorToComponentLocal(FVector((Max.X - Min.X) * 0.5f * SafeScale.X, 0.0f, 0.0f), FieldRotation));
	const FVector AxisY = ComponentToWorld.TransformVector(TransformFieldVectorToComponentLocal(FVector(0.0f, (Max.Y - Min.Y) * 0.5f * SafeScale.Y, 0.0f), FieldRotation));
	const FVector AxisZ = ComponentToWorld.TransformVector(TransformFieldVectorToComponentLocal(FVector(0.0f, 0.0f, (Max.Z - Min.Z) * 0.5f * SafeScale.Z), FieldRotation));

	Scene.AddDebugLine(Center, Center + AxisX, FColor::Red());
	Scene.AddDebugLine(Center, Center + AxisY, FColor::Green());
	Scene.AddDebugLine(Center, Center + AxisZ, FColor::Blue());

	if (DebugVectorScale > 0.0f)
	{
		const FColor VectorColor(255, 180, 0);
		for (int32 Z = 0; Z < VectorField->GetSizeZ(); ++Z)
		{
			const float FieldZ = ComputeGridSampleCoordinate(Min.Z, Max.Z, Z, VectorField->GetSizeZ());
			for (int32 Y = 0; Y < VectorField->GetSizeY(); ++Y)
			{
				const float FieldY = ComputeGridSampleCoordinate(Min.Y, Max.Y, Y, VectorField->GetSizeY());
				for (int32 X = 0; X < VectorField->GetSizeX(); ++X)
				{
					const FVector* FieldVector = VectorField->GetVectorAt(X, Y, Z);
					if (!FieldVector)
					{
						continue;
					}

					const float FieldX = ComputeGridSampleCoordinate(Min.X, Max.X, X, VectorField->GetSizeX());
					const FVector StartLocal = TransformFieldLocalToComponentLocal(FVector(FieldX, FieldY, FieldZ), FieldRotation);
					const FVector Start = ComponentToWorld.TransformPositionWithW(StartLocal);
					const FVector Direction = ComponentToWorld.TransformVector(TransformFieldVectorToComponentLocal(*FieldVector * DebugVectorScale, FieldRotation));
					Scene.AddDebugLine(Start, Start + Direction, VectorColor);
				}
			}
		}
	}
}

void UParticleModuleVectorFieldLocal::Update(const FUpdateContext& Context)
{
	UVectorFieldAsset* VectorField = ResolveVectorField();
	if (!VectorField || !VectorField->IsValidGrid())
	{
		return;
	}

	const UParticleLODLevel* LODLevel = Context.Owner.GetCurrentLODLevel();
	const UParticleModuleRequired* RequiredModule = LODLevel ? LODLevel->GetRequiredModule() : nullptr;
	const bool bParticleDataIsLocalSpace = RequiredModule && RequiredModule->bUseLocalSpace;
	const FQuat FieldRotation = ResolveFieldRotation(LODLevel, Context.Owner.GetEmitterTime());

	const FMatrix ComponentToWorld = Context.Owner.Component
		? Context.Owner.Component->GetWorldMatrix()
		: FMatrix::Identity;
	const FMatrix WorldToComponent = bParticleDataIsLocalSpace
		? FMatrix::Identity
		: ComponentToWorld.GetInverse();

	if (Intensity == 0.0f)
	{
		return;
	}

	const FQuat InvFieldRotation = FieldRotation.Inverse();
	const FVector SafeScale = GetSafeFieldBoundsScale();
	const FVector InvSafeScale(
		1.0f / SafeScale.X,
		1.0f / SafeScale.Y,
		1.0f / SafeScale.Z);
	const FVector FieldOffset = FieldBoundsOffset;
	const float DeltaVelocityScale = Intensity * Context.DeltaTime;

	BEGIN_UPDATE_LOOP
		const FVector ComponentLocalParticlePosition = bParticleDataIsLocalSpace
			? Particle.Location
			: WorldToComponent.TransformPositionWithW(Particle.Location);
		const FVector RelativePosition = ComponentLocalParticlePosition - FieldOffset;
		const FVector RotatedFieldSamplePosition = InvFieldRotation.RotateVector(RelativePosition);
		const FVector FieldSamplePosition(
			RotatedFieldSamplePosition.X * InvSafeScale.X,
			RotatedFieldSamplePosition.Y * InvSafeScale.Y,
			RotatedFieldSamplePosition.Z * InvSafeScale.Z);

		FVector LocalFieldVector = FVector::ZeroVector;
		const bool bSampled = bUseTrilinearSampling
			? VectorField->SampleTrilinear(FieldSamplePosition, LocalFieldVector)
			: VectorField->SampleNearest(FieldSamplePosition, LocalFieldVector);
		if (!bSampled)
		{
			CONTINUE_UPDATE_LOOP
		}

		const FVector ComponentLocalFieldVector = FieldRotation.RotateVector(LocalFieldVector);
		const FVector AppliedVector = bParticleDataIsLocalSpace
			? ComponentLocalFieldVector
			: ComponentToWorld.TransformVector(ComponentLocalFieldVector);

		Particle.BaseVelocity += AppliedVector * DeltaVelocityScale;
	END_UPDATE_LOOP
}
