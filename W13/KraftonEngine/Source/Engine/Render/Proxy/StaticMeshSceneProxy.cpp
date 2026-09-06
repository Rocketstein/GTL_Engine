#include "Render/Proxy/StaticMeshSceneProxy.h"
#include "Component/Primitive/StaticMeshComponent.h"
#include "Mesh/Static/StaticMesh.h"
#include "Mesh/Static/StaticMeshAsset.h"
#include "Materials/Material.h"
#include "Physics/Asset/BodySetup.h"
#include "Math/MathUtils.h"

#include <algorithm>
#include <cmath>

namespace
{
	bool SectionMaterialLess(const FMeshSectionDraw& A, const FMeshSectionDraw& B)
	{
		const uintptr_t AMat = reinterpret_cast<uintptr_t>(A.Material);
		const uintptr_t BMat = reinterpret_cast<uintptr_t>(B.Material);
		if (AMat != BMat)
			return AMat < BMat;

		return A.FirstIndex < B.FirstIndex;
	}

	void SortSectionDrawsByMaterial(TArray<FMeshSectionDraw>& Draws)
	{
		if (Draws.size() > 1)
		{
			std::sort(Draws.begin(), Draws.end(), SectionMaterialLess);
		}
	}

	void AddLine(TArray<FWireLine>& Lines, const FVector& A, const FVector& B)
	{
		Lines.push_back({ A, B });
	}

	void AddWireCircle(TArray<FWireLine>& Lines, const FVector& Center, const FVector& AxisA, const FVector& AxisB,
		float Radius, int32 Segments)
	{
		if (Radius <= 0.0f || Segments < 3) return;

		const float Step = 2.0f * FMath::Pi / static_cast<float>(Segments);
		FVector Prev = Center + AxisA * Radius;
		for (int32 i = 1; i <= Segments; ++i)
		{
			const float Angle = Step * static_cast<float>(i);
			FVector Next = Center + (AxisA * std::cos(Angle) + AxisB * std::sin(Angle)) * Radius;
			AddLine(Lines, Prev, Next);
			Prev = Next;
		}
	}

	void AddWireQuarterArc(TArray<FWireLine>& Lines, const FVector& Center, const FVector& StartDir, const FVector& EndDir,
		float Radius, int32 Segments)
	{
		if (Radius <= 0.0f || Segments < 1) return;

		const float Step = (FMath::Pi * 0.5f) / static_cast<float>(Segments);
		FVector Prev = Center + StartDir * Radius;
		for (int32 i = 1; i <= Segments; ++i)
		{
			const float Angle = Step * static_cast<float>(i);
			FVector Next = Center + (StartDir * std::cos(Angle) + EndDir * std::sin(Angle)) * Radius;
			AddLine(Lines, Prev, Next);
			Prev = Next;
		}
	}

	void BuildSphere(TArray<FWireLine>& Lines, const FVector& Center, const FVector& X, const FVector& Y, const FVector& Z, float Radius)
	{
		AddWireCircle(Lines, Center, X, Y, Radius, 16);
		AddWireCircle(Lines, Center, Y, Z, Radius, 16);
		AddWireCircle(Lines, Center, Z, X, Radius, 16);
	}

	void BuildBox(TArray<FWireLine>& Lines, const FVector& Center, const FVector& X, const FVector& Y, const FVector& Z, const FVector& HalfExtent)
	{
		const FVector EX = X * HalfExtent.X;
		const FVector EY = Y * HalfExtent.Y;
		const FVector EZ = Z * HalfExtent.Z;

		FVector C[8];
		C[0] = Center - EX - EY - EZ;
		C[1] = Center + EX - EY - EZ;
		C[2] = Center + EX + EY - EZ;
		C[3] = Center - EX + EY - EZ;
		C[4] = Center - EX - EY + EZ;
		C[5] = Center + EX - EY + EZ;
		C[6] = Center + EX + EY + EZ;
		C[7] = Center - EX + EY + EZ;

		AddLine(Lines, C[0], C[1]); AddLine(Lines, C[1], C[2]); AddLine(Lines, C[2], C[3]); AddLine(Lines, C[3], C[0]);
		AddLine(Lines, C[4], C[5]); AddLine(Lines, C[5], C[6]); AddLine(Lines, C[6], C[7]); AddLine(Lines, C[7], C[4]);
		AddLine(Lines, C[0], C[4]); AddLine(Lines, C[1], C[5]); AddLine(Lines, C[2], C[6]); AddLine(Lines, C[3], C[7]);
	}

