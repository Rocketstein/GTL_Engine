#pragma once
#include "Core/Containers/Array.h"
#include "Core/Geometry/Primitives/AABB.h"
#include "Core/Geometry/Primitives/Triangle.h"
#include "Core/Math/Vector3.h"

// BLAS용 노드 (이진 트리)
struct FBLASNode
{
    FVector3 Min;
    FVector3 Max;
    int32    LeftChild = -1;
    int32    RightChild = -1;
    int32    TriStart = 0;
    int32    TriCount = 0;

    bool IsLeaf() const { return TriCount > 0; }
};

struct alignas(32) FStaticMeshBVH8Node
{
    float CenterX[8];
    float ExtentX[8];
    float CenterY[8];
    float ExtentY[8];
    float CenterZ[8];
    float ExtentZ[8];

    // 양수면 자식 노드의 인덱스, 음수(~StartIdx)면 리프 노드의 시작 인덱스
    int Children[8];

    // 리프 노드일 때 해당 방에 삼각형이 몇 개 있는지 (최대 8)
    int TriangleCounts[8];
};

class FMeshBLAS
{
  public:
    // 메쉬 로드 시 1회 호출 (기존의 SAH 로직을 활용해 삼각형 단위로 분할)
    void Build(const TArray<Geometry::FTriangle> &InTriangles);

    // 로컬 공간으로 변환된 Ray와 충돌 검사 (HitT는 충돌 거리)
    bool Raycast(const FVector3 &RayOrigin, const FVector3 &RayDir, float &OutHitT,
                 int32 &OutHitTriIndex) const;

    void CompressToBVH8(TArray<FStaticMeshBVH8Node>& OutBVH8Nodes);
    int CollapseRecursive(int CurrentNodeIdx, TArray<FStaticMeshBVH8Node>& OutBVH8Nodes);

  private:
    TArray<FBLASNode>           Nodes;
    TArray<Geometry::FTriangle> SortedTriangles; // BVH 구조에 맞게 재정렬된 삼각형들

    // 재귀적 빌더 및 내부 교차 검사 함수 선언 (cpp에서 구현)
    void BuildRecursive(int32 NodeIdx, int32 Start, int32 End);
    bool RaycastNode(int32 NodeIdx, const FVector3 &RayOrigin, const FVector3 &RayDir,
                     const FVector3 &InvDir, float &HitT, int32 &HitTriIdx) const;

    void BakeTriangleSoA(struct FStaticMeshRaycastData &OutData);
};