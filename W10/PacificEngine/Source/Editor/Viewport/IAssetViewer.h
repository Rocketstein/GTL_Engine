#pragma once

#include "Core/CoreGlobals.h"
#include "Input/InputTypes.h"

class FViewportClient;
struct FRect;

enum class EAssetViewerType : uint8
{
    SkeletalMesh,
    Material,
    Animation,
};

class IAssetViewer
{
public:
    virtual ~IAssetViewer() = default;

    virtual void Release() = 0;
    virtual void Tick(float DeltaTime) = 0;
    virtual void Render(float DeltaTime) = 0;

    virtual bool IsOpen() const = 0;
    virtual void SetOpen(bool bInOpen) = 0;

    virtual uint32 GetViewerId() const = 0;
    virtual EAssetViewerType GetViewerType() const = 0;

    virtual FViewportClient* GetViewportClient() = 0;
    virtual bool GetViewportRect(FRect& OutRect) const = 0;

	virtual void BeginInputFrame() {}
    virtual bool InputKey(const FViewportKeyEvent& Event) { return false; }
    virtual bool InputAxis(const FViewportAxisEvent& Event) { return false; }
    virtual bool InputPointer(const FViewportPointerEvent& Event) { return false; }
    virtual void ResetInputState() {}
    virtual void ResetKeyboardInputState() {}
};
