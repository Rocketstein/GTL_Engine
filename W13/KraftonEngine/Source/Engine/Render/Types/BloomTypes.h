#pragma once

#include "Core/Types/CoreTypes.h"

#include <d3d11.h>

namespace EBloom
{
	constexpr uint32 MaxMipCount = 5;
	constexpr DXGI_FORMAT Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
}

struct FBloomMipResource
{
	ID3D11Texture2D* Texture = nullptr;
	ID3D11RenderTargetView* RTV = nullptr;
	ID3D11ShaderResourceView* SRV = nullptr;
	uint32 Width = 0;
	uint32 Height = 0;

	bool IsValid() const
	{
		return Texture && RTV && SRV && Width > 0 && Height > 0;
	}
};

struct FBloomFrameResources
{
	FBloomMipResource Mips[EBloom::MaxMipCount];
	FBloomMipResource TempMips[EBloom::MaxMipCount];

	bool IsValid() const
	{
		for (uint32 MipIndex = 0; MipIndex < EBloom::MaxMipCount; ++MipIndex)
		{
			if (!Mips[MipIndex].IsValid() || !TempMips[MipIndex].IsValid())
			{
				return false;
			}
		}
		return true;
	}
};
