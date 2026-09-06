#include "Engine/Asset/StaticMesh.h"
#include <algorithm>
#include <cfloat>
#include <functional>

namespace
{
    static constexpr uint32 GStaticMeshBVHLeafTriangleCount = 8;

    FVector3 ReadPositionFromVertexBuffer(const TArray<uint8> &VertexData, uint32 VertexStride,
                                          uint32 VertexIndex)
    {
        const uint8 *Ptr = VertexData.data() +
                           static_cast<size_t>(VertexIndex) * static_cast<size_t>(VertexStride);
        const float *P = reinterpret_cast<const float *>(Ptr);
        return FVector3(P[0], P[1], P[2]);
    }

    Geometry::FAABB MakeTriangleBounds(const FVector3 &A, const FVector3 &B, const FVector3 &C)
    {
        FVector3 Min;
        FVector3 Max;

        Min.X = (std::min)(A.X, (std::min)(B.X, C.X));
        Min.Y = (std::min)(A.Y, (std::min)(B.Y, C.Y));
        Min.Z = (std::min)(A.Z, (std::min)(B.Z, C.Z));

        Max.X = (std::max)(A.X, (std::max)(B.X, C.X));
        Max.Y = (std::max)(A.Y, (std::max)(B.Y, C.Y));
        Max.Z = (std::max)(A.Z, (std::max)(B.Z, C.Z));

        return Geometry::FAABB(Min, Max);
    }

    Geometry::FAABB UnionAABB(const Geometry::FAABB &A, const Geometry::FAABB &B)
    {
        Geometry::FAABB Result;

        Result.Min.X = (std::min)(A.Min.X, B.Min.X);
        Result.Min.Y = (std::min)(A.Min.Y, B.Min.Y);
        Result.Min.Z = (std::min)(A.Min.Z, B.Min.Z);

        Result.Max.X = (std::max)(A.Max.X, B.Max.X);
        Result.Max.Y = (std::max)(A.Max.Y, B.Max.Y);
        Result.Max.Z = (std::max)(A.Max.Z, B.Max.Z);

        return Result;
    }

    Geometry::FAABB ComputeRangeBounds(const TArray<FStaticMeshTrianglePrecompute> &Triangles,
                                       const TArray<uint32> &TriangleIndices, uint32 Begin,
                                       uint32 End)
    {
        if (Begin >= End)
        {
            return Geometry::FAABB();
        }

        Geometry::FAABB Bounds = Triangles[TriangleIndices[Begin]].LocalBounds;
        for (uint32 i = Begin + 1; i < End; ++i)
        {
            Bounds = UnionAABB(Bounds, Triangles[TriangleIndices[i]].LocalBounds);
        }
        return Bounds;
    }

    FVector3 ComputeBoundsCenter(const Geometry::FAABB &Bounds)
    {
        return FVector3((Bounds.Min.X + Bounds.Max.X) * 0.5f, (Bounds.Min.Y + Bounds.Max.Y) * 0.5f,
                        (Bounds.Min.Z + Bounds.Max.Z) * 0.5f);
    }
} // namespace

//void UStaticMesh::BuildFromCookedData(const Asset::FObjCookedData &InCooked)
//{
//    VertexStride = InCooked.VertexStride;
//    VertexCount = InCooked.VertexCount;
//    VertexData = InCooked.VertexData;
//    Indices = InCooked.Indices;
//
//    Sections.clear();
//    Sections.reserve(InCooked.Sections.size());
//    for (const Asset::FStaticMeshSectionData &SectionData : InCooked.Sections)
//    {
//        FStaticMeshSection Section;
//        Section.FirstIndex = SectionData.StartIndex;
//        Section.IndexCount = SectionData.IndexCount;
//        Section.MaterialIndex = SectionData.MaterialIndex;
//        Sections.push_back(Section);
//    }
//
//    MaterialSlots.assign(InCooked.Materials.size(), nullptr);
//
//    BuildBounds();
//    BuildRaycastData();
//}

