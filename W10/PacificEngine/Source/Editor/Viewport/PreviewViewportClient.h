#pragma once

#include "Viewport/ViewportClient.h"
#include "Core/CoreGlobals.h"
#include "Core/EngineTypes.h"
#include "Math/Rotator.h"
#include "Math/Vector.h"
#include "Render/Execute/Context/Scene/ViewTypes.h"
#include "UI/SWindow.h"
#include <d3d11.h>

class FPreviewSceneContext;
class FRenderer;
class FScene;
class FViewport;
struct FRenderCollectContext;
class UCameraComponent;
class UEditorEngine;
class UGizmoComponent;
class UWorld;

class FPreviewViewportClient : public FViewportClient
{
public:
    FPreviewViewportClient();
    ~FPreviewViewportClient() override;

    void Initialize(
        UEditorEngine* InEditorEngine,
        ID3D11Device* InDevice,
        FPreviewSceneContext* InPreviewContext);
    virtual void Release();
    void Tick(float DeltaTime) override;

    void ResetCamera();
    void FocusOnBounds(const FBoundingBox& Bounds, float MaxDistance = 25.0f);

	void SetViewportRect(float X, float Y, float Width, float Height);
    bool GetViewportRect(FRect& OutRect) const;

    FViewport* GetViewport() const override { return Viewport; }
    UCameraComponent* GetCamera() const override;
    virtual UGizmoComponent* GetGizmo();
    UWorld* GetWorld() const;

	void RenderViewportImage();

    void Draw(FViewport* Viewport, float DeltaTime) override;
    void BeginInputFrame() override;
    bool InputKey(const FViewportKeyEvent& Event) override;
    bool InputAxis(const FViewportAxisEvent& Event) override;
    bool InputPointer(const FViewportPointerEvent& Event) override;
    void ResetInputState() override;
    void ResetKeyboardInputState() override;

protected:
    virtual EViewMode GetViewMode() const;
    virtual void BeforeCollect(FRenderCollectContext& CollectContext, FScene& Scene);
    virtual void CollectPreviewWorld(FRenderer& Renderer, UWorld* World, FRenderCollectContext& CollectContext);
    virtual void CollectPreviewOverlays(FRenderer& Renderer, FScene& Scene);

private:
    void ApplyOrbitToCamera();

    UEditorEngine* EditorEngine = nullptr;
    FPreviewSceneContext* PreviewContext = nullptr;

    FViewport* Viewport = nullptr;
    float ViewportX = 0;
    float ViewportY = 0;
    float ViewportWidth = 256;
    float ViewportHeight = 256;

    FVector OrbitPivot = FVector(0.0f, 0.0f, 0.0f);
    float OrbitDistance = 8.0f;
    float OrbitYaw = -135.0f;
    float OrbitPitch = 23.0f;
};
