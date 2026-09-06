#include "Render/Proxy/Particle/ParticleRenderUtils.h"

#include "Mesh/Static/StaticMeshAsset.h"
#include "Render/Types/FrameContext.h"

#include <algorithm>
#include <cmath>

namespace
{
	constexpr float ParticleSpriteTinyNumber = 1.0e-4f;

	FVector SafeNormal(const FVector& Value, const FVector& Fallback)
	{
		const float Length = Value.Length();
		if (Length <= ParticleSpriteTinyNumber)
		{
			return Fallback.Normalized();
		}
		return Value / Length;
	}

	FVector ProjectVectorOnPlane(const FVector& Value, const FVector& PlaneNormal)
	{
		return Value - PlaneNormal * Value.Dot(PlaneNormal);
	}

	void ResolveSpriteAxes(const FDynamicSpriteEmitterReplayDataBase& Source, const FParticleProxyParticle& Particle,
		const FVector& CameraPosition, const FVector& CameraRight, const FVector& CameraUp, const FVector& CameraNormal,
		FVector& OutRight, FVector& OutUp, FVector& OutNormal)
	{
		OutRight = CameraRight;
		OutUp = CameraUp;
		OutNormal = CameraNormal;

		if (Source.ScreenAlignment != EParticleScreenAlignment::PSA_Velocity)
		{
			return;
		}

		FVector VelocityDirection = Particle.Velocity;
		if (VelocityDirection.Length() <= ParticleSpriteTinyNumber)
		{
			VelocityDirection = Particle.Position - Particle.OldPosition;
		}
		if (VelocityDirection.Length() <= ParticleSpriteTinyNumber)
		{
			return;
		}

		const FVector VelocityDir = SafeNormal(VelocityDirection, FVector::ForwardVector);
		const FVector ViewDir = SafeNormal(CameraPosition - Particle.Position, CameraNormal);

		// PSA_Velocity is an axial billboard: the long axis follows the real 3D velocity,
		// while the wing-width axis stays perpendicular to the camera ray so width is not foreshortened.
		FVector Side = ViewDir.Cross(VelocityDir);
		if (Side.Length() <= ParticleSpriteTinyNumber)
		{
			Side = ProjectVectorOnPlane(CameraRight, ViewDir);
		}
		if (Side.Length() <= ParticleSpriteTinyNumber)
		{
			Side = ProjectVectorOnPlane(CameraUp, ViewDir);
		}

		OutRight = SafeNormal(Side, CameraRight);
		OutUp = VelocityDir;
		OutNormal = SafeNormal(OutUp.Cross(OutRight), CameraNormal);
		if (OutNormal.Dot(ViewDir) < 0.0f)
		{
			OutRight *= -1.0f;
			OutNormal = SafeNormal(OutUp.Cross(OutRight), CameraNormal);
		}
	}
}

namespace ParticleRenderUtils
{
	bool IsNonePath(const FString& Path)
	{
		return Path.empty() || Path == "None";
	}

	EParticleSortMode ResolveParticleSortMode(EParticleSortMode SortMode, EParticleBlendMode BlendMode)
	{
		if (SortMode != EParticleSortMode::None)
		{
			return SortMode;
		}

		return BlendMode == EParticleBlendMode::AlphaBlend
			? EParticleSortMode::ViewDepth
			: EParticleSortMode::None;
	}

	bool ShouldSortParticles(EParticleSortMode SortMode)
	{
		return SortMode != EParticleSortMode::None;
	}

	FVector ApplyParticleScale(const FVector& Size, const FVector& Scale)
	{
		return FVector(
			Size.X * std::abs(Scale.X),
			Size.Y * std::abs(Scale.Y),
			Size.Z * std::abs(Scale.Z));
	}

