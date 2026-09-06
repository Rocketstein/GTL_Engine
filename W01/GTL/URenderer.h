#pragma once
#include <d3d11.h>
#include <d3dcompiler.h>
#include "Structs.h"
#include "Constants.h"
#include <iostream>

class URenderer
{
public:
	ID3D11Device* Device = nullptr;
	ID3D11DeviceContext* DeviceContext = nullptr;
	IDXGISwapChain* SwapChain = nullptr;

	ID3D11Texture2D* FrameBuffer = nullptr;
	ID3D11RenderTargetView* FrameBufferRTV = nullptr;
	ID3D11RasterizerState* RasterizerState = nullptr;
	ID3D11Buffer* ConstantBuffer = nullptr;
	ID3D11Buffer* RectConstantBuffer = nullptr;

	FLOAT ClearColor[4] = { 0.025f, 0.025f, 0.025f, 1.0f };
	D3D11_VIEWPORT GameViewportInfo;
	D3D11_VIEWPORT UiViewportInfo;

	ID3D11VertexShader* SimpleVertexShader;
	ID3D11PixelShader* SimplePixelShader;
	ID3D11InputLayout* SimpleInputLayout;
	unsigned int Stride;

	ID3D11VertexShader* TextureVertexShader = nullptr;
	ID3D11PixelShader* TexturePixelShader = nullptr;
	ID3D11InputLayout* TextureInputLayout = nullptr;

	ID3D11VertexShader* RectTextureVertexShader = nullptr;
	ID3D11PixelShader* RectTexturePixelShader = nullptr;
	ID3D11InputLayout* RectTextureInputLayout = nullptr;

	ID3D11SamplerState* TextureSampler = nullptr;
	ID3D11BlendState* AlphaBlendState = nullptr;

	ID3D11Buffer* SpriteQuadBuffer = nullptr;

public:
	void Create(HWND hWindow)
	{
		CreateDeviceAndSwapChain(hWindow);

		CreateFrameBuffer();

		CreateRasterizerState();

	}

