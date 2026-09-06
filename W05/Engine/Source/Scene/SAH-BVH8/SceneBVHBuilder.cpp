#include "SceneBVHBuilder.h"

void FBin::Include(const FBuildObject &Obj)
{
    Count++;
    for (int i = 0; i < 3; ++i)
    {
        BoundsMin[i] = std::min(BoundsMin[i], Obj.Min[i]);
        BoundsMax[i] = std::max(BoundsMax[i], Obj.Max[i]);
    }
}

float FBVHBuilder::GetSurfaceArea(const float Min[3], const float Max[3])
{
    float ExtentX = std::max(0.0f, Max[0] - Min[0]);
    float ExtentY = std::max(0.0f, Max[1] - Min[1]);
    float ExtentZ = std::max(0.0f, Max[2] - Min[2]);
    return 2.0f * (ExtentX * ExtentY + ExtentY * ExtentZ + ExtentZ * ExtentX);
}

// Objects : 씬에 있는 모든 사과들의 정보(Min, Max, Center, ID)가 담긴 임시 배열
// StartIdx, EndIdx : Objects 배열의 StartIdx 부터 EndIdx 까지 조작
// OutNodes : 완성된 이진 트리 노드(FBVHNode)들이 차곡차곡 쌓이는 결과물 저장소
// OutSoA : 앞서 우리가 공들여 만든 "AVX2용 32바이트 정렬 메모리 구조체"의 포인터
// Leaf 노드를 만나는 순간, 사과 데이터를 Center/Extent 형태로 변환해서 이 포인터가 가리키는
// 메모리에 영구적으로 Baking
// SoAOffset : OutSoA 배열의 "어디까지 데이터를 써넣었는가?"를 추적하는 커서
int FBVHBuilder::BuildRecursive(std::vector<FBuildObject> &Objects, int StartIdx, int EndIdx,
                                std::vector<FBVHNode> &OutNodes, FSceneDataSoA *OutSoA,
                                int &SoAOffset)
{
    int NodeIndex = OutNodes.size();
    OutNodes.push_back(FBVHNode());
    FBVHNode &Node = OutNodes[NodeIndex];

    int ObjectCount = EndIdx - StartIdx;

    float NodeMin[3] = {1e30f, 1e30f, 1e30f};
    float NodeMax[3] = {-1e30f, -1e30f, -1e30f};
    float CentroidMin[3] = {1e30f, 1e30f, 1e30f};
    float CentroidMax[3] = {-1e30f, -1e30f, -1e30f};

    // 현재 만들고 있는 노드에 할당된 오브젝트들(StartIdx부터 EndIdx까지)을 대상으로 전체의 min,
    // max, center의 x,y,z를 구함
    for (int i = StartIdx; i < EndIdx; ++i)
    {
        for (int j = 0; j < 3; ++j)
        {
            NodeMin[j] = std::min(NodeMin[j], Objects[i].Min[j]);
            NodeMax[j] = std::max(NodeMax[j], Objects[i].Max[j]);
            CentroidMin[j] = std::min(CentroidMin[j], Objects[i].Center[j]);
            CentroidMax[j] = std::max(CentroidMax[j], Objects[i].Center[j]);
        }
    }

    // 구한 전체의 min, max, center의 x,y,z를 새로 만든 node에 넣기
    Node.MinX = NodeMin[0];
    Node.MinY = NodeMin[1];
    Node.MinZ = NodeMin[2];
    Node.MaxX = NodeMax[0];
    Node.MaxY = NodeMax[1];
    Node.MaxZ = NodeMax[2];

    bool bIsLeaf = (ObjectCount <= MAX_LEAF_SIZE);

    int SplitAxis = 0;
    // x, y, z 중 가장 크기가 큰 축 선택. 그 축이
    // x면 SplitAxis = 0
    // y면 SplitAxis = 1
    // z면 SplitAxis = 2
    float MaxExtent = CentroidMax[0] - CentroidMin[0];
    if (CentroidMax[1] - CentroidMin[1] > MaxExtent)
    {
        SplitAxis = 1;
        MaxExtent = CentroidMax[1] - CentroidMin[1];
    }
    if (CentroidMax[2] - CentroidMin[2] > MaxExtent)
    {
        SplitAxis = 2;
        MaxExtent = CentroidMax[2] - CentroidMin[2];
    }

    // StartIdx부터 EndIdx까지의 오브젝트가 다 겹쳐있을 때
    if (MaxExtent < 1e-5f)
    {
        // 오브젝트가 8보다 많다면
        if (ObjectCount > MAX_LEAF_SIZE)
        {
            // 강제로 반으로 쪼개서 재귀
            int MidIdx = StartIdx + ObjectCount / 2;
            Node.LeftChild = BuildRecursive(Objects, StartIdx, MidIdx, OutNodes, OutSoA, SoAOffset);
            Node.RightChild = BuildRecursive(Objects, MidIdx, EndIdx, OutNodes, OutSoA, SoAOffset);
            return NodeIndex;
        }
        else // 8보다 적으므로 Leaf
        {
            bIsLeaf = true;
        }
    }

    float BestCost = 1e30f;
    int   BestSplitBin = -1;

    if (!bIsLeaf)
    {
        FBin Bins[NUM_BINS];
        for (int i = StartIdx; i < EndIdx; ++i)
        {
            // 해당 축의 CentroidMin을 기준으로 오브젝트가 몇 % 지점에 위치하는가
            float Ratio = (Objects[i].Center[SplitAxis] - CentroidMin[SplitAxis]) / MaxExtent;

            // %에 8을 곱하고 소수점을 버린 다음 혹시 부동소수점 때문에 8이 될까봐 clamp해서 들어갈
            // 바구니의 index 결정
            int BinIdx = std::clamp(static_cast<int>(Ratio * NUM_BINS), 0, NUM_BINS - 1);
            Bins[BinIdx].Include(Objects[i]); // 해당 번호 바구니에 object 정보 삽입
        }

        for (int i = 0; i < NUM_BINS - 1; ++i)
        {
            float LeftBoundsMin[3] = {1e30f, 1e30f, 1e30f};
            float LeftBoundsMax[3] = {-1e30f, -1e30f, -1e30f};
            int   LeftCount = 0;

            float RightBoundsMin[3] = {1e30f, 1e30f, 1e30f};
            float RightBoundsMax[3] = {-1e30f, -1e30f, -1e30f};
            int   RightCount = 0;

            for (int j = 0; j <= i; ++j)
            {
                if (Bins[j].Count == 0)
                    continue;

                // 왼쪽 그룹에 몇개의 사과가 있는지
                LeftCount += Bins[j].Count;
                for (int k = 0; k < 3; ++k) // 왼쪽 그룹의 AABB
                {
                    LeftBoundsMin[k] = std::min(LeftBoundsMin[k], Bins[j].BoundsMin[k]);
                    LeftBoundsMax[k] = std::max(LeftBoundsMax[k], Bins[j].BoundsMax[k]);
                }
            }

            for (int j = i + 1; j < NUM_BINS; ++j)
            {
                if (Bins[j].Count == 0)
                    continue;

                // 오른쪽 그룹에 몇개의 사과가 있는지
                RightCount += Bins[j].Count;
                for (int k = 0; k < 3; ++k) // 오른쪽 그룹의 AABB
                {
                    RightBoundsMin[k] = std::min(RightBoundsMin[k], Bins[j].BoundsMin[k]);
                    RightBoundsMax[k] = std::max(RightBoundsMax[k], Bins[j].BoundsMax[k]);
                }
            }

            if (LeftCount > 0 && RightCount > 0)
            {
                // (왼쪽 그룹의 AABB 겉넓이 x 왼쪽 그룹의 사과 개수) + (오른쪽 그룹의 AABB 겉넓이 x
                // 오른쪽 그룹의 사과 개수)
                float Cost = GetSurfaceArea(LeftBoundsMin, LeftBoundsMax) * LeftCount +
                             GetSurfaceArea(RightBoundsMin, RightBoundsMax) * RightCount;

                // 비용이 가장 작은 경우를 저장하게 됨
                if (Cost < BestCost)
                {
                    BestCost = Cost;
                    BestSplitBin = i;
                }
            }
        }

        float ParentArea = GetSurfaceArea(NodeMin, NodeMax); // 쪼개지 않았을 때의 겉넓이
        float LeafCost = ParentArea * ObjectCount;           // 쪼개지 않았을 때의 비용
        // 최적의 쪼개기와 안쪼개기 비교, object 수가 8보다 크면 어쩔 수 없이 쪼갬
        if (BestCost >= LeafCost && ObjectCount <= MAX_LEAF_SIZE)
            bIsLeaf = true;
    }

    // Leaf노드 일 때
    if (bIsLeaf)
    {
        // Leaf 노드임을 나타내기 위해 -1 넣기
        Node.LeftChild = -1;
        Node.RightChild = -1;
        // 이 노드에 접근할 때 SoAOffset번지 부터 ObjectCount개 만큼 들어있다고 알려주기위한 정보
        Node.SoA_StartIndex = SoAOffset;
        Node.ObjectCount = ObjectCount;

        // [최적화] 배열에 기록할 때 한 번만 계산해서 영원히 사용함
        for (int i = 0; i < 8; ++i) // 무조건 8번 돌기
        {
            int SoAIdx = SoAOffset + i;
            // 진짜 사과
            if (i < ObjectCount)
            {
                // 임시 배열에서 정보를 꺼내서
                float minx = Objects[StartIdx + i].Min[0];
                float maxx = Objects[StartIdx + i].Max[0];
                float miny = Objects[StartIdx + i].Min[1];
                float maxy = Objects[StartIdx + i].Max[1];
                float minz = Objects[StartIdx + i].Min[2];
                float maxz = Objects[StartIdx + i].Max[2];

                // SoA에 넣기
                OutSoA->CenterX[SoAIdx] = (maxx + minx) * 0.5f;
                OutSoA->ExtentX[SoAIdx] = (maxx - minx) * 0.5f;
                OutSoA->CenterY[SoAIdx] = (maxy + miny) * 0.5f;
                OutSoA->ExtentY[SoAIdx] = (maxy - miny) * 0.5f;
                OutSoA->CenterZ[SoAIdx] = (maxz + minz) * 0.5f;
                OutSoA->ExtentZ[SoAIdx] = (maxz - minz) * 0.5f;
                OutSoA->ObjectIDs[SoAIdx] = Objects[StartIdx + i].ObjectID;
                OutSoA->Components[SoAIdx] = Objects[StartIdx + i].Component;
                OutSoA->StaticMeshes[SoAIdx] = Objects[StartIdx + i].StaticMesh;
                OutSoA->InverseWorldMatrices[SoAIdx] = Objects[StartIdx + i].InverseWorldMatrix;
            }
            else // 더미 사과
            {
                OutSoA->CenterX[SoAIdx] = 1e30f;
                OutSoA->ExtentX[SoAIdx] = 0.0f;
                OutSoA->CenterY[SoAIdx] = 1e30f;
                OutSoA->ExtentY[SoAIdx] = 0.0f;
                OutSoA->CenterZ[SoAIdx] = 1e30f;
                OutSoA->ExtentZ[SoAIdx] = 0.0f;
                OutSoA->ObjectIDs[SoAIdx] = -1;
                OutSoA->Components[SoAIdx] = nullptr;
                OutSoA->StaticMeshes[SoAIdx] = nullptr;
                OutSoA->InverseWorldMatrices[SoAIdx] = nullptr;
            }
        }
        // 다음 방 주소로 이동
        SoAOffset += 8;
    }
    else // Leaf가 아닐 때
    {
        // 마땅히 자를 곳을 찾지 못함
        if (BestSplitBin == -1)
        {
            // 반갈
            int MidIdx = StartIdx + ObjectCount / 2;
            Node.LeftChild = BuildRecursive(Objects, StartIdx, MidIdx, OutNodes, OutSoA, SoAOffset);
            Node.RightChild = BuildRecursive(Objects, MidIdx, EndIdx, OutNodes, OutSoA, SoAOffset);
        }
        else // 자를 곳을 찾았다면
        {
            // 왼쪽 바구니와 오른쪽 바구니에 있을 오브젝트끼리 맞바꾸기
            auto MidPtr = std::partition(
                Objects.begin() + StartIdx, Objects.begin() + EndIdx,
                [=](const FBuildObject &Obj)
                {
                    float Ratio = (Obj.Center[SplitAxis] - CentroidMin[SplitAxis]) / MaxExtent;
                    int   BinIdx = std::clamp(static_cast<int>(Ratio * NUM_BINS), 0, NUM_BINS - 1);
                    return BinIdx <= BestSplitBin;
                });

            int MidIdx = std::distance(Objects.begin(), MidPtr); // MidIdx부터 오른쪽 그룹
            if (MidIdx == StartIdx || MidIdx == EndIdx)
                MidIdx = StartIdx + ObjectCount / 2; // MidIdx가 처음이나 끝이라면 그냥 반갈

            // 재귀
            Node.LeftChild = BuildRecursive(Objects, StartIdx, MidIdx, OutNodes, OutSoA, SoAOffset);
            Node.RightChild = BuildRecursive(Objects, MidIdx, EndIdx, OutNodes, OutSoA, SoAOffset);
        }
    }

    // 만들어진 배열의 시작 index
    return NodeIndex;
}

