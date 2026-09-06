#include "Viewport/Viewport.h"
#include "Renderer/D3D11/D3D11RendererModule.h"
#include "RendererModule.h"
#include "ThirdParty/ImGui/imgui.h"
#include "Viewport/ViewportClient.h"

bool FViewport::OnKeyDown(EKey Key, bool bIsRepeat)
{
    return ViewportClient ? ViewportClient->InputKey(Key, bIsRepeat ? EInputEvent::Repeat
                                                                    : EInputEvent::Pressed)
                          : false;
}

bool FViewport::OnKeyUp(EKey Key)
{
    return ViewportClient ? ViewportClient->InputKey(Key, EInputEvent::Released) : false;
}

bool FViewport::OnMouseDown(EKey Button, int32 X, int32 Y)
{
    if (!ViewportClient)
        return false;
    ViewportClient->MouseMove(X, Y);
    return ViewportClient->InputKey(Button, EInputEvent::Pressed);
}

bool FViewport::OnMouseUp(EKey Button, int32 X, int32 Y)
{
    if (!ViewportClient)
        return false;
    ViewportClient->MouseMove(X, Y);
    const bool Handled = ViewportClient->InputKey(Button, EInputEvent::Released);
    if (Button == EKey::LeftMouseButton &&
        !(ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantCaptureMouse) &&
        ViewportClient->GetReleaseType(X, Y) == EPointerReleaseType::Click)
    {
        ViewportClient->ProcessClick(X, Y);
    }
    return Handled;
}

bool FViewport::OnMouseDoubleClick(EKey Button, int32 X, int32 Y)
{
    if (!ViewportClient)
        return false;
    ViewportClient->MouseMove(X, Y);
    return ViewportClient->InputKey(Button, EInputEvent::DoubleClick);
}

bool FViewport::OnMouseMove(int32 X, int32 Y)
{
    return ViewportClient ? ViewportClient->MouseMove(X, Y) : false;
}
bool FViewport::OnRawMouseMove(int32 DeltaX, int32 DeltaY)
{
    return ViewportClient ? ViewportClient->CapturedMouseMove(DeltaX, DeltaY) : false;
}
bool FViewport::OnMouseWheel(float Delta, int32 X, int32 Y)
{
    if (!ViewportClient)
        return false;
    ViewportClient->MouseMove(X, Y);
    return ViewportClient->InputAxis(EKey::MouseWheelAxis, Delta);
}
bool FViewport::OnSizeChanged(int32 Width, int32 Height)
{
    if (GRenderer)
    {
        GRenderer->OnWindowResized(Width, Height);
        return true;
    }
    return false;
}
void FViewport::SetViewportClient(FViewportClient *InViewportClient)
{
    ViewportClient = InViewportClient;
}
FViewportClient *FViewport::GetViewportClient() { return ViewportClient; }
void             FViewport::OnFocusLost()
{
    if (ViewportClient)
        ViewportClient->ResetInputState();
}