	void BuildCapsule(TArray<FWireLine>& Lines, const FVector& Center, const FVector& X, const FVector& Y, const FVector& Z,
		float Radius, float HalfLength)
	{
		const FVector Top = Center + Y * HalfLength;
		const FVector Bottom = Center - Y * HalfLength;

		AddWireCircle(Lines, Top, X, Z, Radius, 16);
		AddWireCircle(Lines, Bottom, X, Z, Radius, 16);
		AddLine(Lines, Top + X * Radius, Bottom + X * Radius);
		AddLine(Lines, Top - X * Radius, Bottom - X * Radius);
		AddLine(Lines, Top + Z * Radius, Bottom + Z * Radius);
		AddLine(Lines, Top - Z * Radius, Bottom - Z * Radius);

		AddWireQuarterArc(Lines, Top, X, Y, Radius, 6);
		AddWireQuarterArc(Lines, Top, X * -1.0f, Y, Radius, 6);
		AddWireQuarterArc(Lines, Top, Z, Y, Radius, 6);
		AddWireQuarterArc(Lines, Top, Z * -1.0f, Y, Radius, 6);

		AddWireQuarterArc(Lines, Bottom, X, Y * -1.0f, Radius, 6);
		AddWireQuarterArc(Lines, Bottom, X * -1.0f, Y * -1.0f, Radius, 6);
		AddWireQuarterArc(Lines, Bottom, Z, Y * -1.0f, Radius, 6);
		AddWireQuarterArc(Lines, Bottom, Z * -1.0f, Y * -1.0f, Radius, 6);
	}

	float MaxAbsScale(const FVector& Scale)
	{
		return std::max({ std::abs(Scale.X), std::abs(Scale.Y), std::abs(Scale.Z), 0.001f });
	}
}

// ============================================================
// FStaticMeshSceneProxy
// ============================================================
FStaticMeshSceneProxy::FStaticMeshSceneProxy(UStaticMeshComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent)
{
	ProxyFlags |= EPrimitiveProxyFlags::StaticMesh;
}

UStaticMeshComponent* FStaticMeshSceneProxy::GetStaticMeshComponent() const
{
	return static_cast<UStaticMeshComponent*>(GetOwner());
}

// ============================================================
// UpdateMaterial — 머티리얼만 변경된 경우 SectionDraws 재구축
// ============================================================
void FStaticMeshSceneProxy::UpdateMaterial()
{
	RebuildSectionDraws();
}

// ============================================================
// UpdateMesh — 메시 버퍼 + 셰이더 교체 후 SectionDraws 재구축
// ============================================================
void FStaticMeshSceneProxy::UpdateMesh()
{
	MeshBuffer = GetOwner()->GetMeshBuffer();
	RebuildSectionDraws();
	RebuildCollisionLines();
}

void FStaticMeshSceneProxy::UpdateTransform()
{
	FPrimitiveSceneProxy::UpdateTransform();
	RebuildCollisionLines();
}

// ============================================================
// UpdateLOD — LOD 레벨 변경 시 MeshBuffer/SectionDraws 스왑
// ============================================================
void FStaticMeshSceneProxy::UpdateLOD(uint32 LODLevel)
{
	if (LODLevel >= LODCount) LODLevel = LODCount - 1;
	if (LODLevel == CurrentLOD) return;

	// 현재 활성 데이터를 LODData 슬롯에 swap (할당/해제 없는 O(1) 교환)
	std::swap(MeshBuffer, LODData[CurrentLOD].MeshBuffer);
	std::swap(SectionDraws, LODData[CurrentLOD].SectionDraws);

	// 새 LOD 데이터를 활성 슬롯에서 swap
	CurrentLOD = LODLevel;
	std::swap(MeshBuffer, LODData[LODLevel].MeshBuffer);
	std::swap(SectionDraws, LODData[LODLevel].SectionDraws);

}

