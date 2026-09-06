// 메시 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "Core/CoreTypes.h"
#include "Object/ObjectIterator.h"
#include "Render/RHI/D3D11/Common/D3D11API.h"
#include "Core/Singleton.h"
#include "Resource/IAssetLoader.h"
#include <map>
#include <string>
#include <memory>

struct FStaticMesh;
struct FStaticMaterial;
struct FImportOptions;
class UStaticMesh;

// 에디터/UI에서 보여줄 수 있는 .obj 리스트 항목
struct FMeshAssetListItem
{
    FString DisplayName;
    FString FullPath;
};

// Wavefront .obj 관련 객체의 생성, 조회, 수명 관리를 담당
class FObjManager : public TSingleton<FObjManager>, public IAssetLoader<UStaticMesh>
{
    friend class TSingleton<FObjManager>;

    // path → UStaticMesh* 캐시 (소유권은 UObjectManager)
    TMap<FString, UStaticMesh*> StaticMeshCache;
    TArray<FMeshAssetListItem> AvailableMeshFiles;
    TArray<FMeshAssetListItem> AvailableObjFiles;

    ID3D11Device* Device = nullptr;

public:
    // IAssetLoader Implementation
    virtual void SetDevice(ID3D11Device* InDevice) override { Device = InDevice; }
    virtual UStaticMesh* Load(const FString& Path) override { return LoadObjStaticMesh(Path); }
    virtual UStaticMesh* Find(const FString& Key) override;
    virtual void Unload(const FString& Key) override;
    virtual void UnloadAll() override { ReleaseAllGPU(); }

    // 원본 경로(.obj 등)를 대응되는 바이너리 캐시 경로로 변환한다.
    // 이미 .bin 이면 그대로 반환한다.
    static FString GetBinaryFilePath(const FString& OriginalPath);

    // 이전 패치/로컬 코드와의 호환용 별칭
    static FString GetBinaryFilePathString(const FString& OriginalPath) { return GetBinaryFilePath(OriginalPath); }

    UStaticMesh* LoadObjStaticMesh(const FString& PathFileName, bool bRefreshAssetLists = true);
    UStaticMesh* LoadObjStaticMesh(const FString& PathFileName, const FImportOptions& Options, bool bRefreshAssetLists = true);

    void ScanMeshCacheFiles();
    const TArray<FMeshAssetListItem>& GetAvailableMeshFiles() const { return AvailableMeshFiles; }
    
    void ScanObjSourceFiles();
    const TArray<FMeshAssetListItem>& GetAvailableObjFiles() const { return AvailableObjFiles; }

    // 캐시된 StaticMesh GPU 리소스 해제 (Shutdown 시 Device 해제 전 호출)
    void ReleaseAllGPU();

private:
    FObjManager() = default;
    bool TryImportStaticMesh(const FString& ObjPath, const FImportOptions* Options, UStaticMesh* StaticMesh, const FString& BinPath);
};
