// 뷰포트 영역의 세부 동작을 구현합니다.
#include "Viewport/GameViewportClient.h"
#include "Input/GameViewportInputController.h"
#include "Component/CameraComponent.h"
#include "Core/RayTypes.h"
#include "Editor/Subsystem/OverlayStatSystem.h"
#include "Engine/Classes/Camera/CameraManager.h"
#include "Engine/Classes/Camera/CameraModifier_CameraShake.h"
#include "GameFramework/World.h"
#include "Math/MathUtils.h"
#include "Render/Scene/Proxies/UI/UIProxy.h"
#include "Render/Scene/Scene.h"
#include "UI/UIComponent.h"
#include "Viewport/Viewport.h"

#include <algorithm>

DEFINE_CLASS(UGameViewportClient, UObject)

namespace
{
FMinimalViewInfo BuildPOVFromCamera(UCameraComponent* Camera)
{
    FMinimalViewInfo POV;
    if (!Camera)
    {
        return POV;
    }

    POV.Location = Camera->GetWorldLocation();
    POV.Rotation = Camera->GetRelativeRotation();
    POV.FOV = Camera->GetFOV() * RAD_TO_DEG;
    POV.AspectRatio = Camera->GetAspectRatio();
    POV.NearZ = Camera->GetNearPlane();
    POV.FarZ = Camera->GetFarPlane();
    POV.bOrthographic = Camera->IsOrthogonal();
    POV.OrthoWidth = Camera->GetOrthoWidth();
    return POV;
}

void ApplyPOVToCamera(UCameraComponent* Camera, const FMinimalViewInfo& POV)
{
    if (!Camera)
    {
        return;
    }

    Camera->SetWorldLocation(POV.Location);
    Camera->SetRelativeRotation(POV.Rotation);

    FCameraState CameraState = Camera->GetCameraState();
    CameraState.FOV = POV.FOV * DEG_TO_RAD;
    Camera->SetCameraState(CameraState);
}

bool IsLiveWorld(const UWorld* World)
{
    if (!World || !GEngine)
    {
        return false;
    }

    for (const FWorldContext& Context : GEngine->GetWorldList())
    {
        if (Context.World == World)
        {
            return true;
        }
    }

    return false;
}
} // namespace

UGameViewportClient::UGameViewportClient()
{
    InputController = std::make_unique<FGameViewportInputController>(this);
}

UGameViewportClient::~UGameViewportClient()
{
    ResetCameraShakes();
}

void UGameViewportClient::BeginInputFrame()
{
    ClearCameraShakeOffsets();

    if (InputController)
    {
        InputController->BeginInputFrame();
    }
}

void UGameViewportClient::Tick(float DeltaTime)
{
    ClearCameraShakeOffsets();

    if (InputController)
    {
        InputController->HandleInput(DeltaTime);
    }

    if (OverlayStatSystem)
    {
        bool bNoCamera = (UCameraComponent::Main == nullptr);
        OverlayStatSystem->ShowNoCameraWarning(bNoCamera);
        if (bNoCamera)
        {
             // UE_LOG(GameViewportClient, Warning, "No Camera::Main found during Tick!");
        }
    }
}

UCameraComponent* UGameViewportClient::GetCamera() const
{
    if (UCameraComponent::Main)
    {
        return UCameraComponent::Main;
    }

    UWorld* World = GEngine ? GEngine->GetWorld() : nullptr;
    if (World && World->GetActiveCamera())
    {
        return World->GetActiveCamera();
    }

    return FallbackCamera;
}

UCameraShakeBase* UGameViewportClient::StartCameraShakeFromAsset(const FString& Path, float Scale)
{
    UWorld* World = GEngine ? GEngine->GetWorld() : nullptr;
    APlayerCameraManager* Manager = GEngine ? GEngine->GetPlayerCameraManager(World) : nullptr;
    if (!Manager)
    {
        Manager = GetOrCreateCameraManager();
    }

    if (!Manager)
    {
        return nullptr;
    }

    FAddCameraShakeParams Params;
    Params.Scale = Scale;
    Params.PlaySpace = ECameraShakePlaySpace::CameraLocal;
    return Manager->StartCameraShakeFromAsset(Path, Params);
}

