#pragma once

#include "Core/Containers/Map.h"
#include "Core/Containers/String.h"
#include "Renderer/D3D11/D3D11Device.h"

namespace Asset
{
    class FAssetCacheManager;
}

class UObject;
class UStaticMesh;
class UTexture;
class UMaterial;

class FAssetObjectManager
{
  public:
    FAssetObjectManager() = default;
    FAssetObjectManager(Asset::FAssetCacheManager *InAssetCacheManager, FD3D11Device *InDevice)
        : AssetCacheManager(InAssetCacheManager), Device(InDevice)
    {
    }

    void SetAssetCacheManager(Asset::FAssetCacheManager *InAssetCacheManager)
    {
        AssetCacheManager = InAssetCacheManager;
    }

    void SetDevice(FD3D11Device *InDevice) { Device = InDevice; }

    Asset::FAssetCacheManager *GetAssetCacheManager() const { return AssetCacheManager; }
    FD3D11Device              *GetDevice() const { return Device; }

    bool IsReady() const { return AssetCacheManager != nullptr && Device != nullptr; }

    UObject *LoadAssetObject(const FString &AssetPath);

    UStaticMesh *LoadStaticMeshObject(const FString &AssetPath);
    UTexture    *LoadTextureObject(const FString &AssetPath);
    UMaterial   *LoadMaterialObject(const FString &AssetPath);

    void BindStaticMeshMaterialSlots(UStaticMesh *StaticMeshAsset);

  private:
    UStaticMesh *FindStaticMeshObject(const FString &AssetPath);
    UTexture    *FindTextureObject(const FString &AssetPath);
    UMaterial   *FindMaterialObject(const FString &AssetPath);
    void         RegisterStaticMeshObject(const FString &AssetPath, UStaticMesh *Mesh);
    void         RegisterTextureObject(const FString &AssetPath, UTexture *Texture);
    void         RegisterMaterialObject(const FString &AssetPath, UMaterial *Material);

  private:
    Asset::FAssetCacheManager *AssetCacheManager = nullptr;
    FD3D11Device              *Device = nullptr;
    TMap<FString, UObject *>   AssetObjectIndex; // <AssetPath, UObject *>
};
