#pragma once

#include "Audio/AudioTypes.h"
#include "Component/ActorComponent.h"
#include "Object/FName.h"

class USoundComponent : public UActorComponent
{
public:
	DECLARE_CLASS(USoundComponent, UActorComponent)

	USoundComponent();

	void BeginPlay() override;
	void EndPlay() override;
	void Serialize(FArchive& Ar) override;
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void PostEditProperty(const char* PropertyName) override;

	bool Play();
	bool PlayPath(const FString& InSoundPath);
	bool PlayByName(const FName& InSoundName);
	bool Stop();
	bool Pause();
	bool Resume();
	bool IsPlaying() const;

	void SetSound(const FName& InSoundName) { SoundName = InSoundName; }
	const FName& GetSound() const { return SoundName; }

	void SetCategory(ESoundCategory InCategory) { Category = InCategory; }
	ESoundCategory GetCategory() const { return Category; }

	void SetLooping(bool bInLooping) { bLoop = bInLooping; }
	bool IsLooping() const { return bLoop; }

	static bool TryParseCategory(const FString& InValue, ESoundCategory& OutCategory);
	static FString CategoryToString(ESoundCategory InCategory);

protected:
	FName SoundName = FName("None");
	FString ActiveHandle;
	ESoundCategory Category = ESoundCategory::SFX;
	bool bLoop = false;
	bool bAutoPlay = false;
	bool bStopOnEndPlay = true;
};

class USFXComponent : public USoundComponent
{
public:
	DECLARE_CLASS(USFXComponent, USoundComponent)

	USFXComponent();

	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void PostEditProperty(const char* PropertyName) override;
};

class UBackgroundSoundComponent : public USoundComponent
{
public:
	DECLARE_CLASS(UBackgroundSoundComponent, USoundComponent)

	UBackgroundSoundComponent();

	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void PostEditProperty(const char* PropertyName) override;
};