void UGameViewportClient::UpdateCameraShakes(float DeltaTime)
{
    ClearCameraShakeOffsets();

    if (!CameraManager)
    {
        return;
    }

    UCameraComponent* Camera = GetCamera();
    if (!Camera)
    {
        return;
    }

    FMinimalViewInfo BasePOV = BuildPOVFromCamera(Camera);
    FMinimalViewInfo ShakenPOV = BasePOV;
    CameraManager->ApplyCameraModifiersToPOV(DeltaTime, ShakenPOV);

    AppliedShakeLocation = ShakenPOV.Location - BasePOV.Location;
    AppliedShakeRotation = ShakenPOV.Rotation - BasePOV.Rotation;
    AppliedShakeFOVDegrees = ShakenPOV.FOV - BasePOV.FOV;
    ShakeCamera = Camera;
    ShakeCameraWorld = Camera->GetWorld();

    ApplyPOVToCamera(Camera, ShakenPOV);
}

APlayerCameraManager* UGameViewportClient::GetOrCreateCameraManager()
{
    if (!CameraManager)
    {
        CameraManager = UObjectManager::Get().CreateObject<APlayerCameraManager>(this);
    }

    return CameraManager;
}

void UGameViewportClient::ResetCameraShakes()
{
    ClearCameraShakeOffsets();

    if (CameraManager)
    {
        UObjectManager::Get().DestroyObject(CameraManager);
        CameraManager = nullptr;
    }

    ShakeCamera = nullptr;
    ShakeCameraWorld = nullptr;
    AppliedShakeLocation = FVector::ZeroVector;
    AppliedShakeRotation = FRotator::ZeroRotator;
    AppliedShakeFOVDegrees = 0.0f;
}

void UGameViewportClient::ClearCameraShakeOffsets()
{
    if (!ShakeCamera)
    {
        ShakeCameraWorld = nullptr;
        AppliedShakeLocation = FVector::ZeroVector;
        AppliedShakeRotation = FRotator::ZeroRotator;
        AppliedShakeFOVDegrees = 0.0f;
        return;
    }

    const bool bCameraWorldIsLive = !ShakeCameraWorld || IsLiveWorld(ShakeCameraWorld);
    if (bCameraWorldIsLive)
    {
        FMinimalViewInfo RestoredPOV = BuildPOVFromCamera(ShakeCamera);
        RestoredPOV.Location -= AppliedShakeLocation;
        RestoredPOV.Rotation -= AppliedShakeRotation;
        RestoredPOV.FOV -= AppliedShakeFOVDegrees;
        ApplyPOVToCamera(ShakeCamera, RestoredPOV);
    }

    ShakeCamera = nullptr;
    ShakeCameraWorld = nullptr;
    AppliedShakeLocation = FVector::ZeroVector;
    AppliedShakeRotation = FRotator::ZeroRotator;
    AppliedShakeFOVDegrees = 0.0f;
}

bool UGameViewportClient::RouteUIPointerEvent(const FViewportPointerEvent& Event)
{
    UWorld* World = GEngine ? GEngine->GetWorld() : nullptr;
    if (!World)
    {
        HoveredUIComponent = nullptr;
        CapturedUIComponent = nullptr;
        return false;
    }

    FScene& Scene = World->GetScene();
    Scene.UpdateDirtyUIProxies();

    if (HoveredUIComponent && !IsUIComponentRegistered(HoveredUIComponent))
    {
        HoveredUIComponent = nullptr;
    }
    if (CapturedUIComponent && !IsUIComponentRegistered(CapturedUIComponent))
    {
        CapturedUIComponent = nullptr;
    }

    const FVector2 ScreenPoint(
        static_cast<float>(Event.LocalPos.x),
        static_cast<float>(Event.LocalPos.y));
    const float ViewportWidth = Viewport ? static_cast<float>(Viewport->GetWidth()) : 0.0f;
    const float ViewportHeight = Viewport ? static_cast<float>(Viewport->GetHeight()) : 0.0f;

    UUIComponent* HitComponent = FindTopmostUIComponentAt(ScreenPoint, ViewportWidth, ViewportHeight);
    UpdateUIHover(HitComponent, Event);

    UUIComponent* TargetComponent = CapturedUIComponent ? CapturedUIComponent : HitComponent;

    if (Event.Type == EPointerEventType::Pressed &&
        Event.Button != EPointerButton::None &&
        HitComponent)
    {
        CapturedUIComponent = HitComponent;
        TargetComponent = HitComponent;
    }

    bool bHandled = false;
    if (TargetComponent)
    {
        bHandled = TargetComponent->HandleUIPointerEvent(Event);
    }

    if (Event.Type == EPointerEventType::Released &&
        Event.Button != EPointerButton::None)
    {
        CapturedUIComponent = nullptr;
    }

    return bHandled || HitComponent != nullptr || TargetComponent != nullptr;
}

