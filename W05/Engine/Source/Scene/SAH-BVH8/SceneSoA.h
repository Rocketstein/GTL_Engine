#pragma once
#include <immintrin.h>
#include <string>
#include <unordered_map>
#include <vector>
#include "ThirdParty/nlohmann/json.hpp"
#include "Core/Math/Matrix.h"

struct FBVH8Node;
class UStaticMeshComponent;
class UStaticMesh;

namespace json
{
    class JSON;
}

class FSceneDataSoA
{
  public:
    int TotalCount = 0;

    // [최적화] Min/Max 대신 Center(중심)와 Extent(크기 절반)를 메모리에 직접 저장
    float *CenterX = nullptr;
    float *ExtentX = nullptr;
    float *CenterY = nullptr;
    float *ExtentY = nullptr;
    float *CenterZ = nullptr;
    float *ExtentZ = nullptr;

    float *DistanceSq = nullptr;
    int   *ObjectIDs = nullptr;
    UStaticMeshComponent **Components = nullptr;
    UStaticMesh **StaticMeshes = nullptr;
    const FMatrix **InverseWorldMatrices = nullptr;

    // 메모리에 올리기
    void Allocate(int Count)
    {
        Free();
        TotalCount = Count;
        size_t FloatBytes = Count * sizeof(float); // (float)4byte x 오브젝트 수
        size_t IntBytes = Count * sizeof(int);     // (int)4byte x 오브젝트 수
        size_t PtrBytes = Count * sizeof(void *);

        size_t MatrixBytes = Count * sizeof(FMatrix);

        // AVX2 레지스터(__m256)의 크기가 32byte이기 때문에
        // 1클럭만에 퍼올릴 수 있게 메모리 시작 주소가 32의 배수가 되도록 align 해줘
        CenterX = (float *)_mm_malloc(FloatBytes, 32);
        ExtentX = (float *)_mm_malloc(FloatBytes, 32);
        CenterY = (float *)_mm_malloc(FloatBytes, 32);
        ExtentY = (float *)_mm_malloc(FloatBytes, 32);
        CenterZ = (float *)_mm_malloc(FloatBytes, 32);
        ExtentZ = (float *)_mm_malloc(FloatBytes, 32);

        DistanceSq = (float *)_mm_malloc(FloatBytes, 32);
        ObjectIDs = (int *)_mm_malloc(IntBytes, 32);
        Components = (UStaticMeshComponent **)_mm_malloc(PtrBytes, 32);
        StaticMeshes = (UStaticMesh **)_mm_malloc(PtrBytes, 32);
        InverseWorldMatrices = (const FMatrix **)_mm_malloc(PtrBytes, 32);
    }

    void Free()
    {
        if (CenterX)
        {
            _mm_free(CenterX);
            CenterX = nullptr;
        }
        if (ExtentX)
        {
            _mm_free(ExtentX);
            ExtentX = nullptr;
        }
        if (CenterY)
        {
            _mm_free(CenterY);
            CenterY = nullptr;
        }
        if (ExtentY)
        {
            _mm_free(ExtentY);
            ExtentY = nullptr;
        }
        if (CenterZ)
        {
            _mm_free(CenterZ);
            CenterZ = nullptr;
        }
        if (ExtentZ)
        {
            _mm_free(ExtentZ);
            ExtentZ = nullptr;
        }
        if (DistanceSq)
        {
            _mm_free(DistanceSq);
            DistanceSq = nullptr;
        }
        if (ObjectIDs)
        {
            _mm_free(ObjectIDs);
            ObjectIDs = nullptr;
        }
        if (Components)
        {
            _mm_free(Components);
            Components = nullptr;
        }
        if (StaticMeshes)
        {
            _mm_free(StaticMeshes);
            StaticMeshes = nullptr;
        }
        if (InverseWorldMatrices)
        {
            _mm_free(InverseWorldMatrices);
            InverseWorldMatrices = nullptr;
        }
        TotalCount = 0;
    }

    ~FSceneDataSoA() { Free(); }

    void BuildSoA(std::vector<UStaticMeshComponent *> &OutComponents,
                  std::vector<FBVH8Node>              &OutFinalBVH8);
};