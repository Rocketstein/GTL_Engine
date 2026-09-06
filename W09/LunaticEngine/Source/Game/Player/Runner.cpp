#include "Runner.h"
#include "Object/Object.h"
#include "Component/Shape/BoxComponent.h"
#include "Component/ScriptComponent.h"
#include "Component/CameraComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Component/BillboardComponent.h"
#include "Component/SoundComponent.h"
#include "Resource/ResourceManager.h"
#include "Engine/Runtime/Engine.h"
#include "Texture/Texture2D.h"

#include "Mesh/ObjManager.h"
#include <Core/Log.h>
#include "Collision/OverlapInfo.h"
#include "Math/Vector.h"


IMPLEMENT_CLASS(ARunner, APawnActor)

namespace
{
constexpr const char* DreamBillboardComponentName = "DreamBillboard";
constexpr const char* DreamBillboardTextureKey = "Game.Texture.Dream";
constexpr const char* DreamBillboardTextureFallback = "Asset/Content/Texture/Dream.png";
constexpr float DreamBillboardInitialOffsetX = 1000.0f;
constexpr float DreamBillboardSpriteSize = 100.0f;
constexpr float DreamBillboardMirroredSpriteWidth = -DreamBillboardSpriteSize;
}

ARunner::ARunner()
{
	// PIE 또는 Shipping Build에서만 Tick을 돌린다.
	// Editor preview 상태에서 자동 전진/중력 Lua가 실행되면 배치 작업 중 위치가 계속 바뀌므로 막아 둔다.
	bNeedsTick = true;
	bTickInEditor = false;

	// Player의 실제 충돌체다.
	// Map floor의 기본 cube mesh는 Z -1~1 범위를 가지므로 floor scale Z=1이면 바닥 상단이 대략 Z=1이다.
	// Player half-height가 1.0이면 중심 Z가 최소 2.0 이상이어야 발 위치가 바닥 상단보다 높아져
	// PlayerController.FindGround()가 "아래쪽 바닥"으로 인식할 수 있다.
	BoxComponent = AddComponent<UBoxComponent>();
	BoxComponent->SetCanDeleteFromDetails(false);
	// TODO: Runner 충돌 크기는 추후 Lua Config 또는 캐릭터 리소스 데이터로 이동한다.
	BoxComponent->SetBoxExtent(FVector(0.65f, 0.7f, 0.75f));
	BoxComponent->SetCollisionEnabled(true);
	BoxComponent->SetGenerateOverlapEvents(true);
	
	SetRootComponent(BoxComponent);
	// 시작 시 MapManager가 아직 chunk를 만들기 전이라 BeginPlay 첫 ground query는 실패할 수 있다.
	// 그래서 바닥 위에서 약간 떨어진 높이로 시작시켜, chunk 생성 후 중력으로 자연스럽게 snap되게 한다.
	SetActorLocation(FVector(2.0f, 0.0f, 5.f));

	// Player 표시용 mesh다. 실제 충돌은 Root BoxComponent가 담당하므로 mesh collision은 끈다.
	MeshComponent = AddComponent<UStaticMeshComponent>();
	MeshComponent->SetCanDeleteFromDetails(false);
	MeshComponent->AttachToComponent(GetRootComponent());
	MeshComponent->SetRelativeScale(FVector(0.5f, 0.5f, 1.0f));
	MeshComponent->SetCollisionEnabled(false);

	// Dream.png billboard. It is intentionally not attached to RootComponent;
	// PlayerController.lua advances it by the player's actual X movement.
	DreamBillboard = AddComponent<UBillboardComponent>();
	DreamBillboard->SetFName(FName(DreamBillboardComponentName));
	DreamBillboard->SetCanDeleteFromDetails(false);
	DreamBillboard->SetCollisionEnabled(false);
	DreamBillboard->SetGenerateOverlapEvents(false);
	DreamBillboard->SetSpriteSize(DreamBillboardMirroredSpriteWidth, DreamBillboardSpriteSize);

	// 이동/중력/HP/score/audio 시작 흐름은 Lua에서 빠르게 조정할 수 있게 둔다.
	PlayerMove = AddComponent<UScriptComponent>();
	PlayerMove->SetCanDeleteFromDetails(false);
	PlayerMove->SetScriptPath("Scripts/Game/PlayerController.lua");

	// 카메라는 Player 자식으로 고정한다.
	// Tick에서 매 프레임 위치를 갱신하지 않고, 초기 상대 위치만 세팅한다.
	MainCamera = AddComponent<UCameraComponent>();
	MainCamera->SetCanDeleteFromDetails(false);
	MainCamera->SetRelativeLocation(FVector(-8.0f, 0.0f, 5.0f));
	MainCamera->AttachToComponent(GetRootComponent());

	// SoundComponent는 SceneComponent가 아니라 transform/attach 대상이 아니다.
	// Actor가 소유만 하고, Lua AudioManager가 GetComponentByType()으로 찾아서 재생한다.
	BackgroundSound = AddComponent<UBackgroundSoundComponent>();
	BackgroundSound->SetCanDeleteFromDetails(false);
	BackgroundSound->SetLooping(true);

	SFXSound = AddComponent<USFXComponent>();
	SFXSound->SetCanDeleteFromDetails(false);
	SFXSound->SetLooping(false);
}

