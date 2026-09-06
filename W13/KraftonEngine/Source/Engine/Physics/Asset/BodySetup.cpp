#include "Physics/Asset/BodySetup.h"

#include "Mesh/Static/StaticMeshAsset.h"
#include "Physics/IPhysicsScene.h"
#include "Physics/PhysicsInterfaceTypes.h"
#include "Physics/PhysXPhysicsScene.h"

#include <utility>

bool UBodySetup::AddShapesToRigidActor(IPhysicsScene* Scene, FPhysicsActorHandle ActorHandle,
	const FVector& Scale3D, const FTransform& RelativeTM, const FTransform& WorldTransform, void* UserData,
	EShapeSetupMode ShapeSetupMode, UPrimitiveComponent* FilterSourceComponent, bool bAllowTriMesh) const
{
	if (!Scene || !ActorHandle.IsValid())
	{
		return false;
	}

	// trimesh 는 static/kinematic 만 백업 가능 — bAllowTriMesh 가 켜진 경우에만 사용한다.
	// 쿡된 trimesh 가 있으면 그것을, 없으면 AggGeom(convex/primitive)을 attach.
	const bool bUseTriMesh = bAllowTriMesh && HasTriMeshCollision();
	if (!bUseTriMesh && AggGeom.GetElementCount() <= 0)
	{
		return false;
	}

	FGeometryAddParams AddParams;
	AddParams.Scale = Scale3D;
	AddParams.LocalTransform = RelativeTM;
	AddParams.WorldTransform = WorldTransform;
	AddParams.UserData = UserData;
	AddParams.ShapeSetupMode = ShapeSetupMode;
	AddParams.FilterSourceComponent = FilterSourceComponent;
	if (bUseTriMesh)
	{
		AddParams.CookedTriMesh = &TriMesh.CookedData;
	}
	else
	{
		AddParams.Geometry = &AggGeom;
	}

	return Scene->AddGeometry(ActorHandle, AddParams);
}

bool UBodySetup::BuildTriMeshFromStaticMesh(const FStaticMesh& Mesh, FString* OutError)
{
	auto SetError = [OutError](const char* Message)
	{
		if (OutError)
		{
			*OutError = Message ? Message : "";
		}
	};

	if (Mesh.Vertices.size() < 3 || Mesh.Indices.size() < 3)
	{
		SetError("StaticMesh has no triangle data.");
		return false;
	}

	FTriMeshCollisionData NewTriMesh;
	NewTriMesh.Vertices.reserve(Mesh.Vertices.size());
	for (const FNormalVertex& Vertex : Mesh.Vertices)
	{
		NewTriMesh.Vertices.push_back(Vertex.pos);
	}

	NewTriMesh.Indices.reserve(Mesh.Indices.size());
	int32 ValidTriangleCount = 0;
	for (size_t Index = 0; Index + 2 < Mesh.Indices.size(); Index += 3)
	{
		const uint32 IA = Mesh.Indices[Index + 0];
		const uint32 IB = Mesh.Indices[Index + 1];
		const uint32 IC = Mesh.Indices[Index + 2];
		if (IA >= NewTriMesh.Vertices.size() || IB >= NewTriMesh.Vertices.size() || IC >= NewTriMesh.Vertices.size())
		{
			continue;
		}

		const FVector& A = NewTriMesh.Vertices[IA];
		const FVector& B = NewTriMesh.Vertices[IB];
		const FVector& C = NewTriMesh.Vertices[IC];
		const FVector Normal = (B - A).Cross(C - A);
		const float AreaSq = Normal.Dot(Normal);
		if (AreaSq <= 1.0e-12f)
		{
			continue;
		}

		NewTriMesh.Indices.push_back(static_cast<int32>(IA));
		NewTriMesh.Indices.push_back(static_cast<int32>(IB));
		NewTriMesh.Indices.push_back(static_cast<int32>(IC));
		++ValidTriangleCount;
	}

	if (ValidTriangleCount == 0)
	{
		SetError("StaticMesh has no valid non-degenerate triangles.");
		return false;
	}

	TArray<uint8> CookedBytes;
	if (!FPhysXPhysicsScene::CookTriangleMesh(NewTriMesh.Vertices, NewTriMesh.Indices, CookedBytes, OutError))
	{
		return false;
	}

	NewTriMesh.CookedData = std::move(CookedBytes);
	NewTriMesh.SourceVertexCount = static_cast<int32>(Mesh.Vertices.size());
	NewTriMesh.SourceTriangleCount = ValidTriangleCount;

	TriMesh = std::move(NewTriMesh);
	SetError("");
	return true;
}
