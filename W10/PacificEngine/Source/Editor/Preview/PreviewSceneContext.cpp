#include "PreviewSceneContext.h"
#include "Component/GizmoComponent.h"
#include "Component/SceneComponent.h"
#include "Component/SkeletalMeshComponent.h"
#include "Component/StaticMeshComponent.h"
#include "EditorEngine.h"
#include "GameFramework/AmbientLightActor.h"
#include "GameFramework/DirectionalLightActor.h"

void FPreviewSceneContext::Initialize(UEditorEngine* InEditorEngine, const FName& InContextName)
{
	EditorEngine = InEditorEngine;
    ContextName = InContextName;

    auto Context = EditorEngine->CreateWorldContext(EWorldType::Preview, ContextName);
    PreviewWorld = Context.World;
    PreviewWorld->InitWorld();

    CreateDefaultLights();
    CreateDefaultCamera();
}

void FPreviewSceneContext::Release()
{
    if (Camera)
    {
        UObjectManager::Get().DestroyObject(Camera);
    }

    if (PreviewWorld && EditorEngine)
    {
        EditorEngine->DestroyWorldContext(ContextName);
    }

    PreviewWorld = nullptr;
    Camera = nullptr;
    ContextName = FName::None;
    EditorEngine = nullptr;
}

void FPreviewSceneContext::Tick(float DeltaTime)
{
    if (!PreviewWorld)
    {
        return;
    }

    PreviewWorld->Tick(DeltaTime, ELevelTick::LEVELTICK_ViewportsOnly);
}

AActor* FPreviewSceneContext::CreatePreviewActor(const FName& Name)
{
    if (!PreviewWorld)
    {
        return nullptr;
    }

    AActor* Actor = PreviewWorld->SpawnActor<AActor>();
    if (Actor)
    {
        Actor->SetFName(Name);
    }

    return Actor;
}

USkeletalMeshComponent* FPreviewSceneContext::CreateSkeletalMeshPreview()
{
    AActor* PreviewActor = CreatePreviewActor(FName("Skeletal Mesh Preview"));
    if (!PreviewActor)
    {
        return nullptr;
    }

    USkeletalMeshComponent* MeshComponent = PreviewActor->AddComponent<USkeletalMeshComponent>();
    PreviewActor->SetRootComponent(MeshComponent);
    return MeshComponent;
}

UStaticMeshComponent* FPreviewSceneContext::CreateStaticMeshPreview()
{
    AActor* PreviewActor = CreatePreviewActor(FName("Static Mesh Preview"));
    if (!PreviewActor)
    {
        return nullptr;
    }

    UStaticMeshComponent* MeshComponent = PreviewActor->AddComponent<UStaticMeshComponent>();
    PreviewActor->SetRootComponent(MeshComponent);
    return MeshComponent;
}

UGizmoComponent* FPreviewSceneContext::CreateGizmo(AActor*& OutTargetActor)
{
    OutTargetActor = CreatePreviewActor(FName("Bone Gizmo Target"));
    if (!OutTargetActor)
    {
        return nullptr;
    }

    USceneComponent* Root = OutTargetActor->AddComponent<USceneComponent>();
    OutTargetActor->SetRootComponent(Root);

    UGizmoComponent* Gizmo = OutTargetActor->AddComponent<UGizmoComponent>();
    if (Gizmo)
    {
        Gizmo->SetWorldSpace(true);
        Gizmo->SetVisibility(false);
    }

    return Gizmo;
}

void FPreviewSceneContext::CreateDefaultLights()
{
    if (!PreviewWorld)
    {
        return;
    }

	ADirectionalLightActor* DirectionalLight = PreviewWorld->SpawnActor<ADirectionalLightActor>();
    if (DirectionalLight)
    {
        DirectionalLight->InitDefaultComponents();
        DirectionalLight->SetFName(FName("Directional Light"));
        DirectionalLight->SetActorLocation(FVector(0.0f, 0.0f, 13.0f));
        DirectionalLight->SetActorRotation(FVector(0.0f, 44.0f, 0.0f));
    }

    AAmbientLightActor* AmbientLight = PreviewWorld->SpawnActor<AAmbientLightActor>();
    if (AmbientLight)
    {
        AmbientLight->InitDefaultComponents();
        AmbientLight->SetFName(FName("Ambient Light"));
        AmbientLight->SetActorLocation(FVector(0.0f, 0.0f, 15.0f));
    }
}

void FPreviewSceneContext::CreateDefaultCamera()
{
    if (!PreviewWorld)
    {
        return;
    }

    Camera = UObjectManager::Get().CreateObject<UCameraComponent>(PreviewWorld);
    if (Camera)
    {
        Camera->OnResize(256, 256);
        PreviewWorld->SetActiveCamera(Camera);
    }
}
