#include "Physics/Cloth/ClothAssetBuilder.h"

#include "Materials/Material.h"
#include "Mesh/Skeletal/SkeletalMesh.h"
#include "Mesh/Skeletal/SkeletalMeshAsset.h"
#include "Mesh/Static/StaticMesh.h"
#include "Mesh/Static/StaticMeshAsset.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <utility>
#include <vector>

namespace
{
	void SetError(FString* OutError, const FString& Message)
	{
		if (OutError)
		{
			*OutError = Message;
		}
	}

	uint64 MakeEdgeKey(uint32 A, uint32 B)
	{
		if (A > B)
		{
			std::swap(A, B);
		}
		return (static_cast<uint64>(A) << 32) | static_cast<uint64>(B);
	}

	long long QuantizeWeldPosition(float Value, double InvTolerance)
	{
		return static_cast<long long>(std::floor(static_cast<double>(Value) * InvTolerance));
	}

	FString MakeWeldPositionKey(long long X, long long Y, long long Z)
	{
		return std::to_string(X) + "," + std::to_string(Y) + "," + std::to_string(Z);
	}

	bool IsFinitePosition(const FVector& Position)
	{
		return std::isfinite(Position.X) && std::isfinite(Position.Y) && std::isfinite(Position.Z);
	}

	bool WeldRawMeshForCloth(
		const TArray<FVector>& Positions,
		const TArray<FVector2>& UVs,
		const TArray<uint32>& Indices,
		float PositionTolerance,
		TArray<FVector>& OutPositions,
		TArray<FVector2>& OutUVs,
		TArray<uint32>& OutIndices,
		uint32* OutWeldedVertexCount)
	{
		OutPositions.clear();
		OutUVs.clear();
		OutIndices.clear();
		if (OutWeldedVertexCount)
		{
			*OutWeldedVertexCount = 0;
		}

		if (Positions.empty() || Indices.size() < 3 || Indices.size() % 3 != 0)
		{
			return false;
		}

		constexpr float MinimumTolerance = 1.0e-6f;
		const float EffectiveTolerance = std::max(PositionTolerance, MinimumTolerance);
		const double InvTolerance = 1.0 / static_cast<double>(EffectiveTolerance);
		const bool bHasUVs = UVs.size() == Positions.size();
		const uint32 InvalidIndex = std::numeric_limits<uint32>::max();
		const uint32 SourceParticleCount = static_cast<uint32>(Positions.size());

		TArray<uint32> OriginalToWelded(Positions.size(), InvalidIndex);
		TMap<FString, TArray<uint32>> BucketedWeldedIndices;
		OutPositions.reserve(Positions.size());
		OutUVs.reserve(Positions.size());

		for (uint32 SourceIndex = 0; SourceIndex < SourceParticleCount; ++SourceIndex)
		{
			const FVector& Position = Positions[SourceIndex];
			if (!IsFinitePosition(Position))
			{
				return false;
			}

			const long long CellX = QuantizeWeldPosition(Position.X, InvTolerance);
			const long long CellY = QuantizeWeldPosition(Position.Y, InvTolerance);
			const long long CellZ = QuantizeWeldPosition(Position.Z, InvTolerance);

			uint32 WeldedIndex = InvalidIndex;
			for (long long OffsetX = -1; OffsetX <= 1 && WeldedIndex == InvalidIndex; ++OffsetX)
			{
				for (long long OffsetY = -1; OffsetY <= 1 && WeldedIndex == InvalidIndex; ++OffsetY)
				{
					for (long long OffsetZ = -1; OffsetZ <= 1; ++OffsetZ)
					{
						const FString NeighborKey = MakeWeldPositionKey(CellX + OffsetX, CellY + OffsetY, CellZ + OffsetZ);
						auto BucketIt = BucketedWeldedIndices.find(NeighborKey);
						if (BucketIt == BucketedWeldedIndices.end())
						{
							continue;
						}

						for (uint32 CandidateIndex : BucketIt->second)
						{
							if (static_cast<size_t>(CandidateIndex) < OutPositions.size() && FVector::Distance(OutPositions[CandidateIndex], Position) <= EffectiveTolerance)
							{
								WeldedIndex = CandidateIndex;
								break;
							}
						}

						if (WeldedIndex != InvalidIndex)
						{
							break;
						}
					}
				}
			}

			if (WeldedIndex == InvalidIndex)
			{
				WeldedIndex = static_cast<uint32>(OutPositions.size());
				OutPositions.push_back(Position);
				OutUVs.push_back(bHasUVs ? UVs[SourceIndex] : FVector2(0.0f, 0.0f));

				const FString CellKey = MakeWeldPositionKey(CellX, CellY, CellZ);
				BucketedWeldedIndices[CellKey].push_back(WeldedIndex);
			}

			OriginalToWelded[SourceIndex] = WeldedIndex;
		}

		if (OutWeldedVertexCount)
		{
			*OutWeldedVertexCount = static_cast<uint32>(Positions.size() - OutPositions.size());
		}

		OutIndices.reserve(Indices.size());
		for (uint32 IndexOffset = 0; IndexOffset + 2 < static_cast<uint32>(Indices.size()); IndexOffset += 3)
		{
			const uint32 SourceI0 = Indices[IndexOffset + 0];
			const uint32 SourceI1 = Indices[IndexOffset + 1];
			const uint32 SourceI2 = Indices[IndexOffset + 2];
			if (SourceI0 >= SourceParticleCount || SourceI1 >= SourceParticleCount || SourceI2 >= SourceParticleCount)
			{
				return false;
			}

			const uint32 I0 = OriginalToWelded[SourceI0];
			const uint32 I1 = OriginalToWelded[SourceI1];
			const uint32 I2 = OriginalToWelded[SourceI2];
			if (I0 == InvalidIndex || I1 == InvalidIndex || I2 == InvalidIndex)
			{
				return false;
			}

			if (I0 == I1 || I1 == I2 || I2 == I0)
			{
				continue;
			}

			OutIndices.push_back(I0);
			OutIndices.push_back(I1);
			OutIndices.push_back(I2);
		}

		return !OutPositions.empty() && OutIndices.size() >= 3;
	}

