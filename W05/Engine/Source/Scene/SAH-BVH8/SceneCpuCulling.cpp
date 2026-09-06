#include "Core/Stats/TimingStats.h"
#include "Engine/Asset/StaticMesh.h"
#include "Engine/Component/StaticMeshComponent.h"
#include "MeshBLAS.h"
#include "SceneRenderer.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <unordered_map>
#include <vector>

namespace
{
    struct FProjectedBounds
    {
        bool  bValid = false;
        bool  bNearPlaneClipped = false;
        bool  bViewportEdgeClipped = false;
        float MinX = 0.0f;
        float MinY = 0.0f;
        float MaxX = 0.0f;
        float MaxY = 0.0f;
        float MinDepth = 1.0f;
        float MaxDepth = 1.0f;
    };

    struct FCullingCandidate
    {
        int              ObjectID = -1;
        float            DistanceSq = 0.0f;
        float            MaxExtent = 0.0f;
        float            ScreenRadiusNdc = 0.0f;
        bool             bSkipOcclusionTest = false;
        FProjectedBounds ProjectedBounds;
    };

    constexpr int DefaultOcclusionBufferWidth = 256;
    constexpr int DefaultOcclusionBufferHeight = 144;
    constexpr int MinOcclusionBufferWidth = 64;
    constexpr int MinOcclusionBufferHeight = 36;
    constexpr int OcclusionBufferDownsampleDivisor = 8;
    // 기본 바이어스가 크면 "거의 항상 visible"로 분류되어 실제 컬링이 일어나지 않는다.
    constexpr float DefaultOccluderDepthBias = 0.0f;

    struct FOcclusionRasterRect
    {
        int MinX = 0;
        int MinY = 0;
        int MaxX = 0;
        int MaxY = 0;
    };

    struct FOcclusionAutoTuning
    {
        float OcclusionTestBias = 0.0f;
        float MinOccluderScreenCoverage = 0.0f;
        float MinOccluderExtent = 0.0f;
        float NearOcclusionSkipDistanceSq = 0.0f;
        float OcclusionScreenRadiusBiasScale = 0.0f;
        float MaxDynamicOcclusionBias = 0.0f;
        float MaxNodeOcclusionScreenCoverage = 1.0f;
        float OcclusionBoundsExtentScale = 1.0f;
        int   MinChildrenForNodeOcclusionTest = 1;
    };

    float ComputeProjectedCoverage(const FProjectedBounds &ProjectedBounds)
    {
        const float ScreenWidth = ProjectedBounds.MaxX - ProjectedBounds.MinX;
        const float ScreenHeight = ProjectedBounds.MaxY - ProjectedBounds.MinY;
        return (ScreenWidth * ScreenHeight) * 0.25f;
    }

    float ComputeProjectedScreenRadiusNdc(const FProjectedBounds &ProjectedBounds)
    {
        const float NdcWidth = ProjectedBounds.MaxX - ProjectedBounds.MinX;
        const float NdcHeight = ProjectedBounds.MaxY - ProjectedBounds.MinY;
        return 0.5f * (std::max)(NdcWidth, NdcHeight);
    }

    FVector3 BuildOcclusionProjectionExtent(const FVector3 &RawExtent, float OcclusionExtentScale)
    {
        const float Scale = (std::clamp)(OcclusionExtentScale, 0.5f, 1.0f);
        return FVector3((std::max)(RawExtent.X * Scale, 1e-3f),
                        (std::max)(RawExtent.Y * Scale, 1e-3f),
                        (std::max)(RawExtent.Z * Scale, 1e-3f));
    }

    FProjectedBounds ProjectAABBToNdcRect(const FVector3 &Center, const FVector3 &Extent,
                                          const FMatrix &ViewProjectionMatrix)
    {
        std::array<FVector3, 8> Corners = {
            FVector3(Center.X - Extent.X, Center.Y - Extent.Y, Center.Z - Extent.Z),
            FVector3(Center.X + Extent.X, Center.Y - Extent.Y, Center.Z - Extent.Z),
            FVector3(Center.X - Extent.X, Center.Y + Extent.Y, Center.Z - Extent.Z),
            FVector3(Center.X + Extent.X, Center.Y + Extent.Y, Center.Z - Extent.Z),
            FVector3(Center.X - Extent.X, Center.Y - Extent.Y, Center.Z + Extent.Z),
            FVector3(Center.X + Extent.X, Center.Y - Extent.Y, Center.Z + Extent.Z),
            FVector3(Center.X - Extent.X, Center.Y + Extent.Y, Center.Z + Extent.Z),
            FVector3(Center.X + Extent.X, Center.Y + Extent.Y, Center.Z + Extent.Z),
        };

        FProjectedBounds Result;
        Result.MinX = 1.0f;
        Result.MinY = 1.0f;
        Result.MaxX = -1.0f;
        Result.MaxY = -1.0f;
        Result.MinDepth = 1.0f;
        Result.MaxDepth = 0.0f;

        int NumProjectedCorners = 0;
        for (const FVector3 &Corner : Corners)
        {
            const DirectX::XMVECTOR Clip = DirectX::XMVector4Transform(
                DirectX::XMVectorSet(Corner.X, Corner.Y, Corner.Z, 1.0f),
                ViewProjectionMatrix.ToXMMatrix());

            const float W = DirectX::XMVectorGetW(Clip);
            if (W <= 1e-5f)
            {
                Result.bNearPlaneClipped = true;
                // AABB 일부가 near plane을 걸칠 수 있다.
                // 이전 코드는 코너 하나라도 W<=0이면 즉시 invalid 처리해서
                // CPU occlusion 테스트/occluder 기여가 모두 스킵되는 문제가 있었다.
                // 뒤쪽 코너만 제외하고 앞쪽 코너로 conservative하게 진행한다.
                continue;
            }
            ++NumProjectedCorners;

            const float InvW = 1.0f / W;
            const float X = DirectX::XMVectorGetX(Clip) * InvW;
            const float Y = DirectX::XMVectorGetY(Clip) * InvW;
            const float Z = DirectX::XMVectorGetZ(Clip) * InvW;

            Result.MinX = (std::min)(Result.MinX, X);
            Result.MinY = (std::min)(Result.MinY, Y);
            Result.MaxX = (std::max)(Result.MaxX, X);
            Result.MaxY = (std::max)(Result.MaxY, Y);
            Result.MinDepth = (std::min)(Result.MinDepth, Z);
            Result.MaxDepth = (std::max)(Result.MaxDepth, Z);
        }
        if (NumProjectedCorners == 0)
        {
            return {};
        }

        const float UnclampedMinX = Result.MinX;
        const float UnclampedMinY = Result.MinY;
        const float UnclampedMaxX = Result.MaxX;
        const float UnclampedMaxY = Result.MaxY;

        Result.MinX = (std::max)(Result.MinX, -1.0f);
        Result.MinY = (std::max)(Result.MinY, -1.0f);
        Result.MaxX = (std::min)(Result.MaxX, 1.0f);
        Result.MaxY = (std::min)(Result.MaxY, 1.0f);
        Result.MinDepth = (std::max)(0.0f, Result.MinDepth);
        Result.MaxDepth = (std::min)(1.0f, Result.MaxDepth);
        Result.bViewportEdgeClipped = (UnclampedMinX < -1.0f) || (UnclampedMinY < -1.0f) ||
                                      (UnclampedMaxX > 1.0f) || (UnclampedMaxY > 1.0f);

        Result.bValid = (Result.MinX < Result.MaxX) && (Result.MinY < Result.MaxY) &&
                        (Result.MinDepth <= Result.MaxDepth);
        return Result;
    }

