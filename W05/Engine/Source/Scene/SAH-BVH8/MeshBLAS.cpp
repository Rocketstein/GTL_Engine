#include "Scene/SAH-BVH8/MeshBLAS.h"
#include "Engine/Asset/StaticMesh.h"
#include <algorithm>
#include <cmath>

namespace
{
    // SAH 연산을 위한 임시 구조체
    struct FTriInfo
    {
        FVector3 Min;
        FVector3 Max;
        FVector3 Center;
    };

    struct FBLASBin
    {
        int      Count = 0;
        FVector3 BoundsMin = FVector3(1e30f, 1e30f, 1e30f);
        FVector3 BoundsMax = FVector3(-1e30f, -1e30f, -1e30f);

        void Include(const FTriInfo &Info)
        {
            Count++;
            BoundsMin.X = std::min(BoundsMin.X, Info.Min.X);
            BoundsMin.Y = std::min(BoundsMin.Y, Info.Min.Y);
            BoundsMin.Z = std::min(BoundsMin.Z, Info.Min.Z);
            BoundsMax.X = std::max(BoundsMax.X, Info.Max.X);
            BoundsMax.Y = std::max(BoundsMax.Y, Info.Max.Y);
            BoundsMax.Z = std::max(BoundsMax.Z, Info.Max.Z);
        }
    };

    void GetTriangleBounds(const Geometry::FTriangle &Tri, FTriInfo &OutInfo)
    {
        OutInfo.Min.X = std::min({Tri.V0.X, Tri.V1.X, Tri.V2.X});
        OutInfo.Min.Y = std::min({Tri.V0.Y, Tri.V1.Y, Tri.V2.Y});
        OutInfo.Min.Z = std::min({Tri.V0.Z, Tri.V1.Z, Tri.V2.Z});

        OutInfo.Max.X = std::max({Tri.V0.X, Tri.V1.X, Tri.V2.X});
        OutInfo.Max.Y = std::max({Tri.V0.Y, Tri.V1.Y, Tri.V2.Y});
        OutInfo.Max.Z = std::max({Tri.V0.Z, Tri.V1.Z, Tri.V2.Z});

        OutInfo.Center = FVector3((Tri.V0.X + Tri.V1.X + Tri.V2.X) / 3.0f,
                                  (Tri.V0.Y + Tri.V1.Y + Tri.V2.Y) / 3.0f,
                                  (Tri.V0.Z + Tri.V1.Z + Tri.V2.Z) / 3.0f);
    }

    float GetBoxArea(const FVector3 &Min, const FVector3 &Max)
    {
        float ExtX = std::max(0.0f, Max.X - Min.X);
        float ExtY = std::max(0.0f, Max.Y - Min.Y);
        float ExtZ = std::max(0.0f, Max.Z - Min.Z);
        return 2.0f * (ExtX * ExtY + ExtY * ExtZ + ExtZ * ExtX);
    }

    // Möller-Trumbore Ray-Triangle Intersection 알고리즘
    bool IntersectTriangle(const FVector3 &RayOrigin, const FVector3 &RayDir,
                           const Geometry::FTriangle &Tri, float &OutT)
    {
        const float EPSILON = 1e-8f;
        FVector3    Edge1 = Tri.V1 - Tri.V0;
        FVector3    Edge2 = Tri.V2 - Tri.V0;

        FVector3 h = FVector3::CrossProduct(RayDir, Edge2);
        float    a = FVector3::DotProduct(Edge1, h);

        // Ray가 삼각형과 평행할 때
        if (a > -EPSILON && a < EPSILON)
            return false;

        float    f = 1.0f / a;
        FVector3 s = RayOrigin - Tri.V0;
        float    u = f * FVector3::DotProduct(s, h);

        if (u < 0.0f || u > 1.0f)
            return false;

        FVector3 q = FVector3::CrossProduct(s, Edge1);
        float    v = f * FVector3::DotProduct(RayDir, q);

        if (v < 0.0f || u + v > 1.0f)
            return false;

        float t = f * FVector3::DotProduct(Edge2, q);
        if (t > EPSILON)
        {
            OutT = t;
            return true;
        }
        return false;
    }

