#pragma once

#include "Core/CoreTypes.h"
#include "Render/RHI/D3D11/Buffers/VertexTypes.h"
#include "Render/RHI/D3D11/Buffers/MeshBuffer.h"
#include "Render/RHI/D3D11/Buffers/VertexBuffer.h"
#include "Render/RHI/D3D11/Buffers/IndexBuffer.h"

// FMeshBuffer는 GPU 버퍼 리소스의 생성과 바인딩을 관리합니다.
class FStaticMeshBuffer : public FMeshBuffer
{
public:
    FStaticMeshBuffer() = default;
    ~FStaticMeshBuffer() { Release(); }
    FStaticMeshBuffer(const FStaticMeshBuffer&)            = delete;
    FStaticMeshBuffer& operator=(const FStaticMeshBuffer&) = delete;
    FStaticMeshBuffer(FStaticMeshBuffer&&)                 = default;
    FStaticMeshBuffer& operator=(FStaticMeshBuffer&&)      = default;

    void                 Create(ID3D11Device* InDevice, const FRuntimePackedMeshData& InPackedMeshData) override;
    bool                 UpdateVertex(ID3D11DeviceContext* Context, const FRuntimePackedVertexData& InPackedVertexData) override { return false; }
    void                 Release() override;
    FVertexBuffer&       GetVertexBuffer() { return VertexBuffer; }
    FIndexBuffer&        GetIndexBuffer() { return IndexBuffer; }
    const FVertexBuffer& GetVertexBuffer() const { return VertexBuffer; }
    const FIndexBuffer&  GetIndexBuffer() const { return IndexBuffer; }
    bool                 IsValid() const override { return VertexBuffer.GetBuffer() != nullptr && VertexBuffer.GetVertexCount() > 0; }
    ID3D11Buffer*        GetVertexBufferRaw() const override { return VertexBuffer.GetBuffer(); }
    ID3D11Buffer*        GetIndexBufferRaw() const override { return IndexBuffer.GetBuffer(); }
    uint32               GetIndexCount() const override { return IndexBuffer.GetIndexCount(); }

private:
    FVertexBuffer             VertexBuffer;
    FIndexBuffer              IndexBuffer;
};
