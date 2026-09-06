#include "Mesh/SkeletalMeshManager.h"
#include "Core/Logging/LogMacros.h"
#include "Mesh/SkeletalMesh.h"
#include "Mesh/FBXImporter.h"
#include "Materials/Material.h"
#include "Serialization/WindowsArchive.h"
#include "Engine/Platform/Paths.h"
#include "Materials/MaterialManager.h"
#include <filesystem>
#include <algorithm>
#include <cwctype>

namespace
{
    std::filesystem::path NormalizePathForCompare(const FString& InPath)
    {
        std::filesystem::path Path = FPaths::ToPath(InPath).lexically_normal();
        std::error_code Ec;
        std::filesystem::path Canonical = std::filesystem::weakly_canonical(Path, Ec);
        return Ec ? Path : Canonical;
    }

    bool IsBinExtension(const std::filesystem::path& Path)
    {
        std::wstring Ext = Path.extension().wstring();
        std::transform(Ext.begin(), Ext.end(), Ext.begin(), ::towlower);
        return Ext == L".bin";
    }

    bool IsFBXExtension(const std::filesystem::path& Path)
    {
        std::wstring Ext = Path.extension().wstring();
        std::transform(Ext.begin(), Ext.end(), Ext.begin(), ::towlower);
        return Ext == L".fbx";
    }

    bool IsInsideCacheFolder(const std::filesystem::path& Path)
    {
        for (const auto& Part : Path)
        {
            std::wstring Name = Part.wstring();
            std::transform(Name.begin(), Name.end(), Name.begin(), ::towlower);
            if (Name == L"cache") return true;
        }
        return false;
    }
}

FString FSkeletalMeshManager::GetBinaryFilePath(const FString& OriginalPath)
{
    const std::filesystem::path SrcPath = NormalizePathForCompare(OriginalPath);
    if (IsBinExtension(SrcPath)) return FPaths::FromPath(SrcPath);

    FString SourcePath, SubResource;
    FPaths::ParseSubResourcePath(OriginalPath, SourcePath, SubResource);
    
    if (!SubResource.empty())
    {
        // Skeletal Mesh 요청인 경우
        if (SubResource.find(SkeletalMeshPrefix::Mesh) == 0)
        {
             return FPaths::BuildSubResourceCachePath(SourcePath, SubResource);
        }
    }
    return "";
}

void FSkeletalMeshManager::ScanMeshCacheFiles()
{
    AvailableMeshFiles.clear();
    const std::filesystem::path AssetRoot = std::filesystem::path(FPaths::RootDir()) / L"Asset";
    if (!std::filesystem::exists(AssetRoot)) return;
    const std::filesystem::path ProjectRoot(FPaths::RootDir());

    const std::wstring MeshPrefixW = L".fbx_" + FPaths::ToWide(SkeletalMeshPrefix::Mesh);

    for (const auto& Entry : std::filesystem::recursive_directory_iterator(AssetRoot))
    {
        if (!Entry.is_regular_file()) continue;
        const std::filesystem::path& Path = Entry.path();
        if (!IsBinExtension(Path) || !IsInsideCacheFolder(Path)) continue;
        
        // 특정 접두사로 끝나는 파일이 USkeletalMesh 자산임
        if (Path.filename().wstring().find(MeshPrefixW) == std::wstring::npos) continue;

        FSkeletalMeshAssetListItem Item;
        FString Stem = FPaths::ToUtf8(Path.stem().wstring());
        Item.DisplayName = Stem;
        std::filesystem::path LogicalPath = Path.parent_path().parent_path() / FPaths::ToPath(Stem);
        Item.FullPath = FPaths::ToUtf8(LogicalPath.lexically_relative(ProjectRoot).generic_wstring());
        AvailableMeshFiles.push_back(std::move(Item));
    }
}

void FSkeletalMeshManager::ScanSkeletonCacheFiles()
{
    AvailableSkeletonFiles.clear();
    const std::filesystem::path AssetRoot = std::filesystem::path(FPaths::RootDir()) / L"Asset";
    if (!std::filesystem::exists(AssetRoot)) return;
    const std::filesystem::path ProjectRoot(FPaths::RootDir());

    const std::wstring SkelPrefixW = L".fbx_" + FPaths::ToWide(SkeletalMeshPrefix::Skeleton);

    for (const auto& Entry : std::filesystem::recursive_directory_iterator(AssetRoot))
    {
        if (!Entry.is_regular_file()) continue;
        const std::filesystem::path& Path = Entry.path();
        if (!IsBinExtension(Path) || !IsInsideCacheFolder(Path)) continue;

        // 원본 스켈레톤 데이터 파일 필터링
        if (Path.filename().wstring().find(SkelPrefixW) == std::wstring::npos) continue;

        FSkeletalMeshAssetListItem Item;
        FString Stem = FPaths::ToUtf8(Path.stem().wstring());
        Item.DisplayName = Stem;
        std::filesystem::path LogicalPath = Path.parent_path().parent_path() / FPaths::ToPath(Stem);
        Item.FullPath = FPaths::ToUtf8(LogicalPath.lexically_relative(ProjectRoot).generic_wstring());
        AvailableSkeletonFiles.push_back(std::move(Item));
    }
}

