#include "RenderPassPipeline.h"

#include "Render/RenderPass/RenderPassRegistry.h"
#include "Render/Types/RenderTypes.h"
#include "Render/Device/D3DDevice.h"
#include "Render/Types/FrameContext.h"
#include "Profiling/Stats.h"
#include "Profiling/GPUProfiler.h"

namespace
{
	void ApplyViewportForPass(const FPassContext& Ctx, ERenderPass PassType)
	{
		ID3D11DeviceContext* DC = Ctx.Device.GetDeviceContext();
		if (!DC)
		{
			return;
		}

		D3D11_VIEWPORT Viewport{};
		Viewport.MinDepth = 0.0f;
		Viewport.MaxDepth = 1.0f;
		D3D11_RECT Scissor{};

		const bool bUseFullViewport =
			PassType == ERenderPass::ShadowMap ||
			PassType == ERenderPass::LightCulling ||
			PassType == ERenderPass::PostProcess ||
			PassType == ERenderPass::FXAA ||
			PassType == ERenderPass::UI ||
			PassType == ERenderPass::ScreenText;

		if (bUseFullViewport)
		{
			Viewport.TopLeftX = 0.0f;
			Viewport.TopLeftY = 0.0f;
			Viewport.Width = Ctx.Frame.ViewportWidth;
			Viewport.Height = Ctx.Frame.ViewportHeight;
			Scissor = { 0, 0, static_cast<LONG>(Ctx.Frame.ViewportWidth), static_cast<LONG>(Ctx.Frame.ViewportHeight) };
		}
		else
		{
			Viewport.TopLeftX = Ctx.Frame.ViewRectX;
			Viewport.TopLeftY = Ctx.Frame.ViewRectY;
			Viewport.Width = Ctx.Frame.ViewRectWidth;
			Viewport.Height = Ctx.Frame.ViewRectHeight;
			Scissor = {
				static_cast<LONG>(Ctx.Frame.ViewRectX),
				static_cast<LONG>(Ctx.Frame.ViewRectY),
				static_cast<LONG>(Ctx.Frame.ViewRectX + Ctx.Frame.ViewRectWidth),
				static_cast<LONG>(Ctx.Frame.ViewRectY + Ctx.Frame.ViewRectHeight)
			};
		}

		if (PassType == ERenderPass::ShadowMap)
		{
			Scissor = { 0, 0, 32767, 32767 };
		}

		if (Viewport.Width > 0.0f && Viewport.Height > 0.0f)
		{
			DC->RSSetViewports(1, &Viewport);
			DC->RSSetScissorRects(1, &Scissor);
		}
	}
}

void FRenderPassPipeline::Initialize()
{
	Passes = FRenderPassRegistry::Get().CreateAll();

	// 패스 객체로부터 상태 테이블 빌드
	for (const auto& Pass : Passes)
	{
		StateTable.Set(Pass->GetPassType(), Pass->GetRenderState());
	}
}

void FRenderPassPipeline::Execute(const FPassContext& Ctx)
{
	for (const auto& Pass : Passes)
	{
		const char* PassName = GetRenderPassName(Pass->GetPassType());

		bool bBegin;
		{
			SCOPE_STAT_CAT(PassName, "3_BeginPass");
			bBegin = Pass->BeginPass(Ctx);
		}
		if (!bBegin) continue;

		ApplyViewportForPass(Ctx, Pass->GetPassType());

		{
			SCOPE_STAT_CAT(PassName, "4_ExecutePass");
			GPU_SCOPE_STAT(PassName);
			Pass->Execute(Ctx);
		}

		{
			SCOPE_STAT_CAT(PassName, "5_EndPass");
			Pass->EndPass(Ctx);
		}
	}
}
