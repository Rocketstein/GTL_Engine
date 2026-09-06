#include "Component/SoundComponent.h"

#include "Audio/AudioManager.h"
#include "Core/AsciiUtils.h"
#include "Object/ObjectFactory.h"
#include "Resource/ResourceManager.h"
#include "Serialization/Archive.h"

#include <algorithm>
#include <cstring>

IMPLEMENT_CLASS(USoundComponent, UActorComponent)
HIDE_FROM_COMPONENT_LIST(USoundComponent)
IMPLEMENT_CLASS(USFXComponent, USoundComponent)
IMPLEMENT_CLASS(UBackgroundSoundComponent, USoundComponent)

namespace
{
	const char* GSoundCategoryNames[] = { "SFX", "Background" };
}

USoundComponent::USoundComponent() = default;

void USoundComponent::BeginPlay()
{
	UActorComponent::BeginPlay();

	if (bAutoPlay)
	{
		Play();
	}
}

void USoundComponent::EndPlay()
{
	if (bStopOnEndPlay)
	{
		Stop();
	}

	UActorComponent::EndPlay();
}

void USoundComponent::Serialize(FArchive& Ar)
{
	UActorComponent::Serialize(Ar);
	Ar << SoundName;
	Ar << Category;
	Ar << bLoop;
	Ar << bAutoPlay;
	Ar << bStopOnEndPlay;
}

void USoundComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	UActorComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "FilePath", EPropertyType::Name, &SoundName });
	OutProps.push_back({ "Stop On EndPlay", EPropertyType::Bool, &bStopOnEndPlay });
}

void USoundComponent::PostEditProperty(const char* PropertyName)
{
	UActorComponent::PostEditProperty(PropertyName);

	if (strcmp(PropertyName, "Category") == 0)
	{
		const uint8 CategoryValue = static_cast<uint8>(Category);
		if (CategoryValue > static_cast<uint8>(ESoundCategory::Background))
		{
			Category = ESoundCategory::SFX;
		}
	}
}

bool USoundComponent::Play()
{
	return PlayByName(SoundName);
}

bool USoundComponent::PlayPath(const FString& InSoundPath)
{
	if (InSoundPath.empty())
	{
		return false;
	}

	Stop();

	if (Category == ESoundCategory::Background)
	{
		ActiveHandle = FAudioManager::Get().PlayBackground(InSoundPath, bLoop);
	}
	else
	{
		ActiveHandle = FAudioManager::Get().PlaySFX(InSoundPath, bLoop);
	}

	return !ActiveHandle.empty();
}

bool USoundComponent::PlayByName(const FName& InSoundName)
{
	if (InSoundName.ToString().empty() || InSoundName == FName("None"))
	{
		return false;
	}

	const FSoundResource* SoundResource = FResourceManager::Get().FindSound(InSoundName);
	if (!SoundResource)
	{
		return false;
	}

	return PlayPath(SoundResource->Path);
}

bool USoundComponent::Stop()
{
	if (ActiveHandle.empty())
	{
		return false;
	}

	const bool bStopped = FAudioManager::Get().StopSound(ActiveHandle);
	ActiveHandle.clear();
	return bStopped;
}

bool USoundComponent::Pause()
{
	return ActiveHandle.empty() ? false : FAudioManager::Get().PauseSound(ActiveHandle);
}

bool USoundComponent::Resume()
{
	return ActiveHandle.empty() ? false : FAudioManager::Get().ResumeSound(ActiveHandle);
}

bool USoundComponent::IsPlaying() const
{
	return ActiveHandle.empty() ? false : FAudioManager::Get().IsSoundPlaying(ActiveHandle);
}

bool USoundComponent::TryParseCategory(const FString& InValue, ESoundCategory& OutCategory)
{
	FString Lower = InValue;
	AsciiUtils::ToLowerInPlace(Lower);

	if (Lower == "sfx")
	{
		OutCategory = ESoundCategory::SFX;
		return true;
	}

	if (Lower == "background" || Lower == "bgm" || Lower == "bg")
	{
		OutCategory = ESoundCategory::Background;
		return true;
	}

	return false;
}

FString USoundComponent::CategoryToString(ESoundCategory InCategory)
{
	return InCategory == ESoundCategory::Background ? "background" : "sfx";
}

USFXComponent::USFXComponent()
{
	Category = ESoundCategory::SFX;
	bLoop = false;
	bAutoPlay = false;
}

void USFXComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	USoundComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Auto Play", EPropertyType::Bool, &bAutoPlay });
}

void USFXComponent::PostEditProperty(const char* PropertyName)
{
	USoundComponent::PostEditProperty(PropertyName);
	Category = ESoundCategory::SFX;
	bLoop = false;
}

UBackgroundSoundComponent::UBackgroundSoundComponent()
{
	Category = ESoundCategory::Background;
	bLoop = true;
	bAutoPlay = true;
}

void UBackgroundSoundComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	USoundComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Loop", EPropertyType::Bool, &bLoop });
	OutProps.push_back({ "Auto Play", EPropertyType::Bool, &bAutoPlay });
}

void UBackgroundSoundComponent::PostEditProperty(const char* PropertyName)
{
	USoundComponent::PostEditProperty(PropertyName);
	Category = ESoundCategory::Background;
}
