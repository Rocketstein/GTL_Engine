#include "Asset/Runtime/StaticMeshRenderResource.h"
#include "Renderer/D3D11/D3D11Device.h"

namespace Asset
{
    std::shared_ptr<FStaticMeshRenderResource>
    FStaticMeshRenderResource::Create(const FObjCookedData &CookedData, FD3D11Device &Device)
    {
        if (!CookedData.IsValid())
        {
            return nullptr;
        }

        std::shared_ptr<FD3D11Buffer> VertexBuffer = std::make_shared<FD3D11Buffer>();
        if (!VertexBuffer->CreateVertexBuffer(&Device, CookedData.VertexData.data(),
                                              static_cast<UINT>(CookedData.VertexData.size()), false))
        {
            return nullptr;
        }

        std::shared_ptr<FD3D11Buffer> IndexBuffer = std::make_shared<FD3D11Buffer>();
        if (!IndexBuffer->CreateIndexBuffer(
                &Device, CookedData.Indices.data(),
                static_cast<UINT>(CookedData.Indices.size() * sizeof(uint32)), false))
        {
            return nullptr;
        }

        std::shared_ptr<FStaticMeshRenderResource> Resource =
            std::make_shared<FStaticMeshRenderResource>();
        Resource->VertexBuffer = std::move(VertexBuffer);
        Resource->IndexBuffer = std::move(IndexBuffer);
        Resource->VertexCount = CookedData.VertexCount;
        Resource->IndexCount = static_cast<uint32>(CookedData.Indices.size());
        Resource->VertexStride = CookedData.VertexStride;
        return Resource;
    }

    bool FStaticMeshRenderResource::IsValid() const
    {
        return VertexBuffer != nullptr && IndexBuffer != nullptr;
    }

    void FStaticMeshRenderResource::Reset()
    {
        VertexBuffer.reset();
        IndexBuffer.reset();
        VertexCount = 0;
        IndexCount = 0;
        VertexStride = 0;
    }
} // namespace Asset
