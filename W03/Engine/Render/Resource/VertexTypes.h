#pragma once

#include "Math/Vector.h"
#include "Math/Matrix.h"

/*
	Vertex 구조체들을 정의하는 Header입니다.
	추후에 다양한 Vertex 구조체들을 추가할 수 있습니다.
*/

struct FVertex
{ 
	FVector Position;
	FVector4 Color;
    float U, V;
	int SubID;
};

struct FUVVertex
{
    FVector Position;
    float u, v;
};

struct FOverlayVertex
{
	float X, Y;
};

struct FMeshData
{
	TArray<FVertex> Vertices;
	TArray<uint32> Indices;
};


struct FUVMeshData
{
    TArray<FUVVertex> Vertices;
    TArray<uint32> Indices;
};


struct FFontInstance

{
    FMatrix MVP;
    float PenX, PenY;
    float CellW, CellH; 
    float StartU, StartV;
    float USize, VSize;
    FVector4 Color;
};
