#include "Scripting/LuaScriptRuntime.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/World.h"
#include "Math/Rotator.h"

#pragma region SolInclude
#ifdef check
#pragma push_macro("check")
#undef check
#define LUA_PROXY_BINDINGS_RESTORE_CHECK_MACRO
#endif

#ifdef checkf
#pragma push_macro("checkf")
#undef checkf
#define LUA_PROXY_BINDINGS_RESTORE_CHECKF_MACRO
#endif

#include <sol/sol.hpp>
#include "Camera/PlayerCameraManager.h"

#ifdef LUA_PROXY_BINDINGS_RESTORE_CHECKF_MACRO
#pragma pop_macro("checkf")
#undef LUA_PROXY_BINDINGS_RESTORE_CHECKF_MACRO
#endif

#ifdef LUA_PROXY_BINDINGS_RESTORE_CHECK_MACRO
#pragma pop_macro("check")
#undef LUA_PROXY_BINDINGS_RESTORE_CHECK_MACRO
#endif
#pragma endregion

namespace
{
	EViewTargetBlendFunction ParseBlendFunction(const FString& Name)
	{
		if (Name == "EaseIn")
		{
			return EViewTargetBlendFunction::EaseIn;
		}
		if (Name == "EaseOut")
		{
			return EViewTargetBlendFunction::EaseOut;
		}
		if (Name == "EaseInOut")
		{
			return EViewTargetBlendFunction::EaseInOut;
		}

		return EViewTargetBlendFunction::Linear;
	}

	APlayerCameraManager* ResolvePlayerCameraManager(FLuaActorProxy& ActorProxy)
	{
		AActor* Actor = ActorProxy.GetActor();
		if (!Actor || !Actor->GetWorld())
		{
			return nullptr;
		}

		AGameModeBase* GameMode = Actor->GetWorld()->GetAuthGameMode();
		return GameMode ? GameMode->GetPlayerCameraManager() : nullptr;
	}

	APlayerController* ResolvePlayerController(FLuaActorProxy& ActorProxy)
	{
		AActor* Actor = ActorProxy.GetActor();
		if (!Actor || !Actor->GetWorld())
		{
			return nullptr;
		}

		APlayerController* PlayerController = Cast<APlayerController>(Actor);
		if (PlayerController)
		{
			return PlayerController;
		}

		AGameModeBase* GameMode = Actor->GetWorld()->GetAuthGameMode();
		return GameMode ? GameMode->GetSpawnedController() : nullptr;
	}

	bool LuaStartCameraFade(
		FLuaActorProxy& ActorProxy,
		float FromAlpha,
		float ToAlpha,
		float Duration,
		float R,
		float G,
		float B,
		float A)
	{
		APlayerCameraManager* CameraManager = ResolvePlayerCameraManager(ActorProxy);
		if (!CameraManager)
		{
			return false;
		}

		CameraManager->StartCameraFade(
			FromAlpha,
			ToAlpha,
			Duration,
			FLinearColor(R, G, B, A));
		return true;
	}

	bool LuaEndCameraFade(FLuaActorProxy& ActorProxy)
	{
		APlayerCameraManager* CameraManager = ResolvePlayerCameraManager(ActorProxy);
		if (!CameraManager)
		{
			return false;
		}

		CameraManager->EndCameraFade();
		return true;
	}

	bool LuaSetViewTarget(FLuaActorProxy& ActorProxy, FLuaActorProxy& TargetProxy)
	{
		APlayerController* PlayerController = ResolvePlayerController(ActorProxy);
		AActor* TargetActor = TargetProxy.GetActor();
		if (!PlayerController || !TargetActor || !TargetActor->GetWorld())
		{
			return false;
		}

		PlayerController->SetViewTarget(TargetActor);
		return true;
	}

	bool LuaSetViewTargetWithBlend(
		FLuaActorProxy& ActorProxy,
		FLuaActorProxy& TargetProxy,
		float BlendTime,
		const FString& BlendFunctionName,
		float BlendExp,
		bool bLockOutgoing)
	{
		APlayerController* PlayerController = ResolvePlayerController(ActorProxy);
		AActor* TargetActor = TargetProxy.GetActor();
		if (!PlayerController || !TargetActor || !TargetActor->GetWorld())
		{
			return false;
		}

		PlayerController->SetViewTargetWithBlend(
			TargetActor,
			BlendTime,
			ParseBlendFunction(BlendFunctionName),
			BlendExp,
			bLockOutgoing);
		return true;
	}
}

