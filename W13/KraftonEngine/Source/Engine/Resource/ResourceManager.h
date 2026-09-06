#pragma once

#include "Core/Types/CoreTypes.h"
#include "Core/Singleton.h"
#include "Core/Types/ResourceTypes.h"
#include "Object/FName.h"
#include <wrl/client.h>

// 리소스를 관리하는 싱글턴.
// Resource.ini에서 리소스 경로/그리드 정보를 읽고, GPU 리소스를 로드/캐싱합니다.
// 컴포넌트는 소유하지 않고 포인터로 공유 데이터를 참조합니다.

struct ID3D11Device;

class FResourceManager : public TSingleton<FResourceManager>
{
	friend class TSingleton<FResourceManager>;

public:
	// Resource.ini에서 경로/그리드 정보 로드 후 GPU 리소스 생성
	void LoadFromFile(const FString& Path, ID3D11Device* InDevice);

	void LoadFromDirectory(const FString& Path, ID3D11Device* InDevice);
	// GPU 리소스 로드 (Device 필요)
	bool LoadGPUResources(ID3D11Device* Device);

	// 모든 GPU 리소스 해제
	void ReleaseGPUResources();

	// --- Font ---
	FFontResource* FindFont(const FName& FontName);
	const FFontResource* FindFont(const FName& FontName) const;
	void RegisterFont(const FName& FontName, const FString& InPath, uint32 Columns = 16, uint32 Rows = 16);

	// --- Font names ---
	TArray<FString> GetFontNames() const;

	// --- SubUV Resource ---
	FSubUVResource* FindSubUVResource(const FName& ResourceName);
	const FSubUVResource* FindSubUVResource(const FName& ResourceName) const;
	void RegisterSubUVResource(const FName& ResourceName, const FString& InPath, uint32 Columns = 1, uint32 Rows = 1);

	// --- SubUV resource names ---
	TArray<FString> GetSubUVResourceNames() const;

	// --- Texture (단일 정적 이미지, 1x1 atlas) ---
	FTextureResource* FindTexture(const FName& TextureName);
	const FTextureResource* FindTexture(const FName& TextureName) const;
	void RegisterTexture(const FName& TextureName, const FString& InPath);

	// --- Texture names ---
	TArray<FString> GetTextureNames() const;

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> FindLoadedTexture(FString InPath);

private:
	FResourceManager() = default;
	~FResourceManager() { ReleaseGPUResources(); }

	TMap<FString, FFontResource>     FontResources;
	TMap<FString, FSubUVResource>    SubUVResources;
	TMap<FString, FTextureResource>  TextureResources;
	TMap<FString, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> LoadedResource;
};
