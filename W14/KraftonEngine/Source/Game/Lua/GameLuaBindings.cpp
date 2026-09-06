#include "Game/Lua/GameLuaBindings.h"

#include "sol/sol.hpp"

#include "Engine/Runtime/Engine.h"
#include "Engine/Runtime/EngineInitHooks.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimationManager.h"
#include "Animation/Montage/AnimMontage.h"
#include "Animation/Notify/AnimNotify_EnableAttack.h"
#include "Animation/Notify/AnimNotifyState_CounterInputWindow.h"
#include "Animation/Notify/AnimNotifyState_ParryWindow.h"
#include "Component/Camera/CameraComponent.h"
#include "Component/SceneComponent.h"
#include "Component/Animation/CharacterLocomotionComponent.h"
#include "Component/Combat/CombatStateComponent.h"
#include "Component/Light/LightComponentBase.h"
#include "Component/Light/SpotLightComponent.h"
#include "Component/Movement/CharacterMovementComponent.h"
#include "Component/Particle/ParticleSystemComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Component/Primitive/HeightFogComponent.h"
#include "Component/Primitive/SkeletalMeshComponent.h"
#include "Component/Primitive/StaticMeshComponent.h"
#include "Core/Types/CollisionTypes.h"
#include "Engine/GameFramework/World.h"
#include "Engine/GameFramework/GameMode/GameplayStatics.h"
#include "Engine/GameFramework/GameMode/PlayerController.h"
#include "Engine/GameFramework/Camera/PlayerCameraManager.h"
#include "Engine/GameFramework/Camera/PerlinNoiseCameraShake.h"
#include "Engine/Render/Types/MinimalViewInfo.h"
#include "Lua/LuaScriptManager.h"
#include "Core/ProjectSettings.h"
#include "Core/Logging/Log.h"
#include "Audio/AudioManager.h"
#include "Game/GameMode/AFinaleGameMode.h"
#include "Game/GameMode/GameState.h"
#include "Game/Leaderboard/LeaderboardStore.h"
#include "Game/Components/EnemySpawnEffect.h"
#include "Engine/Core/Logging/Log.h"
#include "Engine/Viewport/GameViewportClient.h"
#include "Engine/Viewport/Viewport.h"
#include "Particle/ParticleSystem.h"
#include "Particle/ParticleSystemManager.h"
#include "Profiling/Stats/Stats.h"

#include <algorithm>
#include <tuple>
#include <cmath>
#include <vector>




// ============================================================
// 게임-특화 Lua 바인딩 등록 위치 — 현재는 비어 있음.
//
// Engine 의 FLuaScriptManager 가 등록하는 일반 binding (AActor / APawn / FVector /
// UWorld / Anim 등) 만으로 동작하지 않는 game-specific usertype (ACarPawn /
// AGameStateXxx / 전용 enum 등) 이 도입되면 여기에 new_usertype 으로 추가한다.
//
// 호출 시점: UEngine::Init() 이 FLuaScriptManager::Initialize() 를 끝낸 직후.
// 등록은 EngineInitHooks 에 자동으로 걸려 GameEngine / EditorEngine 두 엔트리 모두
// 같은 바인딩이 적용된다 (PIE 호환).
// ============================================================
namespace
{
	constexpr float ScreenProjectionEpsilon = 1.0e-4f;

	struct FScreenProjectionResult
	{
		bool bVisible = false;
		float ScreenX = 0.0f;
		float ScreenY = 0.0f;
		float Distance = 0.0f;
		float ViewDepth = 0.0f;
		float ViewportWidth = 0.0f;
		float ViewportHeight = 0.0f;
		FVector CameraLocation = FVector::ZeroVector;
	};

	void ApplyActiveCameraLetterboxAspect(FMinimalViewInfo& POV, UWorld* World, float ViewportWidth, float ViewportHeight)
	{
		APlayerController* PlayerController = World ? World->GetFirstPlayerController() : nullptr;
		APlayerCameraManager* CameraManager = PlayerController ? PlayerController->GetPlayerCameraManager() : nullptr;
		UCameraComponent* ActiveCamera = CameraManager ? CameraManager->GetActiveCamera() : nullptr;
		if (!ActiveCamera || ViewportWidth <= 0.0f || ViewportHeight <= 0.0f)
		{
			return;
		}

		const FCameraLetterboxState& Letterbox = ActiveCamera->GetLetterboxSettings();
		if (!Letterbox.bEnabled || Letterbox.Amount <= 0.0f)
		{
			return;
		}

		const float Thickness = std::clamp(Letterbox.Thickness * Letterbox.Amount, 0.0f, 0.49f);
		const float VisibleHeightScale = 1.0f - Thickness * 2.0f;
		if (VisibleHeightScale <= ScreenProjectionEpsilon)
		{
			return;
		}

		POV.AspectRatio = (ViewportWidth / ViewportHeight) / VisibleHeightScale;
	}