void UStaticMesh::BuildFromCookedData(const Asset::FObjCookedData &InCooked)
{
    VertexStride = InCooked.VertexStride;
    VertexCount = InCooked.VertexCount;
    VertexData = InCooked.VertexData;
    Indices = InCooked.Indices;

    Sections.clear();
    Sections.reserve(InCooked.Sections.size());
    for (const Asset::FStaticMeshSectionData &SectionData : InCooked.Sections)
    {
        FStaticMeshSection Section;
        Section.FirstIndex = SectionData.StartIndex;
        Section.IndexCount = SectionData.IndexCount;
        Section.MaterialIndex = SectionData.MaterialIndex;
        Sections.push_back(Section);
    }

    MaterialSlots.assign(InCooked.Materials.size(), nullptr);

    BuildBounds();
    BuildRaycastData();
}

bool UStaticMesh::IsValidLowLevel() const
{
    return !GetAssetPath().empty() && VertexCount > 0 && !VertexData.empty();
}

void UStaticMesh::BuildBounds()
{
    Bounds = Geometry::FAABB();

    if (VertexCount == 0 || VertexData.empty() || VertexStride < sizeof(float) * 3)
    {
        return;
    }

    FVector3 Min(FLT_MAX, FLT_MAX, FLT_MAX);
    FVector3 Max(-FLT_MAX, -FLT_MAX, -FLT_MAX);

    for (uint32 i = 0; i < VertexCount; ++i)
    {
        const float *P = reinterpret_cast<const float *>(VertexData.data() +
                                                         static_cast<size_t>(i) * VertexStride);

        Min.X = (std::min)(Min.X, P[0]);
        Min.Y = (std::min)(Min.Y, P[1]);
        Min.Z = (std::min)(Min.Z, P[2]);

        Max.X = (std::max)(Max.X, P[0]);
        Max.Y = (std::max)(Max.Y, P[1]);
        Max.Z = (std::max)(Max.Z, P[2]);
    }

    Bounds = Geometry::FAABB(Min, Max);
}

//void UStaticMesh::BuildRaycastData()
//{
//    RaycastData.Reset();
//
//    if (VertexStride < sizeof(float) * 3 || VertexData.empty() || Indices.size() < 3)
//    {
//        return;
//    }
//
//    BuildTrianglePrecomputeData();
//    BuildBVH();
//    BuildLeafPackets();
//}

void UStaticMesh::BuildRaycastData()
{
    RaycastData.Reset();

    if (VertexStride < sizeof(float) * 3 || VertexData.empty() || Indices.size() < 3)
    {
        return;
    }

    BuildTrianglePrecomputeData();
    BuildBVH();
    BuildLeafPackets();
    CompressBVHTo8();

    TArray<Geometry::FTriangle> StandardTriangles;
    StandardTriangles.reserve(RaycastData.Triangles.size());
    for (const auto &Precomputed : RaycastData.Triangles)
    {
        // Precomputed 구조체에 있는 V0, E1, E2를 사용해 원래의 삼각형 정점을 복원합니다.
        StandardTriangles.push_back(Geometry::FTriangle(
            Precomputed.V0, Precomputed.V0 + Precomputed.E1, Precomputed.V0 + Precomputed.E2));
    }
    MeshBLAS.Build(StandardTriangles);
}

