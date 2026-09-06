#include "Scripting/LuaComponentProxy.h"

#include "Component/ActorComponent.h"
#include "Component/Movement/InterpToMovementComponent.h"
#include "Component/Movement/MovementComponent.h"
#include "Component/Movement/ProjectileMovementComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Component/SceneComponent.h"
#include "Component/SoundComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Component/UIButtonComponent.h"
#include "Component/UIImageComponent.h"
#include "Component/TextRenderComponent.h"
#include "Component/UIScreenTextComponent.h"
#include "Engine/Runtime/Engine.h"
#include "GameFramework/AActor.h"
#include "Mesh/ObjManager.h"
#include "Object/Object.h"
#include "Object/UClass.h"
#include "Scripting/LuaActorProxy.h"
#include "Component/CameraComponent.h"
#include "Component/Shape/BoxComponent.h"
#include "Component/Shape/CapsuleComponent.h"
#include "Component/Shape/SphereComponent.h"
#include "Scripting/LuaActorProxy.h"
#include <algorithm>
#include <cmath>

#ifdef check
#pragma push_macro("check")
#undef check
#define LUA_COMPONENT_RESTORE_CHECK_MACRO
#endif

#ifdef checkf
#pragma push_macro("checkf")
#undef checkf
#define LUA_COMPONENT_RESTORE_CHECKF_MACRO
#endif

#include <sol/sol.hpp>

#ifdef LUA_COMPONENT_RESTORE_CHECKF_MACRO
#pragma pop_macro("checkf")
#undef LUA_COMPONENT_RESTORE_CHECKF_MACRO
#endif

#ifdef LUA_COMPONENT_RESTORE_CHECK_MACRO
#pragma pop_macro("check")
#undef LUA_COMPONENT_RESTORE_CHECK_MACRO
#endif

namespace
{
	AActor* ResolveAliveActor(AActor* Actor)
	{
		return (Actor && IsAliveObject(Actor)) ? Actor : nullptr;
	}

	UActorComponent* ResolveAliveComponent(UActorComponent* Component)
	{
		if (!Component || !IsAliveObject(Component))
		{
			return nullptr;
		}

		AActor* OwnerActor = ResolveAliveActor(Component->GetOwner());
		if (!OwnerActor)
		{
			return nullptr;
		}

		const TArray<UActorComponent*>& OwnerComponents = OwnerActor->GetComponents();
		const auto It = std::find(OwnerComponents.begin(), OwnerComponents.end(), Component);
		return (It != OwnerComponents.end()) ? Component : nullptr;
	}

	FString StripLuaClassPrefix(const FString& Name)
	{
		if (Name.size() > 1 && Name[0] == 'U')
		{
			return Name.substr(1);
		}

		return Name;
	}
}

bool FLuaComponentProxy::IsValid() const
{
	// Proxy는 Component를 소유하지 않는 약한 참조다.
	return GetComponent() != nullptr;
}

UActorComponent* FLuaComponentProxy::GetComponent() const
{
	// Lua에 전달된 Proxy가 오래 보관된 뒤 호출될 수 있으므로 매번 생존 여부를 다시 확인한다.
	return ResolveAliveComponent(Component);
}

FString FLuaComponentProxy::GetName() const
{
	UActorComponent* TargetComponent = GetComponent();
	return TargetComponent ? TargetComponent->GetFName().ToString() : FString();
}

FString FLuaComponentProxy::GetTypeName() const
{
	UActorComponent* TargetComponent = GetComponent();
	if (!TargetComponent || !TargetComponent->GetClass())
	{
		return FString();
	}

	return StripLuaClassPrefix(TargetComponent->GetClass()->GetName());
}

FLuaActorProxy FLuaComponentProxy::GetOwner() const
{
	FLuaActorProxy OwnerProxy;

	UActorComponent* TargetComponent = GetComponent();
	if (!TargetComponent)
	{
		return OwnerProxy;
	}

	// Owner도 ActorProxy로 감싸서 반환한다.
	OwnerProxy.Actor = ResolveAliveActor(TargetComponent->GetOwner());
	return OwnerProxy;
}

bool FLuaComponentProxy::SetActive(bool bActive)
{
	UActorComponent* TargetComponent = GetComponent();
	if (!TargetComponent)
	{
		return false;
	}

	TargetComponent->SetActive(bActive);
	return true;
}

bool FLuaComponentProxy::IsActive() const
{
	UActorComponent* TargetComponent = GetComponent();
	return TargetComponent ? TargetComponent->IsActive() : false;
}