	bool ProjectActiveWorldToScreen(const FVector& WorldPosition, FScreenProjectionResult& OutResult)
	{
		if (!GEngine)
		{
			return false;
		}

		UWorld* World = GEngine->GetWorld();
		UGameViewportClient* ViewportClient = GEngine->GetGameViewportClient();
		FViewport* Viewport = ViewportClient ? ViewportClient->GetViewport() : nullptr;
		if (!World || !Viewport)
		{
			return false;
		}

		const float ViewportWidth = static_cast<float>(Viewport->GetWidth());
		const float ViewportHeight = static_cast<float>(Viewport->GetHeight());
		OutResult.ViewportWidth = ViewportWidth;
		OutResult.ViewportHeight = ViewportHeight;
		if (ViewportWidth <= 1.0f || ViewportHeight <= 1.0f)
		{
			return false;
		}

		FMinimalViewInfo POV;
		if (!World->GetActivePOV(POV))
		{
			return false;
		}

		const FVector ToPoint = WorldPosition - POV.Location;
		const FVector ViewForward = POV.Rotation.GetForwardVector();
		OutResult.CameraLocation = POV.Location;
		OutResult.Distance = ToPoint.Length();
		OutResult.ViewDepth = ToPoint.Dot(ViewForward);
		if (OutResult.ViewDepth <= 0.0f)
		{
			return false;
		}

		ApplyActiveCameraLetterboxAspect(POV, World, ViewportWidth, ViewportHeight);

		const FMatrix ViewProj = POV.CalculateViewProjectionMatrix();
		const float ClipX =
			WorldPosition.X * ViewProj.M[0][0] +
			WorldPosition.Y * ViewProj.M[1][0] +
			WorldPosition.Z * ViewProj.M[2][0] +
			ViewProj.M[3][0];
		const float ClipY =
			WorldPosition.X * ViewProj.M[0][1] +
			WorldPosition.Y * ViewProj.M[1][1] +
			WorldPosition.Z * ViewProj.M[2][1] +
			ViewProj.M[3][1];
		const float ClipZ =
			WorldPosition.X * ViewProj.M[0][2] +
			WorldPosition.Y * ViewProj.M[1][2] +
			WorldPosition.Z * ViewProj.M[2][2] +
			ViewProj.M[3][2];
		const float ClipW =
			WorldPosition.X * ViewProj.M[0][3] +
			WorldPosition.Y * ViewProj.M[1][3] +
			WorldPosition.Z * ViewProj.M[2][3] +
			ViewProj.M[3][3];
		if (std::abs(ClipW) <= ScreenProjectionEpsilon)
		{
			return false;
		}

		const float NdcX = ClipX / ClipW;
		const float NdcY = ClipY / ClipW;
		const float NdcZ = ClipZ / ClipW;
		if (NdcX < -1.0f || NdcX > 1.0f || NdcY < -1.0f || NdcY > 1.0f || NdcZ < 0.0f || NdcZ > 1.0f)
		{
			return false;
		}

		OutResult.ScreenX = (NdcX * 0.5f + 0.5f) * ViewportWidth;
		OutResult.ScreenY = (0.5f - NdcY * 0.5f) * ViewportHeight;
		OutResult.bVisible = true;
		return true;
	}

	sol::table MakeProjectWorldToScreenResult(sol::this_state State, const FVector& WorldPosition)
	{
		sol::state_view Lua(State);
		sol::table Result = Lua.create_table();

		FScreenProjectionResult Projection;
		const bool bVisible = ProjectActiveWorldToScreen(WorldPosition, Projection);

		Result["Visible"] = bVisible;
		Result["X"] = bVisible ? Projection.ScreenX : 0.0f;
		Result["Y"] = bVisible ? Projection.ScreenY : 0.0f;
		Result["Distance"] = Projection.Distance;
		Result["ViewDepth"] = Projection.ViewDepth;
		Result["ViewportWidth"] = Projection.ViewportWidth;
		Result["ViewportHeight"] = Projection.ViewportHeight;
		Result["CameraLocation"] = Projection.CameraLocation;
		Result["CameraX"] = Projection.CameraLocation.X;
		Result["CameraY"] = Projection.CameraLocation.Y;
		Result["CameraZ"] = Projection.CameraLocation.Z;
		return Result;
	}

	APlayerCameraManager* ResolvePlayerCameraManager()
	{
		if (!GEngine || !GEngine->GetWorld())
		{
			return nullptr;
		}
		APlayerController* PC = GEngine->GetWorld()->GetFirstPlayerController();
		return PC ? PC->GetPlayerCameraManager() : nullptr;
	}

	AFinaleGameMode* ResolveFinaleGameMode()
	{
		if (!GEngine || !GEngine->GetWorld())
		{
			return nullptr;
		}
		return Cast<AFinaleGameMode>(GEngine->GetWorld()->GetGameMode());
	}

	UStaticMeshComponent* AttachStaticMeshToSkeletalTarget(
		AActor* Owner,
		const FString& MeshPath,
		const FString& AttachName,
		const FVector& RelativeScale,
		const FVector& RelativeLocation = FVector::ZeroVector,
		const FVector& RelativeRotation = FVector::ZeroVector)
	{
		if (!IsValid(Owner) || MeshPath.empty() || MeshPath == "None" || AttachName.empty())
		{
			UE_LOG("[Equipment] Attach failed: invalid owner, mesh path, or attach name");
			return nullptr;
		}

		USkeletalMeshComponent* OwnerMesh = Owner->GetComponentByClass<USkeletalMeshComponent>();
		if (!IsValid(OwnerMesh))
		{
			UE_LOG("[Equipment] Attach failed: owner has no skeletal mesh");
			return nullptr;
		}

		const FName AttachPoint(AttachName);
		if (!OwnerMesh->HasSocket(AttachPoint))
		{
			UE_LOG("[Equipment] Attach failed: socket/bone not found. AttachName='%s'", AttachName.c_str());
			return nullptr;
		}

		UStaticMeshComponent* WeaponMesh = Owner->AddComponent<UStaticMeshComponent>();
		if (!IsValid(WeaponMesh))
		{
			UE_LOG("[Equipment] Attach failed: could not create static mesh component");
			return nullptr;
		}

		WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		WeaponMesh->SetCanEverAffectNavigation(false);

		if (!WeaponMesh->SetStaticMeshByPath(MeshPath))
		{
			UE_LOG("[Equipment] Attach failed: could not load static mesh. Mesh='%s'", MeshPath.c_str());
			Owner->RemoveComponent(WeaponMesh);
			return nullptr;
		}

		WeaponMesh->AttachToComponent(OwnerMesh, AttachPoint);
		WeaponMesh->SetRelativeLocation(RelativeLocation);
		WeaponMesh->SetRelativeRotation(RelativeRotation);
		WeaponMesh->SetRelativeScale(RelativeScale);
		return WeaponMesh;
	}

	void LuaPrimitiveSetGenerateOverlapEvents(UPrimitiveComponent* Component, bool bGenerateOverlapEvents)
	{
		if (IsValid(Component))
		{
			Component->SetGenerateOverlapEvents(bGenerateOverlapEvents);
		}
	}

	void LuaPrimitiveSetCollisionEnabled(UPrimitiveComponent* Component, int32 Enabled)
	{
		if (IsValid(Component))
		{
			Component->SetCollisionEnabled(static_cast<ECollisionEnabled>(Enabled));
		}
	}

