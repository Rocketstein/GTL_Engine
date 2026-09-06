#include "BloomPass.h"
#include "RenderPassRegistry.h"

#include "Render/Device/D3DDevice.h"
#include "Render/Command/DrawCommandList.h"
#include "Render/Resource/RenderResources.h"
#include "Render/Shader/ShaderManager.h"
#include "Render/Types/FrameContext.h"
#include "Render/Types/RenderConstants.h"

REGISTER_RENDER_PASS(FBloomPass)

namespace
{
	constexpr uint32 BloomSourceSlot = 0;
	constexpr uint32 BloomSRVSlotCount = 1 + EBloom::MaxMipCount;

	struct FBloomPrefilterCB
	{
		float Threshold;
		float SoftKnee;
		float SourceTexelSize[2];
	};

	struct FBloomDownsampleCB
	{
		float SourceTexelSize[2];
		float Pad[2];
	};

	struct FBloomBlurCB
	{
		float SourceTexelSize[2];
		float Direction[2];
		float Radius;
		float Pad[3];
	};

	struct FBloomCompositeCB
	{
		float Intensity;
		float Pad[3];
	};

	float ClampMin(float Value, float MinValue)
	{
		return Value < MinValue ? MinValue : Value;
	}

	float ClampRange(float Value, float MinValue, float MaxValue)
	{
		if (Value < MinValue) return MinValue;
		if (Value > MaxValue) return MaxValue;
		return Value;
	}

	void SetViewport(ID3D11DeviceContext* DC, uint32 Width, uint32 Height)
	{
		D3D11_VIEWPORT Viewport = {};
		Viewport.TopLeftX = 0.0f;
		Viewport.TopLeftY = 0.0f;
		Viewport.Width = static_cast<float>(Width);
		Viewport.Height = static_cast<float>(Height);
		Viewport.MinDepth = 0.0f;
		Viewport.MaxDepth = 1.0f;
		DC->RSSetViewports(1, &Viewport);
	}

	void UnbindBloomSRVs(ID3D11DeviceContext* DC)
	{
		ID3D11ShaderResourceView* NullSRVs[BloomSRVSlotCount] = {};
		DC->VSSetShaderResources(BloomSourceSlot, BloomSRVSlotCount, NullSRVs);
		DC->PSSetShaderResources(BloomSourceSlot, BloomSRVSlotCount, NullSRVs);
	}

	void BindBloomCB(ID3D11DeviceContext* DC, FConstantBuffer& Buffer)
	{
		ID3D11Buffer* RawCB = Buffer.GetBuffer();
		DC->VSSetConstantBuffers(ECBSlot::PerShader0, 1, &RawCB);
		DC->PSSetConstantBuffers(ECBSlot::PerShader0, 1, &RawCB);
	}

	void DrawFullscreen(ID3D11DeviceContext* DC, FShader* Shader)
	{
		Shader->Bind(DC);
		DC->Draw(3, 0);
	}

	void RenderSingleSourcePass(
		ID3D11DeviceContext* DC,
		FShader* Shader,
		FConstantBuffer& ConstantBuffer,
		const void* CBData,
		uint32 CBSize,
		const FBloomMipResource& Target,
		ID3D11ShaderResourceView* SourceSRV)
	{
		UnbindBloomSRVs(DC);

		ID3D11RenderTargetView* TargetRTV = Target.RTV;
		DC->OMSetRenderTargets(1, &TargetRTV, nullptr);
		SetViewport(DC, Target.Width, Target.Height);

		ConstantBuffer.Update(DC, CBData, CBSize);
		BindBloomCB(DC, ConstantBuffer);
		DC->PSSetShaderResources(BloomSourceSlot, 1, &SourceSRV);
		DrawFullscreen(DC, Shader);
	}
}

FBloomPass::FBloomPass()
{
	PassType = ERenderPass::Bloom;
	RenderState = { EDepthStencilState::NoDepth, EBlendState::Opaque,
	                ERasterizerState::SolidNoCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, false };
}

bool FBloomPass::BeginPass(const FPassContext& Ctx)
{
	const FFrameContext& Frame = Ctx.Frame;
	if (!Frame.RenderOptions.ShowFlags.bBloom)
	{
		return false;
	}

	if (!Frame.BloomResources || !Frame.BloomResources->IsValid())
	{
		return false;
	}

	if (!Frame.SceneColorCopyTexture || !Frame.SceneColorCopySRV ||
		!Frame.ViewportRenderTexture || !Frame.ViewportRTV)
	{
		return false;
	}

	if (Frame.ViewportWidth <= 0.0f || Frame.ViewportHeight <= 0.0f)
	{
		return false;
	}

	if (!BloomCB.GetBuffer())
	{
		BloomCB.Create(Ctx.Device.GetDevice(), sizeof(FBloomBlurCB), "BloomCB");
	}

	if (!BloomCB.GetBuffer())
	{
		return false;
	}

	ID3D11DeviceContext* DC = Ctx.Device.GetDeviceContext();

	UnbindBloomSRVs(DC);
	DC->OMSetRenderTargets(0, nullptr, nullptr);
	DC->CopyResource(Frame.SceneColorCopyTexture, Frame.ViewportRenderTexture);

	Ctx.Resources.SetDepthStencilState(Ctx.Device, EDepthStencilState::NoDepth);
	Ctx.Resources.SetBlendState(Ctx.Device, EBlendState::Opaque);
	Ctx.Resources.SetRasterizerState(Ctx.Device, ERasterizerState::SolidNoCull);

	ID3D11Buffer* NullVB = nullptr;
	uint32 Zero = 0;
	DC->IASetVertexBuffers(0, 1, &NullVB, &Zero, &Zero);
	DC->IASetIndexBuffer(nullptr, DXGI_FORMAT_R32_UINT, 0);
	DC->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	Ctx.Cache.bForceAll = true;
	return true;
}