    // Slab Method: Ray-AABB 교차 검사
    bool IntersectAABB(const FVector3 &RayOrigin, const FVector3 &InvDir, const FVector3 &Min,
                       const FVector3 &Max, float MaxT)
    {
        float tx1 = (Min.X - RayOrigin.X) * InvDir.X;
        float tx2 = (Max.X - RayOrigin.X) * InvDir.X;
        float tmin = std::min(tx1, tx2);
        float tmax = std::max(tx1, tx2);

        float ty1 = (Min.Y - RayOrigin.Y) * InvDir.Y;
        float ty2 = (Max.Y - RayOrigin.Y) * InvDir.Y;
        tmin = std::max(tmin, std::min(ty1, ty2));
        tmax = std::min(tmax, std::max(ty1, ty2));

        float tz1 = (Min.Z - RayOrigin.Z) * InvDir.Z;
        float tz2 = (Max.Z - RayOrigin.Z) * InvDir.Z;
        tmin = std::max(tmin, std::min(tz1, tz2));
        tmax = std::min(tmax, std::max(tz1, tz2));

        return tmax >= tmin && tmin < MaxT && tmax > 0.0f;
    }

    float GetNodeArea(const FBLASNode &Node)
    {
        float ExtX = std::max(0.0f, Node.Max.X - Node.Min.X);
        float ExtY = std::max(0.0f, Node.Max.Y - Node.Min.Y);
        float ExtZ = std::max(0.0f, Node.Max.Z - Node.Min.Z);
        return 2.0f * (ExtX * ExtY + ExtY * ExtZ + ExtZ * ExtX);
    }
} // namespace

void FMeshBLAS::Build(const TArray<Geometry::FTriangle> &InTriangles)
{
    SortedTriangles = InTriangles;
    Nodes.clear();

    if (SortedTriangles.empty())
        return;

    Nodes.push_back(FBLASNode());
    BuildRecursive(0, 0, SortedTriangles.size());
}

