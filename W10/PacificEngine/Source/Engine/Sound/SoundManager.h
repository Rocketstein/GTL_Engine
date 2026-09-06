#pragma once

#include <filesystem>
#include <string>

#include "fmod.hpp"
#include "Core/CoreTypes.h"
#include "Core/Singleton.h"
#include "Object/FName.h"
#pragma comment(lib, "ThirdParty/FMod/fmod_vc.lib")
#undef PlaySound

enum class ESoundBus
{
    Master,
	BGM,
	SFX,
	UI,
	Player,
	Ambience,
	Count
};

struct FSoundHandle
{
	uint32 Id = 0;
	uint32 Generation = 0;

	bool IsValid() const { return Id != 0; }
};

struct FSoundDesc
{
	FString Path;
	bool bStream = false; // BGM, 긴 Ambience
	bool bLoop = false;
	bool b3D = false;
};

struct FPlayingSound
{
	uint32 Generation = 0;
	FName SoundKey;
	ESoundBus Bus = ESoundBus::SFX;
	FMOD::Channel* Channel = nullptr;
	float Volume = 1.0f;
	bool bLoop = false;
};

struct FSoundVolumeSettings
{
	FSoundVolumeSettings()
	{
		BusVolumes.fill(1.0f);
	}

	float Master = 1.0f;
	TStaticArray<float, static_cast<size_t>(ESoundBus::Count)> BusVolumes = {};
};

class FSoundManager : public TSingleton<FSoundManager>
{
public:
	bool Initialize();
	void Shutdown();
	void Tick(float DeltaTime);

	bool LoadSound(const FName& Key, const FSoundDesc& Desc);
	FSoundHandle Play(const FName& Key, ESoundBus Bus, float Volume = 1.0f);
	FSoundHandle PlayLoop(const FName& Key, ESoundBus Bus, float Volume = 1.0f);

	void Stop(FSoundHandle Handle);
	void StopBus(ESoundBus Bus);
	void StopAll();

	void SetBusVolume(ESoundBus Bus, float Volume);
	float GetBusVolume(ESoundBus Bus) const;
	void SetMasterVolume(float Volume);
	float GetMasterVolume() const;
	void SetSoundVolume(FSoundHandle Handle, float Volume);
	void SetSoundPosition(FSoundHandle Handle, uint32 PositionMs);
	uint32 GetSoundPosition(FSoundHandle Handle) const;
	uint32 GetSoundLength(const FName& Key) const;
	bool IsPlaying(FSoundHandle Handle) const;

private:
	bool LoadAllSounds();
	FSoundHandle PlayInternal(const FName& Key, ESoundBus Bus, float Volume, bool bForceLoop);
	void ApplyStoredVolumes();
	void ApplyBusVolume(ESoundBus Bus);
	static bool IsSupportedSoundFile(const std::filesystem::path& Path);
	static FName MakeSoundKeyFromPath(const std::filesystem::path& SoundRoot, const std::filesystem::path& FilePath);
	static bool ShouldStreamByDefault(const std::filesystem::path& SoundRoot, const std::filesystem::path& FilePath);
	static bool ShouldLoopByDefault(const std::filesystem::path& SoundRoot, const std::filesystem::path& FilePath);

	FMOD::System* System = nullptr;
	TMap<FName, FMOD::Sound*> Sounds;
	TStaticArray<FMOD::ChannelGroup*, static_cast<size_t>(ESoundBus::Count)> BusGroups = {};
	TMap<uint32, FPlayingSound> PlayingSounds;
	FSoundVolumeSettings VolumeSettings;
	uint32 NextPlayingSoundId = 1;
	bool bInitialized = false;
};