bool FLuaComponentProxy::SetVisible(bool bVisible)
{
	UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(GetComponent());
	if (!PrimitiveComponent)
	{
		return false;
	}

	PrimitiveComponent->SetVisibility(bVisible);
	return true;
}

bool FLuaComponentProxy::IsVisible() const
{
	UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(GetComponent());
	return PrimitiveComponent ? PrimitiveComponent->IsVisible() : false;
}

sol::optional<FVector> FLuaComponentProxy::GetWorldLocation() const
{
	USceneComponent* SceneComponent = Cast<USceneComponent>(GetComponent());
	if (!SceneComponent)
	{
		return sol::nullopt;
	}

	return SceneComponent->GetWorldLocation();
}

bool FLuaComponentProxy::SetWorldLocation(const FVector& InLocation)
{
	USceneComponent* SceneComponent = Cast<USceneComponent>(GetComponent());
	if (!SceneComponent)
	{
		return false;
	}

	SceneComponent->SetWorldLocation(InLocation);
	return true;
}

bool FLuaComponentProxy::SetWorldLocationXYZ(float X, float Y, float Z)
{
	return SetWorldLocation(FVector(X, Y, Z));
}


bool FLuaComponentProxy::SetLocalLocation(const FVector& InLocation)
{
	USceneComponent* SceneComponent = Cast<USceneComponent>(GetComponent());
	if (!SceneComponent)
	{
		return false;
	}

	SceneComponent->SetRelativeLocation(InLocation);
	return true;
}

bool FLuaComponentProxy::SetLocalLocationXYZ(float X, float Y, float Z)
{
	return SetLocalLocation(FVector(X, Y, Z));
}

bool FLuaComponentProxy::AddWorldOffset(const FVector& Delta)
{
	USceneComponent* SceneComponent = Cast<USceneComponent>(GetComponent());
	if (!SceneComponent)
	{
		return false;
	}

	SceneComponent->AddWorldOffset(Delta);
	return true;
}

bool FLuaComponentProxy::AddWorldOffsetXYZ(float X, float Y, float Z)
{
	return AddWorldOffset(FVector(X, Y, Z));
}

bool FLuaComponentProxy::AddLocalOffset(const FVector& Delta)
{
	USceneComponent* SceneComponent = Cast<USceneComponent>(GetComponent());
	if (!SceneComponent)
	{
		return false;
	}

	SceneComponent->SetRelativeLocation(SceneComponent->GetRelativeLocation() + Delta);
	return true;
}

bool FLuaComponentProxy::AddLocalOffsetXYZ(float X, float Y, float Z)
{
	return AddLocalOffset(FVector(X, Y, Z));
}

sol::optional<FRotator> FLuaComponentProxy::GetWorldRotation() const
{
	USceneComponent* SceneComponent = Cast<USceneComponent>(GetComponent());
	if (!SceneComponent)
	{
		return sol::nullopt;
	}

	return SceneComponent->GetComponentRotation();
}

bool FLuaComponentProxy::SetWorldRotation(const FRotator& InRotation)
{
	USceneComponent* SceneComponent = Cast<USceneComponent>(GetComponent());
	if (!SceneComponent)
	{
		return false;
	}

	SceneComponent->SetWorldRotation(InRotation);
	return true;
}

bool FLuaComponentProxy::SetWorldRotationXYZ(float Pitch, float Yaw, float Roll)
{
	return SetWorldRotation(FRotator(Pitch, Yaw, Roll));
}


bool FLuaComponentProxy::SetLocalRotation(const FRotator& InRotation)
{
	USceneComponent* SceneComponent = Cast<USceneComponent>(GetComponent());
	if (!SceneComponent)
	{
		return false;
	}

	SceneComponent->SetRelativeRotation(InRotation);
	return true;
}

bool FLuaComponentProxy::SetLocalRotationXYZ(float Pitch, float Yaw, float Roll)
{
	return SetLocalRotation(FRotator(Pitch, Yaw, Roll));
}

sol::optional<FRotator> FLuaComponentProxy::GetLocalRotation() const
{
	USceneComponent* SceneComponent = Cast<USceneComponent>(GetComponent());
	if (!SceneComponent)
	{
		return sol::nullopt;
	}

	return SceneComponent->GetRelativeRotation();
}