void FSkeletalMeshManager::ScanFBXSourceFiles()
{
    AvailableFBXFiles.clear();
    const std::filesystem::path AssetRoot = std::filesystem::path(FPaths::RootDir()) / L"Asset";
    if (!std::filesystem::exists(AssetRoot)) return;
    const std::filesystem::path ProjectRoot(FPaths::RootDir());

    for (const auto& Entry : std::filesystem::recursive_directory_iterator(AssetRoot))
    {
        if (!Entry.is_regular_file()) continue;
        const std::filesystem::path& Path = Entry.path();
        if (!IsFBXExtension(Path) || IsInsideCacheFolder(Path)) continue;

        FSkeletalMeshAssetListItem Item;
        Item.DisplayName = FPaths::ToUtf8(Path.filename().wstring());
        Item.FullPath = FPaths::ToUtf8(Path.lexically_relative(ProjectRoot).generic_wstring());
        AvailableFBXFiles.push_back(std::move(Item));
    }
}

USkeletalMesh* FSkeletalMeshManager::LoadSkeletalMesh(const FString& PathFileName, bool bRefreshAssetLists)
{
    if (bRefreshAssetLists)
    {
        ScanMeshCacheFiles();
        ScanSkeletonCacheFiles();
        ScanFBXSourceFiles();
    }

    auto It = SkeletalMeshCache.find(PathFileName);
    if (It != SkeletalMeshCache.end()) return It->second;

    FString SourcePath, SubResource;
    FPaths::ParseSubResourcePath(PathFileName, SourcePath, SubResource);

    // 1. 캐시가 없는 경우 원본 FBX로부터 자동 임포트 시도
    bool bHasCacheForThisSource = false;
    const std::filesystem::path RequestedSrcPath = NormalizePathForCompare(SourcePath);
    for (const auto& Item : AvailableMeshFiles)
    {
        FString ItemSource, ItemSub;
        FPaths::ParseSubResourcePath(Item.FullPath, ItemSource, ItemSub);
        if (NormalizePathForCompare(ItemSource) == RequestedSrcPath)
        {
            bHasCacheForThisSource = true;
            break;
        }
    }

    if (!bHasCacheForThisSource && std::filesystem::exists(FPaths::ToPath(SourcePath)))
    {
        if (FFBXImporter::ImportAndCacheAll(SourcePath, FImportOptions::Default()))
        {
            ScanMeshCacheFiles();
            ScanSkeletonCacheFiles();
        }
    }

    FString CacheKey = GetBinaryFilePath(PathFileName);
    
    // 2. 원본 FBX 경로로 요청된 경우, 첫 번째 사용 가능한 번들 서브리소스를 찾아 반환
    if (CacheKey.empty())
    {
        const std::filesystem::path RequestedSrcPath = NormalizePathForCompare(SourcePath);
        for (const auto& Item : AvailableMeshFiles)
        {
            FString ItemSource, ItemSub;
            FPaths::ParseSubResourcePath(Item.FullPath, ItemSource, ItemSub);
            if (NormalizePathForCompare(ItemSource) == RequestedSrcPath)
            {
                return LoadSkeletalMesh(Item.FullPath, false);
            }
        }
        return nullptr;
    }

    if (!std::filesystem::exists(FPaths::ToPath(CacheKey))) return nullptr;

    // 3. 번들 파일(.bin) 로드 및 직렬화
    USkeletalMesh* Container = UObjectManager::Get().CreateObject<USkeletalMesh>();
    FWindowsBinReader Reader(CacheKey);
    if (Reader.IsValid())
    {
        Container->SetAssetPathFileName(PathFileName);
        Container->Serialize(Reader);

        // 하위 메시들의 GPU 리소스 초기화
        for (auto* Sub : Container->GetSubMeshes())
        {
            Sub->InitResources(Device);
        }
    }
    else
    {
        return nullptr;
    }

    SkeletalMeshCache[PathFileName] = Container;
    return Container;
}

USkeletalMesh* FSkeletalMeshManager::LoadSkeletalMesh(const FString& PathFileName, const FImportOptions& Options, bool bRefreshAssetLists)
{
    // Options 지원은 추후 필요시 보강
    return LoadSkeletalMesh(PathFileName, bRefreshAssetLists);
}

USkeletalMesh* FSkeletalMeshManager::Find(const FString& Key)
{
    auto It = SkeletalMeshCache.find(Key);
    return (It != SkeletalMeshCache.end()) ? It->second : nullptr;
}

void FSkeletalMeshManager::Unload(const FString& Key)
{
    SkeletalMeshCache.erase(Key);
}

void FSkeletalMeshManager::ReleaseAllGPU()
{
	for (auto& [Key, Mesh] : SkeletalMeshCache)
    {
		if (!Mesh) continue;
		for (USkeletalSubMesh* SubMesh : Mesh->GetSubMeshes())
        {
			if (!SubMesh) continue;
			FSkeletalSubMesh* Asset = SubMesh->GetSkeletalSubMeshAsset();
			if (Asset && Asset->RenderBuffer)
            {
				Asset->RenderBuffer->Release();
				Asset->RenderBuffer.reset();
			}
		}
	}
    SkeletalMeshCache.clear();
}
