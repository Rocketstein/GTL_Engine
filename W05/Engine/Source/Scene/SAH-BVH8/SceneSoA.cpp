#include "SceneSoA.h"
#include "Core/Math/Vector3.h"
#include "Engine/Component/StaticMeshComponent.h"
#include "ThirdParty/nlohmann/json.hpp"
#include "SceneBVHBuilder.h"
#include <iostream>

void FSceneDataSoA::BuildSoA(std::vector<UStaticMeshComponent *> &OutComponents,
                             std::vector<FBVH8Node>              &OutFinalBVH8)
{
    // 먼저 Object 수 구하기
    int ObjectCount = static_cast<int>(OutComponents.size());
    if (ObjectCount == 0)
        return;

    // AVX2 명령어(__m256)는 데이터를 무조건 8개 단위로 한 입에 집어삼킴
    // Leaf 노드에 사과가 1개뿐이더라도 무조건 8칸을 읽으려고 시도
    // 메모리를 뚫고 나가서 크래시가 발생하기 때문에 더미라도 읽어야함
    // leaf node에 실제로 몇개의 object가 있을 지 모르지만 8개의 방이 준비되어야 하는건 맞기 때문에
    // 여유있게 잡음
    //
    Allocate(ObjectCount * 8);

    // 임시 구조체
    std::vector<FBuildObject> TempBuildObjects;
    TempBuildObjects.reserve(ObjectCount);

    for (int i = 0; i < ObjectCount; ++i)
    {
        UStaticMeshComponent *Comp = OutComponents[i];
        if (!Comp)
            continue;

        const auto &WorldAABB = Comp->GetCachedWorldAABB();

        // 추출한 정보를 토대로 Min, Max, Center, ObjectID 작성
        FBuildObject Obj;
        Obj.Min[0] = WorldAABB.Min.X;
        Obj.Max[0] = WorldAABB.Max.X;
        Obj.Min[1] = WorldAABB.Min.Y;
        Obj.Max[1] = WorldAABB.Max.Y;
        Obj.Min[2] = WorldAABB.Min.Z;
        Obj.Max[2] = WorldAABB.Max.Z;

        Obj.Center[0] = (Obj.Min[0] + Obj.Max[0]) * 0.5f;
        Obj.Center[1] = (Obj.Min[1] + Obj.Max[1]) * 0.5f;
        Obj.Center[2] = (Obj.Min[2] + Obj.Max[2]) * 0.5f;
        Obj.ObjectID = i;
        Obj.Component = Comp;
        Obj.StaticMesh = Comp->GetStaticMesh();
        Obj.InverseWorldMatrix = &Comp->GetCachedInverseWorldMatrix();

        // 임시 구조체에 넣기
        TempBuildObjects.push_back(Obj);
    }

    // SAH를 이용해 예쁘게 구분해야함
    // 8등분은 경우의 수와 계산량이 너무 많음
    // 8등분 대신 우선 2등분으로 임시로 쪼개기
    std::vector<FBVHNode> BinaryBVHNodes;
    BinaryBVHNodes.reserve(ObjectCount * 2); // 총 node의 수는 N x 2 - 1 를 넘을 수 없음
    int SoAWriteOffset = 0;

    // Binary Tree 완성
    int BinaryRootIdx = FBVHBuilder::BuildRecursive(TempBuildObjects, 0, TempBuildObjects.size(),
                                                    BinaryBVHNodes, this, SoAWriteOffset);

    // [최적화] 구조체 변경에 따라 잔여 패딩 채우기도 변경됨
    for (int i = SoAWriteOffset; i < TotalCount; ++i)
    {
        CenterX[i] = CenterY[i] = CenterZ[i] = 1e30f;
        ExtentX[i] = ExtentY[i] = ExtentZ[i] = 0.0f;
        ObjectIDs[i] = -1;
        Components[i] = nullptr;
        StaticMeshes[i] = nullptr;
        InverseWorldMatrices[i] = nullptr;
    }

    // Octree로 압축
    // OutFinalBVH8로 반환
    FBVHCompressor::CompressToBVH8(BinaryBVHNodes, BinaryRootIdx, OutFinalBVH8);
}