	int32 LuaPrimitiveGetCollisionEnabled(UPrimitiveComponent* Component)
	{
		if (!IsValid(Component))
		{
			return static_cast<int32>(ECollisionEnabled::NoCollision);
		}

		return static_cast<int32>(Component->GetCollisionEnabled());
	}

	int32 LuaPrimitiveGetCollisionResponseToChannel(UPrimitiveComponent* Component, int32 Channel)
	{
		if (!IsValid(Component))
		{
			return static_cast<int32>(ECollisionResponse::Ignore);
		}

		return static_cast<int32>(
			Component->GetCollisionResponseToChannel(static_cast<ECollisionChannel>(Channel)));
	}

	void LuaPrimitiveSetCollisionResponseToChannel(UPrimitiveComponent* Component, int32 Channel, int32 Response)
	{
		if (IsValid(Component))
		{
			Component->SetCollisionResponseToChannel(
				static_cast<ECollisionChannel>(Channel),
				static_cast<ECollisionResponse>(Response));
		}
	}

	int32 LuaPrimitiveGetPawnCollisionResponse(UPrimitiveComponent* Component)
	{
		if (!IsValid(Component))
		{
			return static_cast<int32>(ECollisionResponse::Ignore);
		}

		return static_cast<int32>(Component->GetCollisionResponseToChannel(ECollisionChannel::Pawn));
	}

	void LuaPrimitiveSetPawnCollisionResponse(UPrimitiveComponent* Component, int32 Response)
	{
		if (IsValid(Component))
		{
			Component->SetCollisionResponseToChannel(
				ECollisionChannel::Pawn,
				static_cast<ECollisionResponse>(Response));
		}
	}
}

