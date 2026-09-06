#include "AnimNotify_CameraShake.h"

#include "Component/Primitive/SkeletalMeshComponent.h"
#include "GameFramework/World.h"
#include "GameFramework/GameMode/PlayerController.h"
#include "GameFramework/Camera/PlayerCameraManager.h"
#include "GameFramework/Camera/PerlinNoiseCameraShake.h"
#include "GameFramework/Camera/WaveOscillatorCameraShake.h"
#include "Object/Object.h"
#include "Runtime/Engine.h"

namespace
{
	UWorld* ResolveWorld(USkeletalMeshComponent* MeshComp)
	{
		if (IsValid(MeshComp))
		{
			if (UWorld* World = MeshComp->GetWorld())
			{
				return World;
			}
		}
		return GEngine ? GEngine->GetWorld() : nullptr;
	}
}

void UAnimNotify_CameraShake::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* /*Anim*/)
{
	UWorld* World = ResolveWorld(MeshComp);
	if (!World) return;

	APlayerController* PC = World->GetFirstPlayerController();
	APlayerCameraManager* Manager = PC ? PC->GetPlayerCameraManager() : nullptr;
	if (!Manager) return;

	if (bUsePerlinNoise)
	{
		UCameraShakeBase* Base = Manager->StartCameraShake(UPerlinNoiseCameraShake::StaticClass(), Scale);
		if (UPerlinNoiseCameraShake* Shake = Cast<UPerlinNoiseCameraShake>(Base))
		{
			Shake->Duration      = Duration;
			Shake->BlendInTime   = BlendInTime;
			Shake->BlendOutTime  = BlendOutTime;
			Shake->LocAmplitude  = LocationAmplitude;
			Shake->LocFrequency  = LocationFrequency;
			Shake->RotAmplitude  = RotationAmplitude;
			Shake->RotFrequency  = RotationFrequency;
			Shake->FOVAmplitude  = FOVAmplitude;
			Shake->FOVFrequency  = FOVFrequency;
		}
	}
	else
	{
		UCameraShakeBase* Base = Manager->StartCameraShake(UWaveOscillatorCameraShake::StaticClass(), Scale);
		if (UWaveOscillatorCameraShake* Shake = Cast<UWaveOscillatorCameraShake>(Base))
		{
			Shake->Duration      = Duration;
			Shake->BlendInTime   = BlendInTime;
			Shake->BlendOutTime  = BlendOutTime;
			Shake->LocAmplitude  = LocationAmplitude;
			Shake->LocFrequency  = LocationFrequency;
			Shake->RotAmplitude  = RotationAmplitude;
			Shake->RotFrequency  = RotationFrequency;
			Shake->FOVAmplitude  = FOVAmplitude;
			Shake->FOVFrequency  = FOVFrequency;
		}
	}
}
