#pragma once

#include "Render/Types/POVProvider.h"
#include "Render/Types/ViewTypes.h"

class FViewport;
class UWorld;

class IEditorPreviewViewportClient : public IPOVProvider
{
public:
	virtual ~IEditorPreviewViewportClient() = default;

	virtual bool IsRenderable() const = 0;
	virtual bool IsMouseOverViewport() const = 0;

	virtual FViewport* GetViewport() const = 0;
	virtual UWorld* GetPreviewWorld() const = 0;

	virtual FViewportRenderOptions& GetRenderOptions() = 0;
	virtual const FViewportRenderOptions& GetRenderOptions() const = 0;
	virtual bool SupportsGridRendering() const { return false; }

	virtual void NotifyViewportResized(int32 NewWidth, int32 NewHeight) = 0;
	virtual void GetClearColor(float OutClearColor[4]) const
	{
		OutClearColor[0] = 0.12f;
		OutClearColor[1] = 0.12f;
		OutClearColor[2] = 0.13f;
		OutClearColor[3] = 1.0f;
	}
};