    bool TryBuildRasterRect(const FProjectedBounds &ProjectedBounds, int BufferWidth,
                            int BufferHeight, FOcclusionRasterRect &OutRect)
    {
        if (!ProjectedBounds.bValid)
        {
            return false;
        }

        // Sub-pixel projected bounds were being dropped at far distance because min/max mapped to
        // the same integer texel. Expand with floor/ceil and keep at least 1x1 to make tiny
        // far objects/nodes still testable.
        OutRect.MinX = static_cast<int>(std::floor(((ProjectedBounds.MinX * 0.5f) + 0.5f) *
                                                   static_cast<float>(BufferWidth)));
        OutRect.MaxX = static_cast<int>(std::ceil(((ProjectedBounds.MaxX * 0.5f) + 0.5f) *
                                                  static_cast<float>(BufferWidth)));
        OutRect.MinY = static_cast<int>(std::floor(((-ProjectedBounds.MaxY * 0.5f) + 0.5f) *
                                                   static_cast<float>(BufferHeight)));
        OutRect.MaxY = static_cast<int>(std::ceil(((-ProjectedBounds.MinY * 0.5f) + 0.5f) *
                                                  static_cast<float>(BufferHeight)));

        OutRect.MinX = (std::max)(0, OutRect.MinX);
        OutRect.MinY = (std::max)(0, OutRect.MinY);
        OutRect.MaxX = (std::min)(BufferWidth, OutRect.MaxX);
        OutRect.MaxY = (std::min)(BufferHeight, OutRect.MaxY);

        if (OutRect.MinX == OutRect.MaxX && OutRect.MinX < BufferWidth)
        {
            ++OutRect.MaxX;
        }
        if (OutRect.MinY == OutRect.MaxY && OutRect.MinY < BufferHeight)
        {
            ++OutRect.MaxY;
        }
        return OutRect.MinX < OutRect.MaxX && OutRect.MinY < OutRect.MaxY;
    }

