#include "Mesh/ObjManager.h"
#include "Mesh/StaticMesh.h"
#include "Mesh/ObjImporter.h"
#include "Materials/Material.h"
#include "Core/Log.h"
#include "Serialization/WindowsArchive.h"
#include "Engine/Platform/Paths.h"
#include "Materials/MaterialManager.h"
#include <filesystem>
#include <algorithm>

TMap<FString, UStaticMesh*> FObjManager::StaticMeshCache;
TArray<FMeshAssetListItem> FObjManager::AvailableMeshFiles;
TArray<FMeshAssetListItem> FObjManager::AvailableObjFiles;

namespace
{
	std::filesystem::path GetMeshCacheDirectory(const FString& OriginalPath)
	{
		std::filesystem::path SourcePath(FPaths::ToWide(OriginalPath));
		return SourcePath.parent_path() / L"Cache";
	}

	void EnsureMeshCacheDirExists(const FString& OriginalPath)
	{
		FPaths::CreateDir(GetMeshCacheDirectory(OriginalPath).wstring());
	}
}

FString FObjManager::GetBinaryFilePath(const FString& OriginalPath)
{
	std::filesystem::path SrcPath(FPaths::ToWide(OriginalPath));
	std::wstring Ext = SrcPath.extension().wstring();
	std::transform(Ext.begin(), Ext.end(), Ext.begin(), ::towlower);

	// 이미 bin 경로가 들어온 경우에는 그대로 사용
	if (Ext == L".bin")
	{
		return OriginalPath;
	}

	std::filesystem::path BinPath = GetMeshCacheDirectory(OriginalPath) / SrcPath.stem();
	BinPath += L".bin";
	return FPaths::ToUtf8(BinPath.lexically_normal().generic_wstring());
}


void FObjManager::ScanMeshAssets()
{
	AvailableMeshFiles.clear();

	const std::filesystem::path ProjectRoot(FPaths::RootDir());
	const std::filesystem::path ContentRoot = FPaths::ContentDir();
	if (!std::filesystem::exists(ContentRoot))
	{
		return;
	}

	for (const auto& Entry : std::filesystem::recursive_directory_iterator(ContentRoot))
	{
		if (!Entry.is_regular_file()) continue;

		const std::filesystem::path& Path = Entry.path();
		if (Path.extension() != L".bin") continue;
		if (_wcsicmp(Path.parent_path().filename().c_str(), L"Cache") != 0) continue;

		FMeshAssetListItem Item;
		Item.DisplayName = FPaths::ToUtf8(Path.stem().wstring());
		Item.FullPath = FPaths::ToUtf8(Path.lexically_relative(ProjectRoot).generic_wstring());
		AvailableMeshFiles.push_back(std::move(Item));
	}
}

void FObjManager::ScanObjSourceFiles()
{
	AvailableObjFiles.clear();

	const std::filesystem::path DataRoot = FPaths::ContentDir();

	if (!std::filesystem::exists(DataRoot))
	{
		return;
	}

	const std::filesystem::path ProjectRoot(FPaths::RootDir());


	for (const auto& Entry : std::filesystem::recursive_directory_iterator(DataRoot))
	{
		if (!Entry.is_regular_file()) continue;

		const std::filesystem::path& Path = Entry.path();
		std::wstring Ext = Path.extension().wstring();

		// 대소문자 무시
		std::transform(Ext.begin(), Ext.end(), Ext.begin(), ::towlower);
		if (Ext != L".obj") continue;

		FMeshAssetListItem Item;
		Item.DisplayName = FPaths::ToUtf8(Path.filename().wstring());
		Item.FullPath = FPaths::ToUtf8(Path.lexically_relative(ProjectRoot).generic_wstring());
		AvailableObjFiles.push_back(std::move(Item));
	}
}

const TArray<FMeshAssetListItem>& FObjManager::GetAvailableMeshFiles()
{
	return AvailableMeshFiles;
}

const TArray<FMeshAssetListItem>& FObjManager::GetAvailableObjFiles()
{
	return AvailableObjFiles;
}

UStaticMesh* FObjManager::LoadObjStaticMesh(const FString& PathFileName, const FImportOptions& Options, ID3D11Device* InDevice)
{
	FString CacheKey = GetBinaryFilePath(PathFileName);

	// 옵션이 다를 수 있으므로 기존 캐시 무효화
	StaticMeshCache.erase(CacheKey);

	UStaticMesh* StaticMesh = UObjectManager::Get().CreateObject<UStaticMesh>();

	FString BinPath = CacheKey;

	// 항상 리빌드 (옵션이 달라질 수 있음)
	FStaticMesh* NewMeshAsset = new FStaticMesh();
	TArray<FStaticMaterial> ParsedMaterials;

	if (FObjImporter::Import(PathFileName, Options, *NewMeshAsset, ParsedMaterials))
	{
		NewMeshAsset->PathFileName = PathFileName;
		// MaterialIndex 캐싱을 위해 Materials를 먼저 설정

		StaticMesh->SetStaticMaterials(std::move(ParsedMaterials));
		StaticMesh->SetStaticMeshAsset(NewMeshAsset);

		// .bin 저장 (메시 지오메트리 + Material JSON 경로 참조)
		EnsureMeshCacheDirExists(PathFileName);
		FWindowsBinWriter Writer(BinPath);
		if (Writer.IsValid())
		{
			StaticMesh->Serialize(Writer);
		}
	}

	StaticMesh->InitResources(InDevice);
	StaticMeshCache[CacheKey] = StaticMesh;

	// 리프레시
	ScanMeshAssets();
	FMaterialManager::Get().ScanMaterialAssets();

	return StaticMesh;
}