void FBloomPass::Execute(const FPassContext& Ctx)
{
	const FFrameContext& Frame = Ctx.Frame;
	const FBloomFrameResources& BloomResources = *Frame.BloomResources;
	const FViewportRenderOptions& Options = Frame.RenderOptions;

	FShader* PrefilterShader = FShaderManager::Get().GetOrCreate(EShaderPath::BloomPrefilter);
	FShader* DownsampleShader = FShaderManager::Get().GetOrCreate(EShaderPath::BloomDownsample);
	FShader* BlurShader = FShaderManager::Get().GetOrCreate(EShaderPath::BloomBlur);
	FShader* CompositeShader = FShaderManager::Get().GetOrCreate(EShaderPath::BloomComposite);

	if (!PrefilterShader || !PrefilterShader->IsValid() ||
		!DownsampleShader || !DownsampleShader->IsValid() ||
		!BlurShader || !BlurShader->IsValid() ||
		!CompositeShader || !CompositeShader->IsValid())
	{
		return;
	}

	ID3D11DeviceContext* DC = Ctx.Device.GetDeviceContext();

	FBloomPrefilterCB PrefilterData = {};
	PrefilterData.Threshold = ClampMin(Options.BloomThreshold, 0.0f);
	PrefilterData.SoftKnee = ClampRange(Options.BloomSoftKnee, 0.0f, 1.0f);
	PrefilterData.SourceTexelSize[0] = 1.0f / Frame.ViewportWidth;
	PrefilterData.SourceTexelSize[1] = 1.0f / Frame.ViewportHeight;
	RenderSingleSourcePass(
		DC,
		PrefilterShader,
		BloomCB,
		&PrefilterData,
		sizeof(PrefilterData),
		BloomResources.Mips[0],
		Frame.SceneColorCopySRV);

	for (uint32 MipIndex = 1; MipIndex < EBloom::MaxMipCount; ++MipIndex)
	{
		const FBloomMipResource& Source = BloomResources.Mips[MipIndex - 1];
		const FBloomMipResource& Target = BloomResources.Mips[MipIndex];

		FBloomDownsampleCB DownsampleData = {};
		DownsampleData.SourceTexelSize[0] = 1.0f / static_cast<float>(Source.Width);
		DownsampleData.SourceTexelSize[1] = 1.0f / static_cast<float>(Source.Height);
		RenderSingleSourcePass(
			DC,
			DownsampleShader,
			BloomCB,
			&DownsampleData,
			sizeof(DownsampleData),
			Target,
			Source.SRV);
	}

	const float BlurRadius = ClampMin(Options.BloomBlurRadius, 0.001f);
	for (uint32 MipIndex = 0; MipIndex < EBloom::MaxMipCount; ++MipIndex)
	{
		const FBloomMipResource& Mip = BloomResources.Mips[MipIndex];
		const FBloomMipResource& TempMip = BloomResources.TempMips[MipIndex];

		FBloomBlurCB BlurData = {};
		BlurData.SourceTexelSize[0] = 1.0f / static_cast<float>(Mip.Width);
		BlurData.SourceTexelSize[1] = 1.0f / static_cast<float>(Mip.Height);
		BlurData.Direction[0] = 1.0f;
		BlurData.Direction[1] = 0.0f;
		BlurData.Radius = BlurRadius;
		RenderSingleSourcePass(
			DC,
			BlurShader,
			BloomCB,
			&BlurData,
			sizeof(BlurData),
			TempMip,
			Mip.SRV);

		BlurData.Direction[0] = 0.0f;
		BlurData.Direction[1] = 1.0f;
		RenderSingleSourcePass(
			DC,
			BlurShader,
			BloomCB,
			&BlurData,
			sizeof(BlurData),
			Mip,
			TempMip.SRV);
	}

	UnbindBloomSRVs(DC);

	ID3D11RenderTargetView* ViewportRTV = Frame.ViewportRTV;
	DC->OMSetRenderTargets(1, &ViewportRTV, Frame.ViewportDSV);
	SetViewport(DC, static_cast<uint32>(Frame.ViewportWidth), static_cast<uint32>(Frame.ViewportHeight));

	FBloomCompositeCB CompositeData = {};
	CompositeData.Intensity = ClampMin(Options.BloomIntensity, 0.0f);
	BloomCB.Update(DC, &CompositeData, sizeof(CompositeData));
	BindBloomCB(DC, BloomCB);

	ID3D11ShaderResourceView* CompositeSRVs[BloomSRVSlotCount] = {
		Frame.SceneColorCopySRV,
		BloomResources.Mips[0].SRV,
		BloomResources.Mips[1].SRV,
		BloomResources.Mips[2].SRV,
		BloomResources.Mips[3].SRV,
		BloomResources.Mips[4].SRV
	};
	DC->PSSetShaderResources(BloomSourceSlot, BloomSRVSlotCount, CompositeSRVs);
	DrawFullscreen(DC, CompositeShader);
}

void FBloomPass::EndPass(const FPassContext& Ctx)
{
	ID3D11DeviceContext* DC = Ctx.Device.GetDeviceContext();
	UnbindBloomSRVs(DC);

	ID3D11RenderTargetView* ViewportRTV = Ctx.Frame.ViewportRTV;
	DC->OMSetRenderTargets(1, &ViewportRTV, Ctx.Frame.ViewportDSV);
	SetViewport(DC, static_cast<uint32>(Ctx.Frame.ViewportWidth), static_cast<uint32>(Ctx.Frame.ViewportHeight));

	Ctx.Cache.bForceAll = true;
}