    FOcclusionAutoTuning BuildOcclusionAutoTuning(const FCpuCullingOptions &CpuCullingOptions,
                                                  int BufferWidth, int BufferHeight,
                                                  uint32 TotalObjectCount)
    {
        FOcclusionAutoTuning Result;
        Result.OcclusionTestBias = (std::max)(0.0f, CpuCullingOptions.OcclusionTestBias);
        Result.MinOccluderScreenCoverage =
            (std::max)(0.0f, CpuCullingOptions.MinOccluderScreenCoverage);
        Result.MinOccluderExtent = (std::max)(0.0f, CpuCullingOptions.MinOccluderExtent);
        Result.NearOcclusionSkipDistanceSq =
            (CpuCullingOptions.NearOcclusionSkipDistance > 0.0f)
                ? CpuCullingOptions.NearOcclusionSkipDistance *
                      CpuCullingOptions.NearOcclusionSkipDistance
                : 0.0f;
        Result.OcclusionScreenRadiusBiasScale =
            (std::max)(0.0f, CpuCullingOptions.OcclusionScreenRadiusBiasScale);
        Result.MaxDynamicOcclusionBias = (std::max)(0.0f, CpuCullingOptions.MaxDynamicOcclusionBias);
        Result.MaxDynamicOcclusionBias =
            (std::max)(Result.MaxDynamicOcclusionBias, Result.OcclusionTestBias);
        Result.MaxNodeOcclusionScreenCoverage =
            (std::max)(0.0f, CpuCullingOptions.MaxNodeOcclusionScreenCoverage);
        Result.OcclusionBoundsExtentScale =
            (std::clamp)(CpuCullingOptions.OcclusionBoundsExtentScale, 0.5f, 1.0f);
        Result.MinChildrenForNodeOcclusionTest =
            (std::max)(1, static_cast<int>(CpuCullingOptions.MinChildrenForNodeOcclusionTest));

        const float BufferPixelCount = static_cast<float>(BufferWidth * BufferHeight);
        const float ObjectDensity = (BufferPixelCount > 0.0f)
                                        ? static_cast<float>(TotalObjectCount) / BufferPixelCount
                                        : 0.0f;

        if (CpuCullingOptions.bEnableNodeOcclusionCulling ||
            CpuCullingOptions.bEnableObjectOcclusionCulling)
        {
            // Dense scenes benefit from allowing smaller/farther occluders and activating node
            // occlusion a bit earlier.
            const float DensityFactor = (std::min)(1.0f, ObjectDensity / 1.5f);
            Result.MinOccluderScreenCoverage =
                (std::max)(0.0f, Result.MinOccluderScreenCoverage * (1.0f - (0.6f * DensityFactor)));
            Result.MinOccluderExtent =
                (std::max)(0.0f, Result.MinOccluderExtent * (1.0f - (0.45f * DensityFactor)));
            Result.MinChildrenForNodeOcclusionTest =
                (std::max)(1, Result.MinChildrenForNodeOcclusionTest - (DensityFactor >= 0.4f ? 1 : 0));

            // Coarse occlusion buffers can be slightly optimistic, but large bias values quickly
            // disable occlusion entirely ("always visible" path in IsFullyOccluded).
            // Keep adaptive bias very small so auto-tuning never suppresses culling.
            if (CpuCullingOptions.ViewportWidth > 0 && CpuCullingOptions.ViewportHeight > 0)
            {
                const float ViewportPixels = static_cast<float>(CpuCullingOptions.ViewportWidth) *
                                             static_cast<float>(CpuCullingOptions.ViewportHeight);
                const float BufferScale =
                    (BufferPixelCount > 0.0f) ? (ViewportPixels / BufferPixelCount) : 1.0f;
                const float CoarseFactor = (std::max)(0.0f, BufferScale - 1.0f);
                Result.OcclusionTestBias =
                    (std::min)(0.001f, Result.OcclusionTestBias + (0.00001f * CoarseFactor));
            }
        }

        return Result;
    }

    bool IsFullyOccluded(const FProjectedBounds   &ProjectedBounds,
                         const std::vector<float> &DepthBuffer, int BufferWidth, int BufferHeight,
                         float OcclusionTestBias)
    {
        if (ProjectedBounds.bNearPlaneClipped)
        {
            // Near plane을 걸친 AABB는 투영 사각형이 실제보다 과장될 수 있어
            // 오클루전 test 대상에서 제외해 과컬링을 방지한다.
            return false;
        }
        FOcclusionRasterRect Rect;
        if (!TryBuildRasterRect(ProjectedBounds, BufferWidth, BufferHeight, Rect))
        {
            return false;
        }

        for (int Y = Rect.MinY; Y < Rect.MaxY; ++Y)
        {
            for (int X = Rect.MinX; X < Rect.MaxX; ++X)
            {
                const float CellDepth = DepthBuffer[static_cast<size_t>(Y * BufferWidth + X)];
                if (CellDepth >= 1.0f || ProjectedBounds.MinDepth <= CellDepth + OcclusionTestBias)
                {
                    return false;
                }
            }
        }

        return true;
    }

    void RasterizeOccluder(const FCullingCandidate &Candidate, std::vector<float> &DepthBuffer,
                           int BufferWidth, int BufferHeight, float MinOccluderScreenCoverage,
                           float MinOccluderExtent, float MaxOccluderDistanceSq)
    {
        const FProjectedBounds &ProjectedBounds = Candidate.ProjectedBounds;
        if (ProjectedBounds.bNearPlaneClipped)
        {
            // Near plane 교차 AABB를 occluder로 쓰면 과도한 화면 점유/깊이로
            // 뒤쪽 오브젝트가 잘못 컬링될 수 있으므로 보수적으로 스킵한다.
            return;
        }
        FOcclusionRasterRect Rect;
        if (!TryBuildRasterRect(ProjectedBounds, BufferWidth, BufferHeight, Rect))
        {
            return;
        }

        const float ScreenWidth = ProjectedBounds.MaxX - ProjectedBounds.MinX;
        const float ScreenHeight = ProjectedBounds.MaxY - ProjectedBounds.MinY;
        const float Coverage = (ScreenWidth * ScreenHeight) * 0.25f;
        if (Coverage < MinOccluderScreenCoverage)
        {
            return;
        }
        if (Candidate.MaxExtent < MinOccluderExtent)
        {
            return;
        }
        if (Candidate.DistanceSq > MaxOccluderDistanceSq)
        {
            return;
        }

        // AABB를 화면 사각형으로 래스터라이즈할 때는 실제 메시에 비해 과장된 영역이 생긴다.
        // 이때 MinDepth(가장 가까운 깊이)를 쓰면 화면 중앙에서도 과컬링(false occlusion)이 날 수
        // 있다. 보수적으로 MaxDepth(가장 먼 깊이)를 사용해 "확실히 가려지는 경우"에만 가리도록
        // 한다.
        const float ConservativeOccluderDepth =
            (std::min)(1.0f, ProjectedBounds.MaxDepth + DefaultOccluderDepthBias);

        for (int Y = Rect.MinY; Y < Rect.MaxY; ++Y)
        {
            for (int X = Rect.MinX; X < Rect.MaxX; ++X)
            {
                float &CellDepth = DepthBuffer[static_cast<size_t>(Y * BufferWidth + X)];
                CellDepth = (std::min)(CellDepth, ConservativeOccluderDepth);
            }
        }
    }