void ARunner::BeginPlay()
{
	// Lua BeginPlay에서 mesh local transform을 읽으므로 visual setup을 먼저 끝낸다.
	// 이 프로젝트의 Super::BeginPlay()가 ScriptComponent BeginPlay를 호출하기 때문에 일반 순서와 다르게 둔다.
	ApplyDefaultVisual();
	SetupDreamBillboard();
	Super::BeginPlay();
	SetupCamera();
}

void ARunner::ApplyDefaultVisual()
{
	MeshComponent->SetRelativeScale(FVector(0.5f, 0.5f, 1.0f));

#pragma region Set Mesh & Material
	if (GEngine && MeshComponent)
	{
		ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();

		FString MeshPath = FResourceManager::Get().ResolvePath(
			FName("Game.Mesh.Player.Runner"),
			"Asset/Content/Model/POD/Spaceship.obj"
		);

		UStaticMesh* Mesh = FObjManager::LoadObjStaticMesh(MeshPath, Device);
		if (Mesh)
		{
			MeshComponent->SetStaticMesh(Mesh);
			MeshComponent->PostEditProperty("StaticMesh");
		}
		else
		{
			UE_LOG("[Runner] Failed to load mesh: %s", MeshPath.c_str());
		}
	}
#pragma endregion
}

void ARunner::SetupDreamBillboard()
{
	if (!DreamBillboard)
	{
		return;
	}

	DreamBillboard->SetCollisionEnabled(false);
	DreamBillboard->SetGenerateOverlapEvents(false);
	DreamBillboard->SetSpriteSize(DreamBillboardMirroredSpriteWidth, DreamBillboardSpriteSize);
	DreamBillboard->SetWorldLocation(GetActorLocation() + FVector(DreamBillboardInitialOffsetX, 0.0f, 0.0f));

	if (GEngine)
	{
		ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
		const FString TexturePath = FResourceManager::Get().ResolvePath(
			FName(DreamBillboardTextureKey),
			DreamBillboardTextureFallback);

		if (Device)
		{
			if (UTexture2D* Texture = UTexture2D::LoadFromFile(TexturePath, Device))
			{
				DreamBillboard->SetTexture(Texture);
			}
			else
			{
				UE_LOG("[Runner] Failed to load dream billboard texture: %s", TexturePath.c_str());
			}
		}
	}

	DreamBillboard->MarkRenderStateDirty();
}

void ARunner::SetupCamera()
{
	if (MainCamera)
	{
		MainCamera->SetRelativeLocation(FVector(-8.0f, 0.0f, 5.0f));

		// Runner 기준 앞쪽을 바라보게 한다.
		// X+가 전진 방향이므로 카메라는 Runner 뒤(-X)에 있고,
		// Runner 앞(+X)과 약간 위쪽을 한 번만 바라본다.
		// 이후 Tick에서 카메라 위치/회전을 계속 조작하지 않는다.
		MainCamera->LookAt(GetActorLocation() + FVector(5.0f, 0.0f, 1.5f));
	}

	if (GetWorld() && MainCamera)
	{
		GetWorld()->SetActiveCamera(MainCamera);
	}
}

void ARunner::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void ARunner::EndPlay()
{
	Super::EndPlay();
}

void ARunner::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
}

UObject* ARunner::Duplicate(UObject* NewOuter /*= nullptr*/) const
{
	return Super::Duplicate(NewOuter);
}

void ARunner::TickActor(float DeltaSeconds, ELevelTick TickType, FActorTickFunction& ThisTickFunction)
{
	Super::TickActor(DeltaSeconds, TickType, ThisTickFunction);
}