	bool AppendFabricDistanceConstraint(
		FClothFabricCookedData& FabricData,
		const TArray<FVector>& Positions,
		TSet<uint64>& SeenConstraints,
		uint32 A,
		uint32 B)
	{
		const uint32 ParticleCount = static_cast<uint32>(Positions.size());
		if (A >= ParticleCount || B >= ParticleCount || A == B)
		{
			return false;
		}

		const float RestLength = FVector::Distance(Positions[A], Positions[B]);
		if (RestLength <= 1.0e-6f)
		{
			return false;
		}

		const uint64 Key = MakeEdgeKey(A, B);
		if (SeenConstraints.find(Key) != SeenConstraints.end())
		{
			return false;
		}
		SeenConstraints.insert(Key);

		FabricData.ConstraintIndices.push_back(A);
		FabricData.ConstraintIndices.push_back(B);
		FabricData.RestValues.push_back(RestLength);
		return true;
	}

	bool IsPinnedParticle(const UClothAsset& Asset, uint32 Index)
	{
		const TArray<float>& PinMask = Asset.GetPinMask();
		const TArray<float>& InvMasses = Asset.GetInvMasses();
		const bool bPinnedByMask = Index < PinMask.size() && PinMask[Index] > 0.0f;
		const bool bPinnedByMass = Index < InvMasses.size() && InvMasses[Index] <= 0.0f;
		return bPinnedByMask || bPinnedByMass;
	}

	struct FClothTriangleEdgeInfo
	{
		uint32 OppositeVertex = 0;
		bool bHasPairedTriangle = false;
	};

	struct FClothAdjacencyEdge
	{
		uint32 VertexIndex = 0;
		float Distance = 0.0f;
	};