sol::optional<FVector> FLuaComponentProxy::GetWorldScale() const
{
	USceneComponent* SceneComponent = Cast<USceneComponent>(GetComponent());
	if (!SceneComponent)
	{
		return sol::nullopt;
	}

	return SceneComponent->GetWorldScale();
}

bool FLuaComponentProxy::SetWorldScale(const FVector& InScale)
{
	USceneComponent* SceneComponent = Cast<USceneComponent>(GetComponent());
	if (!SceneComponent)
	{
		return false;
	}

	// TODO: USceneComponent에 World Scale setter가 추가되면 그 API로 교체한다.
	SceneComponent->SetRelativeScale(InScale);
	return true;
}

bool FLuaComponentProxy::SetWorldScaleXYZ(float X, float Y, float Z)
{
	return SetWorldScale(FVector(X, Y, Z));
}
sol::optional<FVector> FLuaComponentProxy::GetLocalLocation() const
{
	USceneComponent* SceneComponent = Cast<USceneComponent>(GetComponent());
	if (!SceneComponent)
	{
		return sol::nullopt;
	}

	return SceneComponent->GetRelativeLocation();
}



bool FLuaComponentProxy::SetLocalScale(const FVector& InScale)
{
	USceneComponent* SceneComponent = Cast<USceneComponent>(GetComponent());
	if (!SceneComponent)
	{
		return false;
	}

	SceneComponent->SetRelativeScale(InScale);
	return true;
}

bool FLuaComponentProxy::SetLocalScaleXYZ(float X, float Y, float Z)
{
	return SetLocalScale(FVector(X, Y, Z));
}

sol::optional<FVector> FLuaComponentProxy::GetLocalScale() const
{
	USceneComponent* SceneComponent = Cast<USceneComponent>(GetComponent());
	if (!SceneComponent)
	{
		return sol::nullopt;
	}

	return SceneComponent->GetRelativeScale();
}

sol::optional<FVector> FLuaComponentProxy::GetForwardVector() const
{
	UActorComponent* Comp = GetComponent();
	if (USceneComponent* SceneComp = Cast<USceneComponent>(Comp))
	{
		return SceneComp->GetForwardVector();
	}
	return sol::nullopt;
}

sol::optional<FVector> FLuaComponentProxy::GetRightVector() const
{
	UActorComponent* Comp = GetComponent();
	if (USceneComponent* SceneComp = Cast<USceneComponent>(Comp))
	{
		return SceneComp->GetRightVector();
	}
	return sol::nullopt;
}

sol::optional<FVector> FLuaComponentProxy::GetUpVector() const
{
	UActorComponent* Comp = GetComponent();
	if (USceneComponent* SceneComp = Cast<USceneComponent>(Comp))
	{
		return SceneComp->GetUpVector();
	}
	return sol::nullopt;
}

bool FLuaComponentProxy::SetCollisionEnabled(bool bEnabled)
{
	UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(GetComponent());
	if (!PrimitiveComponent)
	{
		return false;
	}

	PrimitiveComponent->SetCollisionEnabled(bEnabled);
	return true;
}

bool FLuaComponentProxy::SetGenerateOverlapEvents(bool bEnabled)
{
	UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(GetComponent());
	if (!PrimitiveComponent)
	{
		return false;
	}

	PrimitiveComponent->SetGenerateOverlapEvents(bEnabled);
	return true;
}

bool FLuaComponentProxy::IsOverlappingActor(const FLuaActorProxy& OtherActor) const
{
	UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(GetComponent());
	AActor* TargetActor = OtherActor.GetActor();
	if (!PrimitiveComponent || !TargetActor)
	{
		return false;
	}

	return PrimitiveComponent->IsOverlappingActor(TargetActor);
}

FString FLuaComponentProxy::GetShapeType() const
{
	UActorComponent* TargetComponent = GetComponent();
	if (Cast<UBoxComponent>(TargetComponent))
	{
		return "Box";
	}
	if (Cast<USphereComponent>(TargetComponent))
	{
		return "Sphere";
	}
	if (Cast<UCapsuleComponent>(TargetComponent))
	{
		return "Capsule";
	}

	return "Unknown";
}

sol::optional<float> FLuaComponentProxy::GetShapeHalfHeight() const
{
	UActorComponent* TargetComponent = GetComponent();
	if (UBoxComponent* BoxComponent = Cast<UBoxComponent>(TargetComponent))
	{
		return BoxComponent->GetBoxExtent().Z;
	}
	if (USphereComponent* SphereComponent = Cast<USphereComponent>(TargetComponent))
	{
		return SphereComponent->GetSphereRadius();
	}
	if (UCapsuleComponent* CapsuleComponent = Cast<UCapsuleComponent>(TargetComponent))
	{
		return CapsuleComponent->GetCapsuleHalfHeight();
	}

	return sol::nullopt;
}

