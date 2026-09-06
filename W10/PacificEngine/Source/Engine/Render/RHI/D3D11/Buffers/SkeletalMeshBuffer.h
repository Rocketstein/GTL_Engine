#pragma once

#include "Core/CoreTypes.h"
#include "Render/RHI/D3D11/Buffers/VertexTypes.h"
#include "Render/RHI/D3D11/Buffers/MeshBuffer.h"
#include "Render/RHI/D3D11/Buffers/DynamicVertexBuffer.h"
#include "Render/RHI/D3D11/Buffers/IndexBuffer.h"

// FMeshBuffer는 GPU 버퍼 리소스의 생성과 바인딩을 관리합니다.
class FSkeletalMeshBuffer : public FMeshBuffer
{
public:
    FSkeletalMeshBuffer() = default;
    ~FSkeletalMeshBuffer() { Release(); }
    FSkeletalMeshBuffer(const FSkeletalMeshBuffer&)    = delete;
    FSkeletalMeshBuffer& operator=(const FSkeletalMeshBuffer&) = delete;
    FSkeletalMeshBuffer(FSkeletalMeshBuffer&&)                 = default;
    FSkeletalMeshBuffer& operator=(FSkeletalMeshBuffer&&)      = default;

    void                  Create(ID3D11Device* InDevice, const FRuntimePackedMeshData& InPackedMeshData) override;
    bool                  UpdateVertex(ID3D11DeviceContext* Context, const FRuntimePackedVertexData& InPackedVertexData) override;
    bool                  UpdateVertex(ID3D11DeviceContext* Context, const void* Data, uint32 Count);
    void                  Release() override;
    FDynamicVertexBuffer& GetVertexBuffer() { return VertexBuffer; }
    FIndexBuffer&         GetIndexBuffer() { return IndexBuffer; }
    const FDynamicVertexBuffer& GetVertexBuffer() const { return VertexBuffer; }
    const FIndexBuffer&         GetIndexBuffer() const { return IndexBuffer; }
    bool                  IsValid() const override { return VertexBuffer.GetBuffer() != nullptr && VertexBuffer.GetVertexCount() > 0; }
    ID3D11Buffer*         GetVertexBufferRaw() const override { return VertexBuffer.GetBuffer(); }
    ID3D11Buffer*         GetIndexBufferRaw() const override { return IndexBuffer.GetBuffer(); }
    uint32                GetIndexCount() const override { return IndexBuffer.GetIndexCount(); }

private:
    FDynamicVertexBuffer VertexBuffer;
    FIndexBuffer         IndexBuffer;
};
