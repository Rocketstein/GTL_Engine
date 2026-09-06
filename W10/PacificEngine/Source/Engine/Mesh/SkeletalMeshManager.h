#pragma once

#include "Core/CoreTypes.h"
#include "Object/ObjectIterator.h"
#include "Render/RHI/D3D11/Common/D3D11API.h"
#include "Core/Singleton.h"
#include "Resource/IAssetLoader.h"
#include "Mesh/SkeletalMesh.h"
#include <map>
#include <string>
#include <memory>

struct FImportOptions;

// 에디터/UI에서 보여줄 수 있는 .fbx 리스트 항목
struct FSkeletalMeshAssetListItem
{
    FString DisplayName;
    FString FullPath;
};

// SkeletalMesh 관련 객체의 생성, 조회, 수명 관리를 담당
class FSkeletalMeshManager : public TSingleton<FSkeletalMeshManager>, public IAssetLoader<USkeletalMesh>
{
    friend class TSingleton<FSkeletalMeshManager>;

    // path → USkeletalMesh* 캐시 (소유권은 UObjectManager)
    TMap<FString, USkeletalMesh*> SkeletalMeshCache;

    TArray<FSkeletalMeshAssetListItem> AvailableMeshFiles;    
    TArray<FSkeletalMeshAssetListItem> AvailableFBXFiles;
    TArray<FSkeletalMeshAssetListItem> AvailableSkeletonFiles;

    ID3D11Device* Device = nullptr;

public:
    // IAssetLoader Implementation
    virtual void SetDevice(ID3D11Device* InDevice) override { Device = InDevice; }
    virtual USkeletalMesh* Load(const FString& Path) override { return LoadSkeletalMesh(Path); }
    virtual USkeletalMesh* Find(const FString& Key) override;
    virtual void Unload(const FString& Key) override;
    virtual void UnloadAll() override { ReleaseAllGPU(); }

    // 원본 경로(.fbx:Sub 등)를 대응되는 바이너리 캐시 경로로 변환한다.
    static FString GetBinaryFilePath(const FString& OriginalPath);

    USkeletalMesh* LoadSkeletalMesh(const FString& PathFileName, bool bRefreshAssetLists = true);
    USkeletalMesh* LoadSkeletalMesh(const FString& PathFileName, const FImportOptions& Options, bool bRefreshAssetLists = true);

    void ScanMeshCacheFiles();
    const TArray<FSkeletalMeshAssetListItem>& GetAvailableMeshFiles() const { return AvailableMeshFiles; }
    
    void ScanFBXSourceFiles();
    const TArray<FSkeletalMeshAssetListItem>& GetAvailableFBXFiles() const { return AvailableFBXFiles; }

    void ScanSkeletonCacheFiles();
    const TArray<FSkeletalMeshAssetListItem>& GetAvailableSkeletonFiles() const { return AvailableSkeletonFiles; }

    // 캐시된 SkeletalMesh GPU 리소스 해제
    void ReleaseAllGPU();

private:
    FSkeletalMeshManager() = default;
    bool TryImportSkeletalSubMesh(const FString& FBXSourcePath, const FString& SubResourceName, const FImportOptions* Options, USkeletalSubMesh* SubMesh, const FString& BinPath);
};