bool FLuaComponentProxy::SetShapeHalfHeight(float HalfHeight)
{
	const float SafeHalfHeight = (std::max)(0.01f, std::isfinite(HalfHeight) ? HalfHeight : 1.0f);
	UActorComponent* TargetComponent = GetComponent();
	if (UBoxComponent* BoxComponent = Cast<UBoxComponent>(TargetComponent))
	{
		FVector Extent = BoxComponent->GetBoxExtent();
		Extent.Z = SafeHalfHeight;
		BoxComponent->SetBoxExtent(Extent);
		BoxComponent->MarkWorldBoundsDirty();
		BoxComponent->MarkUpdateOverlaps();
		return true;
	}
	if (USphereComponent* SphereComponent = Cast<USphereComponent>(TargetComponent))
	{
		SphereComponent->SetSphereRadius(SafeHalfHeight);
		SphereComponent->MarkWorldBoundsDirty();
		SphereComponent->MarkUpdateOverlaps();
		return true;
	}
	if (UCapsuleComponent* CapsuleComponent = Cast<UCapsuleComponent>(TargetComponent))
	{
		CapsuleComponent->SetCapsuleHalfHeight(SafeHalfHeight);
		CapsuleComponent->MarkWorldBoundsDirty();
		CapsuleComponent->MarkUpdateOverlaps();
		return true;
	}

	return false;
}

sol::optional<float> FLuaComponentProxy::GetShapeRadius() const
{
	UActorComponent* TargetComponent = GetComponent();
	if (UBoxComponent* BoxComponent = Cast<UBoxComponent>(TargetComponent))
	{
		const FVector Extent = BoxComponent->GetBoxExtent();
		return (std::max)(Extent.X, Extent.Y);
	}
	if (USphereComponent* SphereComponent = Cast<USphereComponent>(TargetComponent))
	{
		return SphereComponent->GetSphereRadius();
	}
	if (UCapsuleComponent* CapsuleComponent = Cast<UCapsuleComponent>(TargetComponent))
	{
		return CapsuleComponent->GetCapsuleRadius();
	}

	return sol::nullopt;
}

bool FLuaComponentProxy::SetShapeRadius(float Radius)
{
	const float SafeRadius = (std::max)(0.01f, std::isfinite(Radius) ? Radius : 1.0f);
	UActorComponent* TargetComponent = GetComponent();
	if (UBoxComponent* BoxComponent = Cast<UBoxComponent>(TargetComponent))
	{
		FVector Extent = BoxComponent->GetBoxExtent();
		Extent.X = SafeRadius;
		Extent.Y = SafeRadius;
		BoxComponent->SetBoxExtent(Extent);
		BoxComponent->MarkWorldBoundsDirty();
		BoxComponent->MarkUpdateOverlaps();
		return true;
	}
	if (USphereComponent* SphereComponent = Cast<USphereComponent>(TargetComponent))
	{
		SphereComponent->SetSphereRadius(SafeRadius);
		SphereComponent->MarkWorldBoundsDirty();
		SphereComponent->MarkUpdateOverlaps();
		return true;
	}
	if (UCapsuleComponent* CapsuleComponent = Cast<UCapsuleComponent>(TargetComponent))
	{
		CapsuleComponent->SetCapsuleRadius(SafeRadius);
		CapsuleComponent->MarkWorldBoundsDirty();
		CapsuleComponent->MarkUpdateOverlaps();
		return true;
	}

	return false;
}

sol::optional<FVector> FLuaComponentProxy::GetShapeExtent() const
{
	UActorComponent* TargetComponent = GetComponent();
	if (UBoxComponent* BoxComponent = Cast<UBoxComponent>(TargetComponent))
	{
		return BoxComponent->GetBoxExtent();
	}
	if (USphereComponent* SphereComponent = Cast<USphereComponent>(TargetComponent))
	{
		const float Radius = SphereComponent->GetSphereRadius();
		return FVector(Radius, Radius, Radius);
	}
	if (UCapsuleComponent* CapsuleComponent = Cast<UCapsuleComponent>(TargetComponent))
	{
		return FVector(
			CapsuleComponent->GetCapsuleRadius(),
			CapsuleComponent->GetCapsuleRadius(),
			CapsuleComponent->GetCapsuleHalfHeight());
	}

	return sol::nullopt;
}