void FMeshBLAS::BuildRecursive(int32 NodeIdx, int32 Start, int32 End)
{
    FBLASNode &Node = Nodes[NodeIdx];
    int32      TriCount = End - Start;

    FVector3 NodeMin(1e30f, 1e30f, 1e30f);
    FVector3 NodeMax(-1e30f, -1e30f, -1e30f);
    FVector3 CentroidMin(1e30f, 1e30f, 1e30f);
    FVector3 CentroidMax(-1e30f, -1e30f, -1e30f);

    TArray<FTriInfo> TriInfos;
    TriInfos.resize(TriCount);

    for (int32 i = 0; i < TriCount; ++i)
    {
        GetTriangleBounds(SortedTriangles[Start + i], TriInfos[i]);

        NodeMin.X = std::min(NodeMin.X, TriInfos[i].Min.X);
        NodeMin.Y = std::min(NodeMin.Y, TriInfos[i].Min.Y);
        NodeMin.Z = std::min(NodeMin.Z, TriInfos[i].Min.Z);
        NodeMax.X = std::max(NodeMax.X, TriInfos[i].Max.X);
        NodeMax.Y = std::max(NodeMax.Y, TriInfos[i].Max.Y);
        NodeMax.Z = std::max(NodeMax.Z, TriInfos[i].Max.Z);

        CentroidMin.X = std::min(CentroidMin.X, TriInfos[i].Center.X);
        CentroidMin.Y = std::min(CentroidMin.Y, TriInfos[i].Center.Y);
        CentroidMin.Z = std::min(CentroidMin.Z, TriInfos[i].Center.Z);
        CentroidMax.X = std::max(CentroidMax.X, TriInfos[i].Center.X);
        CentroidMax.Y = std::max(CentroidMax.Y, TriInfos[i].Center.Y);
        CentroidMax.Z = std::max(CentroidMax.Z, TriInfos[i].Center.Z);
    }

    Node.Min = NodeMin;
    Node.Max = NodeMax;

    // 삼각형이 4개 이하이거나, 중심점들이 완벽히 겹쳐있으면 Leaf 노드로 확정
    if (TriCount <= 4 || (CentroidMax - CentroidMin).SizeSquared() < 1e-8f)
    {
        Node.TriStart = Start;
        Node.TriCount = TriCount;
        return;
    }

    // SAH 연산
    int      SplitAxis = 0;
    FVector3 Extent = CentroidMax - CentroidMin;
    if (Extent.Y > Extent.X)
        SplitAxis = 1;
    if (Extent.Z > Extent.Y && Extent.Z > Extent.X)
        SplitAxis = 2;

    const int NUM_BINS = 8;
    FBLASBin  Bins[NUM_BINS];

    float AxisMin = (SplitAxis == 0)   ? CentroidMin.X
                    : (SplitAxis == 1) ? CentroidMin.Y
                                       : CentroidMin.Z;
    float AxisExtent = (SplitAxis == 0) ? Extent.X : (SplitAxis == 1) ? Extent.Y : Extent.Z;

    for (int32 i = 0; i < TriCount; ++i)
    {
        float CenterPos = (SplitAxis == 0)   ? TriInfos[i].Center.X
                          : (SplitAxis == 1) ? TriInfos[i].Center.Y
                                             : TriInfos[i].Center.Z;
        float Ratio = (CenterPos - AxisMin) / AxisExtent;
        int   BinIdx = std::clamp(static_cast<int>(Ratio * NUM_BINS), 0, NUM_BINS - 1);
        Bins[BinIdx].Include(TriInfos[i]);
    }

    float BestCost = 1e30f;
    int   BestSplit = -1;

    for (int i = 0; i < NUM_BINS - 1; ++i)
    {
        FBLASBin LeftGroup, RightGroup;
        for (int j = 0; j <= i; ++j)
        {
            LeftGroup.Count += Bins[j].Count;
            LeftGroup.BoundsMin.X = std::min(LeftGroup.BoundsMin.X, Bins[j].BoundsMin.X);
            LeftGroup.BoundsMin.Y = std::min(LeftGroup.BoundsMin.Y, Bins[j].BoundsMin.Y);
            LeftGroup.BoundsMin.Z = std::min(LeftGroup.BoundsMin.Z, Bins[j].BoundsMin.Z);
            LeftGroup.BoundsMax.X = std::max(LeftGroup.BoundsMax.X, Bins[j].BoundsMax.X);
            LeftGroup.BoundsMax.Y = std::max(LeftGroup.BoundsMax.Y, Bins[j].BoundsMax.Y);
            LeftGroup.BoundsMax.Z = std::max(LeftGroup.BoundsMax.Z, Bins[j].BoundsMax.Z);
        }
        for (int j = i + 1; j < NUM_BINS; ++j)
        {
            RightGroup.Count += Bins[j].Count;
            RightGroup.BoundsMin.X = std::min(RightGroup.BoundsMin.X, Bins[j].BoundsMin.X);
            RightGroup.BoundsMin.Y = std::min(RightGroup.BoundsMin.Y, Bins[j].BoundsMin.Y);
            RightGroup.BoundsMin.Z = std::min(RightGroup.BoundsMin.Z, Bins[j].BoundsMin.Z);
            RightGroup.BoundsMax.X = std::max(RightGroup.BoundsMax.X, Bins[j].BoundsMax.X);
            RightGroup.BoundsMax.Y = std::max(RightGroup.BoundsMax.Y, Bins[j].BoundsMax.Y);
            RightGroup.BoundsMax.Z = std::max(RightGroup.BoundsMax.Z, Bins[j].BoundsMax.Z);
        }

        if (LeftGroup.Count > 0 && RightGroup.Count > 0)
        {
            float Cost = GetBoxArea(LeftGroup.BoundsMin, LeftGroup.BoundsMax) * LeftGroup.Count +
                         GetBoxArea(RightGroup.BoundsMin, RightGroup.BoundsMax) * RightGroup.Count;

            if (Cost < BestCost)
            {
                BestCost = Cost;
                BestSplit = i;
            }
        }
    }

    float LeafCost = GetBoxArea(NodeMin, NodeMax) * TriCount;
    if (BestCost >= LeafCost)
    {
        Node.TriStart = Start;
        Node.TriCount = TriCount;
        return;
    }

    // 삼각형 배열 재정렬 (Partition)
    auto MidPtr = std::partition(SortedTriangles.begin() + Start, SortedTriangles.begin() + End,
                                 [&](const Geometry::FTriangle &Tri)
                                 {
                                     FTriInfo Info;
                                     GetTriangleBounds(Tri, Info);
                                     float CenterPos = (SplitAxis == 0)   ? Info.Center.X
                                                       : (SplitAxis == 1) ? Info.Center.Y
                                                                          : Info.Center.Z;
                                     float Ratio = (CenterPos - AxisMin) / AxisExtent;
                                     int BinIdx = std::clamp(static_cast<int>(Ratio * NUM_BINS), 0,
                                                             NUM_BINS - 1);
                                     return BinIdx <= BestSplit;
                                 });

    int32 MidIdx = static_cast<int32>(std::distance(SortedTriangles.begin(), MidPtr));
    if (MidIdx == Start || MidIdx == End)
        MidIdx = Start + TriCount / 2;

    int32 LeftChildIdx = Nodes.size();
    Nodes.push_back(FBLASNode());
    int32 RightChildIdx = Nodes.size();
    Nodes.push_back(FBLASNode());

    // 레퍼런스(Node)가 vector 재할당 시 무효화될 수 있으므로 다시 접근
    Nodes[NodeIdx].LeftChild = LeftChildIdx;
    Nodes[NodeIdx].RightChild = RightChildIdx;

    BuildRecursive(LeftChildIdx, Start, MidIdx);
    BuildRecursive(RightChildIdx, MidIdx, End);
}