UUIComponent* UGameViewportClient::FindTopmostUIComponentAt(const FVector2& ScreenPoint, float ViewportWidth, float ViewportHeight) const
{
    UWorld* World = GEngine ? GEngine->GetWorld() : nullptr;
    if (!World || ViewportWidth <= 0.0f || ViewportHeight <= 0.0f)
    {
        return nullptr;
    }

    const TArray<FUIProxy*>& UIProxies = World->GetScene().GetUIProxies();
    const FUIProxy* BestProxy = nullptr;
    float BestDistance = 0.0f;

    UCameraComponent* Camera = GetCamera();
    const bool bCanHitWorldUI = Camera != nullptr;
    const FRay WorldRay = bCanHitWorldUI
        ? Camera->DeprojectScreenToWorld(ScreenPoint.X, ScreenPoint.Y, ViewportWidth, ViewportHeight)
        : FRay{};

    auto IsHigher = [](const FUIProxy* A, float ADistance, const FUIProxy* B, float BDistance)
    {
        if (!B)
        {
            return true;
        }
        if (A->RenderSpace != B->RenderSpace)
        {
            return A->RenderSpace == EUIRenderSpace::ScreenSpace;
        }
        if (A->Layer != B->Layer)
        {
            return A->Layer > B->Layer;
        }
        if (A->ZOrder != B->ZOrder)
        {
            return A->ZOrder > B->ZOrder;
        }
        if (A->RenderSpace == EUIRenderSpace::WorldSpace && ADistance != BDistance)
        {
            return ADistance < BDistance;
        }
        return A->ProxyId > B->ProxyId;
    };

    for (const FUIProxy* Proxy : UIProxies)
    {
        if (!Proxy ||
            !Proxy->Owner ||
            !Proxy->bVisible ||
            !Proxy->bHitTestVisible ||
            Proxy->GeometryType != EUIGeometryType::Quad)
        {
            continue;
        }

        float HitDistance = 0.0f;
        bool bHit = false;
        if (Proxy->RenderSpace == EUIRenderSpace::ScreenSpace)
        {
            bHit = Proxy->Owner->HitTestScreenPoint(ScreenPoint, ViewportWidth, ViewportHeight);
        }
        else if (Proxy->RenderSpace == EUIRenderSpace::WorldSpace && bCanHitWorldUI)
        {
            bHit = Proxy->Owner->HitTestWorldRay(
                WorldRay,
                Camera->GetRightVector(),
                Camera->GetUpVector(),
                Camera->GetForwardVector(),
                HitDistance);
        }

        if (bHit && IsHigher(Proxy, HitDistance, BestProxy, BestDistance))
        {
            BestProxy = Proxy;
            BestDistance = HitDistance;
        }
    }

    return BestProxy ? BestProxy->Owner : nullptr;
}

bool UGameViewportClient::IsUIComponentRegistered(const UUIComponent* Component) const
{
    if (!Component)
    {
        return false;
    }

    UWorld* World = GEngine ? GEngine->GetWorld() : nullptr;
    if (!World)
    {
        return false;
    }

    for (const FUIProxy* Proxy : World->GetScene().GetUIProxies())
    {
        if (Proxy && Proxy->Owner == Component)
        {
            return true;
        }
    }

    return false;
}

void UGameViewportClient::UpdateUIHover(UUIComponent* NewHovered, const FViewportPointerEvent& Event)
{
    if (HoveredUIComponent == NewHovered)
    {
        return;
    }

    if (HoveredUIComponent)
    {
        HoveredUIComponent->OnUIPointerLeave(Event);
    }

    HoveredUIComponent = NewHovered;

    if (HoveredUIComponent)
    {
        HoveredUIComponent->OnUIPointerEnter(Event);
    }
}
