#pragma once
#include "ApplicationCore/InputEvent.h"
#include "Core/Platform/PlatformTypes.h"

class FViewportClient;

class FViewport
{
  public:
    bool             OnKeyDown(EKey Key, bool bIsRepeat);
    bool             OnKeyUp(EKey Key);
    bool             OnMouseDown(EKey Button, int32 X, int32 Y);
    bool             OnMouseUp(EKey Button, int32 X, int32 Y);
    bool             OnMouseDoubleClick(EKey Button, int32 X, int32 Y);
    bool             OnMouseMove(int32 X, int32 Y);
    bool             OnRawMouseMove(int32 DeltaX, int32 DeltaY);
    bool             OnMouseWheel(float Delta, int32 X, int32 Y);
    bool             OnSizeChanged(int32 Width, int32 Height);
    void             SetViewportClient(FViewportClient *InViewportClient);
    FViewportClient *GetViewportClient();
    void             OnFocusLost();

  private:
    FViewportClient *ViewportClient = nullptr;
};
