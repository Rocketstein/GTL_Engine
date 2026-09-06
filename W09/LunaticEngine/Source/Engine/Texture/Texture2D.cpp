#include "Texture/Texture2D.h"
#include "Object/ObjectFactory.h"
#include "Core/AsciiUtils.h"
#include "Core/Log.h"
#include "Engine/Platform/DirectoryWatcher.h"
#include "Platform/Paths.h"
#include "DDSTextureLoader.h"
#include "WICTextureLoader.h"

#include <algorithm>
#include <cwctype>
#include <d3d11.h>
#include <filesystem>
#include <wincodec.h>

IMPLEMENT_CLASS(UTexture2D, UObject)

std::map<FString, UTexture2D*> UTexture2D::TextureCache;
TArray<FTextureAssetListItem> UTexture2D::AvailableTextureFiles;
bool UTexture2D::bTextureAssetListDirty = true;
TSet<FString> UTexture2D::PendingTextureRefreshPaths;
FWatchID UTexture2D::TextureAssetWatchID = 0;
FSubscriptionID UTexture2D::TextureAssetWatchSub = 0;
bool UTexture2D::bTextureAssetWatcherInitialized = false;

namespace
{
	FString NormalizeTexturePath(const FString& FilePath)
	{
		if (FilePath.empty())
		{
			return FilePath;
		}

		std::filesystem::path Path(FPaths::ToWide(FilePath));
		const std::filesystem::path Root(FPaths::RootDir());

		if (Path.is_absolute())
		{
			auto Relative = Path.lexically_relative(Root);
			if (!Relative.empty() && Relative.native() != L".")
			{
				return FPaths::ToUtf8(Relative.generic_wstring());
			}
		}

		return FPaths::ToUtf8(Path.lexically_normal().generic_wstring());
	}

	std::wstring ResolveTexturePathOnDisk(const FString& FilePath)
	{
		if (FilePath.empty()) return {};

		std::filesystem::path Path(FPaths::ToWide(FilePath));
		if (Path.is_absolute())
		{
			return Path.lexically_normal().wstring();
		}

		// 1. Root relative (Asset/Content/...)
		std::filesystem::path FullPath = std::filesystem::path(FPaths::RootDir()) / Path;
		if (std::filesystem::exists(FullPath))
		{
			return FullPath.lexically_normal().wstring();
		}

		// 2. Content relative fallback
		std::filesystem::path ContentPath = std::filesystem::path(FPaths::ProjectContentDir()) / Path;
		if (std::filesystem::exists(ContentPath))
		{
			return ContentPath.lexically_normal().wstring();
		}

		// 3. Asset relative fallback
		std::filesystem::path AssetPath = std::filesystem::path(FPaths::AssetDir()) / Path;
		if (std::filesystem::exists(AssetPath))
		{
			return AssetPath.lexically_normal().wstring();
		}

		return FullPath.lexically_normal().wstring();
	}

	bool TryGetTextureWriteTime(const FString& FilePath, std::filesystem::file_time_type& OutWriteTime)
	{
		std::error_code ErrorCode;
		const std::filesystem::path DiskPath(ResolveTexturePathOnDisk(FilePath));
		const std::filesystem::file_time_type WriteTime = std::filesystem::last_write_time(DiskPath, ErrorCode);
		if (ErrorCode)
		{
			return false;
		}

		OutWriteTime = WriteTime;
		return true;
	}

	bool ShouldSkipTextureScanDirectory(const std::filesystem::path& Path)
	{
		const std::wstring Name = Path.filename().wstring();
		return Name == L".git"
			|| Name == L".vs"
			|| Name == L"Bin"
			|| Name == L"Build"
			|| Name == L"Intermediate";
	}

