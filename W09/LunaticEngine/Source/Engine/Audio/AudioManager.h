#pragma once

#include "Audio/AudioTypes.h"
#include "Core/CoreTypes.h"
#include "Core/Singleton.h"

#include <memory>
#include <unordered_map>

class FAudioManager : public TSingleton<FAudioManager>
{
	friend class TSingleton<FAudioManager>;

public:
	FString PlayAudio(const FString& SoundPath, ESoundCategory Category, bool bLoop);
	FString PlaySFX(const FString& SoundPath, bool bLoop = false);
	FString PlayBackground(const FString& SoundPath, bool bLoop = true);
	void SetCategoryVolume(ESoundCategory Category, float Volume);
	float GetCategoryVolume(ESoundCategory Category) const;

	bool StopSound(const FString& Handle);
	bool PauseSound(const FString& Handle);
	bool ResumeSound(const FString& Handle);
	bool IsSoundPlaying(const FString& Handle);

	bool StopBackground();
	bool PauseBackground();
	bool ResumeBackground();
	bool IsBackgroundPlaying() const;

	void StopAll();
	void Update();
	void Shutdown();

private:
	FAudioManager();
	~FAudioManager();

	struct FAudioBackend;
	struct FPlayingSound;

	FString ResolveAbsolutePath(const FString& SoundPath) const;
	FString MakeAlias(ESoundCategory Category);
	bool EnsureInitialized();
	bool CloseSoundInternal(const FString& Alias, bool bErase);
	bool IsAliasPlaying(const FString& Alias) const;
	FPlayingSound* FindActiveSound(const FString& Alias);
	const FPlayingSound* FindActiveSound(const FString& Alias) const;
	void CleanupFinishedSounds();
	float GetClampedVolume(float Volume) const;

private:
	std::unordered_map<FString, std::unique_ptr<FPlayingSound>> ActiveSounds;
	std::unique_ptr<FAudioBackend> Backend;
	FString CurrentBackgroundAlias;
	uint64 NextAliasId = 1;
	float SFXVolume = 1.0f;
	float BackgroundVolume = 1.0f;
};