bool FMeshBLAS::Raycast(const FVector3 &RayOrigin, const FVector3 &RayDir, float &OutHitT,
                        int32 &OutHitTriIndex) const
{
    if (Nodes.empty())
        return false;

    FVector3 InvDir(1.0f / (RayDir.X != 0.0f ? RayDir.X : 1e-6f),
                    1.0f / (RayDir.Y != 0.0f ? RayDir.Y : 1e-6f),
                    1.0f / (RayDir.Z != 0.0f ? RayDir.Z : 1e-6f));

    return RaycastNode(0, RayOrigin, RayDir, InvDir, OutHitT, OutHitTriIndex);
}

bool FMeshBLAS::RaycastNode(int32 NodeIdx, const FVector3 &RayOrigin, const FVector3 &RayDir,
                            const FVector3 &InvDir, float &HitT, int32 &HitTriIdx) const
{
    const FBLASNode &Node = Nodes[NodeIdx];

    // AABB 검사 실패 시 바로 리턴
    if (!IntersectAABB(RayOrigin, InvDir, Node.Min, Node.Max, HitT))
        return false;

    bool bHit = false;

    if (Node.IsLeaf())
    {
        for (int32 i = 0; i < Node.TriCount; ++i)
        {
            int32 TriIdx = Node.TriStart + i;
            float T = 1e30f;
            if (IntersectTriangle(RayOrigin, RayDir, SortedTriangles[TriIdx], T))
            {
                if (T < HitT)
                {
                    HitT = T;
                    HitTriIdx = TriIdx;
                    bHit = true;
                }
            }
        }
        return bHit;
    }

    // 자식 노드 순회 (가까운 노드를 먼저 탐색하는 구조로 최적화 가능하지만, 기본 재귀로 구성)
    bool bHitLeft = RaycastNode(Node.LeftChild, RayOrigin, RayDir, InvDir, HitT, HitTriIdx);
    bool bHitRight = RaycastNode(Node.RightChild, RayOrigin, RayDir, InvDir, HitT, HitTriIdx);

    return bHitLeft || bHitRight;
}