bool FLuaComponentProxy::SetShapeExtent(const FVector& Extent)
{
	const FVector SafeExtent(
		(std::max)(0.01f, std::isfinite(Extent.X) ? Extent.X : 1.0f),
		(std::max)(0.01f, std::isfinite(Extent.Y) ? Extent.Y : 1.0f),
		(std::max)(0.01f, std::isfinite(Extent.Z) ? Extent.Z : 1.0f)
	);

	UActorComponent* TargetComponent = GetComponent();
	if (UBoxComponent* BoxComponent = Cast<UBoxComponent>(TargetComponent))
	{
		BoxComponent->SetBoxExtent(SafeExtent);
		BoxComponent->MarkWorldBoundsDirty();
		BoxComponent->MarkUpdateOverlaps();
		return true;
	}
	if (USphereComponent* SphereComponent = Cast<USphereComponent>(TargetComponent))
	{
		const float Radius = (std::max)((std::max)(SafeExtent.X, SafeExtent.Y), SafeExtent.Z);
		SphereComponent->SetSphereRadius(Radius);
		SphereComponent->MarkWorldBoundsDirty();
		SphereComponent->MarkUpdateOverlaps();
		return true;
	}
	if (UCapsuleComponent* CapsuleComponent = Cast<UCapsuleComponent>(TargetComponent))
	{
		const float Radius = (std::max)(SafeExtent.X, SafeExtent.Y);
		CapsuleComponent->SetCapsuleHalfHeight((std::max)(SafeExtent.Z, Radius));
		CapsuleComponent->SetCapsuleRadius(Radius);
		CapsuleComponent->MarkWorldBoundsDirty();
		CapsuleComponent->MarkUpdateOverlaps();
		return true;
	}

	return false;
}

bool FLuaComponentProxy::SetStaticMesh(const FString& MeshPath)
{
	UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(GetComponent());
	if (!StaticMeshComponent || MeshPath.empty() || MeshPath == "None")
	{
		return false;
	}

	ID3D11Device* Device = GEngine ? GEngine->GetRenderer().GetFD3DDevice().GetDevice() : nullptr;
	if (!Device)
	{
		return false;
	}

	// 현재 프로젝트의 StaticMeshComponent와 같은 OBJ 로딩 경로를 그대로 사용한다.
	UStaticMesh* LoadedMesh = FObjManager::LoadObjStaticMesh(MeshPath, Device);
	if (!LoadedMesh)
	{
		return false;
	}

	StaticMeshComponent->SetStaticMesh(LoadedMesh);
	return true;
}

bool FLuaComponentProxy::SetText(const FString& Text)
{
	UTextRenderComponent* TextComponent = Cast<UTextRenderComponent>(GetComponent());
	if (TextComponent)
	{
		TextComponent->SetText(Text);
		TextComponent->PostEditProperty("Text");
		return true;
	}

	UUIScreenTextComponent* ScreenTextComponent = Cast<UUIScreenTextComponent>(GetComponent());
	if (!ScreenTextComponent)
	{
		return false;
	}

	ScreenTextComponent->SetText(Text);
	ScreenTextComponent->PostEditProperty("Text");
	return true;
}

sol::optional<FString> FLuaComponentProxy::GetText() const
{
	UTextRenderComponent* TextComponent = Cast<UTextRenderComponent>(GetComponent());
	if (TextComponent)
	{
		return TextComponent->GetText();
	}

	UUIScreenTextComponent* ScreenTextComponent = Cast<UUIScreenTextComponent>(GetComponent());
	if (!ScreenTextComponent)
	{
		return sol::nullopt;
	}

	return ScreenTextComponent->GetText();
}

sol::optional<FVector> FLuaComponentProxy::GetScreenPosition() const
{
	if (UUIImageComponent* ImageComponent = Cast<UUIImageComponent>(GetComponent()))
	{
		return ImageComponent->GetScreenPosition();
	}

	if (UUIScreenTextComponent* ScreenTextComponent = Cast<UUIScreenTextComponent>(GetComponent()))
	{
		return ScreenTextComponent->GetScreenPosition();
	}

	return sol::nullopt;
}