	struct FClothTetherQueueNode
	{
		uint32 VertexIndex = 0;
		float Distance = 0.0f;
	};

	struct FClothTetherQueueNodeGreater
	{
		bool operator()(const FClothTetherQueueNode& A, const FClothTetherQueueNode& B) const
		{
			return A.Distance > B.Distance;
		}
	};

	void AppendTetherGraphEdge(TArray<TArray<FClothAdjacencyEdge>>& Adjacency, const TArray<FVector>& Positions, uint32 A, uint32 B)
	{
		const uint32 ParticleCount = static_cast<uint32>(Positions.size());
		if (A >= ParticleCount || B >= ParticleCount || A == B)
		{
			return;
		}

		const float Distance = FVector::Distance(Positions[A], Positions[B]);
		if (Distance <= 1.0e-6f)
		{
			return;
		}

		Adjacency[A].push_back(FClothAdjacencyEdge{ B, Distance });
		Adjacency[B].push_back(FClothAdjacencyEdge{ A, Distance });
	}

	uint32 AppendBendConstraintsFromTrianglesInternal(UClothAsset& Asset, TSet<uint64>& SeenConstraints)
	{
		const TArray<FVector>& Positions = Asset.GetRestPositions();
		const TArray<uint32>& Indices = Asset.GetIndices();
		const uint32 ParticleCount = static_cast<uint32>(Positions.size());
		if (ParticleCount == 0 || Indices.size() < 3)
		{
			return 0;
		}

		TMap<uint64, FClothTriangleEdgeInfo> TriangleEdges;
		uint32 AddedBendConstraintCount = 0;

		auto RegisterTriangleEdge = [&](uint32 A, uint32 B, uint32 Opposite)
		{
			if (A >= ParticleCount || B >= ParticleCount || Opposite >= ParticleCount || A == B || A == Opposite || B == Opposite)
			{
				return;
			}

			const uint64 Key = MakeEdgeKey(A, B);
			auto ExistingIt = TriangleEdges.find(Key);
			if (ExistingIt == TriangleEdges.end())
			{
				TriangleEdges.emplace(Key, FClothTriangleEdgeInfo{ Opposite, false });
				return;
			}

			FClothTriangleEdgeInfo& Existing = ExistingIt->second;
			if (Existing.bHasPairedTriangle || Existing.OppositeVertex == Opposite)
			{
				return;
			}

			if (AppendFabricDistanceConstraint(Asset.FabricData, Positions, SeenConstraints, Existing.OppositeVertex, Opposite))
			{
				++AddedBendConstraintCount;
			}
			Existing.bHasPairedTriangle = true;
		};

		for (uint32 IndexOffset = 0; IndexOffset + 2 < static_cast<uint32>(Indices.size()); IndexOffset += 3)
		{
			const uint32 I0 = Indices[IndexOffset + 0];
			const uint32 I1 = Indices[IndexOffset + 1];
			const uint32 I2 = Indices[IndexOffset + 2];
			if (I0 >= ParticleCount || I1 >= ParticleCount || I2 >= ParticleCount)
			{
				continue;
			}

			RegisterTriangleEdge(I0, I1, I2);
			RegisterTriangleEdge(I1, I2, I0);
			RegisterTriangleEdge(I2, I0, I1);
		}

		return AddedBendConstraintCount;
	}

	FVector CalculateDefaultGridCenter(const TArray<FVector>& SourcePositions)
	{
		if (SourcePositions.empty())
		{
			return FVector::ZeroVector;
		}

		FVector Sum = FVector::ZeroVector;
		for (const FVector& Position : SourcePositions)
		{
			Sum = Sum + Position;
		}

		return Sum * (1.0f / static_cast<float>(SourcePositions.size()));
	}

