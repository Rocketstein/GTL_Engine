#pragma once

#include "Core/CoreTypes.h"
#include "Core/Singleton.h"
#include "Core/ResourceTypes.h"
#include "Object/FName.h"
#include <d3d11.h>
#include <wrl/client.h>

// 리소스를 관리하는 싱글턴.
// Resource.ini에서 리소스 경로/그리드 정보를 읽고, GPU 리소스를 로드/캐싱합니다.
// 컴포넌트는 소유하지 않고 포인터로 공유 데이터를 참조합니다.

class FResourceManager : public TSingleton<FResourceManager>
{
	friend class TSingleton<FResourceManager>;

public:
	// Resource.ini에서 경로/그리드 정보 로드 후 GPU 리소스 생성
	void LoadFromFile(const FString& Path, ID3D11Device* InDevice);
	void LoadFromScanFile(const FString& Path, ID3D11Device* InDevice);

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

	// --- Particle ---
	FParticleResource* FindParticle(const FName& ParticleName);
	const FParticleResource* FindParticle(const FName& ParticleName) const;
	void RegisterParticle(const FName& ParticleName, const FString& InPath, uint32 Columns = 1, uint32 Rows = 1);

	// --- Particle names ---
	TArray<FString> GetParticleNames() const;

	// --- Texture (단일 정적 이미지, 1x1 atlas) ---
	FTextureResource* FindTexture(const FName& TextureName);
	const FTextureResource* FindTexture(const FName& TextureName) const;
	void RegisterTexture(const FName& TextureName, const FString& InPath);

	// --- Texture names ---
	TArray<FString> GetTextureNames(bool bIncludeEditorResources = true) const;

	// --- Mesh (경로 레지스트리 전용) ---
	FMeshResource* FindMesh(const FName& MeshName);
	const FMeshResource* FindMesh(const FName& MeshName) const;
	void RegisterMesh(const FName& MeshName, const FString& InPath);
	TArray<FString> GetMeshNames() const;

	// --- Sound (경로 레지스트리 전용) ---
	FSoundResource* FindSound(const FName& SoundName);
	const FSoundResource* FindSound(const FName& SoundName) const;
	void RegisterSound(const FName& SoundName, const FString& InPath, ESoundCategory Category = ESoundCategory::SFX);
	TArray<FString> GetSoundNames() const;
	TArray<FString> GetSoundNames(ESoundCategory Category) const;

	// --- Material (경로 레지스트리 + 기본 프리로드) ---
	FMaterialResource* FindMaterial(const FName& MaterialName);
	const FMaterialResource* FindMaterial(const FName& MaterialName) const;
	void RegisterMaterial(const FName& MaterialName, const FString& InPath);
	TArray<FString> GetMaterialNames() const;

	// --- Generic path registry ---
	FGenericPathResource* FindPath(const FName& ResourceName);
	const FGenericPathResource* FindPath(const FName& ResourceName) const;
	void RegisterPath(const FName& ResourceName, const FString& InPath);
	FString ResolvePath(const FName& ResourceName, const FString& Fallback = "") const;
	TArray<FString> GetPathNames() const;

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> FindLoadedTexture(FString InPath);

private:
	void DiscoverBitmapFonts(const FString& DirectoryPath);
	bool LoadBitmapFontMetadata(const FString& JsonPath, FFontResource& OutResource) const;

	FResourceManager() = default;
	~FResourceManager() { ReleaseGPUResources(); }

	TMap<FString, FFontResource>     FontResources;
	TMap<FString, FParticleResource> ParticleResources;
	TMap<FString, FTextureResource>  TextureResources;
	TMap<FString, FMeshResource>     MeshResources;
	TMap<FString, FSoundResource>    SoundResources;
	TMap<FString, FMaterialResource> MaterialResources;
	TMap<FString, FGenericPathResource> PathResources;
	TMap<FString, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> LoadedResource;
};