	bool ShouldIncludeInTextureAssetList(const std::filesystem::path& ProjectRelativePath)
	{
		const std::filesystem::path NormalizedPath = ProjectRelativePath.lexically_normal();
		if (NormalizedPath.empty())
		{
			return false;
		}

		auto It = NormalizedPath.begin();
		if (It == NormalizedPath.end() || *It != L"Asset")
		{
			return true;
		}

		++It;
		if (It == NormalizedPath.end())
		{
			return true;
		}

		if (*It == L"Editor")
		{
			return false;
		}

		if (*It != L"Content")
		{
			return true;
		}

		++It;
		return It == NormalizedPath.end() || *It != L"Font";
	}

	bool IsSupportedTexturePathString(const FString& Path)
	{
		return UTexture2D::IsSupportedTextureExtension(std::filesystem::path(FPaths::ToWide(Path)));
	}

	bool LoadCPUTextureRGBA(const FString& FilePath, uint32& OutWidth, uint32& OutHeight, std::vector<uint8>& OutPixels)
	{
		OutWidth = 0;
		OutHeight = 0;
		OutPixels.clear();

		IWICImagingFactory* Factory = nullptr;
		HRESULT HR = CoCreateInstance(
			CLSID_WICImagingFactory,
			nullptr,
			CLSCTX_INPROC_SERVER,
			IID_PPV_ARGS(&Factory));
		if (FAILED(HR) || !Factory)
		{
			return false;
		}

		IWICBitmapDecoder* Decoder = nullptr;
		HR = Factory->CreateDecoderFromFilename(
			FPaths::ToWide(FilePath).c_str(),
			nullptr,
			GENERIC_READ,
			WICDecodeMetadataCacheOnDemand,
			&Decoder);
		if (FAILED(HR) || !Decoder)
		{
			Factory->Release();
			return false;
		}

		IWICBitmapFrameDecode* Frame = nullptr;
		HR = Decoder->GetFrame(0, &Frame);
		if (FAILED(HR) || !Frame)
		{
			Decoder->Release();
			Factory->Release();
			return false;
		}

		HR = Frame->GetSize(&OutWidth, &OutHeight);
		if (FAILED(HR) || OutWidth == 0 || OutHeight == 0)
		{
			Frame->Release();
			Decoder->Release();
			Factory->Release();
			return false;
		}

		IWICFormatConverter* Converter = nullptr;
		HR = Factory->CreateFormatConverter(&Converter);
		if (FAILED(HR) || !Converter)
		{
			Frame->Release();
			Decoder->Release();
			Factory->Release();
			return false;
		}

		HR = Converter->Initialize(
			Frame,
			GUID_WICPixelFormat32bppRGBA,
			WICBitmapDitherTypeNone,
			nullptr,
			0.0f,
			WICBitmapPaletteTypeCustom);
		if (FAILED(HR))
		{
			Converter->Release();
			Frame->Release();
			Decoder->Release();
			Factory->Release();
			return false;
		}

		const size_t PixelCount = static_cast<size_t>(OutWidth) * static_cast<size_t>(OutHeight);
		OutPixels.resize(PixelCount * 4ull);
		HR = Converter->CopyPixels(
			nullptr,
			OutWidth * 4u,
			static_cast<UINT>(OutPixels.size()),
			OutPixels.data());

		Converter->Release();
		Frame->Release();
		Decoder->Release();
		Factory->Release();
		return SUCCEEDED(HR);
	}
}

UTexture2D::~UTexture2D()
{
	if (SRV)
	{
		if (TrackedTextureMemory > 0)
		{
			MemoryStats::SubTextureMemory(TrackedTextureMemory);
			TrackedTextureMemory = 0;
		}

		SRV->Release();
		SRV = nullptr;
	}

	// 캐시에서 제거
	auto It = TextureCache.find(SourceFilePath);
	if (It != TextureCache.end() && It->second == this)
	{
		TextureCache.erase(It);
	}
}

