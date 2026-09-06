#pragma once

#include "Core/CoreTypes.h"
#include "Render/Resources/Bindings/ShaderResourceBinding.h"
#include "Render/Resources/State/RenderStateTypes.h"

class FConstantBuffer;
class FGraphicsProgram;
class FMeshBuffer;
struct ID3D11ShaderResourceView;

// FMeshSectionRenderData는 메시 데이터와 렌더 제출 정보를 다룹니다.
struct FMeshSectionRenderData
{
	FMeshBuffer*			  MeshBuffer  = nullptr;
    FGraphicsProgram*         Shader      = nullptr;
    ID3D11ShaderResourceView* DiffuseSRV  = nullptr;
    ID3D11ShaderResourceView* NormalSRV   = nullptr;
    ID3D11ShaderResourceView* SpecularSRV = nullptr;
    TArray<FShaderResourceBinding> ShaderResourceBindings;
    FString                   MaterialPath;
    uint32                    FirstIndex  = 0;
    uint32                    IndexCount  = 0;

    EBlendState        Blend        = EBlendState::Opaque;
    EDepthStencilState DepthStencil = EDepthStencilState::Default;
    ERasterizerState   Rasterizer   = ERasterizerState::SolidBackCull;

    FConstantBuffer* MaterialCB[2] = {};
};
