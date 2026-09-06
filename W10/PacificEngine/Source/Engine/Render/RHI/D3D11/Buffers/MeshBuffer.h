#pragma once

#include "Core/CoreTypes.h"
#include "Render/RHI/D3D11/Common/D3D11API.h"
#include "Render/RHI/D3D11/Buffers/VertexTypes.h"

struct FRuntimeMeshBufferLayout
{
    uint32                    VertexStride = 0;
    uint32                    VertexCount = 0;
    FVertexSemanticDescriptor VertexSemanticDescriptor;

    bool IsValid() const
    {
        return VertexStride > 0 && VertexCount > 0 && !VertexSemanticDescriptor.IsEmpty();
    }
};

class FMeshBuffer
{
public:
    virtual ~FMeshBuffer() = default;

    virtual void          Create(ID3D11Device* InDevice, const FRuntimePackedMeshData& InPackedMeshData) = 0;
    virtual bool          UpdateVertex(ID3D11DeviceContext* Context, const FRuntimePackedVertexData& InPackedVertexData) { return false; }
    virtual void          Release() = 0;
    virtual bool          IsValid() const = 0;
    virtual ID3D11Buffer* GetVertexBufferRaw() const = 0;
    uint32                GetVertexStride() const { return RuntimeLayout.VertexStride; }
    uint32                GetVertexCount() const { return RuntimeLayout.VertexCount; }
    const FVertexSemanticDescriptor& GetVertexSemanticDescriptor() const { return RuntimeLayout.VertexSemanticDescriptor; }
    const FRuntimeMeshBufferLayout&  GetRuntimeLayout() const { return RuntimeLayout; }
    virtual ID3D11Buffer* GetIndexBufferRaw() const = 0;
    virtual uint32        GetIndexCount() const = 0;

protected:
    void SetRuntimeLayout(const FRuntimePackedVertexData& InPackedVertexData)
    {
        RuntimeLayout.VertexStride = InPackedVertexData.VertexStride;
        RuntimeLayout.VertexCount = InPackedVertexData.VertexCount;
        RuntimeLayout.VertexSemanticDescriptor = InPackedVertexData.VertexSemanticDescriptor;
    }

    void ClearRuntimeLayout()
    {
        RuntimeLayout = {};
    }

private:
    FRuntimeMeshBufferLayout RuntimeLayout;
};