bool FLuaComponentProxy::SetScreenPosition(const FVector& InScreenPosition)
{
	if (UUIImageComponent* ImageComponent = Cast<UUIImageComponent>(GetComponent()))
	{
		ImageComponent->SetScreenPosition(InScreenPosition);
		return true;
	}

	if (UUIScreenTextComponent* ScreenTextComponent = Cast<UUIScreenTextComponent>(GetComponent()))
	{
		ScreenTextComponent->SetScreenPosition(InScreenPosition);
		return true;
	}

	return false;
}

bool FLuaComponentProxy::SetScreenPositionXYZ(float X, float Y, float Z)
{
	return SetScreenPosition(FVector(X, Y, Z));
}

sol::optional<FVector> FLuaComponentProxy::GetScreenSize() const
{
	if (UUIImageComponent* ImageComponent = Cast<UUIImageComponent>(GetComponent()))
	{
		return ImageComponent->GetScreenSize();
	}

	return sol::nullopt;
}

bool FLuaComponentProxy::SetScreenSize(const FVector& InScreenSize)
{
	UUIImageComponent* ImageComponent = Cast<UUIImageComponent>(GetComponent());
	if (!ImageComponent)
	{
		return false;
	}

	ImageComponent->SetScreenSize(InScreenSize);
	return true;
}

bool FLuaComponentProxy::SetScreenSizeXYZ(float X, float Y, float Z)
{
	return SetScreenSize(FVector(X, Y, Z));
}

bool FLuaComponentProxy::SetTexture(const FString& TexturePath)
{
	UUIImageComponent* ImageComponent = Cast<UUIImageComponent>(GetComponent());
	if (!ImageComponent)
	{
		return false;
	}

	return ImageComponent->SetTexturePath(TexturePath);
}

sol::optional<FString> FLuaComponentProxy::GetTexturePath() const
{
	UUIImageComponent* ImageComponent = Cast<UUIImageComponent>(GetComponent());
	if (!ImageComponent)
	{
		return sol::nullopt;
	}

	return ImageComponent->GetTexturePath();
}

bool FLuaComponentProxy::SetTint(const FVector& TintRGB)
{
	return SetTintRGBA(TintRGB.X, TintRGB.Y, TintRGB.Z, 1.0f);
}

bool FLuaComponentProxy::SetTintRGBA(float R, float G, float B, float A)
{
	UUIImageComponent* ImageComponent = Cast<UUIImageComponent>(GetComponent());
	if (ImageComponent)
	{
		ImageComponent->SetTint(FVector4(R, G, B, A));
		return true;
	}

	UUIScreenTextComponent* ScreenTextComponent = Cast<UUIScreenTextComponent>(GetComponent());
	if (!ScreenTextComponent)
	{
		return false;
	}

	ScreenTextComponent->SetColor(FVector4(R, G, B, A));
	return true;
}

bool FLuaComponentProxy::SetLabel(const FString& Label)
{
	UIButtonComponent* ButtonComponent = Cast<UIButtonComponent>(GetComponent());
	if (!ButtonComponent)
	{
		return false;
	}

	ButtonComponent->SetLabel(Label);
	return true;
}

sol::optional<FString> FLuaComponentProxy::GetLabel() const
{
	UIButtonComponent* ButtonComponent = Cast<UIButtonComponent>(GetComponent());
	if (!ButtonComponent)
	{
		return sol::nullopt;
	}

	return ButtonComponent->GetLabel();
}

bool FLuaComponentProxy::IsHovered() const
{
	UIButtonComponent* ButtonComponent = Cast<UIButtonComponent>(GetComponent());
	return ButtonComponent ? ButtonComponent->IsHovered() : false;
}

bool FLuaComponentProxy::IsPressed() const
{
	UIButtonComponent* ButtonComponent = Cast<UIButtonComponent>(GetComponent());
	return ButtonComponent ? ButtonComponent->IsPressed() : false;
}

bool FLuaComponentProxy::WasClicked() const
{
	UIButtonComponent* ButtonComponent = Cast<UIButtonComponent>(GetComponent());
	return ButtonComponent ? ButtonComponent->WasClicked() : false;
}

bool FLuaComponentProxy::SetAudioPath(const FString& AudioPath)
{
	USoundComponent* SoundComponent = Cast<USoundComponent>(GetComponent());
	if (!SoundComponent)
	{
		return false;
	}

	SoundComponent->SetSound(FName(AudioPath));
	return true;
}

sol::optional<FString> FLuaComponentProxy::GetAudioPath() const
{
	USoundComponent* SoundComponent = Cast<USoundComponent>(GetComponent());
	if (!SoundComponent)
	{
		return sol::nullopt;
	}

	return SoundComponent->GetSound().ToString();
}