void UTexture2D::ReleaseAllGPU()
{
	for (auto& [Path, Texture] : TextureCache)
	{
		if (Texture && Texture->SRV)
		{
			if (Texture->TrackedTextureMemory > 0)
			{
				MemoryStats::SubTextureMemory(Texture->TrackedTextureMemory);
				Texture->TrackedTextureMemory = 0;
			}
			Texture->SRV->Release();
			Texture->SRV = nullptr;
		}
	}
	TextureCache.clear();
}

bool UTexture2D::HasPendingTextureRefresh()
{
	EnsureTextureAssetWatcher();
	return !PendingTextureRefreshPaths.empty();
}

void UTexture2D::RefreshChangedTextures(ID3D11Device* Device)
{
	EnsureTextureAssetWatcher();

	if (!Device)
	{
		return;
	}

	if (PendingTextureRefreshPaths.empty())
	{
		return;
	}

	TSet<FString> ChangedPaths;
	std::swap(ChangedPaths, PendingTextureRefreshPaths);

	for (const FString& ChangedPath : ChangedPaths)
	{
		auto It = TextureCache.find(NormalizeTexturePath(ChangedPath));
		if (It != TextureCache.end() && It->second && It->second->HasSourceFileChanged())
		{
			It->second->LoadInternal(It->first, Device);
		}
	}
}

void UTexture2D::ScanTextureAssets()
{
	EnsureTextureAssetWatcher();
	AvailableTextureFiles.clear();

	const std::filesystem::path ProjectRoot(FPaths::RootDir());
	const std::filesystem::path AssetRoot(FPaths::AssetDir());
	if (!std::filesystem::exists(AssetRoot))
	{
		return;
	}

	std::error_code ErrorCode;
	std::filesystem::recursive_directory_iterator It(
		AssetRoot,
		std::filesystem::directory_options::skip_permission_denied,
		ErrorCode);
	std::filesystem::recursive_directory_iterator End;

	while (It != End)
	{
		if (ErrorCode)
		{
			ErrorCode.clear();
			It.increment(ErrorCode);
			continue;
		}

		const std::filesystem::directory_entry& Entry = *It;
		if (Entry.is_directory(ErrorCode) && ShouldSkipTextureScanDirectory(Entry.path()))
		{
			It.disable_recursion_pending();
		}
		else if (Entry.is_regular_file(ErrorCode) && IsSupportedTextureExtension(Entry.path()))
		{
			const std::filesystem::path RelativePath = Entry.path().lexically_relative(ProjectRoot).lexically_normal();
			if (!ShouldIncludeInTextureAssetList(RelativePath))
			{
				It.increment(ErrorCode);
				continue;
			}

			FTextureAssetListItem Item;
			Item.DisplayName = FPaths::ToUtf8(Entry.path().filename().wstring());
			Item.FullPath = FPaths::ToUtf8(RelativePath.generic_wstring());
			Item.SourceFolder = FPaths::ToUtf8(RelativePath.parent_path().generic_wstring());
			AvailableTextureFiles.push_back(std::move(Item));
		}

		It.increment(ErrorCode);
	}

	std::sort(
		AvailableTextureFiles.begin(),
		AvailableTextureFiles.end(),
		[](const FTextureAssetListItem& A, const FTextureAssetListItem& B)
		{
			if (A.SourceFolder != B.SourceFolder)
			{
				return A.SourceFolder < B.SourceFolder;
			}

			if (A.DisplayName != B.DisplayName)
			{
				return A.DisplayName < B.DisplayName;
			}

			return A.FullPath < B.FullPath;
		});

	bTextureAssetListDirty = false;
}

const TArray<FTextureAssetListItem>& UTexture2D::GetAvailableTextureFiles()
{
	EnsureTextureAssetWatcher();
	if (bTextureAssetListDirty)
	{
		ScanTextureAssets();
	}
	return AvailableTextureFiles;
}

bool UTexture2D::IsSupportedTextureExtension(const std::filesystem::path& Path)
{
	std::wstring Ext = Path.extension().wstring();
	std::transform(Ext.begin(), Ext.end(), Ext.begin(), towlower);

	return Ext == L".png"
		|| Ext == L".jpg"
		|| Ext == L".jpeg"
		|| Ext == L".bmp"
		|| Ext == L".tga"
		|| Ext == L".dds";
}

