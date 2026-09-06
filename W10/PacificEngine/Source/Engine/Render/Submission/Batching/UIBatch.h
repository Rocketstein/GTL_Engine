#pragma once

#include "Core/CoreTypes.h"
#include "Core/ResourceTypes.h"
#include "Math/Vector.h"
#include "Render/RHI/D3D11/Buffers/Buffers.h"
#include "Render/RHI/D3D11/Buffers/VertexTypes.h"
#include "Render/Submission/Batching/BatchBuffer.h"

class FUIProxy;

struct FUIBatchRange
{
    uint32 FirstIndex = 0;
    uint32 IndexCount = 0;
};

struct FUIParamsCBData
{
    FVector4 Flags = FVector4(0.0f, 0.0f, 0.0f, 0.0f);
};

struct FUICharacterInfo
{
    float U = 0.0f;
    float V = 0.0f;
    float Width = 0.0f;
    float Height = 0.0f;
};

class FUIBatch
{
public:
    void Create(ID3D11Device* InDevice);
    void Release();
    void Clear();

    FUIBatchRange AddScreenQuad(const FUIProxy& Proxy, float ViewportWidth, float ViewportHeight);
    FUIBatchRange AddScreenText(const FUIProxy& Proxy, const FFontResource* Font, float ViewportWidth, float ViewportHeight);
    FUIBatchRange AddWorldQuad(const FUIProxy& Proxy, const FVector& CameraRight, const FVector& CameraUp);
    FUIBatchRange AddWorldText(const FUIProxy& Proxy, const FFontResource* Font, const FVector& CameraRight, const FVector& CameraUp);
    bool UploadBuffers(ID3D11DeviceContext* Context);
    void UpdateParams(ID3D11DeviceContext* Context);

    ID3D11Buffer* GetVBBuffer() const { return Buffer.GetVBBuffer(); }
    uint32        GetVBStride() const { return Buffer.GetVBStride(); }
    ID3D11Buffer* GetIBBuffer() const { return Buffer.GetIBBuffer(); }
    uint32        GetIndexCount() const { return Buffer.GetIndexCount(); }

    FConstantBuffer* GetSolidParamsCB() { return &SolidParamsCB; }
    FConstantBuffer* GetTextureParamsCB() { return &TextureParamsCB; }
    FConstantBuffer* GetFontParamsCB() { return &FontParamsCB; }
    FConstantBuffer* GetWorldSolidParamsCB() { return &WorldSolidParamsCB; }
    FConstantBuffer* GetWorldTextureParamsCB() { return &WorldTextureParamsCB; }
    FConstantBuffer* GetWorldFontParamsCB() { return &WorldFontParamsCB; }

private:
    void BuildCharInfoMap(uint32 Columns, uint32 Rows);
    void EnsureCharInfoMap(const FFontResource* Font);
    bool GetCharUV(uint32 Codepoint, FVector2& OutUVMin, FVector2& OutUVMax) const;

    TBatchBuffer<FUIVertex> Buffer;
    FConstantBuffer SolidParamsCB;
    FConstantBuffer TextureParamsCB;
    FConstantBuffer FontParamsCB;
    FConstantBuffer WorldSolidParamsCB;
    FConstantBuffer WorldTextureParamsCB;
    FConstantBuffer WorldFontParamsCB;
    TMap<uint32, FUICharacterInfo> CharInfoMap;
    uint32 CachedFontColumns = 0;
    uint32 CachedFontRows = 0;
};