bool FLuaComponentProxy::SetAudioCategory(const FString& CategoryName)
{
	USoundComponent* SoundComponent = Cast<USoundComponent>(GetComponent());
	if (!SoundComponent)
	{
		return false;
	}

	ESoundCategory Category = ESoundCategory::SFX;
	if (!USoundComponent::TryParseCategory(CategoryName, Category))
	{
		return false;
	}

	SoundComponent->SetCategory(Category);
	return true;
}

sol::optional<FString> FLuaComponentProxy::GetAudioCategory() const
{
	USoundComponent* SoundComponent = Cast<USoundComponent>(GetComponent());
	if (!SoundComponent)
	{
		return sol::nullopt;
	}

	return USoundComponent::CategoryToString(SoundComponent->GetCategory());
}

bool FLuaComponentProxy::SetAudioLooping(bool bLooping)
{
	USoundComponent* SoundComponent = Cast<USoundComponent>(GetComponent());
	if (!SoundComponent)
	{
		return false;
	}

	SoundComponent->SetLooping(bLooping);
	return true;
}

bool FLuaComponentProxy::IsAudioLooping() const
{
	USoundComponent* SoundComponent = Cast<USoundComponent>(GetComponent());
	return SoundComponent ? SoundComponent->IsLooping() : false;
}

bool FLuaComponentProxy::PlayAudio()
{
	USoundComponent* SoundComponent = Cast<USoundComponent>(GetComponent());
	return SoundComponent ? SoundComponent->Play() : false;
}

bool FLuaComponentProxy::PlayAudioPath(const FString& AudioPath)
{
	USoundComponent* SoundComponent = Cast<USoundComponent>(GetComponent());
	return SoundComponent ? SoundComponent->PlayPath(AudioPath) : false;
}

bool FLuaComponentProxy::StopAudio()
{
	USoundComponent* SoundComponent = Cast<USoundComponent>(GetComponent());
	return SoundComponent ? SoundComponent->Stop() : false;
}

bool FLuaComponentProxy::PauseAudio()
{
	USoundComponent* SoundComponent = Cast<USoundComponent>(GetComponent());
	return SoundComponent ? SoundComponent->Pause() : false;
}

bool FLuaComponentProxy::ResumeAudio()
{
	USoundComponent* SoundComponent = Cast<USoundComponent>(GetComponent());
	return SoundComponent ? SoundComponent->Resume() : false;
}

bool FLuaComponentProxy::IsAudioPlaying() const
{
	USoundComponent* SoundComponent = Cast<USoundComponent>(GetComponent());
	return SoundComponent ? SoundComponent->IsPlaying() : false;
}

bool FLuaComponentProxy::SetSpeed(float Speed)
{
	const float SafeSpeed = (std::max)(0.0f, std::isfinite(Speed) ? Speed : 0.0f);

	if (UInterpToMovementComponent* InterpComponent = Cast<UInterpToMovementComponent>(GetComponent()))
	{
		InterpComponent->SetSpeed(SafeSpeed);
		return true;
	}

	if (UProjectileMovementComponent* ProjectileComponent = Cast<UProjectileMovementComponent>(GetComponent()))
	{
		ProjectileComponent->SetInitialSpeed(SafeSpeed);
		return true;
	}

	return false;
}

sol::optional<float> FLuaComponentProxy::GetSpeed() const
{
	if (UInterpToMovementComponent* InterpComponent = Cast<UInterpToMovementComponent>(GetComponent()))
	{
		return InterpComponent->GetSpeed();
	}

	if (UProjectileMovementComponent* ProjectileComponent = Cast<UProjectileMovementComponent>(GetComponent()))
	{
		return ProjectileComponent->GetInitialSpeed();
	}

	return sol::nullopt;
}

bool FLuaComponentProxy::MoveTo(const FVector& Target)
{
	if (UInterpToMovementComponent* InterpComponent = Cast<UInterpToMovementComponent>(GetComponent()))
	{
		InterpComponent->MoveTo(Target);
		return true;
	}

	if (UProjectileMovementComponent* ProjectileComponent = Cast<UProjectileMovementComponent>(GetComponent()))
	{
		USceneComponent* UpdatedComponent = ProjectileComponent->GetUpdatedComponent();
		if (!UpdatedComponent)
		{
			ProjectileComponent->ResolveUpdatedComponent();
			UpdatedComponent = ProjectileComponent->GetUpdatedComponent();
		}
		if (!UpdatedComponent)
		{
			return false;
		}

		FVector Direction = Target - UpdatedComponent->GetWorldLocation();
		if (Direction.IsNearlyZero())
		{
			ProjectileComponent->StopSimulating();
			return true;
		}

		Direction.Normalize();
		ProjectileComponent->SetVelocity(Direction);
		return true;
	}

	return false;
}