	bool BuildDefaultPinnedGrid32x32(const TArray<FVector>& SourcePositions, UMaterial* Material, UClothAsset& OutAsset, FString* OutError)
	{
		constexpr uint32 GridSide = 32;
		constexpr float ParticleSpacing = 0.2f;
		constexpr float ClothExtent = ParticleSpacing * static_cast<float>(GridSide - 1);
		constexpr float HalfExtent = ClothExtent * 0.5f;

		OutAsset.FabricData = FClothFabricCookedData();
		OutAsset.RestPositions.clear();
		OutAsset.Indices.clear();
		OutAsset.UVs.clear();
		OutAsset.InvMasses.clear();
		OutAsset.PinMask.clear();
		OutAsset.SetMaterial(Material);

		OutAsset.RestPositions.reserve(GridSide * GridSide);
		OutAsset.UVs.reserve(GridSide * GridSide);
		OutAsset.InvMasses.assign(GridSide * GridSide, 1.0f);
		OutAsset.PinMask.assign(GridSide * GridSide, 0.0f);

		const FVector GridCenter = CalculateDefaultGridCenter(SourcePositions);

		for (uint32 Row = 0; Row < GridSide; ++Row)
		{
			for (uint32 Col = 0; Col < GridSide; ++Col)
			{
				const float U = static_cast<float>(Col) / static_cast<float>(GridSide - 1);
				const float V = static_cast<float>(Row) / static_cast<float>(GridSide - 1);
				const FVector Position(
					GridCenter.X - HalfExtent + static_cast<float>(Col) * ParticleSpacing,
					GridCenter.Y,
					GridCenter.Z + HalfExtent - static_cast<float>(Row) * ParticleSpacing);
				OutAsset.RestPositions.push_back(Position);
				OutAsset.UVs.push_back(FVector2(
					U,
					V));
			}
		}

		auto GridIndex = [GridSide](uint32 Row, uint32 Col)
		{
			return Row * GridSide + Col;
		};

		for (uint32 Row = 0; Row + 1 < GridSide; ++Row)
		{
			for (uint32 Col = 0; Col + 1 < GridSide; ++Col)
			{
				const uint32 I0 = GridIndex(Row, Col);
				const uint32 I1 = GridIndex(Row, Col + 1);
				const uint32 I2 = GridIndex(Row + 1, Col);
				const uint32 I3 = GridIndex(Row + 1, Col + 1);
				OutAsset.Indices.push_back(I0);
				OutAsset.Indices.push_back(I1);
				OutAsset.Indices.push_back(I2);
				OutAsset.Indices.push_back(I1);
				OutAsset.Indices.push_back(I3);
				OutAsset.Indices.push_back(I2);
			}
		}

		for (uint32 Col = 0; Col < GridSide; ++Col)
		{
			const uint32 PinnedIndex = GridIndex(0, Col);
			OutAsset.InvMasses[PinnedIndex] = 0.0f;
			OutAsset.PinMask[PinnedIndex] = 1.0f;
		}

		TSet<uint64> SeenConstraints;
		auto AddConstraint = [&](uint32 A, uint32 B)
		{
			const uint64 Key = MakeEdgeKey(A, B);
			if (SeenConstraints.find(Key) != SeenConstraints.end())
			{
				return;
			}
			SeenConstraints.insert(Key);

			OutAsset.FabricData.ConstraintIndices.push_back(A);
			OutAsset.FabricData.ConstraintIndices.push_back(B);
			OutAsset.FabricData.RestValues.push_back(FVector::Distance(OutAsset.RestPositions[A], OutAsset.RestPositions[B]));
		};

		for (uint32 Row = 0; Row < GridSide; ++Row)
		{
			for (uint32 Col = 0; Col < GridSide; ++Col)
			{
				const uint32 I0 = GridIndex(Row, Col);
				if (Col + 1 < GridSide)
				{
					AddConstraint(I0, GridIndex(Row, Col + 1));
				}
				if (Row + 1 < GridSide)
				{
					AddConstraint(I0, GridIndex(Row + 1, Col));
				}
				if (Row + 1 < GridSide && Col + 1 < GridSide)
				{
					AddConstraint(I0, GridIndex(Row + 1, Col + 1));
					AddConstraint(GridIndex(Row, Col + 1), GridIndex(Row + 1, Col));
				}
				if (Col + 2 < GridSide)
				{
					AddConstraint(I0, GridIndex(Row, Col + 2));
				}
				if (Row + 2 < GridSide)
				{
					AddConstraint(I0, GridIndex(Row + 2, Col));
				}
			}
		}

		OutAsset.FabricData.PhaseIndices.push_back(0);
		OutAsset.FabricData.Sets.push_back(static_cast<uint32>(OutAsset.FabricData.RestValues.size()));
		OutAsset.FabricData.Triangles = OutAsset.Indices;
		FClothAssetBuilder::RebuildTethersFromPins(OutAsset);

		if (!OutAsset.HasValidSimulationData())
		{
			SetError(OutError, "Generated default 32x32 ClothAsset data failed validation.");
			return false;
		}

		return true;
	}

