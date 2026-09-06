#include "FrameContext.h"
#include "Component/CameraComponent.h"
#include "Viewport/Viewport.h"

// Function : Populate frame camera data from active camera component
// input : Camera
// Camera : camera component used when PlayerCameraManager cache is not available
void FFrameContext::SetCameraInfo(const UCameraComponent* Camera)
{
	if (!Camera)
	{
		return;
	}

	SetCameraInfo(Camera->GetCameraState());
}

// Function : Populate frame camera data from final PlayerCameraManager POV
// input : POV
// POV : final camera view after CalcCamera and camera modifiers
void FFrameContext::SetCameraInfo(const FMinimalViewInfo& POV)
{
	View = FMatrix::MakeViewMatrix(
		POV.Rotation.GetRightVector(),
		POV.Rotation.GetUpVector(),
		POV.Rotation.GetForwardVector(),
		POV.Location);

	// Override aspect ratio if letterboxing is active
	const float ProjectionAspect = POV.bConstrainAspectRatio
		? POV.LetterBoxingAspectW / POV.LetterBoxingAspectH
		: POV.AspectRatio;

	if (!POV.bIsOrthogonal)
	{
		Proj = FMatrix::PerspectiveFovLH(POV.FOV, ProjectionAspect, POV.NearZ, POV.FarZ);
	}
	else
	{
		// Override aspect ratio if letterboxing is active
		const float HalfW = POV.OrthoWidth * 0.5f;
		const float HalfH = HalfW / ProjectionAspect;
		Proj = FMatrix::OrthoLH(HalfW * 2.0f, HalfH * 2.0f, POV.NearZ, POV.FarZ);
	}

	CameraPosition = POV.Location;
	CameraForward = POV.Rotation.GetForwardVector();
	CameraRight = POV.Rotation.GetRightVector();
	CameraUp = POV.Rotation.GetUpVector();
	bIsOrtho = POV.bIsOrthogonal;
	OrthoWidth = POV.OrthoWidth;
	NearClip = POV.NearZ;
	FarClip = POV.FarZ;
	PostProcessSettings = POV.PostProcessSettings;

	FrustumVolume.UpdateFromMatrix(View * Proj);
}

void FFrameContext::ApplyConstrainedAR(float TargetAspect) {
	ViewRectX		= 0.0f;
	ViewRectY		= 0.0f;
	ViewRectWidth	= ViewportWidth;
	ViewRectHeight	= ViewportHeight;

	if (ViewportWidth <= 0.0f || ViewportHeight <= 0.0f || TargetAspect <= 0.0f)
	{
		return;
	}

	const float CurrentAspect = ViewportWidth / ViewportHeight;

	if (CurrentAspect > TargetAspect)
	{
		ViewRectWidth = ViewportHeight * TargetAspect;
		ViewRectX = (ViewportWidth - ViewRectWidth) * 0.5f;
	}
	else
	{
		ViewRectHeight = ViewportWidth / TargetAspect;
		ViewRectY = (ViewportHeight - ViewRectHeight) * 0.5f;
	}
}

void FFrameContext::SetViewportInfo(const FViewport* VP)
{
	ViewportWidth    = static_cast<float>(VP->GetWidth());
	ViewportHeight   = static_cast<float>(VP->GetHeight());
	ViewRectX        = 0.0f;
	ViewRectY        = 0.0f;
	ViewRectWidth    = ViewportWidth;
	ViewRectHeight   = ViewportHeight;
	ViewportRTV             = VP->GetRTV();
	ViewportDSV             = VP->GetDSV();
	SceneColorCopySRV       = VP->GetSceneColorCopySRV();
	SceneColorCopyTexture   = VP->GetSceneColorCopyTexture();
	ViewportRenderTexture   = VP->GetRTTexture();
	DepthTexture            = VP->GetDepthTexture();
	DepthCopyTexture        = VP->GetDepthCopyTexture();
	DepthCopySRV            = VP->GetDepthCopySRV();
	StencilCopySRV          = VP->GetStencilCopySRV();
	NormalRTV               = VP->GetNormalRTV();
	NormalSRV               = VP->GetNormalSRV();
	CullingHeatmapRTV       = VP->GetCullingHeatmapRTV();
	CullingHeatmapSRV       = VP->GetCullingHeatmapSRV();
}
