#pragma once

#include "Object/Object.h"
#include "Core/CoreTypes.h"
#include "Engine/Platform/DirectoryWatcher.h"

#include <filesystem>
#include <map>
#include <string>
#include <vector>

struct ID3D11Device;
struct ID3D11ShaderResourceView;

struct FTextureAssetListItem
{
	FString DisplayName;
	FString FullPath;
	FString SourceFolder;
};

// UTexture2D — 텍스처 에셋 (SRV를 소유하는 UObject)
// 같은 경로의 텍스처는 캐시를 통해 하나의 UTexture2D를 공유합니다.
class UTexture2D : public UObject
{
public:
	DECLARE_CLASS(UTexture2D, UObject)

	UTexture2D() = default;
	~UTexture2D() override;

	// 경로로 텍스처를 로드 (캐시 히트 시 기존 객체 반환)
	static UTexture2D* LoadFromFile(const FString& FilePath, ID3D11Device* Device);
	static UTexture2D* LoadFromCached(const FString& FilePath);

	// 캐시된 모든 텍스처의 GPU 리소스 해제 (Shutdown 시 Device 해제 전 호출)
	static void ReleaseAllGPU();
	static bool HasPendingTextureRefresh();
	static void RefreshChangedTextures(ID3D11Device* Device);
	static void ScanTextureAssets();
	static const TArray<FTextureAssetListItem>& GetAvailableTextureFiles();
	static bool IsSupportedTextureExtension(const std::filesystem::path& Path);

	ID3D11ShaderResourceView* GetSRV() const { return SRV; }
	const FString& GetSourcePath() const { return SourceFilePath; }
	uint32 GetWidth() const { return Width; }
	uint32 GetHeight() const { return Height; }
	bool IsLoaded() const { return SRV != nullptr; }
	bool SampleAlpha(float U, float V, float& OutAlpha) const;

private:
	bool LoadInternal(const FString& FilePath, ID3D11Device* Device);
	bool HasSourceFileChanged() const;

	FString SourceFilePath;
	ID3D11ShaderResourceView* SRV = nullptr;
	uint32 Width = 0;
	uint32 Height = 0;
	uint64 TrackedTextureMemory = 0;
	std::vector<uint8> CPUTextureRGBA;
	std::filesystem::file_time_type SourceFileWriteTime{};
	bool bHasSourceFileWriteTime = false;

	// path → UTexture2D* 캐시 (소유권은 UObjectManager)
	static std::map<FString, UTexture2D*> TextureCache;
	static TArray<FTextureAssetListItem> AvailableTextureFiles;
	static bool bTextureAssetListDirty;
	static TSet<FString> PendingTextureRefreshPaths;
	static FWatchID TextureAssetWatchID;
	static FSubscriptionID TextureAssetWatchSub;
	static bool bTextureAssetWatcherInitialized;

	static void EnsureTextureAssetWatcher();
	static void MarkTextureAssetListDirty();
	static void QueueTextureRefresh(const FString& TexturePath);
};
