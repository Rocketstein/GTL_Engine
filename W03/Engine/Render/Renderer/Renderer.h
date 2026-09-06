#pragma once

/*
	실제 렌더링을 담당하는 Class 입니다. (Rendering 최상위 클래스)
*/

#include "Render/Common/RenderTypes.h"

#include "Render/Scene/RenderBus.h"
#include "Render/Device/D3DDevice.h"
#include "Render/Resource/RenderResources.h"
#include "Render/Resource/RenderResourceManager.h"

#include "Engine/Fonts/FontCache.h"

#include <cstddef>

enum class EViewModeIndex : uint32
{
	VMI_Lit,
	VMI_Unlit,
	VMI_Wireframe,
};

enum class EEngineShowFlags : uint64
{
	SF_None = 0,
	SF_Primitives = 1 <<0 ,	//1
	SF_BillboardText = 1 << 1, //2
	Flag_OptionB = 1 << 2, //4
	Flag_OptionC = 1 << 3, //8
};

class FRenderer
{
private:
	FD3DDevice Device;
	FRenderResources Resources;
	FRenderResourceManager RenderResourceManager;

	TArray<FVertex> Grids;

	//	File Path
	const WCHAR* ShaderFilePath          = L"Assets/Shader/ShaderW0.hlsl";
	const WCHAR* BatchedLineShaderFilePath = L"Assets/Shader/BatchedLineShader.hlsl";
	const WCHAR* FontShaderFilePath      = L"Assets/Shader/FontShader.hlsl";
	const WCHAR* FontTextureFIlePath = L"Assets/Fonts/NanumGothic.dds";
	const WCHAR* SubUVShaderFilePath = L"Assets/Shader/ShaderWSubUV.hlsl";

	//	Primitive and Gizmo Input Layout
	D3D11_INPUT_ELEMENT_DESC PrimitiveInputLayout[2] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  static_cast<uint32>(offsetof(FVertex, Position)), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, static_cast<uint32>(offsetof(FVertex, Color)), D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	//	Overlay Input Layout (Screen Quad)
	D3D11_INPUT_ELEMENT_DESC OverlayInputLayout[1] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	// Font Input Layout
	D3D11_INPUT_ELEMENT_DESC FontInputLayout[11] =
	{
		// slot 0 : Vertex Buffer
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 28, D3D11_INPUT_PER_VERTEX_DATA, 0 },

		// slot 1 : Instance Buffer
		{ "MVP", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1,  0, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
		{ "MVP", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
		{ "MVP", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
		{ "MVP", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 48, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
		{ "PEN_POS",   0, DXGI_FORMAT_R32G32_FLOAT,       1, 64, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
		{ "CELL_SIZE", 0, DXGI_FORMAT_R32G32_FLOAT,       1, 72, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
		{ "START_UV",  0, DXGI_FORMAT_R32G32_FLOAT,       1, 80, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
		{ "UV_SIZE",   0, DXGI_FORMAT_R32G32_FLOAT,       1, 88, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
		{ "COLOR",     0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 96, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
	};

	// Font Input Layout
	D3D11_INPUT_ELEMENT_DESC SubUVInputLayout[2] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};


public:

private:
	void RenderComponentPass(ID3D11DeviceContext* InDeviceContext, const FRenderBus& InRenderBus);
	void RenderSubUVCompPass(ID3D11DeviceContext* InDeviceContext, const FRenderBus& InRenderBus);
	void RenderDepthLessPass(ID3D11DeviceContext* InDeviceContext, const FRenderBus& InRenderBus);

	//void RenderEditorPass(ID3D11DeviceContext* InDeviceContext, const FRenderBus& InRenderBus);
	//void RenderGridEditorPass(ID3D11DeviceContext* InDeviceContext, const FRenderBus& InRenderBus);

	void RenderLineBatchPass(ID3D11DeviceContext* InDeviceContext, FRenderBus& InRenderBus);

	void DrawString(ID3D11DeviceContext* InDeviceContext, FRenderBus& InRenderBus);
	void RenderFont(ID3D11DeviceContext* InDeviceContext, FRenderBus& InRenderBus);

	void RenderOverlayPass(ID3D11DeviceContext* InDeviceContext, const FRenderBus& InRenderBus);
	void RenderOutlinePass(ID3D11DeviceContext* InDeviceContext, const FRenderBus& InRenderBus);

	void DrawCommand(ID3D11DeviceContext* InDeviceContext, const FRenderCommand& InCommand);
	void DrawUVCommand(ID3D11DeviceContext* InDeviceContext, const FRenderCommand& InCommand);

public:
	void Create(HWND hWindow);
	void Release();

	void BeginFrame();
	void Render(FRenderBus& InRenderBus);
	void RenderOverlay(const FRenderBus& InRenderBus);	//	반드시 따로 호출해야 함
	void EndFrame();

	FD3DDevice& GetFD3DDevice() { return Device; }
	FRenderResources& GetResources() { return Resources; }

public: 
	EViewModeIndex viewMode = EViewModeIndex::VMI_Unlit;
	uint64 showFlag = (uint64)EEngineShowFlags::SF_Primitives;

};