void FMeshBLAS::BakeTriangleSoA(FStaticMeshRaycastData &OutData)
{
    auto &SoA = OutData.TriangleSoA;

    // 넉넉하게 메모리 할당
    int ReserveCount = OutData.Triangles.size() + (OutData.BVH8Nodes.size() * 8);
    SoA.V0X.reserve(ReserveCount);
    SoA.V0Y.reserve(ReserveCount);
    SoA.V0Z.reserve(ReserveCount);
    SoA.E1X.reserve(ReserveCount);
    SoA.E1Y.reserve(ReserveCount);
    SoA.E1Z.reserve(ReserveCount);
    SoA.E2X.reserve(ReserveCount);
    SoA.E2Y.reserve(ReserveCount);
    SoA.E2Z.reserve(ReserveCount);
    SoA.OriginalIndices.reserve(ReserveCount);

    int CurrentSoAOffset = 0;

    for (auto &Node8 : OutData.BVH8Nodes)
    {
        for (int i = 0; i < 8; ++i)
        {
            // 자식 인덱스가 음수이고 EMPTY_NODE가 아니면 리프 노드!
            if (Node8.Children[i] < 0 && Node8.Children[i] != -2147483648)
            {
                int OldStartIdx = ~Node8.Children[i];
                int TriCount = Node8.TriangleCounts[i];

                // ⭐ 트리 노드가 가리키는 주소를 기존 인덱스에서 "SoA 배열의 인덱스"로 교체
                Node8.Children[i] = ~CurrentSoAOffset;

                // 8개의 공간을 SoA 형태로 채워 넣음
                for (int k = 0; k < 8; ++k)
                {
                    if (k < TriCount)
                    {
                        uint32      TriIdx = OutData.TriangleIndices[OldStartIdx + k];
                        const auto &Tri = OutData.Triangles[TriIdx];
                        SoA.V0X.push_back(Tri.V0.X);
                        SoA.V0Y.push_back(Tri.V0.Y);
                        SoA.V0Z.push_back(Tri.V0.Z);
                        SoA.E1X.push_back(Tri.E1.X);
                        SoA.E1Y.push_back(Tri.E1.Y);
                        SoA.E1Z.push_back(Tri.E1.Z);
                        SoA.E2X.push_back(Tri.E2.X);
                        SoA.E2Y.push_back(Tri.E2.Y);
                        SoA.E2Z.push_back(Tri.E2.Z);
                        SoA.OriginalIndices.push_back(TriIdx);
                    }
                    else
                    {
                        // 8개가 안 채워지면 더미를 넣어 무조건 빗나가게 만듦
                        SoA.V0X.push_back(1e30f);
                        SoA.V0Y.push_back(1e30f);
                        SoA.V0Z.push_back(1e30f);
                        SoA.E1X.push_back(0.0f);
                        SoA.E1Y.push_back(0.0f);
                        SoA.E1Z.push_back(0.0f);
                        SoA.E2X.push_back(0.0f);
                        SoA.E2Y.push_back(0.0f);
                        SoA.E2Z.push_back(0.0f);
                        SoA.OriginalIndices.push_back(0);
                    }
                }
                CurrentSoAOffset += 8;
            }
        }
    }
}
void FMeshBLAS::CompressToBVH8(TArray<FStaticMeshBVH8Node> &OutBVH8Nodes)
{
    OutBVH8Nodes.clear();
    if (Nodes.empty())
        return;
    CollapseRecursive(0, OutBVH8Nodes);
}

