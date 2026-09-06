#include "SceneRenderer.h"
#include "Engine/Component/StaticMeshComponent.h"
#include "Engine/Asset/StaticMesh.h"
#include "MeshBLAS.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
    // 1. Ray-AABB 충돌 (Slab Method)
    // 1. Ray-AABB 충돌 (Slab Method - Fast version)
    __forceinline bool IntersectLocalAABB_Fast(const FVector3 &RayOrigin, const FVector3 &InvDir,
                                        const int DirIsNeg[3], const Geometry::FAABB &Bounds,
                                        float MaxT)
    {
        float tmin, tmax, tymin, tymax, tzmin, tzmax;

        // X축
        if (DirIsNeg[0])
        {
            tmin = (Bounds.Max.X - RayOrigin.X) * InvDir.X;
            tmax = (Bounds.Min.X - RayOrigin.X) * InvDir.X;
        }
        else
        {
            tmin = (Bounds.Min.X - RayOrigin.X) * InvDir.X;
            tmax = (Bounds.Max.X - RayOrigin.X) * InvDir.X;
        }

        // Y축
        if (DirIsNeg[1])
        {
            tymin = (Bounds.Max.Y - RayOrigin.Y) * InvDir.Y;
            tymax = (Bounds.Min.Y - RayOrigin.Y) * InvDir.Y;
        }
        else
        {
            tymin = (Bounds.Min.Y - RayOrigin.Y) * InvDir.Y;
            tymax = (Bounds.Max.Y - RayOrigin.Y) * InvDir.Y;
        }

        if ((tmin > tymax) || (tymin > tmax))
            return false;
        if (tymin > tmin)
            tmin = tymin;
        if (tymax < tmax)
            tmax = tymax;

        // Z축
        if (DirIsNeg[2])
        {
            tzmin = (Bounds.Max.Z - RayOrigin.Z) * InvDir.Z;
            tzmax = (Bounds.Min.Z - RayOrigin.Z) * InvDir.Z;
        }
        else
        {
            tzmin = (Bounds.Min.Z - RayOrigin.Z) * InvDir.Z;
            tzmax = (Bounds.Max.Z - RayOrigin.Z) * InvDir.Z;
        }

        if ((tmin > tzmax) || (tzmin > tmax))
            return false;
        if (tzmin > tmin)
            tmin = tzmin;
        if (tzmax < tmax)
            tmax = tzmax;

        return (tmin < MaxT) && (tmax > 0.0f);
    }

    // 2. Ray-Triangle 충돌 (Möller-Trumbore) - 질문자님이 만들어둔 E1, E2를 100% 활용!
    bool IntersectLocalTriangle(const FVector3 &RayOrigin, const FVector3 &RayDir,
                                const FStaticMeshTrianglePrecompute &Tri, float &OutT)
    {
        const float EPSILON = 1e-8f;
        FVector3    h = FVector3::CrossProduct(RayDir, Tri.E2);
        float       a = FVector3::DotProduct(Tri.E1, h);

        if (a > -EPSILON && a < EPSILON)
            return false;

        float    f = 1.0f / a;
        FVector3 s = RayOrigin - Tri.V0;
        float    u = f * FVector3::DotProduct(s, h);

        if (u < 0.0f || u > 1.0f)
            return false;

        FVector3 q = FVector3::CrossProduct(s, Tri.E1);
        float    v = f * FVector3::DotProduct(RayDir, q);

        if (v < 0.0f || u + v > 1.0f)
            return false;

        float t = f * FVector3::DotProduct(Tri.E2, q);
        if (t > EPSILON)
        {
            OutT = t;
            return true;
        }
        return false;
    }
    // 8개의 삼각형을 한 번에 검사하는 AVX2 Möller-Trumbore 교차 함수
    __forceinline int IntersectTriangles8_AVX2(__m256 vRoX, __m256 vRoY, __m256 vRoZ, __m256 vRdX,
                                        __m256 vRdY, __m256 vRdZ, __m256 vV0X, __m256 vV0Y,
                                        __m256 vV0Z, __m256 vE1X, __m256 vE1Y, __m256 vE1Z,
                                        __m256 vE2X, __m256 vE2Y, __m256 vE2Z, __m256 &outT)
    {
        const __m256 vEps = _mm256_set1_ps(1e-8f);
        const __m256 vNegEps = _mm256_set1_ps(-1e-8f);
        const __m256 vZero = _mm256_setzero_ps();
        const __m256 vOne = _mm256_set1_ps(1.0f);

        // h = cross(Rd, E2)
        __m256 hx = _mm256_sub_ps(_mm256_mul_ps(vRdY, vE2Z), _mm256_mul_ps(vRdZ, vE2Y));
        __m256 hy = _mm256_sub_ps(_mm256_mul_ps(vRdZ, vE2X), _mm256_mul_ps(vRdX, vE2Z));
        __m256 hz = _mm256_sub_ps(_mm256_mul_ps(vRdX, vE2Y), _mm256_mul_ps(vRdY, vE2X));

        // a = dot(E1, h)
        __m256 a = _mm256_add_ps(_mm256_add_ps(_mm256_mul_ps(vE1X, hx), _mm256_mul_ps(vE1Y, hy)),
                                 _mm256_mul_ps(vE1Z, hz));

        // 광선과 평행한 경우 (a > -EPS && a < EPS) 탈락 마스크 생성
        __m256 maskA = _mm256_and_ps(_mm256_cmp_ps(a, vNegEps, _CMP_GT_OQ),
                                     _mm256_cmp_ps(a, vEps, _CMP_LT_OQ));
        __m256 valid = _mm256_andnot_ps(
            maskA, _mm256_castsi256_ps(_mm256_set1_epi32(-1))); // 0xFFFFFFFF for valid ones

        // f = 1.0 / a
        __m256 f = _mm256_div_ps(vOne, a);

        // s = Ro - V0
        __m256 sx = _mm256_sub_ps(vRoX, vV0X);
        __m256 sy = _mm256_sub_ps(vRoY, vV0Y);
        __m256 sz = _mm256_sub_ps(vRoZ, vV0Z);

        // u = f * dot(s, h)
        __m256 u = _mm256_mul_ps(
            f, _mm256_add_ps(_mm256_add_ps(_mm256_mul_ps(sx, hx), _mm256_mul_ps(sy, hy)),
                             _mm256_mul_ps(sz, hz)));

        // u < 0.0 || u > 1.0 탈락
        valid = _mm256_andnot_ps(
            _mm256_or_ps(_mm256_cmp_ps(u, vZero, _CMP_LT_OQ), _mm256_cmp_ps(u, vOne, _CMP_GT_OQ)),
            valid);

        // q = cross(s, E1)
        __m256 qx = _mm256_sub_ps(_mm256_mul_ps(sy, vE1Z), _mm256_mul_ps(sz, vE1Y));
        __m256 qy = _mm256_sub_ps(_mm256_mul_ps(sz, vE1X), _mm256_mul_ps(sx, vE1Z));
        __m256 qz = _mm256_sub_ps(_mm256_mul_ps(sx, vE1Y), _mm256_mul_ps(sy, vE1X));

        // v = f * dot(Rd, q)
        __m256 v = _mm256_mul_ps(
            f, _mm256_add_ps(_mm256_add_ps(_mm256_mul_ps(vRdX, qx), _mm256_mul_ps(vRdY, qy)),
                             _mm256_mul_ps(vRdZ, qz)));

        // v < 0.0 || u + v > 1.0 탈락
        valid = _mm256_andnot_ps(_mm256_or_ps(_mm256_cmp_ps(v, vZero, _CMP_LT_OQ),
                                              _mm256_cmp_ps(_mm256_add_ps(u, v), vOne, _CMP_GT_OQ)),
                                 valid);

        // t = f * dot(E2, q)
        __m256 t = _mm256_mul_ps(
            f, _mm256_add_ps(_mm256_add_ps(_mm256_mul_ps(vE2X, qx), _mm256_mul_ps(vE2Y, qy)),
                             _mm256_mul_ps(vE2Z, qz)));

        // t > EPS 탈락
        valid = _mm256_and_ps(valid, _mm256_cmp_ps(t, vEps, _CMP_GT_OQ));

        outT = t;
        // 유효한 충돌 결과만 8비트 마스크로 추출하여 반환 (bit 0~7)
        return _mm256_movemask_ps(valid);
    }

    struct FMeshStackItem
    {
        int NodeData;
        int TriCount;
    };

    bool RaycastStaticMeshLocal(const UStaticMesh *Mesh, const FVector3 &LocalOrigin,
                                const FVector3 &LocalDir, float &OutHitT, int32 &OutHitTriIndex)
    {
        if (!Mesh || !Mesh->HasRaycastData())
            return false;
        const auto &RaycastData = Mesh->GetRaycastData();

        // ⭐ 이제 이진 트리가 아닌 BVH8Nodes를 순회합니다!
        if (RaycastData.BVH8Nodes.empty())
            return false;

        FVector3 InvDir(1.0f / (LocalDir.X != 0.0f ? LocalDir.X : 1e-6f),
                        1.0f / (LocalDir.Y != 0.0f ? LocalDir.Y : 1e-6f),
                        1.0f / (LocalDir.Z != 0.0f ? LocalDir.Z : 1e-6f));

        __m256 vRoX = _mm256_set1_ps(LocalOrigin.X);
        __m256 vRoY = _mm256_set1_ps(LocalOrigin.Y);
        __m256 vRoZ = _mm256_set1_ps(LocalOrigin.Z);
        __m256 vRdX = _mm256_set1_ps(LocalDir.X);
        __m256 vRdY = _mm256_set1_ps(LocalDir.Y);
        __m256 vRdZ = _mm256_set1_ps(LocalDir.Z);
        __m256 vIdX = _mm256_set1_ps(InvDir.X);
        __m256 vIdY = _mm256_set1_ps(InvDir.Y);
        __m256 vIdZ = _mm256_set1_ps(InvDir.Z);

        FMeshStackItem Stack[64];
        int            StackPtr = 0;
        Stack[StackPtr++] = {0, 0}; // Root is index 0
        bool bHit = false;

        struct FHitChild
        {
            float Dist;
            int   NodeData;
            int   TriCount;
        };

        while (StackPtr > 0)
        {
            FMeshStackItem CurrentItem = Stack[--StackPtr];

            // 1. 리프 노드인 경우 (삼각형 충돌 검사)
            if (CurrentItem.NodeData < 0)
            {
                if (CurrentItem.NodeData == -2147483648)
                    continue; // EMPTY_NODE

                // 이제 StartIdx는 트리 원래 인덱스가 아니라 구워진 SoA 배열의 정확한 시작점입니다.
                int SoAStartIdx = ~CurrentItem.NodeData;

                // ⭐ 런타임 조립(for루프 패킹) 제거! 잘 구워진 SoA 배열을 바로 레지스터로
                // 로드합니다.
                const auto &SoA = RaycastData.TriangleSoA;

                __m256 vV0X = _mm256_loadu_ps(&SoA.V0X[SoAStartIdx]);
                __m256 vV0Y = _mm256_loadu_ps(&SoA.V0Y[SoAStartIdx]);
                __m256 vV0Z = _mm256_loadu_ps(&SoA.V0Z[SoAStartIdx]);

                __m256 vE1X = _mm256_loadu_ps(&SoA.E1X[SoAStartIdx]);
                __m256 vE1Y = _mm256_loadu_ps(&SoA.E1Y[SoAStartIdx]);
                __m256 vE1Z = _mm256_loadu_ps(&SoA.E1Z[SoAStartIdx]);

                __m256 vE2X = _mm256_loadu_ps(&SoA.E2X[SoAStartIdx]);
                __m256 vE2Y = _mm256_loadu_ps(&SoA.E2Y[SoAStartIdx]);
                __m256 vE2Z = _mm256_loadu_ps(&SoA.E2Z[SoAStartIdx]);

                __m256 outT;
                int    hitMask =
                    IntersectTriangles8_AVX2(vRoX, vRoY, vRoZ, vRdX, vRdY, vRdZ, vV0X, vV0Y, vV0Z,
                                             vE1X, vE1Y, vE1Z, vE2X, vE2Y, vE2Z, outT);

                if (hitMask != 0)
                {
                    alignas(32) float HitDistances[8];
                    _mm256_store_ps(HitDistances, outT);
                    for (int k = 0; k < 8; ++k)
                    {
                        if ((hitMask & (1 << k)) && (HitDistances[k] < OutHitT))
                        {
                            OutHitT = HitDistances[k];
                            // 원본 삼각형 인덱스 복구
                            OutHitTriIndex = SoA.OriginalIndices[SoAStartIdx + k];
                            bHit = true;
                        }
                    }
                }
                continue;
            }

            // 2. 내부 노드인 경우 (AABB 8개 교차 검사)
            const auto &Node = RaycastData.BVH8Nodes[CurrentItem.NodeData];

            __m256 vCx = _mm256_load_ps(Node.CenterX);
            __m256 vCy = _mm256_load_ps(Node.CenterY);
            __m256 vCz = _mm256_load_ps(Node.CenterZ);
            __m256 vEx = _mm256_load_ps(Node.ExtentX);
            __m256 vEy = _mm256_load_ps(Node.ExtentY);
            __m256 vEz = _mm256_load_ps(Node.ExtentZ);

            __m256 vMinX = _mm256_sub_ps(vCx, vEx);
            __m256 vMaxX = _mm256_add_ps(vCx, vEx);
            __m256 vMinY = _mm256_sub_ps(vCy, vEy);
            __m256 vMaxY = _mm256_add_ps(vCy, vEy);
            __m256 vMinZ = _mm256_sub_ps(vCz, vEz);
            __m256 vMaxZ = _mm256_add_ps(vCz, vEz);

            __m256 t1x = _mm256_mul_ps(_mm256_sub_ps(vMinX, vRoX), vIdX);
            __m256 t2x = _mm256_mul_ps(_mm256_sub_ps(vMaxX, vRoX), vIdX);
            __m256 tminX = _mm256_min_ps(t1x, t2x);
            __m256 tmaxX = _mm256_max_ps(t1x, t2x);

            __m256 t1y = _mm256_mul_ps(_mm256_sub_ps(vMinY, vRoY), vIdY);
            __m256 t2y = _mm256_mul_ps(_mm256_sub_ps(vMaxY, vRoY), vIdY);
            __m256 tminY = _mm256_min_ps(t1y, t2y);
            __m256 tmaxY = _mm256_max_ps(t1y, t2y);

            __m256 t1z = _mm256_mul_ps(_mm256_sub_ps(vMinZ, vRoZ), vIdZ);
            __m256 t2z = _mm256_mul_ps(_mm256_sub_ps(vMaxZ, vRoZ), vIdZ);
            __m256 tminZ = _mm256_min_ps(t1z, t2z);
            __m256 tmaxZ = _mm256_max_ps(t1z, t2z);

            __m256 tmin = _mm256_max_ps(_mm256_max_ps(tminX, tminY), tminZ);
            __m256 tmax = _mm256_min_ps(_mm256_min_ps(tmaxX, tmaxY), tmaxZ);

            __m256 vZero = _mm256_setzero_ps();
            __m256 vOutHitT = _mm256_set1_ps(OutHitT); // 지금까지 찾은 가장 짧은 거리!

            __m256 mask1 = _mm256_cmp_ps(tmax, _mm256_max_ps(vZero, tmin), _CMP_GE_OQ);
            __m256 mask2 = _mm256_cmp_ps(tmin, vOutHitT, _CMP_LT_OQ);
            __m256 finalMask = _mm256_and_ps(mask1, mask2);

            int HitMask = _mm256_movemask_ps(finalMask);
            if (HitMask == 0)
                continue;

            float Distances[8];
            _mm256_storeu_ps(Distances, tmin);

            FHitChild HitChildren[8];
            int       HitCount = 0;
            for (int j = 0; j < 8; ++j)
            {
                if (HitMask & (1 << j))
                {
                    HitChildren[HitCount++] = {std::max(0.0f, Distances[j]), Node.Children[j],
                                               Node.TriangleCounts[j]};
                }
            }

            if (HitCount > 1)
            {
                for (int i = 1; i < HitCount; ++i)
                {
                    FHitChild Key = HitChildren[i];
                    int       j = i - 1;
                    while (j >= 0 && HitChildren[j].Dist < Key.Dist)
                    {
                        HitChildren[j + 1] = HitChildren[j];
                        j = j - 1;
                    }
                    HitChildren[j + 1] = Key;
                }
            }

            for (int i = 0; i < HitCount; ++i)
            {
                Stack[StackPtr++] = {HitChildren[i].NodeData, HitChildren[i].TriCount};
            }
        }
        return bHit;
    }


} // namespace