	UMaterial* ResolveStaticMeshMaterial(const UStaticMesh* SourceMesh, const FStaticMesh* Mesh)
	{
		if (!SourceMesh)
		{
			return nullptr;
		}

		const TArray<FStaticMaterial>& Materials = SourceMesh->GetStaticMaterials();
		if (Mesh && !Mesh->Sections.empty())
		{
			const int32 MaterialIndex = Mesh->Sections[0].MaterialIndex;
			if (MaterialIndex >= 0 && MaterialIndex < static_cast<int32>(Materials.size()))
			{
				return Materials[MaterialIndex].MaterialInterface;
			}
		}

		return !Materials.empty() ? Materials[0].MaterialInterface : nullptr;
	}

	UMaterial* ResolveSkeletalMeshMaterial(const USkeletalMesh* SourceMesh, const FSkeletalMesh* Mesh)
	{
		if (!SourceMesh)
		{
			return nullptr;
		}

		const TArray<FSkeletalMaterial>& Materials = SourceMesh->GetSkeletalMaterials();
		if (Mesh && !Mesh->Sections.empty())
		{
			const int32 MaterialIndex = Mesh->Sections[0].MaterialIndex;
			if (MaterialIndex >= 0 && MaterialIndex < static_cast<int32>(Materials.size()))
			{
				return Materials[MaterialIndex].MaterialInterface;
			}
		}

		return !Materials.empty() ? Materials[0].MaterialInterface : nullptr;
	}
}

bool FClothAssetBuilder::BuildFromStaticMesh(
	const UStaticMesh* SourceMesh,
	UClothAsset& OutAsset,
	const FClothAssetBuildOptions& Options,
	FString* OutError)
{
	const FStaticMesh* Mesh = SourceMesh ? SourceMesh->GetStaticMeshAsset() : nullptr;
	if (!Mesh)
	{
		SetError(OutError, "StaticMesh has no mesh data.");
		return false;
	}

	TArray<FVector> Positions;
	TArray<FVector4> Colors;
	TArray<FVector2> UVs;
	Positions.reserve(Mesh->Vertices.size());
	Colors.reserve(Mesh->Vertices.size());
	UVs.reserve(Mesh->Vertices.size());

	for (const FNormalVertex& Vertex : Mesh->Vertices)
	{
		Positions.push_back(Vertex.pos);
		Colors.push_back(Vertex.color);
		UVs.push_back(Vertex.tex);
	}

	return BuildFromRawMesh(
		Positions,
		Colors,
		UVs,
		Mesh->Indices,
		ResolveStaticMeshMaterial(SourceMesh, Mesh),
		OutAsset,
		Options,
		OutError);
}

