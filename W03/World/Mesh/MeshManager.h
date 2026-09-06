#pragma once

#include <iostream>

#include "Core/CoreTypes.h"
#include "Math/Vector.h"
#include "Render/Resource/VertexTypes.h"


class FMeshManager
{
private:
	FMeshManager() = default;

	static FMeshData CubeMeshData;
	static FMeshData BoxMeshData;
	static FMeshData PlaneMeshData;
	static FMeshData SphereMeshData;
	static FMeshData TranslationGizmoMeshData;
	static FMeshData RotationGizmoMeshData;
	static FMeshData ScaleGizmoMeshData;
	static FUVMeshData UVRectMeshData;
	static FMeshData QuadVertexData;
	//static FMeshData AxisMeshData;
	// 
	//static FMeshData GridMeshData;
	
	static FMeshData MouseOverlayMeshData;
	static FMeshData NullMeshData;

	static void CreateCube();
	static void CreateBox();
	static void CreatePlane();
	static void CreateSphere(int slices = 20, int stacks = 20);
	static void CreateTranslationGizmo();
	static void CreateRotationGizmo();
	static void CreateScaleGizmo();
	static void CreateUVRect();
	static void CreateQuad();

	//static void CreateAxis();
	//static void CreateGrid();

	static void CreateMouseOverlay();
	static void CreateNull();


#if TEST

	//static void CreateStandfordBunny();
	//static void LoadObj(const char* path, FMeshData& outMeshData);

#endif

	static bool bIsInitialized;

public:
	static FMeshManager& Get()
	{
		static FMeshManager instance;
		return instance;
	}

	FMeshManager(const FMeshManager&) = delete;
	FMeshManager& operator=(const FMeshManager&) = delete;

	static void Initialize();
	static const FMeshData& GetCube(){return Get().CubeMeshData; }
	static const FMeshData& GetPlane(){ return Get().PlaneMeshData; }
	static const FMeshData& GetSphere(){ return Get().SphereMeshData; }
	static const FMeshData& GetTranslationGizmo() { return Get().TranslationGizmoMeshData; }
	static const FMeshData& GetRotationGizmo() { return Get().RotationGizmoMeshData; }
	static const FMeshData& GetScaleGizmo() { return Get().ScaleGizmoMeshData; }
	static const FMeshData& GetBox() { return Get().BoxMeshData; };
	static const FMeshData& GetQuad() { return Get().QuadVertexData; };
	static const FUVMeshData& GetUVRect() { return Get().UVRectMeshData; };
	//static FUVMeshData& UpdateUVRectMesh();

	static const FMeshData& GetMouseOverlay() { return Get().MouseOverlayMeshData; }
	static const FMeshData& GetNull() { return Get().NullMeshData; }
};