UTexture2D* UTexture2D::LoadFromFile(const FString& FilePath, ID3D11Device* Device)
{
	if (FilePath.empty() || !Device) return nullptr;

	const FString NormalizedPath = NormalizeTexturePath(FilePath);

	// 캐시 히트
	auto It = TextureCache.find(NormalizedPath);
	if (It != TextureCache.end())
	{
		if (It->second && It->second->HasSourceFileChanged())
		{
			It->second->LoadInternal(NormalizedPath, Device);
		}
		return It->second;
	}

	// 새 UTexture2D 생성
	UTexture2D* Texture = UObjectManager::Get().CreateObject<UTexture2D>();
	if (!Texture->LoadInternal(FilePath, Device))
	{
		UObjectManager::Get().DestroyObject(Texture);
		return nullptr;
	}

	TextureCache[NormalizedPath] = Texture;
	return Texture;
}

UTexture2D* UTexture2D::LoadFromCached(const FString& FilePath)
{
	if (FilePath.empty()) return nullptr;

	auto It = TextureCache.find(NormalizeTexturePath(FilePath));
	if (It != TextureCache.end())
	{
		return It->second;
	}

	return nullptr;
}

bool UTexture2D::LoadInternal(const FString& FilePath, ID3D11Device* Device)
{
	const FString NormalizedPath = NormalizeTexturePath(FilePath);
	const std::wstring WidePath = ResolveTexturePathOnDisk(NormalizedPath);
	const std::filesystem::path ExtensionPath = std::filesystem::path(WidePath).extension();
	FString Extension = FPaths::ToUtf8(ExtensionPath.generic_wstring());
	AsciiUtils::ToLowerInPlace(Extension);

	ID3D11Resource* Resource = nullptr;
	ID3D11ShaderResourceView* NewSRV = nullptr;
	HRESULT hr = S_OK;
	if (Extension == ".dds")
	{
		hr = DirectX::CreateDDSTextureFromFileEx(
			Device, WidePath.c_str(),
			0,                           // maxsize
			D3D11_USAGE_DEFAULT,         // usage
			D3D11_BIND_SHADER_RESOURCE,  // bindFlags
			0,                           // cpuAccessFlags
			0,                           // miscFlags
			DirectX::DDS_LOADER_DEFAULT,
			&Resource, &NewSRV);
	}
	else
	{
		hr = DirectX::CreateWICTextureFromFileEx(
			Device, WidePath.c_str(),
			0,                                    // maxsize
			D3D11_USAGE_DEFAULT,                  // usage
			D3D11_BIND_SHADER_RESOURCE,           // bindFlags
			0,                                    // cpuAccessFlags
			0,                                    // miscFlags
			DirectX::WIC_LOADER_IGNORE_SRGB,      // sRGB 메타데이터 무시 → UNORM 포맷 강제
			&Resource, &NewSRV);
	}

	if (FAILED(hr) || !NewSRV)
	{
		UE_LOG_CATEGORY(Texture, Error, "Failed to load texture: %s", FilePath.c_str());
		if (Resource)
		{
			Resource->Release();
		}
		if (NewSRV)
		{
			NewSRV->Release();
		}
		return false;
	}

	uint32 NewWidth = 0;
	uint32 NewHeight = 0;
	uint64 NewTrackedTextureMemory = 0;

	// 텍스처 크기 추출
	if (Resource)
	{
		NewTrackedTextureMemory = MemoryStats::CalculateTextureMemory(Resource);

		ID3D11Texture2D* Tex2D = nullptr;
		if (SUCCEEDED(Resource->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&Tex2D)))
		{
			D3D11_TEXTURE2D_DESC Desc;
			Tex2D->GetDesc(&Desc);
			NewWidth = Desc.Width;
			NewHeight = Desc.Height;
			Tex2D->Release();
		}
		Resource->Release();
	}

	std::vector<uint8> NewCPUTextureRGBA;
	LoadCPUTextureRGBA(NormalizedPath, NewWidth, NewHeight, NewCPUTextureRGBA);

	std::filesystem::file_time_type NewWriteTime{};
	const bool bHasNewWriteTime = TryGetTextureWriteTime(NormalizedPath, NewWriteTime);

	if (SRV)
	{
		if (TrackedTextureMemory > 0)
		{
			MemoryStats::SubTextureMemory(TrackedTextureMemory);
			TrackedTextureMemory = 0;
		}
		SRV->Release();
		SRV = nullptr;
	}

	SRV = NewSRV;
	Width = NewWidth;
	Height = NewHeight;
	TrackedTextureMemory = NewTrackedTextureMemory;
	CPUTextureRGBA = std::move(NewCPUTextureRGBA);
	SourceFilePath = NormalizedPath;
	SourceFileWriteTime = NewWriteTime;
	bHasSourceFileWriteTime = bHasNewWriteTime;

	if (TrackedTextureMemory > 0)
	{
		MemoryStats::AddTextureMemory(TrackedTextureMemory);
	}

	return true;
}