bool FClothAssetBuilder::BuildFromSkeletalMesh(
	const USkeletalMesh* SourceMesh,
	UClothAsset& OutAsset,
	const FClothAssetBuildOptions& Options,
	FString* OutError)
{
	const FSkeletalMesh* Mesh = SourceMesh ? SourceMesh->GetSkeletalMeshAsset() : nullptr;
	if (!Mesh)
	{
		SetError(OutError, "SkeletalMesh has no mesh data.");
		return false;
	}

	TArray<FVector> Positions;
	TArray<FVector4> Colors;
	TArray<FVector2> UVs;
	Positions.reserve(Mesh->Vertices.size());
	Colors.reserve(Mesh->Vertices.size());
	UVs.reserve(Mesh->Vertices.size());

	for (const FVertexPNCTBW& Vertex : Mesh->Vertices)
	{
		Positions.push_back(Vertex.Position);
		Colors.push_back(Vertex.Color);
		UVs.push_back(Vertex.UV);
	}

	return BuildFromRawMesh(
		Positions,
		Colors,
		UVs,
		Mesh->Indices,
		ResolveSkeletalMeshMaterial(SourceMesh, Mesh),
		OutAsset,
		Options,
		OutError);
}

bool FClothAssetBuilder::BuildFromRawMesh(
	const TArray<FVector>& Positions,
	const TArray<FVector4>& Colors,
	const TArray<FVector2>& UVs,
	const TArray<uint32>& Indices,
	UMaterial* Material,
	UClothAsset& OutAsset,
	const FClothAssetBuildOptions& Options,
	FString* OutError)
{
	if (Options.bBuildDefaultPinnedGrid32x32)
	{
		(void)Colors;
		(void)UVs;
		(void)Indices;
		return BuildDefaultPinnedGrid32x32(Positions, Material, OutAsset, OutError);
	}

	const uint32 ParticleCount = static_cast<uint32>(Positions.size());
	if (ParticleCount == 0)
	{
		SetError(OutError, "Source mesh has no vertices.");
		return false;
	}

	if (Indices.size() < 3 || Indices.size() % 3 != 0)
	{
		SetError(OutError, "Source mesh must have triangle indices.");
		return false;
	}

	(void)Colors;

	TArray<FVector> BuildPositions;
	TArray<FVector2> BuildUVs;
	TArray<uint32> BuildIndices;
	uint32 WeldedVertexCount = 0;
	if (Options.bWeldVertices)
	{
		if (!WeldRawMeshForCloth(
			Positions,
			UVs,
			Indices,
			Options.WeldPositionTolerance,
			BuildPositions,
			BuildUVs,
			BuildIndices,
			&WeldedVertexCount))
		{
			SetError(OutError, "Could not weld source mesh for cloth simulation.");
			return false;
		}
	}
	else
	{
		BuildPositions = Positions;
		BuildIndices = Indices;
		BuildUVs = UVs.size() == Positions.size() ? UVs : TArray<FVector2>(Positions.size(), FVector2(0.0f, 0.0f));
	}

	const uint32 BuildParticleCount = static_cast<uint32>(BuildPositions.size());
	if (BuildParticleCount == 0)
	{
		SetError(OutError, "Source mesh has no usable cloth vertices.");
		return false;
	}

	if (BuildIndices.size() < 3 || BuildIndices.size() % 3 != 0)
	{
		SetError(OutError, "Source mesh has no usable cloth triangles.");
		return false;
	}

	OutAsset.FabricData = FClothFabricCookedData();
	OutAsset.RestPositions = std::move(BuildPositions);
	OutAsset.Indices = std::move(BuildIndices);
	OutAsset.UVs = std::move(BuildUVs);
	OutAsset.InvMasses.assign(OutAsset.RestPositions.size(), 1.0f);
	OutAsset.PinMask.assign(OutAsset.RestPositions.size(), 0.0f);
	OutAsset.SetMaterial(Material);

	TSet<uint64> SeenConstraints;
	auto AddEdge = [&](uint32 A, uint32 B)
	{
		AppendFabricDistanceConstraint(OutAsset.FabricData, OutAsset.RestPositions, SeenConstraints, A, B);
	};

	for (uint32 IndexOffset = 0; IndexOffset + 2 < static_cast<uint32>(OutAsset.Indices.size()); IndexOffset += 3)
	{
		const uint32 I0 = OutAsset.Indices[IndexOffset + 0];
		const uint32 I1 = OutAsset.Indices[IndexOffset + 1];
		const uint32 I2 = OutAsset.Indices[IndexOffset + 2];
		if (I0 >= BuildParticleCount || I1 >= BuildParticleCount || I2 >= BuildParticleCount)
		{
			SetError(OutError, "Source mesh has out-of-range triangle indices.");
			return false;
		}

		AddEdge(I0, I1);
		AddEdge(I1, I2);
		AddEdge(I2, I0);
	}

	const uint32 EdgeConstraintCount = static_cast<uint32>(OutAsset.FabricData.RestValues.size());
	if (EdgeConstraintCount == 0)
	{
		SetError(OutError, "Could not build cloth edge constraints.");
		return false;
	}

	uint32 BendConstraintCount = 0;
	if (Options.bBuildBendConstraints)
	{
		BendConstraintCount = AppendBendConstraintsFromTrianglesInternal(OutAsset, SeenConstraints);
	}

	OutAsset.FabricData.PhaseIndices.push_back(0);
	OutAsset.FabricData.Sets.push_back(EdgeConstraintCount);
	if (BendConstraintCount > 0)
	{
		OutAsset.FabricData.PhaseIndices.push_back(1);
		OutAsset.FabricData.Sets.push_back(static_cast<uint32>(OutAsset.FabricData.RestValues.size()));
	}
	OutAsset.FabricData.Triangles = OutAsset.Indices;
	RebuildTethersFromPins(OutAsset);

	if (!OutAsset.HasValidSimulationData())
	{
		SetError(OutError, "Generated ClothAsset data failed validation.");
		return false;
	}

	return true;
}