void UStaticMesh::BuildTrianglePrecomputeData()
{
    RaycastData.Triangles.clear();
    RaycastData.TriangleIndices.clear();

    if (Indices.size() < 3)
    {
        return;
    }

    const uint32 TriangleCount = static_cast<uint32>(Indices.size() / 3);
    RaycastData.Triangles.reserve(TriangleCount);
    RaycastData.TriangleIndices.reserve(TriangleCount);

    for (uint32 TriIndex = 0; TriIndex < TriangleCount; ++TriIndex)
    {
        const uint32 I0 = Indices[TriIndex * 3 + 0];
        const uint32 I1 = Indices[TriIndex * 3 + 1];
        const uint32 I2 = Indices[TriIndex * 3 + 2];

        if (I0 >= VertexCount || I1 >= VertexCount || I2 >= VertexCount)
        {
            continue;
        }

        const FVector3 V0 = ReadPositionFromVertexBuffer(VertexData, VertexStride, I0);
        const FVector3 V1 = ReadPositionFromVertexBuffer(VertexData, VertexStride, I1);
        const FVector3 V2 = ReadPositionFromVertexBuffer(VertexData, VertexStride, I2);

        FStaticMeshTrianglePrecompute Precomputed;
        Precomputed.V0 = V0;
        Precomputed.E1 = V1 - V0;
        Precomputed.E2 = V2 - V0;
        Precomputed.LocalBounds = MakeTriangleBounds(V0, V1, V2);
        Precomputed.Index0 = I0;
        Precomputed.Index1 = I1;
        Precomputed.Index2 = I2;

        RaycastData.Triangles.push_back(Precomputed);
        RaycastData.TriangleIndices.push_back(
            static_cast<uint32>(RaycastData.Triangles.size() - 1));
    }
}

void UStaticMesh::BuildBVH()
{
    RaycastData.BVHNodes.clear();

    if (RaycastData.Triangles.empty())
    {
        return;
    }

    std::function<int32(uint32, uint32)> BuildNode = [&](uint32 Begin, uint32 End) -> int32
    {
        FStaticMeshBVHNode Node;
        Node.Bounds =
            ComputeRangeBounds(RaycastData.Triangles, RaycastData.TriangleIndices, Begin, End);

        const uint32 Count = End - Begin;
        const int32  NodeIndex = static_cast<int32>(RaycastData.BVHNodes.size());
        RaycastData.BVHNodes.push_back(Node);

        if (Count <= GStaticMeshBVHLeafTriangleCount)
        {
            FStaticMeshBVHNode &LeafNode = RaycastData.BVHNodes[NodeIndex];
            LeafNode.FirstTriangle = Begin;
            LeafNode.TriangleCount = Count;
            LeafNode.LeftChild = -1;
            LeafNode.RightChild = -1;
            return NodeIndex;
        }

        FVector3 Extents(Node.Bounds.Max.X - Node.Bounds.Min.X,
                         Node.Bounds.Max.Y - Node.Bounds.Min.Y,
                         Node.Bounds.Max.Z - Node.Bounds.Min.Z);

        int Axis = 0;
        if (Extents.Y > Extents.X && Extents.Y >= Extents.Z)
        {
            Axis = 1;
        }
        else if (Extents.Z > Extents.X && Extents.Z > Extents.Y)
        {
            Axis = 2;
        }

        const uint32 Mid = Begin + Count / 2;

        std::nth_element(
            RaycastData.TriangleIndices.begin() + Begin, RaycastData.TriangleIndices.begin() + Mid,
            RaycastData.TriangleIndices.begin() + End,
            [&](uint32 Lhs, uint32 Rhs)
            {
                const FVector3 LC = ComputeBoundsCenter(RaycastData.Triangles[Lhs].LocalBounds);
                const FVector3 RC = ComputeBoundsCenter(RaycastData.Triangles[Rhs].LocalBounds);

                if (Axis == 0)
                {
                    return LC.X < RC.X;
                }
                if (Axis == 1)
                {
                    return LC.Y < RC.Y;
                }
                return LC.Z < RC.Z;
            });

        const int32 LeftChild = BuildNode(Begin, Mid);
        const int32 RightChild = BuildNode(Mid, End);

        FStaticMeshBVHNode &InternalNode = RaycastData.BVHNodes[NodeIndex];
        InternalNode.LeftChild = LeftChild;
        InternalNode.RightChild = RightChild;
        InternalNode.FirstTriangle = 0;
        InternalNode.TriangleCount = 0;

        return NodeIndex;
    };

    BuildNode(0, static_cast<uint32>(RaycastData.TriangleIndices.size()));
}

