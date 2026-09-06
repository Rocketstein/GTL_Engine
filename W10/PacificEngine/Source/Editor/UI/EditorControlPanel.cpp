// 에디터 영역의 세부 동작을 구현합니다.
#include "Editor/UI/EditorControlPanel.h"
#include "Editor/EditorEngine.h"
#include "Editor/Settings/EditorSettings.h"
#include "ImGui/imgui.h"
#include "Component/CameraComponent.h"
#include "Component/GizmoComponent.h"
#include "GameFramework/DecalActor.h"
#include "GameFramework/FakeLightActor.h"
#include "GameFramework/FireballActor.h"
#include "GameFramework/HeightFogActor.h"
#include "GameFramework/StaticMeshActor.h"
#include "GameFramework/AmbientLightActor.h"
#include "GameFramework/DirectionalLightActor.h"
#include "GameFramework/ExpBarActor.h"
#include "GameFramework/PointLightActor.h"
#include "GameFramework/SpotLightActor.h"
#include "GameFramework/GamejamActor/EnemySpawnerActor.h"
#include "Engine/Platform/Paths.h"
#include "Engine/GameFramework/TankActor.h"
#include "Engine/Classes/Camera/CameraManager.h"

#define SEPARATOR()     \
    ;                   \
    ImGui::Spacing();   \
    ImGui::Spacing();   \
    ImGui::Separator(); \
    ImGui::Spacing();   \
    ImGui::Spacing();

// ─── Spawn 테이블 ────────────────────────────────────────────────
// Actor 종류를 추가할 때 이 테이블에만 한 줄 추가하면 됩니다.
// ─────────────────────────────────────────────────────────────────
namespace
{
// FSpawnEntry는 에디터 처리에 필요한 데이터를 묶는 구조체입니다.
struct FSpawnEntry
{
    const char* Label;
    void (*Spawn)(UWorld* World, const FVector& SpawnPoint);
};

template <typename TActor, typename... TArgs>
void SpawnActor(UWorld* World, const FVector& SpawnPoint, bool bInsertToOctree, TArgs&&... Args)
{
    TActor* Actor = World->SpawnActor<TActor>(std::forward<TArgs>(Args)...);
    Actor->SetActorLocation(SpawnPoint);

    if (bInsertToOctree)
        World->InsertActorToOctree(Actor);
}

#define SPAWN_MESH(Label, Path) { Label, [](UWorld* W, const FVector& P) { SpawnActor<AStaticMeshActor>(W, P, true, Path); } }
#define SPAWN_ACTOR(Label, Type, bOctree) { Label, [](UWorld* W, const FVector& P) { SpawnActor<Type>(W, P, bOctree); } }

constexpr FSpawnEntry SpawnTable[] = {
    SPAWN_MESH("Cube", FPaths::ContentRelativePath("Models/_Basic/Cube.OBJ")),
    SPAWN_MESH("Sphere", FPaths::ContentRelativePath("Models/_Basic/Sphere.OBJ")),

    SPAWN_ACTOR("Decal", ADecalActor, true),
    SPAWN_ACTOR("Height Fog", AHeightFogActor, false),
    SPAWN_ACTOR("Fake Light", AFakeLightActor, false),
    SPAWN_ACTOR("Fireball", AFireballActor, false),
    SPAWN_ACTOR("Ambient Light", AAmbientLightActor, false),
    SPAWN_ACTOR("Directional Light", ADirectionalLightActor, false),
    SPAWN_ACTOR("Point Light", APointLightActor, false),
    SPAWN_ACTOR("Spot Light", ASpotLightActor, false),
    SPAWN_ACTOR("EnemySpawner", AEnemySpawnerActor, false),
    SPAWN_ACTOR("Tank", ATankActor, true),
    SPAWN_ACTOR("Exp Bar", AExpBarActor, false),
    SPAWN_ACTOR("CameraManager", APlayerCameraManager, false),
};

#undef SPAWN_MESH
#undef SPAWN_ACTOR

constexpr int32 SpawnTableSize = static_cast<int32>(std::size(SpawnTable));

const char* GetSpawnLabel(void*, int idx)
{
    return SpawnTable[idx].Label;
}
} // namespace

#include "Math/MathUtils.h"

void FEditorControlPanel::Initialize(UEditorEngine* InEditorEngine)
{
    FEditorPanel::Initialize(InEditorEngine);
}

void FEditorControlPanel::Render(float DeltaTime)
{
    (void)DeltaTime;
    if (!EditorEngine)
        return;

    UCameraComponent* Camera = EditorEngine->GetCamera();
    if (!Camera)
    {
        return;
    }

    ImGui::SetNextWindowCollapsed(false, ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(420.0f, 280.0f), ImGuiCond_Once);
    ImGui::Begin("Control Camera");

    float FOV_Deg = Camera->GetFOV() * RAD_TO_DEG;
    if (ImGui::DragFloat("FOV", &FOV_Deg, 0.5f, 1.0f, 90.0f))
        Camera->SetFOV(FOV_Deg * DEG_TO_RAD);

    float Speed = FEditorSettings::Get().CameraSpeed;
    if (ImGui::DragFloat("Speed", &Speed, 0.1f, 0.1f, 200.0f, "%.2f"))
        FEditorSettings::Get().CameraSpeed = Clamp(Speed, 0.1f, 200.0f);

    float OrthoWidth = Camera->GetOrthoWidth();
    if (ImGui::DragFloat("Ortho Width", &OrthoWidth, 0.1f, 0.1f, 1000.0f))
        Camera->SetOrthoWidth(Clamp(OrthoWidth, 0.1f, 1000.0f));

    FVector CamPos = Camera->GetWorldLocation();
    float Location[3] = { CamPos.X, CamPos.Y, CamPos.Z };
    if (ImGui::DragFloat3("Location", Location, 0.1f))
        Camera->SetWorldLocation(FVector(Location[0], Location[1], Location[2]));

    FRotator CamRot = Camera->GetRelativeRotation();
    float Rotation[3] = { CamRot.Roll, CamRot.Pitch, CamRot.Yaw };
    if (ImGui::DragFloat3("Rotation", Rotation, 0.1f))
        Camera->SetRelativeRotation(FRotator(Rotation[1], Rotation[2], CamRot.Roll));

    ImGui::End();
}
