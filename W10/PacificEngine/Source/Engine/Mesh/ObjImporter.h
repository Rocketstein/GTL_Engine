// 메시 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "Core/CoreTypes.h"
#include "Math/Vector.h"
#include "Mesh/MeshImporterCommon.h"

struct FStaticMesh;
struct FStaticMeshSection;
struct FStaticMaterial;

// Raw Data — OBJ 파싱 직후 상태
struct FObjInfo
{
    TArray<FVector> Positions;    // v
    TArray<FVector2> UVs;         // vt
    TArray<FVector> Normals;      // vn
    TArray<uint32> PosIndices;    // f - position index
    TArray<uint32> UVIndices;     // f - uv index
    TArray<uint32> NormalIndices; // f - normal index

    FString ObjectName; // object name (optional)

    FString MaterialLibraryFilePath;
    TArray<FStaticMeshSection> Sections;
};

// MTL 재질 정보
struct FObjMaterialInfo
{
    FString MaterialSlotName = "None";      // newmtl
    FVector Kd = FVector(1.0f, 1.0f, 1.0f); // diffuse color
    FString map_Kd;                         // diffuse texture file path
    FString map_Bump;                       // normal texture file path
    FString map_Ks;                         // specular texture file path

    FVector Ka = FVector(0.0f, 0.0f, 0.0f); // ambient color
    FVector Ks = FVector(1.0f, 1.0f, 1.0f); // specular color
    float Ns = 32.0f;                       // specular exponent
    float Ni = 1.0f;                        // optical density
    int32 illum = 2;                        // illumination model
};

// OBJ/MTL 파싱 + Raw→Cooked 변환
struct FObjImporter
{
    static bool Import(const FString& ObjFilePath, FStaticMesh& OutMesh, TArray<FStaticMaterial>& OutMaterials);
    static bool Import(const FString& ObjFilePath, const FImportOptions& Options, FStaticMesh& OutMesh, TArray<FStaticMaterial>& OutMaterials);

private:
    static bool ParseObj(const FString& ObjFilePath, FObjInfo& OutObjInfo);
    static bool ParseMtl(const FString& MtlFilePath, TArray<FObjMaterialInfo>& OutMaterials);
    static bool Convert(const FObjInfo& ObjInfo, const TArray<FObjMaterialInfo>& MtlInfos, const FImportOptions& Options, FStaticMesh& OutMesh, TArray<FStaticMaterial>& OutMaterials);

    static FString ConvertMtlInfoToJson(const FObjMaterialInfo* MtlInfo);
};
