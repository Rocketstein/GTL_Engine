#pragma once

#include "Input/InputTypes.h"

class UGameViewportClient;
class FGameViewportInputController;

class FGameViewportTool
{
public:
    FGameViewportTool(UGameViewportClient* InOwner, FGameViewportInputController* InController)
        : Owner(InOwner), Controller(InController)
    {
    }

    virtual ~FGameViewportTool() = default;

    virtual bool InputKey(const FViewportKeyEvent& Event) { return false; }
    virtual bool InputAxis(const FViewportAxisEvent& Event) { return false; }
    virtual bool InputPointer(const FViewportPointerEvent& Event) { return false; }

    virtual void BeginInputFrame() {}
    virtual void ResetState() {}

protected:
    UGameViewportClient* Owner = nullptr;
    FGameViewportInputController* Controller = nullptr;
};