void FLuaScriptRuntime::BindComponentProxyType()
{
	sol::state& Lua = GetLuaState();

	Lua.new_usertype<FLuaComponentProxy>(
		"ComponentProxy",
		"IsValid", &FLuaComponentProxy::IsValid,
		"Name", sol::property(&FLuaComponentProxy::GetName),
		"Owner", sol::property(&FLuaComponentProxy::GetOwner),
		"TypeName", sol::property(&FLuaComponentProxy::GetTypeName),
		"GetTypeName", &FLuaComponentProxy::GetTypeName,
		"SetActive", &FLuaComponentProxy::SetActive,
		"IsActive", &FLuaComponentProxy::IsActive,
		"SetVisible", &FLuaComponentProxy::SetVisible,
		"IsVisible", &FLuaComponentProxy::IsVisible,
		"GetWorldLocation", &FLuaComponentProxy::GetWorldLocation,
		"SetWorldLocation", sol::overload(
			static_cast<bool(FLuaComponentProxy::*)(const FVector&)>(&FLuaComponentProxy::SetWorldLocation),
			&FLuaComponentProxy::SetWorldLocationXYZ),
		"SetWorldLocationXYZ", &FLuaComponentProxy::SetWorldLocationXYZ,
		"GetLocalLocation", &FLuaComponentProxy::GetLocalLocation,
		"SetLocalLocation", sol::overload(
			static_cast<bool(FLuaComponentProxy::*)(const FVector&)>(&FLuaComponentProxy::SetLocalLocation),
			&FLuaComponentProxy::SetLocalLocationXYZ),
		"SetLocalLocationXYZ", &FLuaComponentProxy::SetLocalLocationXYZ,
		"AddWorldOffset", sol::overload(
			static_cast<bool(FLuaComponentProxy::*)(const FVector&)>(&FLuaComponentProxy::AddWorldOffset),
			&FLuaComponentProxy::AddWorldOffsetXYZ),
		"AddWorldOffsetXYZ", &FLuaComponentProxy::AddWorldOffsetXYZ,
		"AddLocalOffset", sol::overload(
			static_cast<bool(FLuaComponentProxy::*)(const FVector&)>(&FLuaComponentProxy::AddLocalOffset),
			&FLuaComponentProxy::AddLocalOffsetXYZ),
		"AddLocalOffsetXYZ", &FLuaComponentProxy::AddLocalOffsetXYZ,
		"GetWorldRotation", &FLuaComponentProxy::GetWorldRotation,
		"SetWorldRotation", sol::overload(
			static_cast<bool(FLuaComponentProxy::*)(const FRotator&)>(&FLuaComponentProxy::SetWorldRotation),
			&FLuaComponentProxy::SetWorldRotationXYZ),
		"SetWorldRotationXYZ", &FLuaComponentProxy::SetWorldRotationXYZ,
		"GetLocalRotation", &FLuaComponentProxy::GetLocalRotation,
		"SetLocalRotation", sol::overload(
			static_cast<bool(FLuaComponentProxy::*)(const FRotator&)>(&FLuaComponentProxy::SetLocalRotation),
			&FLuaComponentProxy::SetLocalRotationXYZ),
		"SetLocalRotationXYZ", &FLuaComponentProxy::SetLocalRotationXYZ,
		"GetWorldScale", &FLuaComponentProxy::GetWorldScale,
		"SetWorldScale", sol::overload(
			static_cast<bool(FLuaComponentProxy::*)(const FVector&)>(&FLuaComponentProxy::SetWorldScale),
			&FLuaComponentProxy::SetWorldScaleXYZ),
		"SetWorldScaleXYZ", &FLuaComponentProxy::SetWorldScaleXYZ,
		"GetLocalScale", &FLuaComponentProxy::GetLocalScale,
		"SetLocalScale", sol::overload(
			static_cast<bool(FLuaComponentProxy::*)(const FVector&)>(&FLuaComponentProxy::SetLocalScale),
			&FLuaComponentProxy::SetLocalScaleXYZ),
		"SetLocalScaleXYZ", &FLuaComponentProxy::SetLocalScaleXYZ,
		"GetForwardVector", &FLuaComponentProxy::GetForwardVector,
		"GetRightVector", &FLuaComponentProxy::GetRightVector,
		"GetUpVector", &FLuaComponentProxy::GetUpVector,
		"SetCollisionEnabled", &FLuaComponentProxy::SetCollisionEnabled,
		"SetGenerateOverlapEvents", &FLuaComponentProxy::SetGenerateOverlapEvents,
		"IsOverlappingActor", &FLuaComponentProxy::IsOverlappingActor,
		"GetShapeType", &FLuaComponentProxy::GetShapeType,
		"GetShapeHalfHeight", &FLuaComponentProxy::GetShapeHalfHeight,
		"SetShapeHalfHeight", &FLuaComponentProxy::SetShapeHalfHeight,
		"GetShapeRadius", &FLuaComponentProxy::GetShapeRadius,
		"SetShapeRadius", &FLuaComponentProxy::SetShapeRadius,
		"GetShapeExtent", &FLuaComponentProxy::GetShapeExtent,
		"SetShapeExtent", &FLuaComponentProxy::SetShapeExtent,
		"SetStaticMesh", &FLuaComponentProxy::SetStaticMesh,
		"SetText", &FLuaComponentProxy::SetText,
		"GetText", &FLuaComponentProxy::GetText,
		"GetScreenPosition", &FLuaComponentProxy::GetScreenPosition,
		"SetScreenPosition", sol::overload(
			static_cast<bool(FLuaComponentProxy::*)(const FVector&)>(&FLuaComponentProxy::SetScreenPosition),
			&FLuaComponentProxy::SetScreenPositionXYZ),
		"SetScreenPositionXYZ", &FLuaComponentProxy::SetScreenPositionXYZ,
		"GetScreenSize", &FLuaComponentProxy::GetScreenSize,
		"SetScreenSize", sol::overload(
			static_cast<bool(FLuaComponentProxy::*)(const FVector&)>(&FLuaComponentProxy::SetScreenSize),
			&FLuaComponentProxy::SetScreenSizeXYZ),
		"SetScreenSizeXYZ", &FLuaComponentProxy::SetScreenSizeXYZ,
		"SetTexture", &FLuaComponentProxy::SetTexture,
		"GetTexturePath", &FLuaComponentProxy::GetTexturePath,
		"SetTint", sol::overload(
			static_cast<bool(FLuaComponentProxy::*)(const FVector&)>(&FLuaComponentProxy::SetTint),
			&FLuaComponentProxy::SetTintRGBA),
		"SetLabel", &FLuaComponentProxy::SetLabel,
		"GetLabel", &FLuaComponentProxy::GetLabel,
		"IsHovered", &FLuaComponentProxy::IsHovered,
		"IsPressed", &FLuaComponentProxy::IsPressed,
		"WasClicked", &FLuaComponentProxy::WasClicked,
		"SetAudioPath", &FLuaComponentProxy::SetAudioPath,
		"GetAudioPath", &FLuaComponentProxy::GetAudioPath,
		"SetAudioCategory", &FLuaComponentProxy::SetAudioCategory,
		"GetAudioCategory", &FLuaComponentProxy::GetAudioCategory,
		"SetAudioLooping", &FLuaComponentProxy::SetAudioLooping,
		"IsAudioLooping", &FLuaComponentProxy::IsAudioLooping,
		"PlayAudio", sol::overload(
			static_cast<bool(FLuaComponentProxy::*)()>(&FLuaComponentProxy::PlayAudio),
			&FLuaComponentProxy::PlayAudioPath),
		"StopAudio", &FLuaComponentProxy::StopAudio,
		"PauseAudio", &FLuaComponentProxy::PauseAudio,
		"ResumeAudio", &FLuaComponentProxy::ResumeAudio,
		"IsAudioPlaying", &FLuaComponentProxy::IsAudioPlaying,
		"SetSpeed", &FLuaComponentProxy::SetSpeed,
		"GetSpeed", &FLuaComponentProxy::GetSpeed,
		"MoveTo", sol::overload(
			static_cast<bool(FLuaComponentProxy::*)(const FVector&)>(&FLuaComponentProxy::MoveTo),
			&FLuaComponentProxy::MoveToXYZ),
		"MoveBy", sol::overload(
			static_cast<bool(FLuaComponentProxy::*)(const FVector&)>(&FLuaComponentProxy::MoveBy),
			&FLuaComponentProxy::MoveByXYZ),
		"StopMove", &FLuaComponentProxy::StopMove,
		"IsMoveDone", &FLuaComponentProxy::IsMoveDone,
			//ToDelete
		//"StartCameraShake", &FLuaComponentProxy::StartCameraShake,
		//"AddHitEffect", &FLuaComponentProxy::AddHitEffect,
		"SetBoxExtent", sol::overload(
			&FLuaComponentProxy::SetBoxExtent,
			&FLuaComponentProxy::SetBoxExtentXYZ),
		"GetBoxExtent", &FLuaComponentProxy::GetBoxExtent);
}