	void CreateDeviceAndSwapChain(HWND hWindow)
	{
		D3D_FEATURE_LEVEL featurelevels[] = { D3D_FEATURE_LEVEL_11_0 };

		DXGI_SWAP_CHAIN_DESC swapchaindesc = { };
		swapchaindesc.BufferDesc.Width = 0;
		swapchaindesc.BufferDesc.Height = 0;
		swapchaindesc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		swapchaindesc.SampleDesc.Count = 1;
		swapchaindesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapchaindesc.BufferCount = 2;
		swapchaindesc.OutputWindow = hWindow;
		swapchaindesc.Windowed = TRUE;
		swapchaindesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

		D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_DEBUG, featurelevels, ARRAYSIZE(featurelevels), D3D11_SDK_VERSION, &swapchaindesc, &SwapChain, &Device, nullptr, &DeviceContext);
		UpdateViewportInfo();
	}

	void UpdateViewportInfo()
	{
		DXGI_SWAP_CHAIN_DESC swapchaindesc = { };
		SwapChain->GetDesc(&swapchaindesc);

		float width = (float)swapchaindesc.BufferDesc.Width;
		float height = (float)swapchaindesc.BufferDesc.Height;
		GameViewportInfo = {
			0.0f,
			(height - width) * 0.5f,
			width,
			// height,
			width,		// viewport가 윈도우 위아래로 삐져나가게 배치
			0.0f,
			1.0f
		};
		UiViewportInfo = {
			0.0f,
			0.0f,
			width,
			height,
			0.0f,
			1.0f
		};
	}

	void ReleaseDeviceAndSwapChain()
	{
		if (DeviceContext)
		{
			DeviceContext->Flush();
		}

		if (SwapChain)
		{
			SwapChain->Release();
			SwapChain = nullptr;
		}

		if (Device)
		{
			Device->Release();
			Device = nullptr;
		}

		if (DeviceContext)
		{
			DeviceContext->Release();
			DeviceContext = nullptr;
		}
	}

	void CreateFrameBuffer()
	{
		SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&FrameBuffer);

		D3D11_RENDER_TARGET_VIEW_DESC framebufferRTVdesc = {};
		framebufferRTVdesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
		framebufferRTVdesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

		Device->CreateRenderTargetView(FrameBuffer, &framebufferRTVdesc, &FrameBufferRTV);
	}

	void ReleaseFrameBuffer()
	{
		if (FrameBuffer)
		{
			FrameBuffer->Release();
			FrameBuffer = nullptr;
		}

		if (FrameBufferRTV)
		{
			FrameBufferRTV->Release();
			FrameBufferRTV = nullptr;
		}
	}

	void CreateRasterizerState()
	{
		D3D11_RASTERIZER_DESC rasterizerdesc = {};
		rasterizerdesc.FillMode = D3D11_FILL_SOLID;
		rasterizerdesc.CullMode = D3D11_CULL_BACK;

		Device->CreateRasterizerState(&rasterizerdesc, &RasterizerState);
	}

	void ReleaseRasterizerState()
	{
		if (RasterizerState)
		{
			RasterizerState->Release();
			RasterizerState = nullptr;
		}
	}

	void CreateShader()
	{
		ID3DBlob* vertexshaderCSO;
		ID3DBlob* pixelshaderCSO;
		ID3DBlob* errorBlob = nullptr;

		D3DCompileFromFile(L"ShaderW0.hlsl", nullptr, nullptr, "mainVS", "vs_5_0", 0, 0, &vertexshaderCSO, &errorBlob);

		//std::string str = (char*)errorBlob->GetBufferPointer();
		Device->CreateVertexShader(vertexshaderCSO->GetBufferPointer(), vertexshaderCSO->GetBufferSize(), nullptr, &SimpleVertexShader);

		D3DCompileFromFile(L"ShaderW0.hlsl", nullptr, nullptr, "mainPS", "ps_5_0", 0, 0, &pixelshaderCSO, nullptr);

		Device->CreatePixelShader(pixelshaderCSO->GetBufferPointer(), pixelshaderCSO->GetBufferSize(), nullptr, &SimplePixelShader);

		D3D11_INPUT_ELEMENT_DESC layout[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};

		Device->CreateInputLayout(layout, ARRAYSIZE(layout), vertexshaderCSO->GetBufferPointer(), vertexshaderCSO->GetBufferSize(), &SimpleInputLayout);


		vertexshaderCSO->Release();
		pixelshaderCSO->Release();

		D3DCompileFromFile(L"ShaderW1.hlsl", nullptr, nullptr, "mainVS", "vs_5_0", 0, 0, &vertexshaderCSO, &errorBlob);
		Device->CreateVertexShader(vertexshaderCSO->GetBufferPointer(), vertexshaderCSO->GetBufferSize(), nullptr, &TextureVertexShader);
		D3D11_INPUT_ELEMENT_DESC textureLayout[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};
		Device->CreateInputLayout(textureLayout, ARRAYSIZE(textureLayout), vertexshaderCSO->GetBufferPointer(), vertexshaderCSO->GetBufferSize(), &TextureInputLayout);
		vertexshaderCSO->Release();
		vertexshaderCSO = nullptr;

		D3DCompileFromFile(L"ShaderW1.hlsl", nullptr, nullptr, "mainPS", "ps_5_0", 0, 0, &pixelshaderCSO, &errorBlob);
		Device->CreatePixelShader(pixelshaderCSO->GetBufferPointer(), pixelshaderCSO->GetBufferSize(), nullptr, &TexturePixelShader);
		pixelshaderCSO->Release();
		vertexshaderCSO = nullptr;

		D3DCompileFromFile(L"ShaderSpriteRect.hlsl", nullptr, nullptr, "mainVS", "vs_5_0", 0, 0, &vertexshaderCSO, &errorBlob);
		Device->CreateVertexShader(vertexshaderCSO->GetBufferPointer(), vertexshaderCSO->GetBufferSize(), nullptr, &RectTextureVertexShader);

		D3D11_INPUT_ELEMENT_DESC rectTextureLayout[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};

		Device->CreateInputLayout(rectTextureLayout, ARRAYSIZE(rectTextureLayout), vertexshaderCSO->GetBufferPointer(), vertexshaderCSO->GetBufferSize(), &RectTextureInputLayout);

		vertexshaderCSO->Release();
		vertexshaderCSO = nullptr;

		D3DCompileFromFile(L"ShaderSpriteRect.hlsl", nullptr, nullptr, "mainPS", "ps_5_0", 0, 0, &pixelshaderCSO, &errorBlob);
		Device->CreatePixelShader(pixelshaderCSO->GetBufferPointer(), pixelshaderCSO->GetBufferSize(), nullptr, &RectTexturePixelShader);

		pixelshaderCSO->Release();
		pixelshaderCSO = nullptr;
		
		Stride = sizeof(FVertexSimple);
	}

	void ReleaseShader()
	{
		if (SimpleInputLayout)
		{
			SimpleInputLayout->Release();
			SimpleInputLayout = nullptr;
		}

		if (SimplePixelShader)
		{
			SimplePixelShader->Release();
			SimplePixelShader = nullptr;
		}

		if (SimpleVertexShader)
		{
			SimpleVertexShader->Release();
			SimpleVertexShader = nullptr;
		}

		if (TextureInputLayout)
		{
			TextureInputLayout->Release();
			TextureInputLayout = nullptr;
		}

		if (TexturePixelShader)
		{
			TexturePixelShader->Release();
			TexturePixelShader = nullptr;
		}

		if (TextureVertexShader)
		{
			TextureVertexShader->Release();
			TextureVertexShader = nullptr;
		}

		if (RectTextureInputLayout)
		{
			RectTextureInputLayout->Release();
			RectTextureInputLayout = nullptr;
		}

		if (RectTexturePixelShader)
		{
			RectTexturePixelShader->Release();
			RectTexturePixelShader = nullptr;
		}

		if (RectTextureVertexShader)
		{
			RectTextureVertexShader->Release();
			RectTextureVertexShader = nullptr;
		}
	}

	void Release()
	{
		ReleaseSpriteQuad();
		ReleaseTextureStates();

		RasterizerState->Release();

		DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);

		ReleaseRectConstantBuffer();
		ReleaseFrameBuffer();
		ReleaseDeviceAndSwapChain();
	}

	void SwapBuffer()
	{
		SwapChain->Present(1, 0);
	}

	void Prepare()
	{
		DeviceContext->ClearRenderTargetView(FrameBufferRTV, ClearColor);
		DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		DeviceContext->RSSetState(RasterizerState);
		DeviceContext->OMSetRenderTargets(1, &FrameBufferRTV, nullptr);
		DeviceContext->OMSetBlendState(nullptr, nullptr, 0xffffffff);
	}

	void SetGameViewport()
	{
		DeviceContext->RSSetViewports(1, &GameViewportInfo);
	}

	void SetUiViewport()
	{
		DeviceContext->RSSetViewports(1, &UiViewportInfo);
	}

	void PrepareShader()
	{
		DeviceContext->VSSetShader(SimpleVertexShader, nullptr, 0);
		DeviceContext->PSSetShader(SimplePixelShader, nullptr, 0);
		DeviceContext->IASetInputLayout(SimpleInputLayout);

		if (ConstantBuffer)
		{
			DeviceContext->VSSetConstantBuffers(0, 1, &ConstantBuffer);
		}
	}

	void PrepareTextureShader(ID3D11ShaderResourceView* textureSRV)
	{
		DeviceContext->VSSetShader(TextureVertexShader, nullptr, 0);
		DeviceContext->PSSetShader(TexturePixelShader, nullptr, 0);
		DeviceContext->IASetInputLayout(TextureInputLayout);

		if (ConstantBuffer)
		{
			DeviceContext->VSSetConstantBuffers(0, 1, &ConstantBuffer);
		}

		DeviceContext->PSSetShaderResources(0, 1, &textureSRV);
		DeviceContext->PSSetSamplers(0, 1, &TextureSampler);
		DeviceContext->OMSetBlendState(AlphaBlendState, nullptr, 0xffffffff);
	}

	void PrepareRectTextureShader(ID3D11ShaderResourceView* textureSRV)
	{
		DeviceContext->VSSetShader(RectTextureVertexShader, nullptr, 0);
		DeviceContext->PSSetShader(RectTexturePixelShader, nullptr, 0);
		DeviceContext->IASetInputLayout(RectTextureInputLayout);

		if (RectConstantBuffer)
		{
			DeviceContext->VSSetConstantBuffers(0, 1, &RectConstantBuffer);
		}

		DeviceContext->PSSetShaderResources(0, 1, &textureSRV);
		DeviceContext->PSSetSamplers(0, 1, &TextureSampler);
		DeviceContext->OMSetBlendState(AlphaBlendState, nullptr, 0xffffffff);
	}

	void RenderPrimitive(ID3D11Buffer* pBuffer, UINT numVertices)
	{
		UINT offset = 0;
		DeviceContext->IASetVertexBuffers(0, 1, &pBuffer, &Stride, &offset);
		DeviceContext->Draw(numVertices, 0);
	}
	
	void RenderTexturePrimitive(ID3D11Buffer* pBuffer, UINT numVertices)
	{
		UINT stride = sizeof(FVertexTexture);
		UINT offset = 0;
		DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		DeviceContext->IASetVertexBuffers(0, 1, &pBuffer, &stride, &offset);
		DeviceContext->Draw(numVertices, 0);
	}

	void RenderPrimitiveInstanced(ID3D11Buffer* pBuffer, UINT numVertices, UINT instanceCount)
	{
		UINT offset = 0;
		DeviceContext->IASetVertexBuffers(0, 1, &pBuffer, &Stride, &offset);
		DeviceContext->DrawInstanced(numVertices, instanceCount, 0, 0);
	}

	ID3D11Buffer* CreateVertexBuffer(FVertexSimple* vertices, UINT byteWidth)
	{
		D3D11_BUFFER_DESC vertexbufferdesc = {};
		vertexbufferdesc.ByteWidth = byteWidth;
		vertexbufferdesc.Usage = D3D11_USAGE_IMMUTABLE;
		vertexbufferdesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

		D3D11_SUBRESOURCE_DATA vertexbufferSRD = { vertices };

		ID3D11Buffer* vertexBuffer;

		Device->CreateBuffer(&vertexbufferdesc, &vertexbufferSRD, &vertexBuffer);

		return vertexBuffer;
	}

	ID3D11Buffer* CreateVertexBuffer(FVertexTexture* vertices, UINT byteWidth)
	{
		D3D11_BUFFER_DESC vertexbufferdesc = {};
		vertexbufferdesc.ByteWidth = byteWidth;
		vertexbufferdesc.Usage = D3D11_USAGE_IMMUTABLE;
		vertexbufferdesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

		D3D11_SUBRESOURCE_DATA vertexbufferSRD = {};
		vertexbufferSRD.pSysMem = vertices;

		ID3D11Buffer* vertexBuffer = nullptr;
		Device->CreateBuffer(&vertexbufferdesc, &vertexbufferSRD, &vertexBuffer);

		return vertexBuffer;
	}

	void ReleaseVertexBuffer(ID3D11Buffer* vertexBuffer)
	{
		vertexBuffer->Release();
	}

	struct  FConstants
	{
		FVector4 PosRadius;
		FVector4 Angle;
		FVector4 Color;
		int useConstantColor;
		float pad[3];
	};

	struct FRectConstants
	{
		FVector4 Position[MAX_BALLS];
		FVector4 Size[MAX_BALLS];
		FVector4 Angle[MAX_BALLS];
		int Count;
		float Pad[3];
	};

	void CreateConstantBuffer()
	{
		D3D11_BUFFER_DESC constantbufferdesc = {};
		constantbufferdesc.ByteWidth = (sizeof(FConstants) + 0xF) & 0xFFFFFFF0;
		constantbufferdesc.Usage = D3D11_USAGE_DYNAMIC;
		constantbufferdesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		constantbufferdesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

		Device->CreateBuffer(&constantbufferdesc, nullptr, &ConstantBuffer);
	}

	void CreateRectConstantBuffer()
	{
		D3D11_BUFFER_DESC constantbufferdesc = {};
		constantbufferdesc.ByteWidth = (sizeof(FRectConstants) + 0xF) & 0xFFFFFFF0;
		constantbufferdesc.Usage = D3D11_USAGE_DYNAMIC;
		constantbufferdesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		constantbufferdesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

		Device->CreateBuffer(&constantbufferdesc, nullptr, &RectConstantBuffer);
	}

	void ReleaseConstantBuffer()
	{
		if (ConstantBuffer)
		{
			ConstantBuffer->Release();
			ConstantBuffer = nullptr;
		}
	}

	void ReleaseRectConstantBuffer()
	{
		if (RectConstantBuffer)
		{
			RectConstantBuffer->Release();
			RectConstantBuffer = nullptr;
		}
	}

	float GetAspectScaleX() const
	{
		if (GameViewportInfo.Width <= 0.0f || GameViewportInfo.Height <= 0.0f)
			return 1.0f;

		return GameViewportInfo.Height / GameViewportInfo.Width;
	}

	void UpdateConstant(const FVector4* PosRadius, const float* Angle, const int& Count)
	{
		if (ConstantBuffer)
		{
			D3D11_MAPPED_SUBRESOURCE constantbufferMSR{};

			DeviceContext->Map(ConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &constantbufferMSR);
			FConstants* constants = (FConstants*)constantbufferMSR.pData;
			{
				constants->PosRadius = PosRadius[0];
				constants->Angle = FVector4(Angle[0], GetAspectScaleX(), 0.0f, 0.0f);
				constants->Color = FVector4(1, 1, 1, 1);
				constants->useConstantColor = 0;
			}
			DeviceContext->Unmap(ConstantBuffer, 0);
		}
	}

	void UpdateConstant(const FVector4& PosRadius, const float& Angle, const FVector4& color)
	{
		if (ConstantBuffer)
		{
			D3D11_MAPPED_SUBRESOURCE constantbufferMSR{};

			DeviceContext->Map(ConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &constantbufferMSR);
			FConstants* constants = (FConstants*)constantbufferMSR.pData;
			{
				constants->PosRadius = PosRadius;
				constants->Angle = FVector4(Angle, GetAspectScaleX(), 0.0f, 0.0f);
				constants->Color = color;
				constants->useConstantColor = 1;
			}
			DeviceContext->Unmap(ConstantBuffer, 0);
		}
	}

	void UpdateConstant(const FVector4& PosRadius, const float& Angle)
	{
		if (ConstantBuffer)
		{
			D3D11_MAPPED_SUBRESOURCE constantbufferMSR{};

			DeviceContext->Map(ConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &constantbufferMSR);
			FConstants* constants = (FConstants*)constantbufferMSR.pData;
			{
				constants->PosRadius = PosRadius;
				constants->Angle = FVector4(Angle, GetAspectScaleX(), 0.0f, 0.0f);
				constants->Color = FVector4(1, 1, 1, 1);
				constants->useConstantColor = 0;
			}
			DeviceContext->Unmap(ConstantBuffer, 0);
		}
	}

	void CreateTextureStates()
	{
		D3D11_SAMPLER_DESC samplerDesc = {};
		samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
		samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
		samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
		samplerDesc.MinLOD = 0;
		samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

		Device->CreateSamplerState(&samplerDesc, &TextureSampler);

		D3D11_BLEND_DESC blendDesc = {};
		blendDesc.RenderTarget[0].BlendEnable = TRUE;
		blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
		blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
		blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
		blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
		blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

		Device->CreateBlendState(&blendDesc, &AlphaBlendState);
	}

	void ReleaseTextureStates()
	{
		if (AlphaBlendState)
		{
			AlphaBlendState->Release();
			AlphaBlendState = nullptr;
		}

		if (TextureSampler)
		{
			TextureSampler->Release();
			TextureSampler = nullptr;
		}
	}

	void CreateSpriteQuad()
	{
		FVertexTexture quadVertices[] =
		{
			// triangle 1
			{ -1.0f,  1.0f, 0.0f, 0.0f, 0.0f },
			{  1.0f, -1.0f, 0.0f, 1.0f, 1.0f },
			{ -1.0f, -1.0f, 0.0f, 0.0f, 1.0f },

			// triangle 2
			{ -1.0f,  1.0f, 0.0f, 0.0f, 0.0f },
			{  1.0f,  1.0f, 0.0f, 1.0f, 0.0f },
			{  1.0f, -1.0f, 0.0f, 1.0f, 1.0f },
		};

		SpriteQuadBuffer = CreateVertexBuffer(quadVertices, sizeof(quadVertices));
	}
	void ReleaseSpriteQuad()
	{
		if (SpriteQuadBuffer)
		{
			SpriteQuadBuffer->Release();
			SpriteQuadBuffer = nullptr;
		}
	}

	void DrawSprite(
		ID3D11ShaderResourceView* textureSRV,
		const FVector& position,
		float rotation,
		float radius,
		bool bLookLeft
	)
	{
		if (textureSRV == nullptr || SpriteQuadBuffer == nullptr)
			return;

		FVector4 posRadius = FVector4(position.x, position.y, position.z, radius);
		float angle = rotation;

		PrepareTextureShader(textureSRV);
		UpdateConstant(posRadius, angle);
		RenderTexturePrimitive(SpriteQuadBuffer, 6);
	}

	void DrawSprite(
		ID3D11ShaderResourceView* textureSRV,
		const FVector& position,
		float radius,
		bool bLookLeft
	)
	{
		float left = position.x - radius;
		float right = position.x + radius;
		float top = position.y + radius;
		float bottom = position.y - radius;

		FVertexTexture vertices[6];

		if (!bLookLeft)
		{
			vertices[0] = { left,  top,    0.0f, 0.0f, 0.0f };
			vertices[1] = { right, bottom, 0.0f, 1.0f, 1.0f };
			vertices[2] = { left,  bottom, 0.0f, 0.0f, 1.0f };

			vertices[3] = { left,  top,    0.0f, 0.0f, 0.0f };
			vertices[4] = { right, top,    0.0f, 1.0f, 0.0f };
			vertices[5] = { right, bottom, 0.0f, 1.0f, 1.0f };
		}
		else
		{
			vertices[0] = { left,  top,    0.0f, 1.0f, 0.0f };
			vertices[1] = { right, bottom, 0.0f, 0.0f, 1.0f };
			vertices[2] = { left,  bottom, 0.0f, 1.0f, 1.0f };

			vertices[3] = { left,  top,    0.0f, 1.0f, 0.0f };
			vertices[4] = { right, top,    0.0f, 0.0f, 0.0f };
			vertices[5] = { right, bottom, 0.0f, 0.0f, 1.0f };
		}

		ID3D11Buffer* tempBuffer = CreateVertexBuffer(vertices, sizeof(vertices));

		PrepareTextureShader(textureSRV);
		RenderTexturePrimitive(tempBuffer, 6);

		ReleaseVertexBuffer(tempBuffer);
	}

	void DrawRectSprite(
		ID3D11ShaderResourceView* textureSRV,
		const FVector& position,
		float rotation,
		float halfWidth,
		float halfHeight,
		bool bLookLeft
	)
	{
		if (textureSRV == nullptr || SpriteQuadBuffer == nullptr)
			return;

		FVector4 positions[1] = { FVector4(position.x, position.y, position.z, 0.0f) };
		FVector4 sizes[1] = { FVector4(halfWidth, halfHeight, 0.0f, 0.0f) };
		float angles[1] = { rotation };

		PrepareRectTextureShader(textureSRV);
		UpdateRectConstant(positions, sizes, angles, 1);
		RenderTexturePrimitive(SpriteQuadBuffer, 6);
	}

	void UpdateRectConstant(const FVector4* positions, const FVector4* sizes, const float* angles, int count)
	{
		if (RectConstantBuffer)
		{
			D3D11_MAPPED_SUBRESOURCE mapped;
			DeviceContext->Map(RectConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);

			FRectConstants* data = (FRectConstants*)mapped.pData;

			data->Count = count;

			for (int i = 0; i < count; ++i)
			{
				data->Position[i] = positions[i];
				data->Size[i] = sizes[i];
				data->Angle[i] = FVector4(angles[i], 0.0f, 0.0f, 0.0f);
			}

			for (int i = count; i < MAX_BALLS; ++i)
			{
				data->Position[i] = FVector4(0, 0, 0, 0);
				data->Size[i] = FVector4(0, 0, 0, 0);
				data->Angle[i] = FVector4(0, 0, 0, 0);
			}

			DeviceContext->Unmap(RectConstantBuffer, 0);
		}
	}

	void PrepareColorShader()
	{
		DeviceContext->VSSetShader(SimpleVertexShader, nullptr, 0);
		DeviceContext->PSSetShader(SimplePixelShader, nullptr, 0);
		DeviceContext->IASetInputLayout(SimpleInputLayout);

		// Unbind texture so it doesn't bleed into color shader
		ID3D11ShaderResourceView* nullSRV = nullptr;
		DeviceContext->PSSetShaderResources(0, 1, &nullSRV);

		// Disable alpha blend for normal actors
		DeviceContext->OMSetBlendState(nullptr, nullptr, 0xffffffff);

		if (ConstantBuffer)
			DeviceContext->VSSetConstantBuffers(0, 1, &ConstantBuffer);
	}

	void PrepareColorShaderWithAlpha()
	{
		DeviceContext->VSSetShader(SimpleVertexShader, nullptr, 0);
		DeviceContext->PSSetShader(SimplePixelShader, nullptr, 0);
		DeviceContext->IASetInputLayout(SimpleInputLayout);

		// Unbind texture
		ID3D11ShaderResourceView* nullSRV = nullptr;
		DeviceContext->PSSetShaderResources(0, 1, &nullSRV);

		// Enable alpha blend for particles
		DeviceContext->OMSetBlendState(AlphaBlendState, nullptr, 0xffffffff);

		if (ConstantBuffer)
			DeviceContext->VSSetConstantBuffers(0, 1, &ConstantBuffer);
	}
};

