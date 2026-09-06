#pragma once
#include "Core/Types/CoreTypes.h"

#include <d3d11.h>

namespace EDOF
{
	// CoC/Blur run at 1/Downscale resolution; Composite stays full-res.
	constexpr uint32 Downscale = 2;
}

struct FDOFResources
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

struct FDOF
{
	FDOFResources CoCResources;
	FDOFResources BlurResources;

	bool IsValid() const
	{
		return (CoCResources.IsValid() && BlurResources.IsValid());
	}
};