void FLuaScriptRuntime::BindActorProxyType()
{
	sol::state& Lua = GetLuaState();

	Lua.new_usertype<FLuaActorProxy>(
		"ActorProxy",
		"IsValid", &FLuaActorProxy::IsValid,
		"Name", sol::property(&FLuaActorProxy::GetName),
		"UUID", sol::property(&FLuaActorProxy::GetUUID),
		"Tag", sol::property(&FLuaActorProxy::GetTag, &FLuaActorProxy::SetTag),
		"Location", sol::property(&FLuaActorProxy::GetWorldLocation, &FLuaActorProxy::SetWorldLocation),
		"Rotation", sol::property(&FLuaActorProxy::GetWorldRotation, &FLuaActorProxy::SetWorldRotation),
		"Scale", sol::property(&FLuaActorProxy::GetWorldScale, &FLuaActorProxy::SetWorldScale),
		"Velocity", sol::property(&FLuaActorProxy::GetVelocity, &FLuaActorProxy::SetVelocity),
		"HasTag", &FLuaActorProxy::HasTag,
		"GetWorldLocation", &FLuaActorProxy::GetWorldLocation,
		"SetWorldLocation", sol::overload(
			static_cast<void(FLuaActorProxy::*)(const FVector&)>(&FLuaActorProxy::SetWorldLocation),
			&FLuaActorProxy::SetWorldLocationXYZ),
		"SetWorldLocationXYZ", &FLuaActorProxy::SetWorldLocationXYZ,
		"GetWorldRotation", &FLuaActorProxy::GetWorldRotation,
		"SetWorldRotation", sol::overload(
			static_cast<void(FLuaActorProxy::*)(const FRotator&)>(&FLuaActorProxy::SetWorldRotation),
			&FLuaActorProxy::SetWorldRotationXYZ),
		"SetWorldRotationXYZ", &FLuaActorProxy::SetWorldRotationXYZ,
		"GetWorldScale", &FLuaActorProxy::GetWorldScale,
		"SetWorldScale", sol::overload(
			static_cast<void(FLuaActorProxy::*)(const FVector&)>(&FLuaActorProxy::SetWorldScale),
			&FLuaActorProxy::SetWorldScaleXYZ),
		"SetWorldScaleXYZ", &FLuaActorProxy::SetWorldScaleXYZ,
		"GetComponent", &FLuaActorProxy::GetComponent,
		"GetComponentByType", &FLuaActorProxy::GetComponentByType,
		"FindComponentByClass", &FLuaActorProxy::FindComponentByClass,
		"GetScriptComponent", &FLuaActorProxy::GetScriptComponent,
		"GetStaticMeshComponent", &FLuaActorProxy::GetStaticMeshComponent,
		"AddWorldOffset", sol::overload(
			static_cast<void(FLuaActorProxy::*)(const FVector&)>(&FLuaActorProxy::AddWorldOffset),
			&FLuaActorProxy::AddWorldOffsetXYZ),
		"GetForwardVector", &FLuaActorProxy::GetForwardVector,
		"GetRightVector", &FLuaActorProxy::GetRightVector,
		"GetUpVector", &FLuaActorProxy::GetUpVector,
		"FindGround", [](const FLuaActorProxy& ActorProxy, float MaxDistance, float SkinWidth)
		{
			const FLuaGroundHit GroundHit = ActorProxy.FindGround(MaxDistance, SkinWidth);
			sol::table Result = FLuaScriptRuntime::Get().GetLuaState().create_table();
			Result["hit"] = GroundHit.bHit;
			Result["location"] = GroundHit.Location;
			Result["normal"] = GroundHit.Normal;
			Result["ground_z"] = GroundHit.GroundZ;
			Result["distance"] = GroundHit.Distance;
			Result["actor"] = GroundHit.Actor;
			Result["component"] = GroundHit.Component;
			return Result;
		},
		"MoveTo", sol::overload(
			static_cast<void(FLuaActorProxy::*)(const FVector&)>(&FLuaActorProxy::MoveTo),
			&FLuaActorProxy::MoveTo2D,
			&FLuaActorProxy::MoveTo3D),
		"MoveBy", sol::overload(
			static_cast<void(FLuaActorProxy::*)(const FVector&)>(&FLuaActorProxy::MoveBy),
			&FLuaActorProxy::MoveBy2D,
			&FLuaActorProxy::MoveBy3D),
		"MoveToActor", &FLuaActorProxy::MoveToActor,
		"StopMove", &FLuaActorProxy::StopMove,
		"IsMoveDone", &FLuaActorProxy::IsMoveDone,
		"SetMoveSpeed", &FLuaActorProxy::SetMoveSpeed,
		"GetMoveSpeed", &FLuaActorProxy::GetMoveSpeed,
		"GetDamage", &FLuaActorProxy::GetDamage,
		"SetDamage", &FLuaActorProxy::SetDamage,
		"PlayCameraModifier", [](FLuaActorProxy& ActorProxy, const FString& ScriptPath, sol::optional<sol::table> ParamsTable)
		{
			// Lua gameplay scripts can live on the pawn.
			// Camera playback is routed to the player controller so modifiers affect the current player view.
			AActor* Actor = ActorProxy.GetActor();
			APlayerController* PlayerController = Cast<APlayerController>(Actor);
			if (!PlayerController && Actor && Actor->GetWorld())
			{
				AGameModeBase* GameMode = Actor->GetWorld()->GetAuthGameMode();
				PlayerController = GameMode ? GameMode->GetSpawnedController() : nullptr;
			}

			if (!PlayerController)
			{
				return false;
			}

			TMap<FString, float> Params;
			if (ParamsTable)
			{
				for (auto& Pair : *ParamsTable)
				{
					sol::object Key = Pair.first.as<sol::object>();
					sol::object Value = Pair.second.as<sol::object>();
					if (Key.get_type() != sol::type::string || Value.get_type() != sol::type::number)
					{
						continue;
					}

					Params[Key.as<FString>()] = Value.as<float>();
				}
			}

			PlayerController->PlayCameraModifier(ScriptPath, Params);
			return true;
		},
		"StartLetterBoxing", [](FLuaActorProxy& ActorProxy, float AspectW, float AspectH) 
		{
				AActor* Actor = ActorProxy.GetActor();
				if (!Actor || !Actor->GetWorld())
				{
					return false;
				}
				AGameModeBase* GameMode = Actor->GetWorld()->GetAuthGameMode();
				APlayerCameraManager* CM = GameMode ? GameMode->GetPlayerCameraManager() : nullptr;
				if (!CM) return false;

				CM->StartLetterBoxing(AspectW, AspectH);
				return true;

		},
		"EndLetterBoxing", [](FLuaActorProxy& ActorProxy) 
		{
			AActor* Actor = ActorProxy.GetActor();
			if (!Actor || !Actor->GetWorld())
			{
				return false;
			}
			AGameModeBase* GameMode = Actor->GetWorld()->GetAuthGameMode();
			APlayerCameraManager* CM = GameMode ? GameMode->GetPlayerCameraManager() : nullptr;
			if (!CM) return false;

			CM->EndLetterBoxing();
			return true;
		},
		"StartCameraFade", &LuaStartCameraFade,
		"EndCameraFade", &LuaEndCameraFade,
		"SetViewTarget", &LuaSetViewTarget,
		"SetViewTargetWithBlend", &LuaSetViewTargetWithBlend,
		// TODO: Bind SetGammaCorrection once gameplay code has a public render-pipeline runtime-options access path.
		"PrintLocation", &FLuaActorProxy::PrintLocation,
		"Destroy", &FLuaActorProxy::Destroy);
}
