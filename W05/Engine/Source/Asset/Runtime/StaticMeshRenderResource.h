#pragma once

#include "Asset/Cooked/ObjCookedData.h"
#include "Renderer/D3D11/Resources/D3D11Buffer.h"
#include <memory>
class FD3D11Device;

namespace Asset
{
    struct FStaticMeshRenderResource
    {
        std::shared_ptr<FD3D11Buffer> VertexBuffer;
        std::shared_ptr<FD3D11Buffer> IndexBuffer;

        uint32 VertexCount = 0;
        uint32 IndexCount = 0;
        uint32 VertexStride = 0;

        static std::shared_ptr<FStaticMeshRenderResource> Create(const FObjCookedData &CookedData,
                                                                 FD3D11Device         &Device);

        bool IsValid() const;
        void Reset();
    };
} // namespace Asset
