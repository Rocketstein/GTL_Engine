#pragma once

#include "Core/CoreTypes.h"
#include "Math/Transform.h"
#include "Mesh/MeshImporterCommon.h"
#include "Mesh/StaticMeshAsset.h"
#include "Object/FName.h"
#include "ThirdParty/FBX_SDK/include/fbxsdk.h"

#include <filesystem>
#include <memory>
#include <unordered_map>

struct FSkeletalSubMesh;
class UAnimationSequence;
class UMaterial;
class USkeleton;
class UStaticMesh;

struct FFBXImporter
{
    struct FImportedSkeletalMesh
    {
        FName Name;
        FSkeletalSubMesh* MeshData;
        FName SkeletonName;
        TArray<FStaticMaterial> Materials;

        FImportedSkeletalMesh(FName InName, FSkeletalSubMesh* InMeshData, FName InSkeletonName, TArray<FStaticMaterial>&& InMaterials);
        ~FImportedSkeletalMesh();

        FImportedSkeletalMesh(const FImportedSkeletalMesh&) = delete;
        FImportedSkeletalMesh& operator=(const FImportedSkeletalMesh&) = delete;
    };

    struct FImportedFBXAssets
    {
		TArray<FStaticMesh*> StaticMeshes;

        TArray<FImportedSkeletalMesh*> SkeletalMeshes;
        TArray<USkeleton*> Skeletons;
        TArray<UAnimationSequence*> Animations;
        TArray<FStaticMaterial> Materials;

        FImportedFBXAssets();
        ~FImportedFBXAssets();

        FImportedFBXAssets(const FImportedFBXAssets&) = delete;
        FImportedFBXAssets& operator=(const FImportedFBXAssets&) = delete;

        FImportedFBXAssets(FImportedFBXAssets&& Other) noexcept;
        FImportedFBXAssets& operator=(FImportedFBXAssets&& Other) noexcept;
    };

    static bool ImportAndCacheAll(const FString& FBXFilePath, const FImportOptions& Options);
	static bool ImportStaticAndCacheAll(const FString& FBXFilePath, const FImportOptions& Options, UStaticMesh* OutMesh);
    static bool ImportAll(const FString& FBXFilePath, const FImportOptions& Options, FImportedFBXAssets& OutAssets);

private:
    static void Initialize();

    static void ExtractBoneNodeRecursive(FbxNode* Node, int ParentIndex, USkeleton* OutSkeleton);
    static void ExtractMeshAndSkinning(FbxNode* Node, const FString& FBXFilePath, const FImportOptions& Options, FImportedFBXAssets& InAsset);
    static void ExtractAnimations(FbxScene* Scene, FImportedFBXAssets& OutAssets);
    static std::unique_ptr<FSkeletalSubMesh> ParseSkeletalGeometry(FbxNode* InNode, FbxMesh* InFbxMesh, const FImportOptions& Options, TArray<FStaticMaterial>& OutMaterials);
    static std::unique_ptr<FStaticMesh> ParseStaticGeometry(FbxNode* InNode, FbxMesh* InFbxMesh, const FImportOptions& Options, TArray<FStaticMaterial>& OutMaterials);
    static int32 GetFbxPolygonMaterialIndex(FbxNode* Node, FbxMesh* Mesh, int32 PolygonIndex);
    static FVector GetSafeInverseScale(const FVector& Scale);
    static FTransform GetTransformFromNode(FbxNode* Node);
    static FTransform GetTransformFromMatrix(const FMatrix& Matrix);
    static FMatrix ConvertFbxMatrix(const FbxMatrix& Matrix);
    static FMatrix ConvertFbxMatrix(const FbxAMatrix& Matrix);
    static FMatrix GetGeometryMatrix(FbxNode* Node);
    static void ApplyBindPoseToSkeleton(FbxMesh* InFbxMesh, USkeleton* InSkeleton);
    static void ApplySkinBindDataToMesh(FbxMesh* InFbxMesh, USkeleton* InSkeleton, FSkeletalSubMesh* InMesh);
    static FVector GetSkeletonMeshBindInverseScale(USkeleton* Skeleton, const FMatrix& FallbackMeshBindGlobal);
    static USkeleton* FindOwnerSkeletonByBoneNode(FbxNode* BoneNode, const TArray<USkeleton*>& Skeletons, int32* OutBoneIndex = nullptr, FbxNode** OutMatchedBoneNode = nullptr);
    static bool ApplyRigidParentWeightFallback(FbxNode* MeshNode, USkeleton* Skeleton, int32 BoneIndex, FbxNode* BoneNode, FSkeletalSubMesh* Mesh);
    static void ExtractWeights(FbxCluster* InCluster, int InBoneIndex);
    static void ApplyWeightsToSkeleton(FSkeletalSubMesh* InMesh);

    // Material related
    static FString BuildValidMaterialSlotName(const FString& InName, int32 FallbackIndex);
    static FString MakeUniqueGeneratedMaterialName(const FString& BaseName);
    static void AppendFileTexturesFromProperty(const FbxProperty& Property, TArray<FbxFileTexture*>& OutTextures);
    static FbxFileTexture* GetFirstDiffuseTexture(FbxSurfaceMaterial* Material);
    static std::filesystem::path ResolveTexturePathFromFbx(FbxFileTexture* Texture, const FString& FBXFilePath);
    static FVector4 GetMaterialSectionColor(FbxSurfaceMaterial* Material, bool bHasDiffuseTexture);
    static float GetMaterialSpecularPower(FbxSurfaceMaterial* Material);
    static float GetMaterialSpecularStrength(FbxSurfaceMaterial* Material);
    static FString CreateOrLoadMaterialAsset(FbxSurfaceMaterial* Material, const FString& FBXFilePath);
    static UMaterial* ResolveNodeMaterialInterface(FbxNode* Node, int32 FbxMaterialIndex, const FString& FBXFilePath);
    static void AssignImportedMaterialsToSlots(FbxNode* Node, const FString& FBXFilePath, TArray<FStaticMaterial>& Materials);

    static bool bInitialized;
    static FbxManager* SdkManager;

    static TArray<TArray<int>> CtrlPointToVertexIndex;
    using FBoneWeighting = std::pair<uint16, float>;
    static TArray<TArray<FBoneWeighting>> BoneWeighting;

    static TMap<const FbxSurfaceMaterial*, UMaterial*> ImportedMaterialCache;
    static TMap<const FbxSurfaceMaterial*, FString> ImportedMaterialJsonPaths;
    static TMap<FString, int32> GeneratedMaterialNameCounts;
};
