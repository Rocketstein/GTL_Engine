#pragma once

#include "SceneRenderer.h"
#include "SceneSoA.h"
#include <algorithm>
#include <vector>

struct FBuildObject
{
    float Min[3];
    float Max[3];
    float Center[3];
    int   ObjectID;
    UStaticMeshComponent *Component = nullptr;
    UStaticMesh *StaticMesh = nullptr;
    const FMatrix *InverseWorldMatrix = nullptr;
};

struct FBin
{
    int   Count = 0;
    float BoundsMin[3] = {1e30f, 1e30f, 1e30f};
    float BoundsMax[3] = {-1e30f, -1e30f, -1e30f};

    void Include(const FBuildObject &Obj);
};

class FBVHBuilder
{
  public:
    static constexpr int NUM_BINS = 8;
    static constexpr int MAX_LEAF_SIZE = 8; // AVX2를 위해 Leaf당 8개 제한 필수

    static float GetSurfaceArea(const float Min[3], const float Max[3]);
    static int   BuildRecursive(std::vector<FBuildObject> &Objects, int StartIdx, int EndIdx,
                                std::vector<FBVHNode> &OutNodes, FSceneDataSoA *OutSoA,
                                int &SoAOffset);
};

class FBVHCompressor
{
  public:
    static void CompressToBVH8(const std::vector<FBVHNode> &BinNodes, int BinRootIdx,
                               std::vector<FBVH8Node> &OutBVH8Nodes);

  private:
    static float GetArea(const FBVHNode &Node);
    static int   CollapseRecursive(const std::vector<FBVHNode> &BinNodes, int CurrentBinIdx,
                                   std::vector<FBVH8Node> &OutBVH8Nodes);
};