bool UTexture2D::HasSourceFileChanged() const
{
	if (SourceFilePath.empty())
	{
		return false;
	}

	std::filesystem::file_time_type CurrentWriteTime{};
	if (!TryGetTextureWriteTime(SourceFilePath, CurrentWriteTime))
	{
		return false;
	}

	if (!bHasSourceFileWriteTime)
	{
		return true;
	}

	return CurrentWriteTime != SourceFileWriteTime;
}

void UTexture2D::EnsureTextureAssetWatcher()
{
	if (bTextureAssetWatcherInitialized)
	{
		return;
	}

	bTextureAssetWatcherInitialized = true;
	TextureAssetWatchID = FDirectoryWatcher::Get().Watch(FPaths::AssetDir(), "Asset/");
	if (TextureAssetWatchID == 0)
	{
		return;
	}

	TextureAssetWatchSub = FDirectoryWatcher::Get().Subscribe(
		TextureAssetWatchID,
		[](const TSet<FString>& ChangedPaths)
		{
			for (const FString& Path : ChangedPaths)
			{
				if (IsSupportedTexturePathString(Path))
				{
					MarkTextureAssetListDirty();
					QueueTextureRefresh(Path);
				}
			}
		});
}

void UTexture2D::MarkTextureAssetListDirty()
{
	bTextureAssetListDirty = true;
}

void UTexture2D::QueueTextureRefresh(const FString& TexturePath)
{
	if (TexturePath.empty())
	{
		return;
	}

	PendingTextureRefreshPaths.insert(NormalizeTexturePath(TexturePath));
}

bool UTexture2D::SampleAlpha(float U, float V, float& OutAlpha) const
{
	OutAlpha = 1.0f;

	if (CPUTextureRGBA.empty() || Width == 0 || Height == 0)
	{
		return false;
	}

	const float ClampedU = std::clamp(U, 0.0f, 1.0f);
	const float ClampedV = std::clamp(V, 0.0f, 1.0f);
	const uint32 X = std::min<uint32>(static_cast<uint32>(ClampedU * static_cast<float>(Width - 1)), Width - 1);
	const uint32 Y = std::min<uint32>(static_cast<uint32>(ClampedV * static_cast<float>(Height - 1)), Height - 1);
	const size_t PixelIndex = (static_cast<size_t>(Y) * static_cast<size_t>(Width) + static_cast<size_t>(X)) * 4ull;

	if (PixelIndex + 3ull >= CPUTextureRGBA.size())
	{
		return false;
	}

	OutAlpha = static_cast<float>(CPUTextureRGBA[PixelIndex + 3ull]) / 255.0f;
	return true;
}