FRaycastHit RaycastScene(FSceneDataSoA *SoA, FBVH8Node *BVH8Nodes, int RootNodeIndex,
                         const FVector3 &RayOrigin, const FVector3 &RayDir)
{
    FRaycastHit BestHit;
    float BestHitDistanceSq = std::numeric_limits<float>::max();
    float BestHitRayT = BestHit.Distance;
    const float RayDirLenSq =
        (RayDir.X * RayDir.X) + (RayDir.Y * RayDir.Y) + (RayDir.Z * RayDir.Z);

    FVector3 InvDir(1.0f / (RayDir.X != 0.0f ? RayDir.X : 1e-6f),
                    1.0f / (RayDir.Y != 0.0f ? RayDir.Y : 1e-6f),
                    1.0f / (RayDir.Z != 0.0f ? RayDir.Z : 1e-6f));

    __m256 vRoX = _mm256_set1_ps(RayOrigin.X);
    __m256 vRoY = _mm256_set1_ps(RayOrigin.Y);
    __m256 vRoZ = _mm256_set1_ps(RayOrigin.Z);
    __m256 vIdX = _mm256_set1_ps(InvDir.X);
    __m256 vIdY = _mm256_set1_ps(InvDir.Y);
    __m256 vIdZ = _mm256_set1_ps(InvDir.Z);

    // [안전장치] 자식 8개를 모두 밀어넣을 수 있으므로 스택 크기를 128로 넉넉하게 늘립니다.
    int Stack[128];
    int StackPtr = 0;
    Stack[StackPtr++] = RootNodeIndex;

    // 히트된 자식들을 정렬하기 위한 구조체
    struct FHitChild
    {
        float Dist;
        int   NodeData;
    };

    while (StackPtr > 0)
    {
        int CurrentData = Stack[--StackPtr]; // 내부 노드 인덱스거나, 리프 노드(~StartIdx)

        // ---------------------------------------------------------
        // 1. 스택에서 꺼낸 것이 Leaf 노드(SoA)인 경우 즉시 정밀 검사
        // ---------------------------------------------------------
        if (CurrentData < 0)
        {
            if (CurrentData == EMPTY_NODE)
                continue;

            int StartIdx = ~CurrentData;

            struct FLeafCandidate
            {
                float Dist;
                int   Slot;
                int   ObjID;
            };

            FLeafCandidate Candidates[8];
            int            CandidateCount = 0;

            // 1) Leaf 안 8개 오브젝트에 대해 월드 AABB 1차 필터
            for (int k = 0; k < 8; ++k)
            {
                const int SoAIdx = StartIdx + k;
                const int ObjID = SoA->ObjectIDs[SoAIdx];
                if (ObjID == -1)
                    continue;

                ++BestHit.LeafObjectsVisited;

                const float MinX = SoA->CenterX[SoAIdx] - SoA->ExtentX[SoAIdx];
                const float MaxX = SoA->CenterX[SoAIdx] + SoA->ExtentX[SoAIdx];
                const float MinY = SoA->CenterY[SoAIdx] - SoA->ExtentY[SoAIdx];
                const float MaxY = SoA->CenterY[SoAIdx] + SoA->ExtentY[SoAIdx];
                const float MinZ = SoA->CenterZ[SoAIdx] - SoA->ExtentZ[SoAIdx];
                const float MaxZ = SoA->CenterZ[SoAIdx] + SoA->ExtentZ[SoAIdx];

                const float t1x = (MinX - RayOrigin.X) * InvDir.X;
                const float t2x = (MaxX - RayOrigin.X) * InvDir.X;
                const float tminX = std::min(t1x, t2x);
                const float tmaxX = std::max(t1x, t2x);

                const float t1y = (MinY - RayOrigin.Y) * InvDir.Y;
                const float t2y = (MaxY - RayOrigin.Y) * InvDir.Y;
                const float tminY = std::min(t1y, t2y);
                const float tmaxY = std::max(t1y, t2y);

                const float t1z = (MinZ - RayOrigin.Z) * InvDir.Z;
                const float t2z = (MaxZ - RayOrigin.Z) * InvDir.Z;
                const float tminZ = std::min(t1z, t2z);
                const float tmaxZ = std::max(t1z, t2z);

                const float tmin = std::max(std::max(tminX, tminY), tminZ);
                const float tmax = std::min(std::min(tmaxX, tmaxY), tmaxZ);
                if (tmax < std::max(0.0f, tmin) || tmin >= BestHitRayT)
                {
                    ++BestHit.LeafAABBRejected;
                    continue;
                }

                Candidates[CandidateCount++] = {std::max(0.0f, tmin), SoAIdx, ObjID};
            }

            // 2) 가까운 후보부터 BLAS를 돌리기 위해 leaf 내부 정렬
            for (int i = 1; i < CandidateCount; ++i)
            {
                FLeafCandidate Key = Candidates[i];
                int            j = i - 1;
                while (j >= 0 && Candidates[j].Dist > Key.Dist)
                {
                    Candidates[j + 1] = Candidates[j];
                    --j;
                }
                Candidates[j + 1] = Key;
            }

            // 3) 정렬된 순서로 BLAS 정밀 검사
            for (int i = 0; i < CandidateCount; ++i)
            {
                if (Candidates[i].Dist >= BestHitRayT)
                    continue;

                const int SoAIdx = Candidates[i].Slot;
                UStaticMeshComponent *Comp = SoA->Components[SoAIdx];
                UStaticMesh *Mesh = SoA->StaticMeshes[SoAIdx];
                const FMatrix *InvWorldMat = SoA->InverseWorldMatrices[SoAIdx];
                if (!Comp || !Mesh || !InvWorldMat)
                    continue;

                FVector3 LocalOrigin = InvWorldMat->TransformPosition(RayOrigin);
                FVector3 LocalDir = InvWorldMat->TransformVector(RayDir);

                float HitT = BestHitRayT;
                int32 HitTri = -1;
                ++BestHit.BLASTests;

                if (RaycastStaticMeshLocal(Mesh, LocalOrigin, LocalDir, HitT, HitTri) &&
                    HitT < BestHitRayT)
                {
                    ++BestHit.BLASHits;
                    BestHit.bHit = true;
                    BestHitRayT = HitT;
                    BestHitDistanceSq = HitT * HitT * RayDirLenSq;
                    BestHit.Distance = BestHitDistanceSq;
                    BestHit.HitObjectID = Candidates[i].ObjID;
                    BestHit.HitTriangleIndex = HitTri;
                }
            }
            continue; // 리프 노드 검사가 끝났으므로 다음 스택 팝
        }

        // ---------------------------------------------------------
        // 2. 내부 노드인 경우 AVX2를 통해 8개의 자식 AABB와 교차 검사
        // ---------------------------------------------------------
        FBVH8Node &Node = BVH8Nodes[CurrentData];

        __m256 vCx = _mm256_load_ps(Node.CenterX);
        __m256 vCy = _mm256_load_ps(Node.CenterY);
        __m256 vCz = _mm256_load_ps(Node.CenterZ);
        __m256 vEx = _mm256_load_ps(Node.ExtentX);
        __m256 vEy = _mm256_load_ps(Node.ExtentY);
        __m256 vEz = _mm256_load_ps(Node.ExtentZ);

        __m256 vMinX = _mm256_sub_ps(vCx, vEx);
        __m256 vMaxX = _mm256_add_ps(vCx, vEx);
        __m256 vMinY = _mm256_sub_ps(vCy, vEy);
        __m256 vMaxY = _mm256_add_ps(vCy, vEy);
        __m256 vMinZ = _mm256_sub_ps(vCz, vEz);
        __m256 vMaxZ = _mm256_add_ps(vCz, vEz);

        __m256 t1x = _mm256_mul_ps(_mm256_sub_ps(vMinX, vRoX), vIdX);
        __m256 t2x = _mm256_mul_ps(_mm256_sub_ps(vMaxX, vRoX), vIdX);
        __m256 tminX = _mm256_min_ps(t1x, t2x);
        __m256 tmaxX = _mm256_max_ps(t1x, t2x);

        __m256 t1y = _mm256_mul_ps(_mm256_sub_ps(vMinY, vRoY), vIdY);
        __m256 t2y = _mm256_mul_ps(_mm256_sub_ps(vMaxY, vRoY), vIdY);
        __m256 tminY = _mm256_min_ps(t1y, t2y);
        __m256 tmaxY = _mm256_max_ps(t1y, t2y);

        __m256 t1z = _mm256_mul_ps(_mm256_sub_ps(vMinZ, vRoZ), vIdZ);
        __m256 t2z = _mm256_mul_ps(_mm256_sub_ps(vMaxZ, vRoZ), vIdZ);
        __m256 tminZ = _mm256_min_ps(t1z, t2z);
        __m256 tmaxZ = _mm256_max_ps(t1z, t2z);

        __m256 tmin = _mm256_max_ps(_mm256_max_ps(tminX, tminY), tminZ);
        __m256 tmax = _mm256_min_ps(_mm256_min_ps(tmaxX, tmaxY), tmaxZ);

        __m256 vZero = _mm256_setzero_ps();
        __m256 vBestDist = _mm256_set1_ps(BestHitRayT); // 선형 Ray T 기반 프루닝 거리

        __m256 mask1 = _mm256_cmp_ps(tmax, _mm256_max_ps(vZero, tmin), _CMP_GE_OQ);
        __m256 mask2 = _mm256_cmp_ps(tmin, vBestDist, _CMP_LT_OQ);
        __m256 finalMask = _mm256_and_ps(mask1, mask2);

        int HitMask = _mm256_movemask_ps(finalMask);
        if (HitMask == 0)
            continue;

        // ---------------------------------------------------------
        // 3. 거리 추출 및 정렬 후 스택 삽입
        // ---------------------------------------------------------
        float Distances[8];
        _mm256_storeu_ps(Distances, tmin); // AVX2에서 구한 거리(tmin) 배열을 메모리로 추출

        FHitChild HitChildren[8];
        int       HitCount = 0;

        for (int j = 0; j < 8; ++j)
        {
            if (HitMask & (1 << j))
            {
                // 광선 시작점이 박스 내부일 경우 tmin이 음수이므로 0으로 보정
                float HitDist = std::max(0.0f, Distances[j]);
                HitChildren[HitCount++] = {HitDist, Node.Children[j]};
            }
        }

        // 거리가 "먼" 것부터 "가까운" 순서로 정렬 (내림차순)
        if (HitCount > 1)
        {
            for (int i = 1; i < HitCount; ++i)
            {
                FHitChild Key = HitChildren[i];
                int       j = i - 1;
                // 내림차순이므로 Key.Dist가 더 크면 앞으로 당겨옵니다.
                while (j >= 0 && HitChildren[j].Dist < Key.Dist)
                {
                    HitChildren[j + 1] = HitChildren[j];
                    j = j - 1;
                }
                HitChildren[j + 1] = Key;
            }
        }

        // 스택에 밀어넣기 (먼 노드가 먼저 들어가고, 가장 가까운 노드가 마지막에 들어가므로 꺼낼 땐
        // 가까운 것부터 나옴)
        for (int i = 0; i < HitCount; ++i)
        {
            Stack[StackPtr++] = HitChildren[i].NodeData;
        }
    }

    return BestHit;
}
