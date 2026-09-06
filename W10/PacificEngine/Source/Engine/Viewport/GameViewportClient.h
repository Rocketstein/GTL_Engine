// 뷰포트 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "Core/Logging/LogMacros.h"
#include "Engine/Runtime/Engine.h"
#include "Math/Rotator.h"
#include "Math/Vector.h"
#include "Object/Object.h"
#include "Viewport/ViewportClient.h"
#include "Input/GameViewportInputController.h"

class FViewport;
class FOverlayStatSystem;
class UUIComponent;
class UCameraShakeBase;
class APlayerCameraManager;
struct FViewportPointerEvent;

// UGameViewportClient는 게임 월드와 실제 뷰포트 출력을 연결합니다.
class UGameViewportClient : public UObject, public FViewportClient
{
public:
    DECLARE_CLASS(UGameViewportClient, UObject)

    UGameViewportClient();
    ~UGameViewportClient() override;

    // FViewportClient 인터페이스입니다.
    void Draw(FViewport* Viewport, float DeltaTime) override {}
    void BeginInputFrame() override;
    void Tick(float DeltaTime) override;

    class UCameraComponent* GetCamera() const override;

    bool InputKey(const FViewportKeyEvent& Event) override
    {
        return InputController && InputController->InputKey(Event);
    }

    bool InputAxis(const FViewportAxisEvent& Event) override
    {
        return InputController && InputController->InputAxis(Event);
    }

    bool InputPointer(const FViewportPointerEvent& Event) override
    {
        if (RouteUIPointerEvent(Event))
        {
            return true;
        }

        return InputController && InputController->InputPointer(Event);
    }

    void ResetInputState() override
    {
        if (InputController)
        {
            InputController->BeginInputFrame();
        }
    }

    // 렌더링 대상 뷰포트를 설정합니다.
    void SetViewport(FViewport* InViewport) { Viewport = InViewport; }
    FViewport* GetViewport() const override { return Viewport; }

    void SetFallbackCamera(class UCameraComponent* InCamera) { FallbackCamera = InCamera; }
    void SetOverlayStatSystem(FOverlayStatSystem* InOverlayStatSystem) { OverlayStatSystem = InOverlayStatSystem; }
    UCameraShakeBase* StartCameraShakeFromAsset(const FString& Path, float Scale = 1.0f);
    void UpdateCameraShakes(float DeltaTime);
    void ResetCameraShakes();

private:
    bool RouteUIPointerEvent(const FViewportPointerEvent& Event);
    UUIComponent* FindTopmostUIComponentAt(const FVector2& ScreenPoint, float ViewportWidth, float ViewportHeight) const;
    bool IsUIComponentRegistered(const UUIComponent* Component) const;
    void UpdateUIHover(UUIComponent* NewHovered, const FViewportPointerEvent& Event);
    APlayerCameraManager* GetOrCreateCameraManager();
    void ClearCameraShakeOffsets();

private:
    FViewport* Viewport = nullptr;
    std::unique_ptr<FGameViewportInputController> InputController;

    class UCameraComponent* FallbackCamera = nullptr;
    FOverlayStatSystem* OverlayStatSystem = nullptr;
    UUIComponent* HoveredUIComponent = nullptr;
    UUIComponent* CapturedUIComponent = nullptr;

    APlayerCameraManager* CameraManager = nullptr;
    class UCameraComponent* ShakeCamera = nullptr;
    class UWorld* ShakeCameraWorld = nullptr;
    FVector AppliedShakeLocation = FVector::ZeroVector;
    FRotator AppliedShakeRotation = FRotator::ZeroRotator;
    float AppliedShakeFOVDegrees = 0.0f;
};