uint32 FClothAssetBuilder::AppendBendConstraintsFromTriangles(UClothAsset& Asset)
{
	FClothFabricCookedData& FabricData = Asset.FabricData;
	if (FabricData.PhaseIndices.size() > 1)
	{
		return 0;
	}

	const uint32 OriginalConstraintCount = static_cast<uint32>(FabricData.RestValues.size());
	if (OriginalConstraintCount == 0)
	{
		return 0;
	}

	TSet<uint64> SeenConstraints;
	for (uint32 IndexOffset = 0; IndexOffset + 1 < static_cast<uint32>(FabricData.ConstraintIndices.size()); IndexOffset += 2)
	{
		SeenConstraints.insert(MakeEdgeKey(FabricData.ConstraintIndices[IndexOffset], FabricData.ConstraintIndices[IndexOffset + 1]));
	}

	const uint32 AddedBendConstraintCount = AppendBendConstraintsFromTrianglesInternal(Asset, SeenConstraints);
	if (AddedBendConstraintCount == 0)
	{
		return 0;
	}

	if (FabricData.PhaseIndices.empty())
	{
		FabricData.PhaseIndices.push_back(0);
	}

	if (FabricData.Sets.empty())
	{
		FabricData.Sets.push_back(OriginalConstraintCount);
	}
	else
	{
		FabricData.Sets.back() = OriginalConstraintCount;
	}

	const uint32 BendSetIndex = static_cast<uint32>(FabricData.Sets.size());
	FabricData.PhaseIndices.push_back(BendSetIndex);
	FabricData.Sets.push_back(static_cast<uint32>(FabricData.RestValues.size()));
	FabricData.Triangles = Asset.GetIndices();
	return AddedBendConstraintCount;
}