	void GatherParticles(const FDynamicEmitterReplayDataBase& Source, const FFrameContext& Frame,
		const FMatrix& LocalToWorld, TArray<FParticleProxyParticle>& OutParticles)
	{
		const int32 ActiveCount = Source.ActiveParticleCount;
		const int32 Stride = Source.ParticleStride;
		const uint8* ParticleDataBytes = Source.DataContainer.ParticleData;
		const uint16* ParticleIndices = Source.DataContainer.ParticleIndices;

		if (ActiveCount <= 0 || !ParticleDataBytes || Stride <= 0)
		{
			return;
		}

		OutParticles.reserve(OutParticles.size() + static_cast<size_t>(ActiveCount));
		for (int32 i = 0; i < ActiveCount; ++i)
		{
			const int32 ParticleIndex = ParticleIndices ? ParticleIndices[i] : i;
			const FBaseParticle* BaseParticle = reinterpret_cast<const FBaseParticle*>(ParticleDataBytes + static_cast<size_t>(ParticleIndex) * Stride);
			if (!BaseParticle)
			{
				continue;
			}

			FParticleProxyParticle Particle;
			Particle.SourceParticleIndex = ParticleIndex;
			Particle.OldPosition = Source.bUseLocalSpace
				? LocalToWorld.TransformPositionWithW(BaseParticle->OldLocation)
				: BaseParticle->OldLocation;
			Particle.Position = Source.bUseLocalSpace
				? LocalToWorld.TransformPositionWithW(BaseParticle->Location)
				: BaseParticle->Location;
			Particle.Velocity = Source.bUseLocalSpace
				? LocalToWorld.TransformVector(BaseParticle->Velocity)
				: BaseParticle->Velocity;
			Particle.Size = ApplyParticleScale(BaseParticle->Size, Source.Scale);
			Particle.Color = BaseParticle->Color;
			Particle.Rotation = BaseParticle->Rotation;
			Particle.RelativeTime = BaseParticle->RelativeTime;
			Particle.Age = BaseParticle->OneOverMaxLifetime > FMath::Epsilon
				? BaseParticle->RelativeTime / BaseParticle->OneOverMaxLifetime
				: 0.0f;
			const bool bSpriteReplayData =
				Source.EmitterType == EDynamicEmitterType::Sprite ||
				Source.EmitterType == EDynamicEmitterType::Mesh ||
				Source.EmitterType == EDynamicEmitterType::Beam ||
				Source.EmitterType == EDynamicEmitterType::Ribbon;
			const FDynamicSpriteEmitterReplayDataBase* SpriteSource = bSpriteReplayData
				? static_cast<const FDynamicSpriteEmitterReplayDataBase*>(&Source)
				: nullptr;
			if (SpriteSource)
			{
				const int32 PayloadOffset = SpriteSource->SubUVPayloadOffset;
				if (PayloadOffset >= 0 && PayloadOffset + static_cast<int32>(sizeof(FParticleSubUVPayload)) <= Stride)
				{
					const FParticleSubUVPayload* SubUVPayload = reinterpret_cast<const FParticleSubUVPayload*>(
						ParticleDataBytes + static_cast<size_t>(ParticleIndex) * Stride + PayloadOffset);
					Particle.SubImageIndex = SubUVPayload ? SubUVPayload->ImageIndex : 0.0f;
				}
			}

			const FVector Diff = Particle.Position - Frame.CameraPosition;
			Particle.CameraDistanceSq = Diff.Dot(Diff);
			Particle.ViewDepth = Diff.Dot(Frame.CameraForward);
			OutParticles.push_back(Particle);
		}
	}

	void SortParticlesForView(TArray<FParticleProxyParticle>& Particles, EParticleSortMode SortMode)
	{
		std::sort(Particles.begin(), Particles.end(),
			[SortMode](const FParticleProxyParticle& A, const FParticleProxyParticle& B)
			{
				switch (SortMode)
				{
				case EParticleSortMode::DistanceToCamera:
					return A.CameraDistanceSq > B.CameraDistanceSq;
				case EParticleSortMode::ViewDepth:
					return A.ViewDepth > B.ViewDepth;
				case EParticleSortMode::Age:
					return A.RelativeTime > B.RelativeTime;
				default:
					return false;
				}
			});
	}