void FBVHCompressor::CompressToBVH8(const std::vector<FBVHNode> &BinNodes, int BinRootIdx,
                                    std::vector<FBVH8Node> &OutBVH8Nodes)
{
    OutBVH8Nodes.clear();
    OutBVH8Nodes.reserve(BinNodes.size() /
                         4); // 이제 8개씩 묶을 거니까 binary tree 크기에서 1/4만큼 공간 확보
    CollapseRecursive(BinNodes, BinRootIdx, OutBVH8Nodes); // Octree로 압축
}

float FBVHCompressor::GetArea(const FBVHNode &Node)
{
    float ExtX = std::max(0.0f, Node.MaxX - Node.MinX);
    float ExtY = std::max(0.0f, Node.MaxY - Node.MinY);
    float ExtZ = std::max(0.0f, Node.MaxZ - Node.MinZ);
    return 2.0f * (ExtX * ExtY + ExtY * ExtZ + ExtZ * ExtX);
}

int FBVHCompressor::CollapseRecursive(const std::vector<FBVHNode> &BinNodes, int CurrentBinIdx,
                                      std::vector<FBVH8Node> &OutBVH8Nodes)
{
    // binary tree에서 node를 가져온다
    const FBVHNode &CurrentNode = BinNodes[CurrentBinIdx];

    if (CurrentNode.IsLeaf())
    {
        // 이 주소가 다음 node가 아니라 object의 주소임을 알려주기위해 비트를 뒤집어 음수로 표시
        return ~(CurrentNode.SoA_StartIndex);
    }

    // CollectedNodes에 일단 CurrentNode의 왼쪽, 오른쪽 child 넣기
    std::vector<int> CollectedNodes;
    CollectedNodes.push_back(CurrentNode.LeftChild);
    CollectedNodes.push_back(CurrentNode.RightChild);

    // CollectedNodes의 크기가 7보다 클 때까지 반복
    // 모두 LeafNode면 break
    while (CollectedNodes.size() < 8)
    {
        float LargestArea = -1.0f;
        int   NodeToSplitIdx = -1;

        // CollectedNodes내에서 가장 크기가 큰 AABB 선택
        for (size_t i = 0; i < CollectedNodes.size(); ++i)
        {
            const FBVHNode &CNode = BinNodes[CollectedNodes[i]];
            if (!CNode.IsLeaf())
            {
                float Area = GetArea(CNode);
                if (Area > LargestArea)
                {
                    LargestArea = Area;
                    NodeToSplitIdx = i;
                }
            }
        }

        if (NodeToSplitIdx == -1)
            break;

        // 가장 큰 AABB를 지우고 child 둘로 나눠서 다시 넣기
        int SplitTarget = CollectedNodes[NodeToSplitIdx];
        CollectedNodes.erase(CollectedNodes.begin() + NodeToSplitIdx);
        CollectedNodes.push_back(BinNodes[SplitTarget].LeftChild);
        CollectedNodes.push_back(BinNodes[SplitTarget].RightChild);
    }

    int NewBVH8Idx = OutBVH8Nodes.size();
    OutBVH8Nodes.push_back(FBVH8Node());

    // CollectedNodes 안에 몇개가 있던지 8번 순회
    for (int i = 0; i < 8; ++i)
    {
        if (i < CollectedNodes.size())
        {
            int             ChildBinIdx = CollectedNodes[i];
            const FBVHNode &ChildBinNode = BinNodes[ChildBinIdx];

            // [최적화] 옥트리 노드에도 Center와 Extent를 구워서 저장함
            // Frsutum Cull 할 때 사용
            OutBVH8Nodes[NewBVH8Idx].CenterX[i] = (ChildBinNode.MaxX + ChildBinNode.MinX) * 0.5f;
            OutBVH8Nodes[NewBVH8Idx].ExtentX[i] = (ChildBinNode.MaxX - ChildBinNode.MinX) * 0.5f;
            OutBVH8Nodes[NewBVH8Idx].CenterY[i] = (ChildBinNode.MaxY + ChildBinNode.MinY) * 0.5f;
            OutBVH8Nodes[NewBVH8Idx].ExtentY[i] = (ChildBinNode.MaxY - ChildBinNode.MinY) * 0.5f;
            OutBVH8Nodes[NewBVH8Idx].CenterZ[i] = (ChildBinNode.MaxZ + ChildBinNode.MinZ) * 0.5f;
            OutBVH8Nodes[NewBVH8Idx].ExtentZ[i] = (ChildBinNode.MaxZ - ChildBinNode.MinZ) * 0.5f;

            if (ChildBinNode.IsLeaf())
            {
                // 비트를 반전해서 기록
                // 비트를 반전하면 음수가 됨 == object의 주소이다
                OutBVH8Nodes[NewBVH8Idx].Children[i] = ~(ChildBinNode.SoA_StartIndex);
            }
            else
            {
                OutBVH8Nodes[NewBVH8Idx].Children[i] =
                    CollapseRecursive(BinNodes, ChildBinIdx, OutBVH8Nodes);
            }
        }
        else // 더미
        {
            OutBVH8Nodes[NewBVH8Idx].Children[i] = EMPTY_NODE;
            OutBVH8Nodes[NewBVH8Idx].CenterX[i] = 1e30f;
            OutBVH8Nodes[NewBVH8Idx].ExtentX[i] = 0.0f;
            OutBVH8Nodes[NewBVH8Idx].CenterY[i] = 1e30f;
            OutBVH8Nodes[NewBVH8Idx].ExtentY[i] = 0.0f;
            OutBVH8Nodes[NewBVH8Idx].CenterZ[i] = 1e30f;
            OutBVH8Nodes[NewBVH8Idx].ExtentZ[i] = 0.0f;
        }
    }
    // 방금 만든 이 방의 번호 return
    return NewBVH8Idx;
}