UStaticMesh* FObjManager::LoadObjStaticMesh(const FString& PathFileName, ID3D11Device* InDevice)
{
	FString CacheKey = GetBinaryFilePath(PathFileName);

	// BinPath 기반 캐시 확인
	auto It = StaticMeshCache.find(CacheKey);
	if (It != StaticMeshCache.end())
	{
		return It->second;
	}

	// UStaticMesh 생성 + FStaticMesh 소유권 이전 + 머티리얼 설정
	UStaticMesh* StaticMesh = UObjectManager::Get().CreateObject<UStaticMesh>();

	FString BinPath = CacheKey;
	bool bNeedRebuild = true;
	FString ObjPath = PathFileName;
	const std::filesystem::path RequestedPathW(FPaths::ToWide(PathFileName));
	std::wstring RequestedExt = RequestedPathW.extension().wstring();
	std::transform(RequestedExt.begin(), RequestedExt.end(), RequestedExt.begin(), ::towlower);

	// .bin 직접 선택된 경우에는 먼저 로드해서 원본 OBJ 경로를 확인한다.
	if (RequestedExt == L".bin")
	{
		FWindowsBinReader Reader(BinPath);
		if (Reader.IsValid())
		{
			StaticMesh->Serialize(Reader);
			bNeedRebuild = false;

			if (StaticMesh->GetStaticMeshAsset() && !StaticMesh->GetStaticMeshAsset()->PathFileName.empty())
			{
				ObjPath = StaticMesh->GetStaticMeshAsset()->PathFileName;
				const std::filesystem::path ObjPathW(FPaths::ToWide(ObjPath));
				const std::filesystem::path BinPathW(FPaths::ToWide(BinPath));
				if (std::filesystem::exists(ObjPathW) &&
					std::filesystem::last_write_time(ObjPathW) > std::filesystem::last_write_time(BinPathW))
				{
					bNeedRebuild = true;
				}
			}
		}
	}

	// OBJ 경로로 들어온 경우 타임스탬프 비교 (디스크 캐시 확인)
	std::filesystem::path BinPathW(FPaths::ToWide(BinPath));
	std::filesystem::path PathFileNameW(FPaths::ToWide(PathFileName));
	if (RequestedExt != L".bin" && std::filesystem::exists(BinPathW))
	{
		if (!std::filesystem::exists(PathFileNameW) ||
			std::filesystem::last_write_time(BinPathW) >= std::filesystem::last_write_time(PathFileNameW))
		{
			bNeedRebuild = false;
		}
	}

	if (!bNeedRebuild && RequestedExt != L".bin")
	{
		// BIN 파일에서 통째로 로드 (Material은 JSON 경로로 FMaterialManager를 통해 복원)
		FWindowsBinReader Reader(BinPath);
		if (Reader.IsValid())
		{
			StaticMesh->Serialize(Reader);
		}
		else
		{
			bNeedRebuild = true; // 읽기 실패 시 강제 파싱
		}
	}

	if (bNeedRebuild)
	{
		// 무거운 OBJ 파싱 진행
		FStaticMesh* NewMeshAsset = new FStaticMesh();
		TArray<FStaticMaterial> ParsedMaterials;

		if (FObjImporter::Import(ObjPath, *NewMeshAsset, ParsedMaterials))
		{
			// MaterialIndex 캐싱을 위해 Materials를 먼저 설정
			StaticMesh->SetStaticMaterials(std::move(ParsedMaterials));
			StaticMesh->SetStaticMeshAsset(NewMeshAsset);

			// 파싱 결과를 하드디스크에 굽기 (다음 로딩 속도 최적화)
			EnsureMeshCacheDirExists(ObjPath);
			FWindowsBinWriter Writer(BinPath);
			if (Writer.IsValid())
			{
				StaticMesh->Serialize(Writer);
			}
		}
	}

	StaticMesh->InitResources(InDevice);

	// 캐시 등록
	StaticMeshCache[CacheKey] = StaticMesh;

	ScanMeshAssets();
	FMaterialManager::Get().ScanMaterialAssets();

	return StaticMesh;
}


void FObjManager::ReleaseAllGPU()
{
	for (auto& [Key, Mesh] : StaticMeshCache)
	{
		if (Mesh)
		{
			FStaticMesh* Asset = Mesh->GetStaticMeshAsset();
			if (Asset && Asset->RenderBuffer)
			{
				Asset->RenderBuffer->Release();
				Asset->RenderBuffer.reset();
			}
			// LOD 버퍼도 해제
			for (uint32 LOD = 1; LOD < UStaticMesh::MAX_LOD_COUNT; ++LOD)
			{
				FMeshBuffer* LODBuffer = Mesh->GetLODMeshBuffer(LOD);
				if (LODBuffer)
				{
					LODBuffer->Release();
				}
			}
		}
	}
	StaticMeshCache.clear();
}