void FStaticMeshSceneProxy::RebuildCollisionLines()
{
	CachedCollisionLines.clear();

	UStaticMeshComponent* SMC = GetStaticMeshComponent();
	UStaticMesh* Mesh = SMC ? SMC->GetStaticMesh() : nullptr;
	const UBodySetup* BodySetup = Mesh ? Mesh->GetBodySetup() : nullptr;
	if (!SMC || !BodySetup || (BodySetup->AggGeom.GetElementCount() <= 0 && !BodySetup->TriMesh.HasSourceMesh()))
	{
		return;
	}

	const FMatrix& WorldMatrix = SMC->GetWorldMatrix();
	const FQuat ComponentQuat = WorldMatrix.ToQuat().GetNormalized();
	FVector Scale = SMC->GetWorldScale();
	const FVector AbsScale(std::max(std::abs(Scale.X), 0.001f), std::max(std::abs(Scale.Y), 0.001f), std::max(std::abs(Scale.Z), 0.001f));

	auto ToWorld = [&](const FVector& Local)
	{
		return WorldMatrix.TransformPositionWithW(Local);
	};

	const FVector X = ComponentQuat.RotateVector(FVector(1, 0, 0));
	const FVector Y = ComponentQuat.RotateVector(FVector(0, 1, 0));
	const FVector Z = ComponentQuat.RotateVector(FVector(0, 0, 1));

	for (const FKSphereElem& Elem : BodySetup->AggGeom.SphereElems)
	{
		BuildSphere(CachedCollisionLines, ToWorld(Elem.Center), X, Y, Z, Elem.Radius * MaxAbsScale(Scale));
	}

	for (const FKBoxElem& Elem : BodySetup->AggGeom.BoxElems)
	{
		const FQuat ElemQuat = ComponentQuat * Elem.Rotation.ToQuaternion();
		const FVector BoxX = ElemQuat.RotateVector(FVector(1, 0, 0));
		const FVector BoxY = ElemQuat.RotateVector(FVector(0, 1, 0));
		const FVector BoxZ = ElemQuat.RotateVector(FVector(0, 0, 1));
		BuildBox(CachedCollisionLines,
			ToWorld(Elem.Center),
			BoxX,
			BoxY,
			BoxZ,
			FVector(Elem.HalfExtent.X * AbsScale.X, Elem.HalfExtent.Y * AbsScale.Y, Elem.HalfExtent.Z * AbsScale.Z));
	}

	for (const FKSphylElem& Elem : BodySetup->AggGeom.SphylElems)
	{
		const FQuat ElemQuat = ComponentQuat * Elem.Rotation.ToQuaternion();
		const FVector CapX = ElemQuat.RotateVector(FVector(1, 0, 0));
		const FVector CapY = ElemQuat.RotateVector(FVector(0, 1, 0));
		const FVector CapZ = ElemQuat.RotateVector(FVector(0, 0, 1));
		const float Radius = Elem.Radius * std::max(AbsScale.X, AbsScale.Z);
		const float HalfLength = Elem.Length * 0.5f * AbsScale.Y;
		BuildCapsule(CachedCollisionLines, ToWorld(Elem.Center), CapX, CapY, CapZ, Radius, HalfLength);
	}

	const FTriMeshCollisionData& TriMesh = BodySetup->TriMesh;
	if (TriMesh.HasSourceMesh())
	{
		for (size_t Index = 0; Index + 2 < TriMesh.Indices.size(); Index += 3)
		{
			const int32 IA = TriMesh.Indices[Index + 0];
			const int32 IB = TriMesh.Indices[Index + 1];
			const int32 IC = TriMesh.Indices[Index + 2];
			if (IA < 0 || IB < 0 || IC < 0
				|| IA >= static_cast<int32>(TriMesh.Vertices.size())
				|| IB >= static_cast<int32>(TriMesh.Vertices.size())
				|| IC >= static_cast<int32>(TriMesh.Vertices.size()))
			{
				continue;
			}

			const FVector A = ToWorld(TriMesh.Vertices[IA]);
			const FVector B = ToWorld(TriMesh.Vertices[IB]);
			const FVector C = ToWorld(TriMesh.Vertices[IC]);
			AddLine(CachedCollisionLines, A, B);
			AddLine(CachedCollisionLines, B, C);
			AddLine(CachedCollisionLines, C, A);
		}
	}
}

// ============================================================
// RebuildSectionDraws — 모든 LOD의 SectionDraws 재구축
// ============================================================
void FStaticMeshSceneProxy::RebuildSectionDraws()
{
	UStaticMeshComponent* SMC = GetStaticMeshComponent();
	UStaticMesh* Mesh = SMC->GetStaticMesh();
	if (!Mesh || !Mesh->GetStaticMeshAsset())
	{
		for (uint32 lod = 0; lod < MAX_LOD; ++lod)
		{
			LODData[lod].MeshBuffer = nullptr;
			LODData[lod].SectionDraws.clear();
		}

		LODCount = 1;
		CurrentLOD = 0;
		MeshBuffer = nullptr;
		SectionDraws.clear();

		return;
	}

	const auto& Slots = Mesh->GetStaticMaterials();
	const auto& Overrides = SMC->GetOverrideMaterials();
	LODCount = Mesh->GetLODCount();

	// 각 LOD별 SectionDraws + MeshBuffer 구축
	for (uint32 lod = 0; lod < LODCount; ++lod)
	{
		const auto& Sections = Mesh->GetLODSections(lod);
		LODData[lod].MeshBuffer = Mesh->GetLODMeshBuffer(lod);
		LODData[lod].SectionDraws.clear();
		LODData[lod].SectionDraws.reserve(Sections.size());

		for (const FStaticMeshSection& Section : Sections)
		{
			FMeshSectionDraw Draw;
			Draw.FirstIndex = Section.FirstIndex;
			Draw.IndexCount = Section.NumTriangles * 3;

			int32 i = Section.MaterialIndex;
			if (i >= 0 && i < static_cast<int32>(Slots.size()))
			{
				if (i < static_cast<int32>(Overrides.size()) && Overrides[i])
					Draw.Material = Overrides[i];
				else if (Slots[i].MaterialInterface)
					Draw.Material = Slots[i].MaterialInterface;
			}

			LODData[lod].SectionDraws.push_back(Draw);
		}

		SortSectionDrawsByMaterial(LODData[lod].SectionDraws);
	}

	// LOD0을 활성 슬롯으로 설정
	CurrentLOD = 0;
	std::swap(MeshBuffer, LODData[0].MeshBuffer);
	std::swap(SectionDraws, LODData[0].SectionDraws);

}
