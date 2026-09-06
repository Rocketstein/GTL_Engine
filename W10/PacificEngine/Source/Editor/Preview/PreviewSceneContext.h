#pragma once

#include "Component/CameraComponent.h"
#include "GameFramework/World.h"

class AActor;
class UEditorEngine;
class UStaticMeshComponent;
class USkeletalMeshComponent;
class UGizmoComponent;

// preview 전용 world와 공통 preview object 생성을 관리하는 wrapper
class FPreviewSceneContext
{
public:
    void Initialize(UEditorEngine* InEditorEngine, const FName& InContextName = FName("Preview"));
    void Release();
    void Tick(float DeltaTime);

    UWorld* GetWorld() const { return PreviewWorld; }
    FScene* GetScene() const { return PreviewWorld ? &PreviewWorld->GetScene() : nullptr; }

	UCameraComponent* GetCamera() { return Camera; }

    AActor* CreatePreviewActor(const FName& Name);
    USkeletalMeshComponent* CreateSkeletalMeshPreview();
    UStaticMeshComponent* CreateStaticMeshPreview();
    UGizmoComponent* CreateGizmo(AActor*& OutTargetActor);

private:
    void CreateDefaultLights();
    void CreateDefaultCamera();

    UEditorEngine* EditorEngine = nullptr;
    FName ContextName = FName::None;
    UCameraComponent* Camera = nullptr;

    UWorld* PreviewWorld = nullptr;
};
