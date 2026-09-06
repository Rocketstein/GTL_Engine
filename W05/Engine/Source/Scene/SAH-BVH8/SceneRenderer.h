#pragma once

#include "SceneSoA.h"
#include "Core/Containers/Array.h"
#include <immintrin.h>
#include <vector>
#include "Core/Math/Vector3.h"
#include "Core/Math/Matrix.h"

constexpr int EMPTY_NODE = -2147483648;

struct FPlane
{
    float nx, ny, nz, d;
};

struct FVisibleObject
{
    int   ObjectID;
    float DistanceSq;

    bool operator<(const FVisibleObject &Other) const { return DistanceSq < Other.DistanceSq; }
};

struct FBVHNode
{
    float MinX, MinY, MinZ;
    float MaxX, MaxY, MaxZ;
    int   LeftChild, RightChild;

    int SoA_StartIndex;
    int ObjectCount;

    bool IsLeaf() const { return LeftChild == -1; }
};

struct alignas(32) FBVH8Node
{
    // [최적화] 트리 탐색 속도를 올리기 위해 옥트리 노드도 Center/Extent 구조로 교체
    float CenterX[8];
    float ExtentX[8];
    float CenterY[8];
    float ExtentY[8];
    float CenterZ[8];
    float ExtentZ[8];

    int Children[8];
};

struct FRaycastHit
{
    bool  bHit = false;
    float Distance = 1e30f;
    int32 HitObjectID = -1; // 맞춘 컴포넌트의 인덱스
    int32 HitTriangleIndex = -1;

    // Picking 경로 진단용 카운터
    uint32 LeafObjectsVisited = 0;
    uint32 LeafAABBRejected = 0;
    uint32 BLASTests = 0;
    uint32 BLASHits = 0;
};

struct FCpuOcclusionCullStats
{
    uint32 NodeFrustumTested = 0;
    uint32 NodeFrustumCulled = 0;
    uint32 NodeFrustumTestedObjects = 0;
    uint32 NodeFrustumCulledObjects = 0;
    uint32 ObjectFrustumInput = 0;
    uint32 ObjectFrustumCulled = 0;
    uint32 NodeOcclusionTested = 0;
    uint32 NodeOcclusionCulled = 0;
    uint32 NodeOcclusionTestedObjects = 0;
    uint32 NodeOcclusionCulledObjects = 0;
    uint32 InputCandidates = 0;
    uint32 VisibleObjects = 0;
    uint32 CulledObjects = 0;
    uint32 DistanceLodInput = 0;
    uint32 DistanceLodCulled = 0;
    uint32 OcclusionBufferWidth = 0;
    uint32 OcclusionBufferHeight = 0;
};

struct FCpuCullingOptions
{
    bool bEnableNodeFrustumCulling = false;
    bool bEnableObjectFrustumCulling = false;
    bool bEnableNodeOcclusionCulling = false;
    bool bEnableObjectOcclusionCulling = false;
    bool bEnableDistanceLodCulling = false; // TODO: not implemented yet
    uint32 MinChildrenForNodeFrustumTest = 3;
    uint32 MinChildrenForNodeOcclusionTest = 3;
    float OcclusionTestBias = 0.0002f;
    float MinOccluderScreenCoverage = 0.0f;
    float MinOccluderExtent = 0.0f;
    // 0 means "unlimited". Avoid over-restricting occluders in far camera views.
    float MaxOccluderDistance = 0.0f;
    // 0 means "disabled". Near objects are often unstable for CPU AABB occlusion, so skip testing.
    float NearOcclusionSkipDistance = 0.0f;
    // Additional bias scaled by projected NDC radius.
    float OcclusionScreenRadiusBiasScale = 0.001f;
    // Upper bound for dynamic bias to avoid disabling occlusion entirely.
    float MaxDynamicOcclusionBias = 0.001f;
    // Large projected node AABBs are prone to false-occlusion; skip node occlusion above this coverage.
    float MaxNodeOcclusionScreenCoverage = 0.5f;
    // AABB 기반 과대 투영을 줄이기 위한 occlusion 전용 extent 스케일.
    float OcclusionBoundsExtentScale = 0.85f;
    uint32 ViewportWidth = 0;
    uint32 ViewportHeight = 0;
};

void CullAndSortWithBVH8(FSceneDataSoA *SoA, FBVH8Node *BVH8Nodes, int RootNodeIndex,
                         const FVector3 &CamLoc, const FPlane FrustumPlanes[6],
                         const FMatrix &ViewProjectionMatrix,
                         std::vector<FVisibleObject> &OutRenderQueue,
                         FCpuOcclusionCullStats *OutCpuOcclusionStats = nullptr,
                         const FCpuCullingOptions *InCpuCullingOptions = nullptr);

FRaycastHit RaycastScene(FSceneDataSoA *SoA, FBVH8Node *BVH8Nodes, int RootNodeIndex,
                         const FVector3 &RayOrigin, const FVector3 &RayDir);