bool FLuaComponentProxy::MoveToXYZ(float X, float Y, float Z)
{
	return MoveTo(FVector(X, Y, Z));
}

bool FLuaComponentProxy::MoveBy(const FVector& Delta)
{
	if (UInterpToMovementComponent* InterpComponent = Cast<UInterpToMovementComponent>(GetComponent()))
	{
		InterpComponent->MoveBy(Delta);
		return true;
	}

	if (UProjectileMovementComponent* ProjectileComponent = Cast<UProjectileMovementComponent>(GetComponent()))
	{
		if (Delta.IsNearlyZero())
		{
			ProjectileComponent->StopSimulating();
			return true;
		}

		FVector Direction = Delta;
		Direction.Normalize();
		ProjectileComponent->SetVelocity(Direction);
		return true;
	}

	return false;
}

bool FLuaComponentProxy::MoveByXYZ(float X, float Y, float Z)
{
	return MoveBy(FVector(X, Y, Z));
}

bool FLuaComponentProxy::StopMove()
{
	if (UInterpToMovementComponent* InterpComponent = Cast<UInterpToMovementComponent>(GetComponent()))
	{
		InterpComponent->StopMove();
		return true;
	}

	if (UProjectileMovementComponent* ProjectileComponent = Cast<UProjectileMovementComponent>(GetComponent()))
	{
		ProjectileComponent->StopSimulating();
		return true;
	}

	return false;
}

bool FLuaComponentProxy::IsMoveDone() const
{
	if (UInterpToMovementComponent* InterpComponent = Cast<UInterpToMovementComponent>(GetComponent()))
	{
		return InterpComponent->IsMoveDone();
	}

	if (UProjectileMovementComponent* ProjectileComponent = Cast<UProjectileMovementComponent>(GetComponent()))
	{
		return ProjectileComponent->GetVelocity().IsNearlyZero();
	}

	return false;
}
//ToDelete
//bool FLuaComponentProxy::StartCameraShake(float Intensity, float Duration)
//{
//	UActorComponent* TargetComp = this->GetComponent();
//	if (TargetComp && TargetComp->IsA<UCameraComponent>())
//	{
//		UCameraComponent* Camera = static_cast<UCameraComponent*>(TargetComp);
//		Camera->StartCameraShake(Intensity, Duration);
//		return true;
//	}
//	return false;
//}
//
//bool FLuaComponentProxy::AddHitEffect(float Intensity, float Duration)
//{
//	UActorComponent* TargetComp = this->GetComponent();
//	if (TargetComp && TargetComp->IsA<UCameraComponent>())
//	{
//		UCameraComponent* Camera = static_cast<UCameraComponent*>(TargetComp);
//		Camera->AddHitEffect(Intensity, Duration);
//		return true;
//	}
//	return false;
//}

bool FLuaComponentProxy::SetBoxExtent(const FVector& Extent)
{
	UActorComponent* TargetComp = this->GetComponent();
	UBoxComponent* BoxComponent = (TargetComp && TargetComp->IsA<UBoxComponent>()) ? static_cast<UBoxComponent*>(TargetComp) : nullptr;
	if (!BoxComponent)
	{
	return false;
}

	const FVector SafeExtent(
		std::max(0.01f, Extent.X),
		std::max(0.01f, Extent.Y),
		std::max(0.01f, Extent.Z)
	);

	BoxComponent->SetBoxExtent(SafeExtent);
	BoxComponent->MarkTransformDirty();
	BoxComponent->MarkWorldBoundsDirty();

	return true;
}

bool FLuaComponentProxy::SetBoxExtentXYZ(float X, float Y, float Z)
{
	return SetBoxExtent(FVector(X, Y, Z));
}

sol::optional<FVector> FLuaComponentProxy::GetBoxExtent() const
{
	UBoxComponent* BoxComponent = Cast<UBoxComponent>(GetComponent());
	if (!BoxComponent)
	{
		return sol::nullopt;
	}

	return BoxComponent->GetBoxExtent();
}