	FVector2 BuildSubUV(const FDynamicSpriteEmitterReplayDataBase& Source, const FParticleProxyParticle& Particle, float U, float V,
		int32 ResolvedSubImagesX, int32 ResolvedSubImagesY)
	{
		if (!Source.bUseSubUV)
		{
			return FVector2(U, V);
		}

		const int32 SubImagesX = (std::max)(1, ResolvedSubImagesX);
		const int32 SubImagesY = (std::max)(1, ResolvedSubImagesY);
		const int32 TotalFrames = SubImagesX * SubImagesY;
		if (TotalFrames <= 1)
		{
			return FVector2(U, V);
		}

		const bool bHasSubImagePayload = Source.SubUVPayloadOffset >= 0;
		const float RelativeTime = FMath::Clamp(Particle.RelativeTime, 0.0f, 1.0f);
		const float FramePosition = bHasSubImagePayload
			? Particle.SubImageIndex
			: Source.SubUVFrameRate > 0.0f
			? (std::max)(0.0f, Particle.Age) * Source.SubUVFrameRate
			: RelativeTime * static_cast<float>(TotalFrames);
		const int32 RawFrameIndex = static_cast<int32>(std::floor(FramePosition));
		const int32 FrameIndex = (!bHasSubImagePayload && Source.bLoopSubUV)
			? ((RawFrameIndex % TotalFrames) + TotalFrames) % TotalFrames
			: (std::min)((std::max)(RawFrameIndex, 0), TotalFrames - 1);
		const int32 FrameX = FrameIndex % SubImagesX;
		const int32 FrameY = FrameIndex / SubImagesX;
		const float InvX = 1.0f / static_cast<float>(SubImagesX);
		const float InvY = 1.0f / static_cast<float>(SubImagesY);

		return FVector2((static_cast<float>(FrameX) + U) * InvX, (static_cast<float>(FrameY) + V) * InvY);
	}

	void BuildSpriteVertices(const FDynamicSpriteEmitterReplayDataBase& Source, const TArray<FParticleProxyParticle>& Particles,
		const FVector& CameraPosition, const FVector& CameraRight, const FVector& CameraUp, int32 ResolvedSubImagesX, int32 ResolvedSubImagesY,
		TArray<FVertexPNCTT>& OutVertices, TArray<uint32>& OutIndices)
	{
		const FVector CameraNormal = SafeNormal(CameraUp.Cross(CameraRight), FVector::ForwardVector);
		const FVector BaseCameraRight = SafeNormal(CameraRight, FVector::RightVector);
		const FVector BaseCameraUp = SafeNormal(CameraUp, FVector::UpVector);

		for (const FParticleProxyParticle& Particle : Particles)
		{
			const uint32 BaseIndex = static_cast<uint32>(OutVertices.size());
			const float HalfWidth = (std::max)(0.001f, Particle.Size.X * 0.5f);
			const float HalfHeight = (std::max)(0.001f, Particle.Size.Y * 0.5f);

			FVector AlignmentRight;
			FVector AlignmentUp;
			FVector AlignmentNormal;
			ResolveSpriteAxes(Source, Particle, CameraPosition, BaseCameraRight, BaseCameraUp, CameraNormal, AlignmentRight, AlignmentUp, AlignmentNormal);

			const float C = std::cos(Particle.Rotation);
			const float S = std::sin(Particle.Rotation);
			const FVector Right = AlignmentRight * C - AlignmentUp * S;
			const FVector Up = AlignmentRight * S + AlignmentUp * C;
			const FVector4 Tangent(Right, 1.0f);

			const FVector P0 = Particle.Position - Right * HalfWidth + Up * HalfHeight;
			const FVector P1 = Particle.Position + Right * HalfWidth + Up * HalfHeight;
			const FVector P2 = Particle.Position - Right * HalfWidth - Up * HalfHeight;
			const FVector P3 = Particle.Position + Right * HalfWidth - Up * HalfHeight;

			const FVector4 Color = Particle.Color.ToVector4();

			FVertexPNCTT V0, V1, V2, V3;
			V0.Position = P0; V0.Normal = AlignmentNormal; V0.Color = Color; V0.UV = BuildSubUV(Source, Particle, 0.0f, 0.0f, ResolvedSubImagesX, ResolvedSubImagesY); V0.Tangent = Tangent;
			V1.Position = P1; V1.Normal = AlignmentNormal; V1.Color = Color; V1.UV = BuildSubUV(Source, Particle, 1.0f, 0.0f, ResolvedSubImagesX, ResolvedSubImagesY); V1.Tangent = Tangent;
			V2.Position = P2; V2.Normal = AlignmentNormal; V2.Color = Color; V2.UV = BuildSubUV(Source, Particle, 0.0f, 1.0f, ResolvedSubImagesX, ResolvedSubImagesY); V2.Tangent = Tangent;
			V3.Position = P3; V3.Normal = AlignmentNormal; V3.Color = Color; V3.UV = BuildSubUV(Source, Particle, 1.0f, 1.0f, ResolvedSubImagesX, ResolvedSubImagesY); V3.Tangent = Tangent;

			OutVertices.push_back(V0);
			OutVertices.push_back(V1);
			OutVertices.push_back(V2);
			OutVertices.push_back(V3);

			OutIndices.push_back(BaseIndex + 0);
			OutIndices.push_back(BaseIndex + 1);
			OutIndices.push_back(BaseIndex + 2);
			OutIndices.push_back(BaseIndex + 2);
			OutIndices.push_back(BaseIndex + 1);
			OutIndices.push_back(BaseIndex + 3);
		}
	}