    bool OcclusionCullConservative(const std::vector<FCullingCandidate> &Candidates,
                                   std::vector<FVisibleObject> &OutRenderQueue, int BufferWidth,
                                   int BufferHeight, float BaseOcclusionTestBias,
                                   float OcclusionScreenRadiusBiasScale,
                                   float MaxDynamicOcclusionBias,
                                   float MinOccluderScreenCoverage, float MinOccluderExtent,
                                   float MaxOccluderDistanceSq,
                                   uint32 *OutCulledObjects = nullptr)
    {
        if (Candidates.empty())
        {
            return false;
        }

        std::vector<float> DepthBuffer(static_cast<size_t>(BufferWidth * BufferHeight), 1.0f);

        std::vector<FCullingCandidate> SortedCandidates = Candidates;
        std::sort(SortedCandidates.begin(), SortedCandidates.end(),
                  [](const FCullingCandidate &A, const FCullingCandidate &B)
                  { return A.DistanceSq < B.DistanceSq; });

        // Front-to-back: 가까운 visible을 occluder로 먼저 누적해야 far object가 잘 걸러짐.
        bool   bCulledAtLeastOne = false;
        uint32 CulledObjects = 0;
        for (const FCullingCandidate &Candidate : SortedCandidates)
        {
            const float DynamicBias = (std::min)(
                Candidate.ScreenRadiusNdc > 0.0f
                    ? (BaseOcclusionTestBias +
                       (Candidate.ScreenRadiusNdc * OcclusionScreenRadiusBiasScale))
                    : BaseOcclusionTestBias,
                MaxDynamicOcclusionBias);
            const bool bShouldSkipOcclusion = Candidate.bSkipOcclusionTest;
            if (!Candidate.ProjectedBounds.bValid ||
                bShouldSkipOcclusion ||
                !IsFullyOccluded(Candidate.ProjectedBounds, DepthBuffer, BufferWidth, BufferHeight,
                                 DynamicBias))
            {
                OutRenderQueue.push_back({Candidate.ObjectID, Candidate.DistanceSq});
                RasterizeOccluder(Candidate, DepthBuffer, BufferWidth, BufferHeight,
                                  MinOccluderScreenCoverage, MinOccluderExtent,
                                  MaxOccluderDistanceSq);
                continue;
            }

            ++CulledObjects;
            bCulledAtLeastOne = true;
        }

        if (OutCulledObjects != nullptr)
        {
            *OutCulledObjects = CulledObjects;
        }
        return bCulledAtLeastOne;
    }
} // namespace