int FMeshBLAS::CollapseRecursive(int CurrentNodeIdx, TArray<FStaticMeshBVH8Node> &OutBVH8Nodes)
{
    const FBLASNode &CurrentNode = Nodes[CurrentNodeIdx];

    if (CurrentNode.IsLeaf())
    {
        return ~(CurrentNode.TriStart); // 리프 노드 표시 (비트 반전)
    }

    std::vector<int> CollectedNodes;
    CollectedNodes.push_back(CurrentNode.LeftChild);
    CollectedNodes.push_back(CurrentNode.RightChild);

    while (CollectedNodes.size() < 8)
    {
        float LargestArea = -1.0f;
        int   NodeToSplitIdx = -1;

        for (size_t i = 0; i < CollectedNodes.size(); ++i)
        {
            const FBLASNode &CNode = Nodes[CollectedNodes[i]];
            if (!CNode.IsLeaf())
            {
                float Area = GetNodeArea(CNode);
                if (Area > LargestArea)
                {
                    LargestArea = Area;
                    NodeToSplitIdx = i;
                }
            }
        }

        if (NodeToSplitIdx == -1)
            break;

        int SplitTarget = CollectedNodes[NodeToSplitIdx];
        CollectedNodes.erase(CollectedNodes.begin() + NodeToSplitIdx);
        CollectedNodes.push_back(Nodes[SplitTarget].LeftChild);
        CollectedNodes.push_back(Nodes[SplitTarget].RightChild);
    }

    int NewBVH8Idx = OutBVH8Nodes.size();
    OutBVH8Nodes.push_back(FStaticMeshBVH8Node());

    for (int i = 0; i < 8; ++i)
    {
        if (i < CollectedNodes.size())
        {
            int              ChildBinIdx = CollectedNodes[i];
            const FBLASNode &ChildBinNode = Nodes[ChildBinIdx];

            OutBVH8Nodes[NewBVH8Idx].CenterX[i] = (ChildBinNode.Max.X + ChildBinNode.Min.X) * 0.5f;
            OutBVH8Nodes[NewBVH8Idx].ExtentX[i] = (ChildBinNode.Max.X - ChildBinNode.Min.X) * 0.5f;
            OutBVH8Nodes[NewBVH8Idx].CenterY[i] = (ChildBinNode.Max.Y + ChildBinNode.Min.Y) * 0.5f;
            OutBVH8Nodes[NewBVH8Idx].ExtentY[i] = (ChildBinNode.Max.Y - ChildBinNode.Min.Y) * 0.5f;
            OutBVH8Nodes[NewBVH8Idx].CenterZ[i] = (ChildBinNode.Max.Z + ChildBinNode.Min.Z) * 0.5f;
            OutBVH8Nodes[NewBVH8Idx].ExtentZ[i] = (ChildBinNode.Max.Z - ChildBinNode.Min.Z) * 0.5f;

            if (ChildBinNode.IsLeaf())
            {
                OutBVH8Nodes[NewBVH8Idx].Children[i] = ~(ChildBinNode.TriStart);
                OutBVH8Nodes[NewBVH8Idx].TriangleCounts[i] = ChildBinNode.TriCount;
            }
            else
            {
                OutBVH8Nodes[NewBVH8Idx].Children[i] = CollapseRecursive(ChildBinIdx, OutBVH8Nodes);
                OutBVH8Nodes[NewBVH8Idx].TriangleCounts[i] = 0;
            }
        }
        else
        {
            // 더미 노드 채우기
            OutBVH8Nodes[NewBVH8Idx].Children[i] = -2147483648; // EMPTY_NODE
            OutBVH8Nodes[NewBVH8Idx].TriangleCounts[i] = 0;
            OutBVH8Nodes[NewBVH8Idx].CenterX[i] = 1e30f;
            OutBVH8Nodes[NewBVH8Idx].ExtentX[i] = 0.0f;
            OutBVH8Nodes[NewBVH8Idx].CenterY[i] = 1e30f;
            OutBVH8Nodes[NewBVH8Idx].ExtentY[i] = 0.0f;
            OutBVH8Nodes[NewBVH8Idx].CenterZ[i] = 1e30f;
            OutBVH8Nodes[NewBVH8Idx].ExtentZ[i] = 0.0f;
        }
    }
    return NewBVH8Idx;
}