void UStaticMesh::BuildLeafPackets()
{
    RaycastData.LeafPackets.clear();

    if (RaycastData.Triangles.empty())
    {
        return;
    }

    // 현재 구조상 LeafPackets는 TriangleIndices 순서대로 전역 패킹
    // BVH leaf 최대 삼각형 수가 8이므로 Packet8과 폭을 맞춘다.
    const uint32 TriangleCount = static_cast<uint32>(RaycastData.TriangleIndices.size());
    const uint32 PacketCount = (TriangleCount + 7u) / 8u;

    RaycastData.LeafPackets.reserve(PacketCount);

    for (uint32 PacketIndex = 0; PacketIndex < PacketCount; ++PacketIndex)
    {
        FStaticMeshTrianglePacket8 Packet;
        Packet.TriangleCount = 0;

        for (uint32 Lane = 0; Lane < 8u; ++Lane)
        {
            const uint32 GlobalTri = PacketIndex * 8u + Lane;
            if (GlobalTri >= TriangleCount)
            {
                break;
            }

            const uint32 TriangleIndex = RaycastData.TriangleIndices[GlobalTri];
            const FStaticMeshTrianglePrecompute &Tri = RaycastData.Triangles[TriangleIndex];

            Packet.V0X[Lane] = Tri.V0.X;
            Packet.V0Y[Lane] = Tri.V0.Y;
            Packet.V0Z[Lane] = Tri.V0.Z;

            Packet.E1X[Lane] = Tri.E1.X;
            Packet.E1Y[Lane] = Tri.E1.Y;
            Packet.E1Z[Lane] = Tri.E1.Z;

            Packet.E2X[Lane] = Tri.E2.X;
            Packet.E2Y[Lane] = Tri.E2.Y;
            Packet.E2Z[Lane] = Tri.E2.Z;

            Packet.TriangleIndices[Lane] = TriangleIndex;
            ++Packet.TriangleCount;
        }

        RaycastData.LeafPackets.push_back(Packet);
    }
}