// SoA : 데이터 원본 베이스
// BVH8Nodes : CompressToBVH8에서 완성한 BVH8배열
// RootNodeIndex : 시작 index
// CamLoc : 카메라 위치
// FrustumPlanes : 절두체 6개 평면
// OutRenderQueue : frustum cull을 통과한 object를 DistanceSq 순으로 정렬한 반환 배열
void CullAndSortWithBVH8(FSceneDataSoA *SoA, FBVH8Node *BVH8Nodes, int RootNodeIndex,
                         const FVector3 &CamLoc, const FPlane FrustumPlanes[6],
                         const FMatrix               &ViewProjectionMatrix,
                         std::vector<FVisibleObject> &OutRenderQueue,
                         FCpuOcclusionCullStats      *OutCpuOcclusionStats,
                         const FCpuCullingOptions    *InCpuCullingOptions)
{
    // 이전 프레임의 object 명단을 지우고 모든 object 수만큼 미리 reserve
    OutRenderQueue.clear();
    OutRenderQueue.reserve(SoA->TotalCount);

    // 한번에 8개 검사할거라서 같은 값을 8개 넣기
    // 카메라의 x, y, z 값
    __m256 vCamX = _mm256_set1_ps(CamLoc.X);
    __m256 vCamY = _mm256_set1_ps(CamLoc.Y);
    __m256 vCamZ = _mm256_set1_ps(CamLoc.Z);

    // 부동 소수점을 절대값으로 만드는 식
    // 조건문 없이 가능
    __m256 vAbsMask = _mm256_castsi256_ps(_mm256_set1_epi32(0x7FFFFFFF));

    int Stack[64];
    int StackPtr = 0; // 현재 스택에 들어있는 데이터 수
    Stack[StackPtr++] = RootNodeIndex;

    std::vector<FCullingCandidate> Candidates;
    Candidates.reserve(SoA->TotalCount);
    FCpuCullingOptions CpuCullingOptions = {};
    if (InCpuCullingOptions != nullptr)
    {
        CpuCullingOptions = *InCpuCullingOptions;
    }

    const int BufferWidth = CpuCullingOptions.ViewportWidth > 0
                                ? (std::max)(MinOcclusionBufferWidth,
                                             static_cast<int>(CpuCullingOptions.ViewportWidth) /
                                                 OcclusionBufferDownsampleDivisor)
                                : DefaultOcclusionBufferWidth;
    const int BufferHeight = CpuCullingOptions.ViewportHeight > 0
                                 ? (std::max)(MinOcclusionBufferHeight,
                                              static_cast<int>(CpuCullingOptions.ViewportHeight) /
                                                  OcclusionBufferDownsampleDivisor)
                                 : DefaultOcclusionBufferHeight;

    std::vector<float> NodeDepthBuffer(static_cast<size_t>(BufferWidth * BufferHeight), 1.0f);

    struct FNodeChildWorkItem
    {
        int   ChildData = EMPTY_NODE;
        float DistanceSq = 0.0f;
        int   ChildIndex = 0;
    };

    const int MinChildrenForNodeFrustumTest =
        (std::max)(1, static_cast<int>(CpuCullingOptions.MinChildrenForNodeFrustumTest));
    const FOcclusionAutoTuning OcclusionAutoTuning =
        BuildOcclusionAutoTuning(CpuCullingOptions, BufferWidth, BufferHeight, SoA->TotalCount);
    const int MinChildrenForNodeOcclusionTest = OcclusionAutoTuning.MinChildrenForNodeOcclusionTest;
    const float OcclusionTestBias = OcclusionAutoTuning.OcclusionTestBias;
    const float MinOccluderScreenCoverage = OcclusionAutoTuning.MinOccluderScreenCoverage;
    const float MinOccluderExtent = OcclusionAutoTuning.MinOccluderExtent;
    const float NearOcclusionSkipDistanceSq = OcclusionAutoTuning.NearOcclusionSkipDistanceSq;
    const float OcclusionScreenRadiusBiasScale = OcclusionAutoTuning.OcclusionScreenRadiusBiasScale;
    const float MaxDynamicOcclusionBias = OcclusionAutoTuning.MaxDynamicOcclusionBias;
    const float MaxNodeOcclusionScreenCoverage = OcclusionAutoTuning.MaxNodeOcclusionScreenCoverage;
    const float OcclusionBoundsExtentScale = OcclusionAutoTuning.OcclusionBoundsExtentScale;
    const float MaxOccluderDistanceSq = (CpuCullingOptions.MaxOccluderDistance > 0.0f)
                                            ? CpuCullingOptions.MaxOccluderDistance *
                                                  CpuCullingOptions.MaxOccluderDistance
                                            : std::numeric_limits<float>::max();
    std::unordered_map<int, uint32> NodeSubtreeObjectCountCache;
    auto CountObjectsInNodeSubtree = [&](auto &&Self, int NodeIndex) -> uint32
    {
        const auto Cached = NodeSubtreeObjectCountCache.find(NodeIndex);
        if (Cached != NodeSubtreeObjectCountCache.end())
        {
            return Cached->second;
        }

        uint32 TotalObjects = 0;
        const FBVH8Node &SubtreeNode = BVH8Nodes[NodeIndex];
        for (int ChildSlot = 0; ChildSlot < 8; ++ChildSlot)
        {
            const int ChildData = SubtreeNode.Children[ChildSlot];
            if (ChildData == EMPTY_NODE)
            {
                continue;
            }
            if (ChildData >= 0)
            {
                TotalObjects += Self(Self, ChildData);
                continue;
            }

            const int StartIdx = ~ChildData;
            for (int k = 0; k < 8; ++k)
            {
                if (SoA->ObjectIDs[StartIdx + k] != -1)
                {
                    ++TotalObjects;
                }
            }
        }

        NodeSubtreeObjectCountCache[NodeIndex] = TotalObjects;
        return TotalObjects;
    };
    auto CountObjectsForChild = [&](int ChildData) -> uint32
    {
        if (ChildData == EMPTY_NODE)
        {
            return 0;
        }
        if (ChildData >= 0)
        {
            return CountObjectsInNodeSubtree(CountObjectsInNodeSubtree, ChildData);
        }

        uint32 TotalObjects = 0;
        const int StartIdx = ~ChildData;
        for (int k = 0; k < 8; ++k)
        {
            if (SoA->ObjectIDs[StartIdx + k] != -1)
            {
                ++TotalObjects;
            }
        }
        return TotalObjects;
    };

    while (StackPtr > 0)
    {
        // stack에 씌여진 index를 가져와서 BVH8Nodes 배열에서 찾기
        int        CurrentNodeIdx = Stack[--StackPtr];
        FBVH8Node &Node = BVH8Nodes[CurrentNodeIdx];

        int NodeValidChildCount = 0;
        for (int j = 0; j < 8; ++j)
        {
            if (Node.Children[j] != EMPTY_NODE)
            {
                ++NodeValidChildCount;
            }
        }

        // sparse node(유효 child 수가 매우 적은 경우)에서는 node 단위 frustum 테스트 오버헤드가
        // leaf/object frustum 테스트 절감보다 커질 수 있다.
        // 이런 경우는 node frustum을 건너뛰고 object 단계 frustum으로 처리한다.
        const bool bRunNodeFrustumTest =
            CpuCullingOptions.bEnableNodeFrustumCulling &&
            NodeValidChildCount >= MinChildrenForNodeFrustumTest;
        // node occlusion도 투영/래스터 비용이 커서 sparse node에서는 손해가 날 수 있다.
        // child가 적은 경우 object 단계 occlusion(또는 최종 draw)으로 넘겨 병목을 줄인다.
        const bool bRunNodeOcclusionTest =
            CpuCullingOptions.bEnableNodeOcclusionCulling &&
            NodeValidChildCount >= MinChildrenForNodeOcclusionTest;

        int Mask = 0xFF;
        if (bRunNodeFrustumTest)
        {
            // 해당 node의 정보 가져오기
            __m256 vCenterX = _mm256_load_ps(Node.CenterX);
            __m256 vCenterY = _mm256_load_ps(Node.CenterY);
            __m256 vCenterZ = _mm256_load_ps(Node.CenterZ);

            __m256 vExtentsX = _mm256_load_ps(Node.ExtentX);
            __m256 vExtentsY = _mm256_load_ps(Node.ExtentY);
            __m256 vExtentsZ = _mm256_load_ps(Node.ExtentZ);

            // 8개 상자 모두 보인다고 가정하고 비트 연산으로 바꿔나감
            __m256 vVisible = _mm256_castsi256_ps(_mm256_set1_epi32(0xFFFFFFFF));

            // AABB를 Frustum 6개의 평면에 대해서 연산
            for (int p = 0; p < 6; ++p)
            {
                // 평면의 정보를 메모리에 올림
                __m256 vPx = _mm256_set1_ps(FrustumPlanes[p].nx);
                __m256 vPy = _mm256_set1_ps(FrustumPlanes[p].ny);
                __m256 vPz = _mm256_set1_ps(FrustumPlanes[p].nz);
                __m256 vPd = _mm256_set1_ps(FrustumPlanes[p].d);

                // AABB의 Center가 평면과 얼마나 떨어져 있는지 계산
                // (AABB Center의 X) x (평면의 법선 X) + (AABB Center의 Y) x (평면의 법선 Y) + (AABB
                // Center의 Z) x (평면의 법선 Z) + (평면의 d)
                __m256 vDistCenter = _mm256_add_ps(
                    _mm256_add_ps(_mm256_mul_ps(vCenterX, vPx), _mm256_mul_ps(vCenterY, vPy)),
                    _mm256_add_ps(_mm256_mul_ps(vCenterZ, vPz), vPd));

                // 평면 법선 벡터의 절대값
                __m256 vAbsPx = _mm256_and_ps(vPx, vAbsMask);
                __m256 vAbsPy = _mm256_and_ps(vPy, vAbsMask);
                __m256 vAbsPz = _mm256_and_ps(vPz, vAbsMask);

                // (AABB Extent의 X) x (|평면의 법선 X|) + (AABB Extent의 Y) x (|평면의 법선 Y|) + (AABB
                // Extent의 Z) x (|평면의 법선 Z|)
                __m256 vRadius = _mm256_add_ps(
                    _mm256_add_ps(_mm256_mul_ps(vExtentsX, vAbsPx),
                                  _mm256_mul_ps(vExtentsY, vAbsPy)),
                    _mm256_mul_ps(vExtentsZ, vAbsPz));

                // vRadius + vDistCenter이 0보다 작다면 안보임
                __m256 vOutside = _mm256_cmp_ps(_mm256_add_ps(vDistCenter, vRadius),
                                                _mm256_setzero_ps(), _CMP_LT_OQ);
                vVisible = _mm256_andnot_ps(vOutside, vVisible); // 해당 비트 탈락
            }

            Mask = _mm256_movemask_ps(vVisible);
        }

        std::array<FNodeChildWorkItem, 8> ChildWorkItems = {};
        int                               ChildWorkItemCount = 0;
        for (int j = 0; j < 8; ++j)
        {
            const int ChildData = Node.Children[j];
            if (ChildData == EMPTY_NODE)
            {
                continue;
            }

            uint32 ChildObjectCount = 0;
            if (bRunNodeFrustumTest)
            {
                if (OutCpuOcclusionStats != nullptr)
                {
                    ++OutCpuOcclusionStats->NodeFrustumTested;
                    ChildObjectCount = CountObjectsForChild(ChildData);
                    OutCpuOcclusionStats->NodeFrustumTestedObjects += ChildObjectCount;
                }

                if (!(Mask & (1 << j)))
                {
                    if (OutCpuOcclusionStats != nullptr)
                    {
                        ++OutCpuOcclusionStats->NodeFrustumCulled;
                        OutCpuOcclusionStats->NodeFrustumCulledObjects += ChildObjectCount;
                    }
                    continue;
                }
            }

            const float ChildDx = Node.CenterX[j] - CamLoc.X;
            const float ChildDy = Node.CenterY[j] - CamLoc.Y;
            const float ChildDz = Node.CenterZ[j] - CamLoc.Z;
            ChildWorkItems[ChildWorkItemCount++] = {
                ChildData, ChildDx * ChildDx + ChildDy * ChildDy + ChildDz * ChildDz, j};
        }

        std::sort(ChildWorkItems.begin(), ChildWorkItems.begin() + ChildWorkItemCount,
                  [](const FNodeChildWorkItem &A, const FNodeChildWorkItem &B)
                  { return A.DistanceSq < B.DistanceSq; });

        std::array<int, 8> DeferredInternalChildren = {};
        int                DeferredInternalChildCount = 0;

        // Node occlusion은 near -> far 순으로 처리해야 깊이 버퍼가 먼저 채워져서
        // far child/object가 실제로 컬링된다.
        for (int ChildWorkItemIndex = 0; ChildWorkItemIndex < ChildWorkItemCount;
             ++ChildWorkItemIndex)
        {
            const FNodeChildWorkItem &ChildItem = ChildWorkItems[ChildWorkItemIndex];
            const int                 ChildData = ChildItem.ChildData;
            const int                 ChildIndex = ChildItem.ChildIndex;
            FProjectedBounds          ChildProjectedBounds;
            uint32                    ChildObjectCount = 0;

            if (bRunNodeOcclusionTest)
            {
                if (OutCpuOcclusionStats != nullptr)
                {
                    ++OutCpuOcclusionStats->NodeOcclusionTested;
                    ChildObjectCount = CountObjectsForChild(ChildData);
                    OutCpuOcclusionStats->NodeOcclusionTestedObjects += ChildObjectCount;
                }

                ChildProjectedBounds = ProjectAABBToNdcRect(
                    FVector3(Node.CenterX[ChildIndex], Node.CenterY[ChildIndex],
                             Node.CenterZ[ChildIndex]),
                    BuildOcclusionProjectionExtent(
                        FVector3(Node.ExtentX[ChildIndex], Node.ExtentY[ChildIndex],
                                 Node.ExtentZ[ChildIndex]),
                        OcclusionBoundsExtentScale),
                    ViewProjectionMatrix);
                const float NodeCoverage = ComputeProjectedCoverage(ChildProjectedBounds);
                const bool bSkipNodeOcclusionByCoverage =
                    NodeCoverage > MaxNodeOcclusionScreenCoverage;
                const float NodeScreenRadiusNdc =
                    ComputeProjectedScreenRadiusNdc(ChildProjectedBounds);
                const float NodeDynamicBias = (std::min)(
                    OcclusionTestBias + (NodeScreenRadiusNdc * OcclusionScreenRadiusBiasScale),
                    MaxDynamicOcclusionBias);
                const bool bSkipNodeOcclusionByNearDistance =
                    (NearOcclusionSkipDistanceSq > 0.0f &&
                     ChildItem.DistanceSq <= NearOcclusionSkipDistanceSq);
                if (!bSkipNodeOcclusionByCoverage && !bSkipNodeOcclusionByNearDistance &&
                    IsFullyOccluded(ChildProjectedBounds, NodeDepthBuffer, BufferWidth, BufferHeight,
                                    NodeDynamicBias))
                {
                    if (OutCpuOcclusionStats != nullptr)
                    {
                        ++OutCpuOcclusionStats->NodeOcclusionCulled;
                        OutCpuOcclusionStats->NodeOcclusionCulledObjects += ChildObjectCount;
                    }
                    continue;
                }

                // 내부 노드 AABB를 바로 occluder로 쓰면 과컬링이 발생할 수 있다.
                // leaf 묶음(실오브젝트에 더 가까운 bounds)만 node depth에 반영해
                // 보수성을 유지하면서도 node occlusion 효율을 높인다.
                if (ChildData < 0 && ChildData != EMPTY_NODE)
                {
                    FCullingCandidate ChildCandidate;
                    ChildCandidate.ProjectedBounds = ChildProjectedBounds;
                    ChildCandidate.DistanceSq = ChildItem.DistanceSq;
                    ChildCandidate.MaxExtent = (std::max)(
                        (std::max)(Node.ExtentX[ChildIndex], Node.ExtentY[ChildIndex]),
                        Node.ExtentZ[ChildIndex]);
                    RasterizeOccluder(ChildCandidate, NodeDepthBuffer, BufferWidth, BufferHeight,
                                      MinOccluderScreenCoverage, MinOccluderExtent,
                                      MaxOccluderDistanceSq);
                }
            }

            // 마스크 비트가 1인 자식 방들만 골라서 들어감
            // ChildData가 양수면 자식이 있는 방이라는 뜻
            if (ChildData >= 0)
            {
                DeferredInternalChildren[DeferredInternalChildCount++] = ChildData;
            }
            else if (ChildData !=
                     EMPTY_NODE) // ChildData가 음수인데 Empty_Node도 아님 == Leaf Node이다
            {
                // 반전 시켰던 비트를 다시 원래대로
                // SoA를 뒤지는 과정을 시작
                int StartIdx = ~ChildData;

                // object 8개의 정보 가져오기
                __m256 lCenterX = _mm256_load_ps(SoA->CenterX + StartIdx);
                __m256 lCenterY = _mm256_load_ps(SoA->CenterY + StartIdx);
                __m256 lCenterZ = _mm256_load_ps(SoA->CenterZ + StartIdx);

                __m256 lExtentsX = _mm256_load_ps(SoA->ExtentX + StartIdx);
                __m256 lExtentsY = _mm256_load_ps(SoA->ExtentY + StartIdx);
                __m256 lExtentsZ = _mm256_load_ps(SoA->ExtentZ + StartIdx);

                int lMask = 0;
                if (CpuCullingOptions.bEnableObjectFrustumCulling)
                {
                    // AABB의 중심은 평면에 들어왔어도 object는 아닐 수 있음
                    // object 를 frustumcull
                    __m256 lVisible = _mm256_castsi256_ps(_mm256_set1_epi32(0xFFFFFFFF));

                    for (int p = 0; p < 6; ++p)
                    {
                        __m256 pPx = _mm256_set1_ps(FrustumPlanes[p].nx);
                        __m256 pPy = _mm256_set1_ps(FrustumPlanes[p].ny);
                        __m256 pPz = _mm256_set1_ps(FrustumPlanes[p].nz);
                        __m256 pPd = _mm256_set1_ps(FrustumPlanes[p].d);

                        __m256 pDistCenter =
                            _mm256_add_ps(_mm256_add_ps(_mm256_mul_ps(lCenterX, pPx),
                                                        _mm256_mul_ps(lCenterY, pPy)),
                                          _mm256_add_ps(_mm256_mul_ps(lCenterZ, pPz), pPd));

                        __m256 pAbsPx = _mm256_and_ps(pPx, vAbsMask);
                        __m256 pAbsPy = _mm256_and_ps(pPy, vAbsMask);
                        __m256 pAbsPz = _mm256_and_ps(pPz, vAbsMask);

                        __m256 pRadius = _mm256_add_ps(
                            _mm256_add_ps(_mm256_mul_ps(lExtentsX, pAbsPx),
                                          _mm256_mul_ps(lExtentsY, pAbsPy)),
                            _mm256_mul_ps(lExtentsZ, pAbsPz));

                        __m256 pOutside = _mm256_cmp_ps(_mm256_add_ps(pDistCenter, pRadius),
                                                        _mm256_setzero_ps(), _CMP_LT_OQ);
                        lVisible = _mm256_andnot_ps(pOutside, lVisible);
                    }

                    lMask = _mm256_movemask_ps(lVisible);
                }
                else
                {
                    // Object Frustum Culling이 꺼진 경우 leaf의 유효 object를 모두 후보로 넣는다.
                    for (int k = 0; k < 8; ++k)
                    {
                        if (SoA->ObjectIDs[StartIdx + k] != -1)
                        {
                            lMask |= (1 << k);
                        }
                    }
                }

                // 살아남은 object가 없다면 다음으로 넘어가기
                if (lMask == 0)
                    continue;

                // 살아남은 object는 카메라와의 거리 구하기
                __m256 lDx = _mm256_sub_ps(lCenterX, vCamX);
                __m256 lDy = _mm256_sub_ps(lCenterY, vCamY);
                __m256 lDz = _mm256_sub_ps(lCenterZ, vCamZ);
                // 피타고라스
                __m256 lDistSq =
                    _mm256_add_ps(_mm256_add_ps(_mm256_mul_ps(lDx, lDx), _mm256_mul_ps(lDy, lDy)),
                                  _mm256_mul_ps(lDz, lDz));

                // 거리 값을 SoA에 저장
                _mm256_store_ps(SoA->DistanceSq + StartIdx, lDistSq);

                // 렌더 대상이 됨. RenderQueue에 담기
                for (int k = 0; k < 8; ++k)
                {
                    if ((lMask & (1 << k)) && SoA->ObjectIDs[StartIdx + k] != -1)
                    {
                        const int         ObjectIndex = StartIdx + k;
                        FCullingCandidate Candidate;
                        Candidate.ObjectID = SoA->ObjectIDs[ObjectIndex];
                        Candidate.DistanceSq = SoA->DistanceSq[ObjectIndex];
                        Candidate.MaxExtent = (std::max)(
                            (std::max)(SoA->ExtentX[ObjectIndex], SoA->ExtentY[ObjectIndex]),
                            SoA->ExtentZ[ObjectIndex]);

                        const bool bNeedProjectedBoundsForOcclusion =
                            CpuCullingOptions.bEnableNodeOcclusionCulling ||
                            CpuCullingOptions.bEnableObjectOcclusionCulling;
                        if (bNeedProjectedBoundsForOcclusion)
                        {
                            Candidate.ProjectedBounds = ProjectAABBToNdcRect(
                                FVector3(SoA->CenterX[ObjectIndex], SoA->CenterY[ObjectIndex],
                                         SoA->CenterZ[ObjectIndex]),
                                BuildOcclusionProjectionExtent(
                                    FVector3(SoA->ExtentX[ObjectIndex], SoA->ExtentY[ObjectIndex],
                                             SoA->ExtentZ[ObjectIndex]),
                                    OcclusionBoundsExtentScale),
                                ViewProjectionMatrix);
                            Candidate.ScreenRadiusNdc =
                                ComputeProjectedScreenRadiusNdc(Candidate.ProjectedBounds);
                            Candidate.bSkipOcclusionTest =
                                NearOcclusionSkipDistanceSq > 0.0f &&
                                Candidate.DistanceSq <= NearOcclusionSkipDistanceSq;
                        }
                        Candidates.push_back(Candidate);
                        if (CpuCullingOptions.bEnableNodeOcclusionCulling)
                        {
                            RasterizeOccluder(Candidate, NodeDepthBuffer, BufferWidth, BufferHeight,
                                              MinOccluderScreenCoverage, MinOccluderExtent,
                                              MaxOccluderDistanceSq);
                        }
                    }
                    if (CpuCullingOptions.bEnableObjectFrustumCulling && SoA->ObjectIDs[StartIdx + k] != -1)
                    {
                        if (OutCpuOcclusionStats != nullptr)
                        {
                            ++OutCpuOcclusionStats->ObjectFrustumInput;
                            if ((lMask & (1 << k)) == 0)
                            {
                                ++OutCpuOcclusionStats->ObjectFrustumCulled;
                            }
                        }
                    }
                }
            }
        }

        // stack은 LIFO이므로 far -> near 순서로 push해야 near child가 먼저 처리된다.
        for (int i = DeferredInternalChildCount - 1; i >= 0; --i)
        {
            Stack[StackPtr++] = DeferredInternalChildren[i];
        }
    }

    uint32 ObjectOcclusionCulledObjects = 0;
    if (CpuCullingOptions.bEnableObjectOcclusionCulling)
    {
        SCOPED_TIMING_STAT("Viewport.Culling.CPUOcclusion");
        OcclusionCullConservative(Candidates, OutRenderQueue, BufferWidth, BufferHeight,
                                  OcclusionTestBias, OcclusionScreenRadiusBiasScale,
                                  MaxDynamicOcclusionBias, MinOccluderScreenCoverage,
                                  MinOccluderExtent, MaxOccluderDistanceSq,
                                  &ObjectOcclusionCulledObjects);
    }
    else
    {
        for (const FCullingCandidate &Candidate : Candidates)
        {
            OutRenderQueue.push_back({Candidate.ObjectID, Candidate.DistanceSq});
        }
    }

    if (OutCpuOcclusionStats != nullptr)
    {
        OutCpuOcclusionStats->InputCandidates = static_cast<uint32>(Candidates.size());
        OutCpuOcclusionStats->VisibleObjects = static_cast<uint32>(OutRenderQueue.size());
        OutCpuOcclusionStats->CulledObjects =
            CpuCullingOptions.bEnableObjectOcclusionCulling ? ObjectOcclusionCulledObjects : 0;
        OutCpuOcclusionStats->DistanceLodInput = static_cast<uint32>(Candidates.size());
        OutCpuOcclusionStats->DistanceLodCulled = 0;
        OutCpuOcclusionStats->OcclusionBufferWidth = static_cast<uint32>(BufferWidth);
        OutCpuOcclusionStats->OcclusionBufferHeight = static_cast<uint32>(BufferHeight);
    }

    std::sort(OutRenderQueue.begin(), OutRenderQueue.end());
}
