#pragma once

#include "Core/CoreTypes.h"
#include "Camera/MinimalViewInfo.h"
#include "Math/Matrix.h"
#include "Math/Vector.h"
#include "Render/Types/ViewTypes.h"
#include "Render/Types/LODContext.h"
#include "Collision/ConvexVolume.h"

#include <d3d11.h>

class UCameraComponent;
class FViewport;
class FGPUOcclusionCulling;

/*
	FFrameContext - per-frame/per-viewport read-only state.
	Camera, viewport, render settings, occlusion, LOD context.
	Populated once per frame by the render pipeline, then read by
	Renderer, Proxies, and RenderCollector.
*/
struct FFrameContext
{
	FMatrix View;
	FMatrix Proj;
	FVector CameraPosition;
	FVector CameraForward;
	FVector CameraRight;
	FVector CameraUp;
	float NearClip = 0.1f;
	float FarClip = 1000.0f;
	FPostProcessSettings PostProcessSettings;

	uint8 bIsOrtho : 1;
	uint8 bIsLightView : 1;
	float OrthoWidth = 10.0f;

	// Viewport
	float ViewRectX = 0.0f;
	float ViewRectY = 0.0f;
	float ViewRectWidth = 0.0f;
	float ViewRectHeight = 0.0f;
	float ViewportWidth  = 0.0f;
	float ViewportHeight = 0.0f;

	ID3D11RenderTargetView* ViewportRTV = nullptr;
	ID3D11DepthStencilView* ViewportDSV = nullptr;
	ID3D11ShaderResourceView* SceneColorCopySRV = nullptr;
	ID3D11Texture2D* SceneColorCopyTexture = nullptr;
	ID3D11Texture2D* ViewportRenderTexture = nullptr;

	ID3D11Texture2D* DepthTexture = nullptr;
	ID3D11Texture2D* DepthCopyTexture = nullptr;
	ID3D11ShaderResourceView* DepthCopySRV = nullptr;
	ID3D11ShaderResourceView* StencilCopySRV = nullptr;

	ID3D11RenderTargetView* NormalRTV = nullptr;
	ID3D11ShaderResourceView* NormalSRV = nullptr;

	ID3D11RenderTargetView* CullingHeatmapRTV = nullptr;
	ID3D11ShaderResourceView* CullingHeatmapSRV = nullptr;

	uint32 CursorViewportX = UINT32_MAX;
	uint32 CursorViewportY = UINT32_MAX;

	FViewportRenderOptions RenderOptions;

	FVector WireframeColor = FVector(0.0f, 0.0f, 0.7f);

	FGPUOcclusionCulling* OcclusionCulling = nullptr;
	FConvexVolume FrustumVolume;
	FLODUpdateContext LODContext;

	bool IsFixedOrtho() const
	{
		return bIsOrtho
			&& RenderOptions.ViewportType != ELevelViewportType::Perspective
			&& RenderOptions.ViewportType != ELevelViewportType::FreeOrthographic;
	}

	void SetCameraInfo(const UCameraComponent* Camera);
	void SetCameraInfo(const FMinimalViewInfo& POV);
	void SetViewportInfo(const FViewport* VP);
	void ApplyConstrainedAR(float TargetAspect);

	void SetViewportSize(float InWidth, float InHeight)
	{
		ViewportWidth = InWidth;
		ViewportHeight = InHeight;
	}

	void SetRenderOptions(const FViewportRenderOptions& InOptions)
	{
		RenderOptions = InOptions;
	}

	void ClearViewportResources()
	{
		ViewportRTV = nullptr;
		ViewportDSV = nullptr;
		SceneColorCopySRV = nullptr;
		SceneColorCopyTexture = nullptr;
		ViewportRenderTexture = nullptr;
		DepthTexture = nullptr;
		DepthCopyTexture = nullptr;
		DepthCopySRV = nullptr;
		StencilCopySRV = nullptr;
		NormalRTV = nullptr;
		NormalSRV = nullptr;
		CullingHeatmapRTV = nullptr;
		CullingHeatmapSRV = nullptr;
	}
};
