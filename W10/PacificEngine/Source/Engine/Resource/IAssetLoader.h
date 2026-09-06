#pragma once

#include "Core/CoreTypes.h"

struct ID3D11Device;

/**
 * @brief Generic interface for loading and managing assets.
 * @tparam T The asset type (e.g., UStaticMesh, UMaterial, FTextureResource).
 */
template <typename T>
class IAssetLoader
{
public:
    virtual ~IAssetLoader() = default;

    /**
     * @brief Sets the D3D11 device to be used for GPU resource creation.
     */
    virtual void SetDevice(ID3D11Device* InDevice) = 0;

    /**
     * @brief Loads an asset from the specified path.
     * @param Path The path to the asset file.
     * @return T* Pointer to the loaded asset, or nullptr if loading fails.
     */
    virtual T* Load(const FString& Path) = 0;

    /**
     * @brief Finds an already loaded asset by its key (usually the path).
     * @param Key The key associated with the asset.
     * @return T* Pointer to the asset if found, otherwise nullptr.
     */
    virtual T* Find(const FString& Key) = 0;

    /**
     * @brief Unloads a specific asset by its key.
     */
    virtual void Unload(const FString& Key) = 0;

    /**
     * @brief Unloads all managed assets.
     */
    virtual void UnloadAll() = 0;
};