	void BuildMeshVertices(const TArray<FParticleProxyParticle>& Particles, const TArray<FNormalVertex>& MeshVertices,
		const TArray<uint32>& MeshIndices, TArray<FVertexPNCTT>& OutVertices, TArray<uint32>& OutIndices)
	{
		if (Particles.empty() || MeshVertices.empty() || MeshIndices.empty())
		{
			return;
		}

		for (const uint32 RawIndex : MeshIndices)
		{
			if (RawIndex >= static_cast<uint32>(MeshVertices.size()))
			{
				return;
			}
		}

		OutVertices.reserve(OutVertices.size() + Particles.size() * MeshVertices.size());
		OutIndices.reserve(OutIndices.size() + Particles.size() * MeshIndices.size());

		for (const FParticleProxyParticle& Particle : Particles)
		{
			const uint32 BaseVertex = static_cast<uint32>(OutVertices.size());

			for (const FNormalVertex& RawVert : MeshVertices)
			{
				const FVector ScaledPosition = RawVert.pos * Particle.Size;
				const FVector RotatedNormal = FVector::RotateZ(Particle.Rotation, RawVert.normal).Normalized();
				const FVector RotatedTangent = FVector::RotateZ(Particle.Rotation, FVector(RawVert.tangent.X, RawVert.tangent.Y, RawVert.tangent.Z));

				FVertexPNCTT Vertex;
				Vertex.Position = Particle.Position + FVector::RotateZ(Particle.Rotation, ScaledPosition);
				Vertex.Normal = RotatedNormal;
				Vertex.Color = RawVert.color * Particle.Color.ToVector4();
				Vertex.UV = RawVert.tex;
				Vertex.Tangent = FVector4(RotatedTangent, RawVert.tangent.W);
				OutVertices.push_back(Vertex);
			}

			for (const uint32 RawIndex : MeshIndices)
			{
				OutIndices.push_back(BaseVertex + RawIndex);
			}
		}
	}

	float ComputePacketSortDepth(const TArray<FParticleProxyParticle>& Particles, EParticleSortMode SortMode)
	{
		float SortDepth = 0.0f;
		for (const FParticleProxyParticle& Particle : Particles)
		{
			const float ParticleDepth = SortMode == EParticleSortMode::DistanceToCamera
				? Particle.CameraDistanceSq
				: Particle.ViewDepth;
			SortDepth = (std::max)(SortDepth, ParticleDepth);
		}
		return SortDepth;
	}

	bool IsTranslucentPacket(const FParticleRenderPacket& Packet)
	{
		return Packet.BlendMode != EParticleBlendMode::Opaque;
	}

	bool SortRenderPacketForDraw(const FParticleRenderPacket& A, const FParticleRenderPacket& B)
	{
		const bool bTranslucentA = IsTranslucentPacket(A);
		const bool bTranslucentB = IsTranslucentPacket(B);

		if (bTranslucentA && bTranslucentB)
		{
			if (A.TranslucencySortPriority != B.TranslucencySortPriority)
			{
				return A.TranslucencySortPriority < B.TranslucencySortPriority;
			}

			if (A.SortDepth != B.SortDepth)
			{
				return A.SortDepth > B.SortDepth;
			}
		}

		return A.FirstIndex < B.FirstIndex;
	}
}