void RegisterGameLuaBindings(sol::state& Lua)
{
	// "Engine" 테이블은 엔진 측 바인딩(FLuaScriptManager)이 이미 만들어 두지만,
	// 등록 순서에 의존하지 않도록 없으면 생성한다.
	sol::table Engine = Lua["Engine"].valid() ? Lua["Engine"] : Lua.create_named_table("Engine");

	// Engine.OpenScene("MapName" 또는 "Scene/Foo.Scene") — 다음 frame Tick 끝에서
	// active world 를 destroy 하고 해당 scene 으로 교체한다. World->Tick 바깥에서
	// 일어나므로 버튼 클릭(Lua) 콜백에서 호출해도 use-after-free 가 없다.
	Engine.set_function(
		"OpenScene",
		[](const FString& SceneNameOrPath)
		{
			if (GEngine)
			{
				GEngine->RequestTransitionToScene(SceneNameOrPath);
			}
		}
	);
	Engine.set_function(
		"ProjectWorldToScreen",
		sol::overload(
			[](sol::this_state State, const FVector& WorldPosition) -> sol::table
			{
				return MakeProjectWorldToScreenResult(State, WorldPosition);
			},
			[](sol::this_state State, float X, float Y, float Z) -> sol::table
			{
				return MakeProjectWorldToScreenResult(State, FVector(X, Y, Z));
			}
		)
	);

	sol::object PrimitiveComponentObject = Lua["PrimitiveComponent"];
	if (PrimitiveComponentObject.is<sol::table>())
	{
		sol::table PrimitiveComponent = PrimitiveComponentObject.as<sol::table>();
		PrimitiveComponent.set_function("SetGenerateOverlapEvents", &LuaPrimitiveSetGenerateOverlapEvents);
		PrimitiveComponent.set_function("SetCollisionEnabled", &LuaPrimitiveSetCollisionEnabled);
		PrimitiveComponent.set_function("GetCollisionEnabled", &LuaPrimitiveGetCollisionEnabled);
		PrimitiveComponent.set_function("GetCollisionResponseToChannel", &LuaPrimitiveGetCollisionResponseToChannel);
		PrimitiveComponent.set_function("SetCollisionResponseToChannel", &LuaPrimitiveSetCollisionResponseToChannel);
		PrimitiveComponent.set_function("GetPawnCollisionResponse", &LuaPrimitiveGetPawnCollisionResponse);
		PrimitiveComponent.set_function("SetPawnCollisionResponse", &LuaPrimitiveSetPawnCollisionResponse);
	}

	sol::table Animation = Lua["Animation"].valid() ? Lua["Animation"] : Lua.create_named_table("Animation");
	Animation.set_function(
		"LoadMontage",
		[](const FString& Path) -> UAnimMontage*
		{
			if (Path.empty() || Path == "None")
			{
				return nullptr;
			}
			return FAnimationManager::Get().LoadMontage(Path);
		}
	);
	Animation.set_function(
		"PlayOnActor",
		[](AActor* Actor, const FString& AnimPath, bool bLooping) -> bool
		{
			if (!IsValid(Actor) || AnimPath.empty() || AnimPath == "None")
			{
				return false;
			}
			USkeletalMeshComponent* Mesh = Actor->GetComponentByClass<USkeletalMeshComponent>();
			return IsValid(Mesh) ? Mesh->PlayAnimationByPath(AnimPath, bLooping) : false;
		}
	);

	Animation.set_function(
		"ConsumeEnableAttack",
		[](UAnimInstance* AnimInstance) -> bool
		{
			return UAnimNotify_EnableAttack::ConsumeEnableAttack(AnimInstance);
		}
	);
	Animation.set_function(
		"IsParryWindowActive",
		[](UAnimInstance* AnimInstance) -> bool
		{
			return UAnimNotifyState_ParryWindow::IsParryWindowActive(AnimInstance);
		}
	);
	Animation.set_function(
		"IsCounterInputWindowActive",
		[](UAnimInstance* AnimInstance) -> bool
		{
			return UAnimNotifyState_CounterInputWindow::IsCounterInputWindowActive(AnimInstance);
		}
	);
	Animation.set_function(
		"ReportSuccessfulParry",
		[](UAnimInstance* AnimInstance) -> bool
		{
			return UAnimNotifyState_ParryWindow::ReportSuccessfulParry(AnimInstance);
		}
	);
	Animation.set_function(
		"ConsumeSuccessfulParry",
		[](UAnimInstance* AnimInstance) -> bool
		{
			return UAnimNotifyState_ParryWindow::ConsumeSuccessfulParry(AnimInstance);
		}
	);
	Animation.set_function(
		"ConsumeCounterOpportunityAttacker",
		[](UAnimInstance* AnimInstance) -> AActor*
		{
			return UAnimNotifyState_ParryWindow::ConsumeSuccessfulParryAttacker(AnimInstance);
		}
	);
	Animation.set_function(
		"ConsumeCounterOpportunity",
		[](sol::this_state State, UAnimInstance* AnimInstance) -> sol::object
		{
			SCOPE_STAT_CAT("Lua.Animation.ConsumeCounterOpportunity", "Combat");
			sol::state_view Lua(State);
			AActor* Attacker = nullptr;
			FVector HitLocation = FVector::ZeroVector;
			bool bHasHitLocation = false;
			if (!UAnimNotifyState_ParryWindow::ConsumeSuccessfulParryData(
				AnimInstance,
				Attacker,
				HitLocation,
				bHasHitLocation))
			{
				return sol::make_object(Lua, sol::nil);
			}

			sol::table Result = Lua.create_table();
			if (IsValid(Attacker))
			{
				Result["attacker"] = Attacker;
			}
			if (bHasHitLocation)
			{
				Result["hitLocation"] = HitLocation;
			}
			return sol::make_object(Lua, Result);
		}
	);

	sol::table Particle = Lua["Particle"].valid() ? Lua["Particle"] : Lua.create_named_table("Particle");
	Particle.set_function(
		"SetRuntimeSpawnRateScale",
		[](UParticleSystemComponent* Component, float Scale)
		{
			SCOPE_STAT_CAT("Lua.Particle.SetRuntimeSpawnRateScale", "Particles");
			if (!IsValid(Component))
			{
				UE_LOG("[Particle] SetRuntimeSpawnRateScale failed: invalid component");
				return;
			}

			Component->SetRuntimeSpawnRateScale(Scale);
		}
	);
	Particle.set_function(
		"AttachSystemToSocket",
		[](AActor* Owner, const FString& TemplatePath, const FString& SocketName) -> UParticleSystemComponent*
		{
			SCOPE_STAT_CAT("Lua.Particle.AttachSystemToSocket", "Particles");
			if (!IsValid(Owner) || TemplatePath.empty() || TemplatePath == "None")
			{
				UE_LOG("[Particle] AttachSystemToSocket failed: invalid owner or template path");
				return nullptr;
			}

			USkeletalMeshComponent* OwnerMesh = Owner->GetComponentByClass<USkeletalMeshComponent>();
			if (!IsValid(OwnerMesh))
			{
				UE_LOG("[Particle] AttachSystemToSocket failed: owner has no skeletal mesh");
				return nullptr;
			}

			if (!OwnerMesh->HasSocket(FName(SocketName)))
			{
				UE_LOG("[Particle] AttachSystemToSocket failed: socket not found. Socket='%s'", SocketName.c_str());
				return nullptr;
			}

			UParticleSystemComponent* ParticleComponent = Owner->AddComponent<UParticleSystemComponent>();
			if (!IsValid(ParticleComponent))
			{
				UE_LOG("[Particle] AttachSystemToSocket failed: could not create particle component");
				return nullptr;
			}

			ParticleComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			ParticleComponent->SetCanEverAffectNavigation(false);

			UParticleSystem* Template = FParticleSystemManager::Get().Load(TemplatePath);
			if (!IsValid(Template))
			{
				UE_LOG("[Particle] AttachSystemToSocket failed: could not load particle system. Path='%s'", TemplatePath.c_str());
				Owner->RemoveComponent(ParticleComponent);
				return nullptr;
			}

			ParticleComponent->SetAutoActivate(false);
			ParticleComponent->SetTemplate(Template);
			ParticleComponent->AttachToComponent(OwnerMesh, FName(SocketName));
			ParticleComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 0.0f));
			ParticleComponent->SetRelativeRotation(FVector(0.0f, 0.0f, 0.0f));
			ParticleComponent->SetRelativeScale(FVector(1.0f, 1.0f, 1.0f));
			ParticleComponent->Deactivate();
			UE_LOG("[Particle] AttachSystemToSocket success: Owner=%s Socket=%s Template=%s Component=%s",
				Owner->GetName().c_str(),
				SocketName.c_str(),
				TemplatePath.c_str(),
				ParticleComponent->GetName().c_str());
			return ParticleComponent;
		}
	);
	Particle.set_function(
		"SpawnSystemAtLocation",
		[](AActor* WorldContext, const FString& TemplatePath, const FVector& Location) -> UParticleSystemComponent*
		{
			SCOPE_STAT_CAT("Lua.Particle.SpawnSystemAtLocation", "Particles");
			if (!IsValid(WorldContext) || !WorldContext->GetWorld())
			{
				UE_LOG("[Particle] SpawnSystemAtLocation failed: invalid world context");
				return nullptr;
			}

			return FGameplayStatics::SpawnEmitterAtLocation(
				WorldContext->GetWorld(),
				TemplatePath,
				Location,
				FRotator(),
				false);
		}
	);
	Particle.set_function(
		"SetSystemWorldLocation",
		[](UParticleSystemComponent* Component, const FVector& Location)
		{
			SCOPE_STAT_CAT("Lua.Particle.SetSystemWorldLocation", "Particles");
			if (IsValid(Component))
			{
				Component->SetWorldLocation(Location);
			}
		}
	);
	Particle.set_function(
		"SetSystemWorldScale",
		[](UParticleSystemComponent* Component, float UniformScale)
		{
			SCOPE_STAT_CAT("Lua.Particle.SetSystemWorldScale", "Particles");
			if (IsValid(Component))
			{
				const float Scale = std::max(0.0f, UniformScale);
				Component->SetRelativeScale(FVector(Scale, Scale, Scale));
			}
		}
	);
	Particle.set_function(
		"SetRibbonEdgeSourceComponents",
		[](UParticleSystemComponent* RibbonComponent, USceneComponent* SourceComponent, USceneComponent* TargetComponent)
		{
			SCOPE_STAT_CAT("Lua.Particle.SetRibbonEdgeSourceComponents", "Particles");
			if (!IsValid(RibbonComponent) || !IsValid(SourceComponent) || !IsValid(TargetComponent))
			{
				UE_LOG("[Particle] SetRibbonEdgeSourceComponents failed: invalid component");
				return false;
			}

			RibbonComponent->SetRibbonEdgeSourceComponents(SourceComponent, TargetComponent);
			return true;
		}
	);

	sol::table Equipment = Lua["Equipment"].valid() ? Lua["Equipment"] : Lua.create_named_table("Equipment");
	Equipment.set_function(
		"AttachStaticMeshToSocket",
		[](AActor* Owner, const FString& MeshPath, const FString& SocketName, const FVector& RelativeScale) -> UStaticMeshComponent*
		{
			return AttachStaticMeshToSkeletalTarget(Owner, MeshPath, SocketName, RelativeScale);
		}
	);
	Equipment.set_function(
		"AttachStaticMeshToBone",
		sol::overload(
			[](AActor* Owner, const FString& MeshPath, const FString& BoneName, const FVector& RelativeScale) -> UStaticMeshComponent*
			{
				return AttachStaticMeshToSkeletalTarget(Owner, MeshPath, BoneName, RelativeScale);
			},
			[](AActor* Owner, const FString& MeshPath, const FString& BoneName, const FVector& RelativeScale, const FVector& RelativeLocation, const FVector& RelativeRotation) -> UStaticMeshComponent*
			{
				return AttachStaticMeshToSkeletalTarget(Owner, MeshPath, BoneName, RelativeScale, RelativeLocation, RelativeRotation);
			}
		)
	);

	// Game.TogglePause — pause key entry point. Resolves the active GameMode and
	// lets it decide pause vs. resume from its own phase.
	sol::table Game = Lua["Game"].valid() ? Lua["Game"] : Lua.create_named_table("Game");
	Game.set_function(
		"EnterCutscene",
		[]()
		{
			if (AFinaleGameMode* GM = ResolveFinaleGameMode())
			{
				GM->OnEnteringCutscene();
			}
		}
	);
	Game.set_function(
		"ExitCutscene",
		[]()
		{
			if (AFinaleGameMode* GM = ResolveFinaleGameMode())
			{
				GM->OnExitingCutscene();
			}
		}
	);

	Game.set_function(
		"TogglePause",
		[]()
		{
			if (!GEngine || !GEngine->GetWorld()) return;
			if (AFinaleGameMode* GM = Cast<AFinaleGameMode>(GEngine->GetWorld()->GetGameMode()))
			{
				GM->TogglePause();
			}
		}
	);
	Game.set_function(
		"QuitToTitle",
		[]()
		{
			if (!GEngine || !GEngine->GetWorld()) return;
			if (AFinaleGameMode* GM = Cast<AFinaleGameMode>(GEngine->GetWorld()->GetGameMode()))
			{
				GM->OnGameQuit();
			}
		}
	);
	// Phase string (e.g. "Playing", "Dead", "Defeated") — GameFlowController
	// polls this each Tick to drive phase-dependent UI (death overlay, etc.).
	Game.set_function(
		"GetPhase",
		[]() -> FString
		{
			if (!GEngine || !GEngine->GetWorld()) return "None";
			if (AFinaleGameMode* GM = Cast<AFinaleGameMode>(GEngine->GetWorld()->GetGameMode()))
			{
				if (AFinaleGameState* GS = Cast<AFinaleGameState>(GM->GetGameState()))
				{
					return GS->GetPhaseString();
				}
			}
			return "None";
		}
	);
	Game.set_function(
		"PlayerDeath",
		[]() 
		{
			if (!GEngine || !GEngine->GetWorld()) return;
			if (AFinaleGameMode* GM = Cast<AFinaleGameMode>(GEngine->GetWorld()->GetGameMode()))
				GM->OnPlayerDeath();
		});
	Game.set_function(
		"Revive",
		[]()
		{
			if (!GEngine || !GEngine->GetWorld()) return;
			if (AFinaleGameMode* GM = Cast<AFinaleGameMode>(GEngine->GetWorld()->GetGameMode()))
			{
				GM->OnPlayerRevive();
			}
		}
	);
	Game.set_function(
		"Defeated",
		[]()
		{
			if (!GEngine || !GEngine->GetWorld()) return;
			if (AFinaleGameMode* GM = Cast<AFinaleGameMode>(GEngine->GetWorld()->GetGameMode()))
			{
				GM->OnPlayerDefeated();
			}
		}
	);
	// Game.OnBossSlain — the boss-death event from the encounter director
	// (BossIntroDirector). Resolves the run into the Victory phase; gated on
	// Playing in the GameMode, so it no-ops if the run already ended.
	Game.set_function(
		"OnBossSlain",
		[]()
		{
			if (AFinaleGameMode* GM = ResolveFinaleGameMode())
			{
				GM->OnBossSlain(FName("Boss"));
			}
		}
	);
	// Current camera fade alpha (0..1). Lets UI track the engine post-process
	// fade exactly (e.g. redden the death icon in lockstep with the fade-out).
	Game.set_function(
		"GetCameraFade",
		[]() -> float
		{
			if (!GEngine || !GEngine->GetWorld()) return 0.0f;
			APlayerController* PC = GEngine->GetWorld()->GetFirstPlayerController();
			APlayerCameraManager* Manager = PC ? PC->GetPlayerCameraManager() : nullptr;
			return Manager ? Manager->GetFadeAmount() : 0.0f;
		}
	);
	// Fade (black) from an explicit alpha — unlike CameraManager.FadeIn which
	// always starts at 1.0. Lets a revive continue from the death fade's current
	// alpha instead of snapping to black first.
	Game.set_function(
		"CameraFade",
		[](float FromAlpha, float ToAlpha, float Duration)
		{
			if (!GEngine || !GEngine->GetWorld()) return;
			APlayerController* PC = GEngine->GetWorld()->GetFirstPlayerController();
			APlayerCameraManager* Manager = PC ? PC->GetPlayerCameraManager() : nullptr;
			if (Manager)
			{
				Manager->StartCameraFade(FromAlpha, ToAlpha, Duration, FLinearColor::Black(), false, true);
			}
		}
	);
	// Game.ViewLeaderboard — victory/defeat LEADERBOARD button. Drives the phase
	// to Leaderboard (gated in OnLeaderBoardView); the leaderboard screen is TODO.
	Game.set_function(
		"ViewLeaderboard",
		[]()
		{
			if (!GEngine || !GEngine->GetWorld()) return;
			if (AFinaleGameMode* GM = Cast<AFinaleGameMode>(GEngine->GetWorld()->GetGameMode()))
			{
				GM->OnLeaderBoardView();
			}
		}
	);
	// Game.GetActiveTime — seconds of Playing time elapsed up to victory. The
	// leaderboard reads this at submit (the run is frozen by then) for the record.
	Game.set_function(
		"GetActiveTime",
		[]() -> float
		{
			if (!GEngine || !GEngine->GetWorld()) return 0.0f;
			if (AFinaleGameMode* GM = Cast<AFinaleGameMode>(GEngine->GetWorld()->GetGameMode()))
			{
				return GM->GetActiveTime();
			}
			return 0.0f;
		}
	);
	// Game.GetReviveCount — number of revives the player used this run.
	Game.set_function(
		"GetReviveCount",
		[]() -> int
		{
			if (!GEngine || !GEngine->GetWorld()) return 0;
			if (AFinaleGameMode* GM = Cast<AFinaleGameMode>(GEngine->GetWorld()->GetGameMode()))
			{
				return static_cast<int>(GM->GetReviveCount());
			}
			return 0;
		}
	);

	// Runtime helpers used by scene directors and encounter scripts.
	sol::table Camera = Lua["Camera"].valid() ? Lua["Camera"] : Lua.create_named_table("Camera");
	// Camera.Shake(scale [, params])
	//   scale  : 전체 세기 배율 (필수).
	//   params : 선택 테이블. 지정한 키만 펄린 기본값을 덮어쓴다 (노티파이 패널과 동일 9필드 + playSpace).
	//     duration, blendIn, blendOut, fovAmplitude, fovFrequency : number
	//     locAmplitude, locFrequency : Vector(x,y,z) 또는 { x=, y=, z= }
	//     rotAmplitude, rotFrequency : { pitch=, yaw=, roll= } (x/y/z 또는 Vector 도 허용)
	//     playSpace                  : 0=CameraLocal(기본) 1=World 2=UserDefined
	//   반환 : 셰이크 핸들 (실패 시 nil). 핸들로 :StopShake() / :IsFinished() 호출 가능.
	Camera.set_function(
		"Shake",
		[](float Scale, sol::optional<sol::table> Params) -> UCameraShakeBase*
		{
			APlayerCameraManager* Manager = ResolvePlayerCameraManager();
			if (!Manager)
			{
				return nullptr;
			}

			ECameraShakePlaySpace PlaySpace = ECameraShakePlaySpace::CameraLocal;
			if (Params)
			{
				sol::object PS = (*Params)["playSpace"];
				if (PS.is<int>())
				{
					const int V = PS.as<int>();
					if (V >= 0 && V <= 2)
					{
						PlaySpace = static_cast<ECameraShakePlaySpace>(V);
					}
				}
			}

			UPerlinNoiseCameraShake* Shake = Manager->StartCameraShake<UPerlinNoiseCameraShake>(Scale, PlaySpace);
			if (!Shake)
			{
				return nullptr;
			}

			if (Params)
			{
				const sol::table& P = *Params;

				auto ReadFloat = [&P](const char* Key, float Def) -> float
				{
					sol::object O = P[Key];
					return O.is<double>() ? static_cast<float>(O.as<double>()) : Def;
				};
				// Vector: Vector(x,y,z) usertype 또는 { x=, y=, z= } 테이블.
				auto ReadVector = [&P](const char* Key, const FVector& Def) -> FVector
				{
					sol::object O = P[Key];
					if (O.is<FVector>())
					{
						return O.as<FVector>();
					}
					if (O.is<sol::table>())
					{
						sol::table T = O.as<sol::table>();
						sol::object X = T["x"], Y = T["y"], Z = T["z"];
						return FVector(
							X.is<double>() ? static_cast<float>(X.as<double>()) : Def.X,
							Y.is<double>() ? static_cast<float>(Y.as<double>()) : Def.Y,
							Z.is<double>() ? static_cast<float>(Z.as<double>()) : Def.Z);
					}
					return Def;
				};
				// Rotator: { pitch=, yaw=, roll= } (x/y/z 별칭) 또는 Vector(x→pitch,y→yaw,z→roll).
				auto ReadRotator = [&P](const char* Key, const FRotator& Def) -> FRotator
				{
					sol::object O = P[Key];
					if (O.is<FVector>())
					{
						const FVector V = O.as<FVector>();
						return FRotator(V.X, V.Y, V.Z);
					}
					if (O.is<sol::table>())
					{
						sol::table T = O.as<sol::table>();
						auto Pick = [&T](const char* A, const char* B, float D) -> float
						{
							sol::object Oa = T[A];
							if (Oa.is<double>()) return static_cast<float>(Oa.as<double>());
							sol::object Ob = T[B];
							return Ob.is<double>() ? static_cast<float>(Ob.as<double>()) : D;
						};
						return FRotator(
							Pick("pitch", "x", Def.Pitch),
							Pick("yaw",   "y", Def.Yaw),
							Pick("roll",  "z", Def.Roll));
					}
					return Def;
				};

				Shake->Duration     = ReadFloat("duration",      Shake->Duration);
				Shake->BlendInTime  = ReadFloat("blendIn",       Shake->BlendInTime);
				Shake->BlendOutTime = ReadFloat("blendOut",      Shake->BlendOutTime);
				Shake->LocAmplitude = ReadVector("locAmplitude", Shake->LocAmplitude);
				Shake->LocFrequency = ReadVector("locFrequency", Shake->LocFrequency);
				Shake->RotAmplitude = ReadRotator("rotAmplitude", Shake->RotAmplitude);
				Shake->RotFrequency = ReadRotator("rotFrequency", Shake->RotFrequency);
				Shake->FOVAmplitude = ReadFloat("fovAmplitude",  Shake->FOVAmplitude);
				Shake->FOVFrequency = ReadFloat("fovFrequency",  Shake->FOVFrequency);
			}

			return Shake;
		}
	);

	sol::table Combat = Lua["Combat"].valid() ? Lua["Combat"] : Lua.create_named_table("Combat");
	Combat.set_function(
		"SetTeam",
		[](AActor* Actor, int32 Team) -> bool
		{
			if (!IsValid(Actor))
			{
				return false;
			}
			UCombatStateComponent* CombatState = Actor->GetComponentByClass<UCombatStateComponent>();
			if (!IsValid(CombatState))
			{
				return false;
			}
			CombatState->SetTeam(static_cast<ECombatTeam>(Team));
			return true;
		}
	);

	sol::table Locomotion = Lua["Locomotion"].valid() ? Lua["Locomotion"] : Lua.create_named_table("Locomotion");
	Locomotion.set_function(
		"SetEnabled",
		[](AActor* Actor, bool bEnabled) -> bool
		{
			if (!IsValid(Actor))
			{
				return false;
			}

			bool bChangedAny = false;
			if (UCharacterMovementComponent* Movement = Actor->GetComponentByClass<UCharacterMovementComponent>())
			{
				Movement->SetMovementInputEnabled(bEnabled);
				if (!bEnabled)
				{
					Movement->StopMovementImmediately();
				}
				bChangedAny = true;
			}

			if (UCharacterLocomotionComponent* Driver = Actor->GetComponentByClass<UCharacterLocomotionComponent>())
			{
				Driver->SetDriverEnabled(bEnabled);
				bChangedAny = true;
			}

			return bChangedAny;
		}
	);

	// ============================================================
	// Leaderboard — file-backed top-N store (Saves/Leaderboard.txt). The UI is an
	// RmlUi overlay driven from GameFlowController.lua: submit on victory (writes a
	// record), view-only elsewhere (defeat/pause/title). Lua has no file I/O, so
	// persistence lives in C++ (GameLeaderboard, see LeaderboardStore.h).
	// ============================================================
	sol::table Leaderboard = Lua["Leaderboard"].valid() ? Lua["Leaderboard"] : Lua.create_named_table("Leaderboard");
	Leaderboard.set_function(
		"Submit",
		[](const FString& Name, float TimeSec, int Revives)
		{
			GameLeaderboard::Submit(Name, TimeSec, Revives);
		}
	);
	// Returns a 1-based array of { name, time, revives } sorted best-first.
	Leaderboard.set_function(
		"GetEntries",
		[](sol::this_state S) -> sol::table
		{
			sol::state_view View(S);
			sol::table Out = View.create_table();
			std::vector<GameLeaderboard::FEntry> Entries = GameLeaderboard::Load();
			int Index = 1;
			for (const GameLeaderboard::FEntry& E : Entries)
			{
				sol::table Row = View.create_table();
				Row["name"]    = E.Name;
				Row["time"]    = E.TimeSec;
				Row["revives"] = E.Revives;
				Out[Index++]   = Row;
			}
			return Out;
		}
	);

	// ============================================================
	// Debug flow triggers (dev only)
	// Bound to number keys in GameFlowController.lua to drive the macro state
	// machine without real combat/gameplay. Each fires one flow event on the
	// active GameMode; the handlers self-gate on EGamePhase, so a key pressed
	// in the wrong phase is a harmless no-op. Strip the DEBUG_KEYS block in the
	// Lua controller (or this table) before shipping.
	// ============================================================
	auto ResolveFinaleGM = []() -> AFinaleGameMode*
	{
		if (!GEngine || !GEngine->GetWorld()) return nullptr;
		return Cast<AFinaleGameMode>(GEngine->GetWorld()->GetGameMode());
	};

	sol::table Debug = Lua["Debug"].valid() ? Lua["Debug"] : Lua.create_named_table("Debug");
	Debug.set_function("PlayerDeath",  [ResolveFinaleGM]() { if (AFinaleGameMode* GM = ResolveFinaleGM()) GM->OnPlayerDeath(); });
	Debug.set_function("PlayerRevive", [ResolveFinaleGM]() { if (AFinaleGameMode* GM = ResolveFinaleGM()) GM->OnPlayerRevive(); });
	Debug.set_function("BossSlain",   [ResolveFinaleGM]() { if (AFinaleGameMode* GM = ResolveFinaleGM()) GM->OnBossSlain(FName("DebugBoss")); });
	Debug.set_function("Victory",     [ResolveFinaleGM]() { if (AFinaleGameMode* GM = ResolveFinaleGM()) GM->OnVictory(); });
	Debug.set_function("Cutscene",    [ResolveFinaleGM]() { if (AFinaleGameMode* GM = ResolveFinaleGM()) GM->OnEnteringCutscene(); });
	Debug.set_function("Leaderboard", [ResolveFinaleGM]() { if (AFinaleGameMode* GM = ResolveFinaleGM()) GM->OnLeaderBoardView(); });
	Debug.set_function("PrintPhase",  [ResolveFinaleGM]()
	{
		if (AFinaleGameMode* GM = ResolveFinaleGM())
		{
			if (AFinaleGameState* GS = Cast<AFinaleGameState>(GM->GetGameState()))
			{
				UE_LOG("[Debug] Current phase: %s", GS->GetPhaseString().c_str());
			}
		}
	});

	// ---- Options ----
	// 런타임 source of truth 는 FProjectSettings (in-memory singleton).
	// SetMasterVolume: 메모리 갱신 + FAudioManager 에 즉시 적용(apply-on-change).
	// Save: ProjectSettings.ini 에 영속화 (load→mutate→save 라 다른 설정을 안 덮어씀).
	sol::table Options = Lua.create_named_table("Options");

	Options.set_function(
		"GetMasterVolume",
		[]() -> float
		{
			return FProjectSettings::Get().Audio.MasterVolume;
		}
	);

	Options.set_function(
		"SetMasterVolume",
		[](float Volume)
		{
			const float Clamped = std::clamp(Volume, 0.0f, 1.0f);
			FProjectSettings::Get().Audio.MasterVolume = Clamped;
			FAudioManager::Get().SetMasterVolume(Clamped);
		}
	);

	Options.set_function(
		"Save",
		[]()
		{
			FProjectSettings::Get().SaveToFile(FProjectSettings::GetDefaultPath());
		}
	);

	// ---- Scene (mini-phase atmosphere control) ----
	// Helpers a choreography script lerps against each Tick. Each resolves the
	// typed component off the actor — GetComponentByClass<T>() hands back a
	// correctly-typed pointer (unlike Reflection.Call / GetComponentByName, which
	// marshal to a base UObject/USceneComponent handle that can't reach the
	// derived setters) — and routes to the C++ setter, which pushes the change to
	// the render scene immediately. Actors come from World.FindActorsByTag/Class.
	sol::table Scene = Lua.create_named_table("Scene");

	Scene.set_function(
		"SetSpotColor",
		[](AActor* Actor, float R, float G, float B, sol::optional<float> A)
		{
			if (!IsValid(Actor)) return;
			if (USpotLightComponent* Light = Actor->GetComponentByClass<USpotLightComponent>())
			{
				Light->SetLightColor(FVector4(R, G, B, A.value_or(1.0f)));
			}
		}
	);
	Scene.set_function(
		"SetSpotIntensity",
		[](AActor* Actor, float Intensity)
		{
			if (!IsValid(Actor)) return;
			if (USpotLightComponent* Light = Actor->GetComponentByClass<USpotLightComponent>())
			{
				Light->SetIntensity(Intensity);
			}
		}
	);
	// Returns the current spot color as r,g,b,a so a script can lerp from the
	// authored start without hardcoding it. Defaults to white if no light found.
	Scene.set_function(
		"GetSpotColor",
		[](AActor* Actor) -> std::tuple<float, float, float, float>
		{
			if (IsValid(Actor))
			{
				if (USpotLightComponent* Light = Actor->GetComponentByClass<USpotLightComponent>())
				{
					const FVector4 C = Light->GetLightColor();
					return { C.X, C.Y, C.Z, C.W };
				}
			}
			return { 1.0f, 1.0f, 1.0f, 1.0f };
		}
	);
	Scene.set_function(
		"SetFogColor",
		[](AActor* Actor, float R, float G, float B, sol::optional<float> A)
		{
			if (!IsValid(Actor)) return;
			if (UHeightFogComponent* Fog = Actor->GetComponentByClass<UHeightFogComponent>())
			{
				Fog->SetFogInscatteringColor(FVector4(R, G, B, A.value_or(1.0f)));
			}
		}
	);
	// Defaults to the HeightFogComponent's authored start color if none found.
	Scene.set_function(
		"GetFogColor",
		[](AActor* Actor) -> std::tuple<float, float, float, float>
		{
			if (IsValid(Actor))
			{
				if (UHeightFogComponent* Fog = Actor->GetComponentByClass<UHeightFogComponent>())
				{
					const FVector4 C = Fog->GetFogInscatteringColor();
					return { C.X, C.Y, C.Z, C.W };
				}
			}
			return { 0.45f, 0.55f, 0.65f, 1.0f };
		}
	);
	Scene.set_function(
		"SetFogDensity",
		[](AActor* Actor, float Density)
		{
			if (!IsValid(Actor)) return;
			if (UHeightFogComponent* Fog = Actor->GetComponentByClass<UHeightFogComponent>())
			{
				Fog->SetFogDensity(Density);
			}
		}
	);
	// Blood-moon reveal: AActor::SetVisible marks every primitive's render
	// visibility dirty, so a hidden actor placed in the scene pops in on true.
	Scene.set_function(
		"SetActorVisible",
		[](AActor* Actor, bool bVisible)
		{
			if (IsValid(Actor)) Actor->SetVisible(bVisible);
		}
	);
	// Blood-moon particle: resolve the typed PSC off the actor here. Going through
	// GetComponentByClass avoids the base-handle gotcha — FindActorsByTag hands Lua
	// a base AActor, so actor:GetParticleSystemComponent()/psc:Activate() can nil out.
	Scene.set_function(
		"SetParticleActive",
		[](AActor* Actor, bool bActive, sol::optional<bool> bReset) -> bool
		{
			if (!IsValid(Actor)) return false;
			UParticleSystemComponent* PSC = Actor->GetComponentByClass<UParticleSystemComponent>();
			if (!PSC)
			{
				UE_LOG("[Scene.SetParticleActive] no UParticleSystemComponent on actor '%s'", Actor->GetName().c_str());
				return false;
			}
			if (bActive) PSC->Activate(bReset.value_or(false));
			else         PSC->Deactivate();
			return true;
		}
	);
	Scene.set_function(
		"IsParticleActive",
		[](AActor* Actor) -> bool
		{
			if (!IsValid(Actor)) return false;
			UParticleSystemComponent* PSC = Actor->GetComponentByClass<UParticleSystemComponent>();
			return PSC && PSC->IsActive();
		}
	);
	// 스폰 디렉터 인보크: 타입드 컴포넌트를 C++ 에서 풀어 PlayEffect 호출(base-handle gotcha 회피).
	Scene.set_function(
		"PlayEnemySpawnEffect",
		[](AActor* Actor) -> bool
		{
			if (!IsValid(Actor)) return false;
			UEnemySpawnEffectComponent* Effect = Actor->GetComponentByClass<UEnemySpawnEffectComponent>();
			if (!Effect)
			{
				UE_LOG("[Scene.PlayEnemySpawnEffect] no UEnemySpawnEffectComponent on actor '%s'", Actor->GetName().c_str());
				return false;
			}
			Effect->PlayEffect();
			return true;
		}
	);
	Scene.set_function(
		"IsEnemySpawnPlaying",
		[](AActor* Actor) -> bool
		{
			if (!IsValid(Actor)) return false;
			UEnemySpawnEffectComponent* Effect = Actor->GetComponentByClass<UEnemySpawnEffectComponent>();
			return Effect && Effect->IsPlaying();
		}
	);
}

// 자기-등록 — Editor / Game 측이 RegisterGameLuaBindings 함수명을 모르고도
// FEngineInitHooks::RunAll() 한 번이면 호출되도록 static initializer 로 등록.
namespace
{
	void RunRegisterGameLuaBindings()
	{
		RegisterGameLuaBindings(FLuaScriptManager::GetState());
	}

	struct GameLuaBindingsAutoReg
	{
		GameLuaBindingsAutoReg() { FEngineInitHooks::Register(&RunRegisterGameLuaBindings); }
	};

	static GameLuaBindingsAutoReg gAutoReg;
}
