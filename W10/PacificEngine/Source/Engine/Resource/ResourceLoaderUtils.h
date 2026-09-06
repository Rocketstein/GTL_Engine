#pragma once

#include "Core/CoreTypes.h"

struct ID3D11Device;
struct ID3D11ShaderResourceView;
struct FTextureAtlasResource;

namespace ResourceLoaderUtils
{
    /**
     * @brief Loads a texture from file and creates a Shader Resource View (SRV).
     * Handles .dds and WIC-supported formats (.png, .jpg, etc.).
     * Also updates memory statistics.
     * 
     * @param Device D3D11 device for resource creation.
     * @param Resource The resource structure to populate (Path must be set).
     * @return bool True if successful, false otherwise.
     */
    bool LoadTextureSRV(ID3D11Device* Device, FTextureAtlasResource& Resource);

    /**
     * @brief Releases the SRV and updates memory statistics.
     */
    void ReleaseTextureSRV(FTextureAtlasResource& Resource);
}