uint32 FClothAssetBuilder::RebuildTethersFromPins(UClothAsset& Asset, float LengthScale)
{
	FClothFabricCookedData& FabricData = Asset.FabricData;
	FabricData.Anchors.clear();
	FabricData.TetherLengths.clear();

	const TArray<FVector>& Positions = Asset.GetRestPositions();
	const TArray<uint32>& Indices = Asset.GetIndices();
	const uint32 ParticleCount = static_cast<uint32>(Positions.size());
	if (ParticleCount == 0 || Indices.size() < 3)
	{
		return 0;
	}

	TArray<TArray<FClothAdjacencyEdge>> Adjacency(ParticleCount);
	for (uint32 IndexOffset = 0; IndexOffset + 2 < static_cast<uint32>(Indices.size()); IndexOffset += 3)
	{
		const uint32 I0 = Indices[IndexOffset + 0];
		const uint32 I1 = Indices[IndexOffset + 1];
		const uint32 I2 = Indices[IndexOffset + 2];
		AppendTetherGraphEdge(Adjacency, Positions, I0, I1);
		AppendTetherGraphEdge(Adjacency, Positions, I1, I2);
		AppendTetherGraphEdge(Adjacency, Positions, I2, I0);
	}

	constexpr uint32 InvalidAnchor = std::numeric_limits<uint32>::max();
	constexpr float InfiniteDistance = std::numeric_limits<float>::max();
	TArray<float> Distances(ParticleCount, InfiniteDistance);
	TArray<uint32> SourceAnchors(ParticleCount, InvalidAnchor);
	TArray<uint32> PinnedVertices;
	PinnedVertices.reserve(ParticleCount);

	std::priority_queue<FClothTetherQueueNode, std::vector<FClothTetherQueueNode>, FClothTetherQueueNodeGreater> Queue;
	uint32 PinnedCount = 0;
	for (uint32 Index = 0; Index < ParticleCount; ++Index)
	{
		if (!IsPinnedParticle(Asset, Index))
		{
			continue;
		}

		++PinnedCount;
		PinnedVertices.push_back(Index);
		Distances[Index] = 0.0f;
		SourceAnchors[Index] = Index;
		Queue.push(FClothTetherQueueNode{ Index, 0.0f });
	}

	if (PinnedCount == 0)
	{
		return 0;
	}

	while (!Queue.empty())
	{
		const FClothTetherQueueNode Node = Queue.top();
		Queue.pop();

		if (Node.VertexIndex >= ParticleCount || Node.Distance > Distances[Node.VertexIndex] + 1.0e-5f)
		{
			continue;
		}

		for (const FClothAdjacencyEdge& Edge : Adjacency[Node.VertexIndex])
		{
			const float CandidateDistance = Node.Distance + Edge.Distance;
			if (CandidateDistance >= Distances[Edge.VertexIndex])
			{
				continue;
			}

			Distances[Edge.VertexIndex] = CandidateDistance;
			SourceAnchors[Edge.VertexIndex] = SourceAnchors[Node.VertexIndex];
			Queue.push(FClothTetherQueueNode{ Edge.VertexIndex, CandidateDistance });
		}
	}

	const float EffectiveLengthScale = std::max(0.0f, LengthScale);
	FabricData.Anchors.reserve(ParticleCount);
	FabricData.TetherLengths.reserve(ParticleCount);
	for (uint32 Index = 0; Index < ParticleCount; ++Index)
	{
		if (IsPinnedParticle(Asset, Index))
		{
			FabricData.Anchors.push_back(Index);
			FabricData.TetherLengths.push_back(0.0f);
			continue;
		}

		uint32 Anchor = SourceAnchors[Index];
		float Distance = Distances[Index];
		if (Anchor == InvalidAnchor || Distance == InfiniteDistance)
		{
			float BestDistance = InfiniteDistance;
			uint32 BestAnchor = PinnedVertices[0];
			for (uint32 PinnedIndex : PinnedVertices)
			{
				const float CandidateDistance = FVector::Distance(Positions[Index], Positions[PinnedIndex]);
				if (CandidateDistance < BestDistance)
				{
					BestDistance = CandidateDistance;
					BestAnchor = PinnedIndex;
				}
			}

			Anchor = BestAnchor;
			Distance = BestDistance;
		}

		FabricData.Anchors.push_back(Anchor);
		FabricData.TetherLengths.push_back(Distance * EffectiveLengthScale);
	}

	return static_cast<uint32>(FabricData.TetherLengths.size());
}