void UStaticMesh::CompressBVHTo8()
{
    RaycastData.BVH8Nodes.clear();
    if (RaycastData.BVHNodes.empty())
        return;

    // 1. 이진 트리를 8진 트리로 압축
    CollapseRecursive(0);

    // 2. SoA 베이킹 (흩어진 삼각형 8개씩 모아두기)
    auto &SoA = RaycastData.TriangleSoA;
    int   ReserveCount = RaycastData.Triangles.size() + (RaycastData.BVH8Nodes.size() * 8);

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

    for (auto &Node8 : RaycastData.BVH8Nodes)
    {
        for (int i = 0; i < 8; ++i)
        {
            if (Node8.Children[i] < 0 && Node8.Children[i] != -2147483648) // 리프 노드
            {
                int OldStartIdx = ~Node8.Children[i];
                int TriCount = Node8.TriangleCounts[i];

                Node8.Children[i] = ~CurrentSoAOffset; // 주소를 SoA 인덱스로 교체

                for (int k = 0; k < 8; ++k)
                {
                    if (k < TriCount)
                    {
                        uint32      TriIdx = RaycastData.TriangleIndices[OldStartIdx + k];
                        const auto &Tri = RaycastData.Triangles[TriIdx];

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
                    else // 남는 공간은 빗나가도록 더미 값 채우기
                    {
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

int UStaticMesh::CollapseRecursive(int CurrentNodeIdx)
{
    const FStaticMeshBVHNode &CurrentNode = RaycastData.BVHNodes[CurrentNodeIdx];

    if (CurrentNode.IsLeaf())
    {
        return ~(CurrentNode.FirstTriangle);
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
            const FStaticMeshBVHNode &CNode = RaycastData.BVHNodes[CollectedNodes[i]];
            if (!CNode.IsLeaf())
            {
                float ExtX = std::max(0.0f, CNode.Bounds.Max.X - CNode.Bounds.Min.X);
                float ExtY = std::max(0.0f, CNode.Bounds.Max.Y - CNode.Bounds.Min.Y);
                float ExtZ = std::max(0.0f, CNode.Bounds.Max.Z - CNode.Bounds.Min.Z);
                float Area = 2.0f * (ExtX * ExtY + ExtY * ExtZ + ExtZ * ExtX);

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
        CollectedNodes.push_back(RaycastData.BVHNodes[SplitTarget].LeftChild);
        CollectedNodes.push_back(RaycastData.BVHNodes[SplitTarget].RightChild);
    }

    int NewBVH8Idx = RaycastData.BVH8Nodes.size();
    RaycastData.BVH8Nodes.push_back(FStaticMeshBVH8Node());

    for (int i = 0; i < 8; ++i)
    {
        if (i < CollectedNodes.size())
        {
            int                       ChildBinIdx = CollectedNodes[i];
            const FStaticMeshBVHNode &ChildBinNode = RaycastData.BVHNodes[ChildBinIdx];

            RaycastData.BVH8Nodes[NewBVH8Idx].CenterX[i] =
                (ChildBinNode.Bounds.Max.X + ChildBinNode.Bounds.Min.X) * 0.5f;
            RaycastData.BVH8Nodes[NewBVH8Idx].ExtentX[i] =
                (ChildBinNode.Bounds.Max.X - ChildBinNode.Bounds.Min.X) * 0.5f;
            RaycastData.BVH8Nodes[NewBVH8Idx].CenterY[i] =
                (ChildBinNode.Bounds.Max.Y + ChildBinNode.Bounds.Min.Y) * 0.5f;
            RaycastData.BVH8Nodes[NewBVH8Idx].ExtentY[i] =
                (ChildBinNode.Bounds.Max.Y - ChildBinNode.Bounds.Min.Y) * 0.5f;
            RaycastData.BVH8Nodes[NewBVH8Idx].CenterZ[i] =
                (ChildBinNode.Bounds.Max.Z + ChildBinNode.Bounds.Min.Z) * 0.5f;
            RaycastData.BVH8Nodes[NewBVH8Idx].ExtentZ[i] =
                (ChildBinNode.Bounds.Max.Z - ChildBinNode.Bounds.Min.Z) * 0.5f;

            if (ChildBinNode.IsLeaf())
            {
                RaycastData.BVH8Nodes[NewBVH8Idx].Children[i] = ~(ChildBinNode.FirstTriangle);
                RaycastData.BVH8Nodes[NewBVH8Idx].TriangleCounts[i] = ChildBinNode.TriangleCount;
            }
            else
            {
                RaycastData.BVH8Nodes[NewBVH8Idx].Children[i] = CollapseRecursive(ChildBinIdx);
                RaycastData.BVH8Nodes[NewBVH8Idx].TriangleCounts[i] = 0;
            }
        }
        else
        {
            RaycastData.BVH8Nodes[NewBVH8Idx].Children[i] = -2147483648; // EMPTY_NODE
            RaycastData.BVH8Nodes[NewBVH8Idx].TriangleCounts[i] = 0;
            RaycastData.BVH8Nodes[NewBVH8Idx].CenterX[i] = 1e30f;
            RaycastData.BVH8Nodes[NewBVH8Idx].ExtentX[i] = 0.0f;
            RaycastData.BVH8Nodes[NewBVH8Idx].CenterY[i] = 1e30f;
            RaycastData.BVH8Nodes[NewBVH8Idx].ExtentY[i] = 0.0f;
            RaycastData.BVH8Nodes[NewBVH8Idx].CenterZ[i] = 1e30f;
            RaycastData.BVH8Nodes[NewBVH8Idx].ExtentZ[i] = 0.0f;
        }
    }
    return NewBVH8Idx;
}