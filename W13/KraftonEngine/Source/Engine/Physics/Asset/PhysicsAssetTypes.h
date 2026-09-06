#pragma once

#include "Core/Types/CoreTypes.h"
#include "Math/Vector.h"
#include "Math/Rotator.h"
#include "Object/Reflection/ObjectMacros.h"
#include "Object/Reflection/UStruct.h"
#include "Serialization/Archive.h"

#include "Source/Engine/Physics/Asset/PhysicsAssetTypes.generated.h"

// =====================================================================================
// FKAggregateGeom — 한 바디(= 본 1개)의 충돌 프리미티브 모음.
// 발제의 FKAggregateGeom(Sphere / Box / Sphyl / Convex) 대응. 모든 요소 좌표는
// 소속 UBodySetup 의 "본 로컬 공간" 기준이다.
//
// [B 제안 / A 확정 예정] 데이터 계층(2-1)의 오너는 A. 다만 에디터(2-2)·랙돌(3)이
// 최대 소비자이므로 B 가 시그니처 초안을 먼저 제시해 인터페이스 협상 비용을 줄인다.
// Convex 는 이번 주 필수 라인이 아니라 제외(필요 시 A 가 추가).
// =====================================================================================

// 구(Sphere)
USTRUCT()
struct FKSphereElem
{
	GENERATED_BODY()

	UPROPERTY(Edit, Save, Category="Shape", DisplayName="Center")
	FVector Center = FVector(0.f, 0.f, 0.f);
	UPROPERTY(Edit, Save, Category="Shape", DisplayName="Radius", Min=0.f, Speed=0.1f)
	float   Radius = 1.f;
};

// 박스(Box)
USTRUCT()
struct FKBoxElem
{
	GENERATED_BODY()

	UPROPERTY(Edit, Save, Category="Shape", DisplayName="Center")
	FVector  Center     = FVector(0.f, 0.f, 0.f);
	UPROPERTY(Edit, Save, Category="Shape", DisplayName="Rotation")
	FRotator Rotation   = FRotator(0.f, 0.f, 0.f);
	UPROPERTY(Edit, Save, Category="Shape", DisplayName="Half Extent", Min=0.f, Speed=0.1f)
	FVector  HalfExtent = FVector(0.5f, 0.5f, 0.5f);
};

// 캡슐(Sphyl) — 랙돌 본 주력 프리미티브. PxCapsuleGeometry(radius, halfHeight) 로 환산.
USTRUCT()
struct FKSphylElem
{
	GENERATED_BODY()

	UPROPERTY(Edit, Save, Category="Shape", DisplayName="Center")
	FVector  Center   = FVector(0.f, 0.f, 0.f);
	UPROPERTY(Edit, Save, Category="Shape", DisplayName="Rotation")
	FRotator Rotation = FRotator(0.f, 0.f, 0.f);
	UPROPERTY(Edit, Save, Category="Shape", DisplayName="Radius", Min=0.f, Speed=0.1f)
	float    Radius   = 0.5f;
	UPROPERTY(Edit, Save, Category="Shape", DisplayName="Length", Min=0.f, Speed=0.1f)
	float    Length   = 1.f;   // 원통부 길이(반구 제외)
};

USTRUCT()
struct FKAggregateGeom
{
	GENERATED_BODY()

	UPROPERTY(Edit, Save, Category="Primitives", DisplayName="Spheres")
	TArray<FKSphereElem> SphereElems;
	UPROPERTY(Edit, Save, Category="Primitives", DisplayName="Boxes")
	TArray<FKBoxElem>    BoxElems;
	UPROPERTY(Edit, Save, Category="Primitives", DisplayName="Capsules")
	TArray<FKSphylElem>  SphylElems;   // 랙돌 주력

	int32 GetElementCount() const
	{
		return static_cast<int32>(SphereElems.size() + BoxElems.size() + SphylElems.size());
	}
};

struct FTriMeshCollisionData
{
	TArray<FVector> Vertices;
	TArray<int32> Indices;
	TArray<uint8> CookedData;
	int32 SourceVertexCount = 0;
	int32 SourceTriangleCount = 0;
	int32 CookedFormatVersion = 1;

	void Clear()
	{
		Vertices.clear();
		Indices.clear();
		CookedData.clear();
		SourceVertexCount = 0;
		SourceTriangleCount = 0;
	}

	bool HasSourceMesh() const
	{
		return Vertices.size() >= 3 && Indices.size() >= 3;
	}

	bool HasCookedData() const
	{
		return !CookedData.empty();
	}
};

inline FArchive& operator<<(FArchive& Ar, FTriMeshCollisionData& Data)
{
	Ar << Data.Vertices;
	Ar << Data.Indices;
	Ar << Data.CookedData;
	Ar << Data.SourceVertexCount;
	Ar << Data.SourceTriangleCount;
	Ar << Data.CookedFormatVersion;
	return Ar;
}
