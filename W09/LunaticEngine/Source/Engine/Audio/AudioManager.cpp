#include "Audio/AudioManager.h"

#include "Core/Log.h"
#include "Platform/Paths.h"
#include "Resource/ResourceManager.h"

#include "miniaudio/miniaudio.h"

#include <algorithm>
#include <filesystem>
#include <string>

namespace
{
	const char* GetCategoryName(ESoundCategory Category)
	{
		return Category == ESoundCategory::Background ? "Background" : "SFX";
	}
}

struct FAudioManager::FAudioBackend
{
	ma_engine Engine {};
	bool bInitialized = false;
};

struct FAudioManager::FPlayingSound
{
	FString Alias;
	FString SourcePath;
	ESoundCategory Category = ESoundCategory::SFX;
	bool bLoop = false;
	bool bPaused = false;
	ma_sound Sound {};
};

FAudioManager::FAudioManager() = default;

FAudioManager::~FAudioManager()
{
	Shutdown();
}

FString FAudioManager::PlayAudio(const FString& SoundPath, ESoundCategory Category, bool bLoop)
{
	CleanupFinishedSounds();

	const FString AbsolutePath = ResolveAbsolutePath(SoundPath);
	if (AbsolutePath.empty())
	{
		UE_LOG_CATEGORY(Audio, Error, "Failed to resolve sound path: %s", SoundPath.c_str());
		return FString();
	}

	if (!EnsureInitialized())
	{
		return FString();
	}

	if (Category == ESoundCategory::Background)
	{
		StopBackground();
	}

	const FString Alias = MakeAlias(Category);
	auto PlayingSound = std::make_unique<FPlayingSound>();
	PlayingSound->Alias = Alias;
	PlayingSound->SourcePath = AbsolutePath;
	PlayingSound->Category = Category;
	PlayingSound->bLoop = bLoop;

	const ma_uint32 SoundFlags = MA_SOUND_FLAG_NO_SPATIALIZATION
		| (Category == ESoundCategory::Background ? MA_SOUND_FLAG_STREAM : 0);

	const std::wstring WidePath = FPaths::ToWide(AbsolutePath);
	ma_result Result = ma_sound_init_from_file_w(
		&Backend->Engine,
		WidePath.c_str(),
		SoundFlags,
		nullptr,
		nullptr,
		&PlayingSound->Sound);

	if (Result != MA_SUCCESS)
	{
		UE_LOG_CATEGORY(
			Audio, Error, "Failed to load %s: %s | %s",
			GetCategoryName(Category),
			AbsolutePath.c_str(),
			ma_result_description(Result));
		return FString();
	}

	ma_sound_set_looping(&PlayingSound->Sound, bLoop ? MA_TRUE : MA_FALSE);
	ma_sound_set_volume(&PlayingSound->Sound, GetCategoryVolume(Category));

	Result = ma_sound_start(&PlayingSound->Sound);
	if (Result != MA_SUCCESS)
	{
		UE_LOG_CATEGORY(
			Audio, Error, "Failed to start %s: %s | %s",
			GetCategoryName(Category),
			AbsolutePath.c_str(),
			ma_result_description(Result));
		ma_sound_uninit(&PlayingSound->Sound);
		return FString();
	}

	if (Category == ESoundCategory::Background)
	{
		CurrentBackgroundAlias = Alias;
	}

	ActiveSounds.emplace(Alias, std::move(PlayingSound));

	UE_LOG_CATEGORY(Audio, Info, "Playing %s: %s", GetCategoryName(Category), AbsolutePath.c_str());
	return Alias;
}

FString FAudioManager::PlaySFX(const FString& SoundPath, bool bLoop)
{
	return PlayAudio(SoundPath, ESoundCategory::SFX, bLoop);
}

FString FAudioManager::PlayBackground(const FString& SoundPath, bool bLoop)
{
	return PlayAudio(SoundPath, ESoundCategory::Background, bLoop);
}

void FAudioManager::SetCategoryVolume(ESoundCategory Category, float Volume)
{
	const float ClampedVolume = GetClampedVolume(Volume);
	if (Category == ESoundCategory::Background)
	{
		BackgroundVolume = ClampedVolume;
	}
	else
	{
		SFXVolume = ClampedVolume;
	}

	for (auto& Pair : ActiveSounds)
	{
		FPlayingSound& PlayingSound = *Pair.second;
		if (PlayingSound.Category != Category)
		{
			continue;
		}

		ma_sound_set_volume(&PlayingSound.Sound, ClampedVolume);
	}
}

float FAudioManager::GetCategoryVolume(ESoundCategory Category) const
{
	return Category == ESoundCategory::Background ? BackgroundVolume : SFXVolume;
}

bool FAudioManager::StopSound(const FString& Handle)
{
	if (Handle.empty())
	{
		return false;
	}

	return CloseSoundInternal(Handle, true);
}

bool FAudioManager::PauseSound(const FString& Handle)
{
	FPlayingSound* PlayingSound = FindActiveSound(Handle);
	if (PlayingSound == nullptr)
	{
		return false;
	}

	if (!ma_sound_is_playing(&PlayingSound->Sound))
	{
		return true;
	}

	const ma_result Result = ma_sound_stop(&PlayingSound->Sound);
	if (Result != MA_SUCCESS)
	{
		UE_LOG_CATEGORY(Audio, Error, "Failed to pause sound: %s | %s", Handle.c_str(), ma_result_description(Result));
		return false;
	}

	PlayingSound->bPaused = true;
	return true;
}

bool FAudioManager::ResumeSound(const FString& Handle)
{
	FPlayingSound* PlayingSound = FindActiveSound(Handle);
	if (PlayingSound == nullptr)
	{
		return false;
	}

	const ma_result Result = ma_sound_start(&PlayingSound->Sound);
	if (Result != MA_SUCCESS)
	{
		UE_LOG_CATEGORY(Audio, Error, "Failed to resume sound: %s | %s", Handle.c_str(), ma_result_description(Result));
		return false;
	}

	PlayingSound->bPaused = false;
	return true;
}

bool FAudioManager::IsSoundPlaying(const FString& Handle)
{
	CleanupFinishedSounds();
	return IsAliasPlaying(Handle);
}

bool FAudioManager::StopBackground()
{
	if (CurrentBackgroundAlias.empty())
	{
		return false;
	}

	return StopSound(CurrentBackgroundAlias);
}

bool FAudioManager::PauseBackground()
{
	if (CurrentBackgroundAlias.empty())
	{
		return false;
	}

	return PauseSound(CurrentBackgroundAlias);
}

bool FAudioManager::ResumeBackground()
{
	if (CurrentBackgroundAlias.empty())
	{
		return false;
	}

	return ResumeSound(CurrentBackgroundAlias);
}

bool FAudioManager::IsBackgroundPlaying() const
{
	return !CurrentBackgroundAlias.empty() && IsAliasPlaying(CurrentBackgroundAlias);
}

void FAudioManager::StopAll()
{
	TArray<FString> Handles;
	Handles.reserve(ActiveSounds.size());
	for (const auto& Pair : ActiveSounds)
	{
		Handles.push_back(Pair.first);
	}

	for (const FString& Handle : Handles)
	{
		CloseSoundInternal(Handle, true);
	}
}

void FAudioManager::Update()
{
	CleanupFinishedSounds();
}

void FAudioManager::Shutdown()
{
	StopAll();
	CurrentBackgroundAlias.clear();

	if (Backend && Backend->bInitialized)
	{
		ma_engine_uninit(&Backend->Engine);
	}

	Backend.reset();
}

FString FAudioManager::ResolveAbsolutePath(const FString& SoundPath) const
{
	if (SoundPath.empty())
	{
		return FString();
	}

	std::filesystem::path Path(FPaths::ToWide(SoundPath));
	if (Path.is_absolute())
	{
		if (std::filesystem::exists(Path))
		{
			return FPaths::ToUtf8(Path.lexically_normal().wstring());
		}
		return FString();
	}

	if (const FSoundResource* SoundResource = FResourceManager::Get().FindSound(FName(SoundPath)))
	{
		std::filesystem::path ResourcePath(FPaths::ToWide(SoundResource->Path));
		std::filesystem::path FullResourcePath = (std::filesystem::path(FPaths::RootDir()) / ResourcePath).lexically_normal();
		if (std::filesystem::exists(FullResourcePath))
		{
			return FPaths::ToUtf8(FullResourcePath.wstring());
		}
	}

	std::filesystem::path Root(FPaths::RootDir());
	std::filesystem::path Combined = (Root / Path).lexically_normal();
	if (std::filesystem::exists(Combined))
	{
		return FPaths::ToUtf8(Combined.wstring());
	}

	return FString();
}

FString FAudioManager::MakeAlias(ESoundCategory Category)
{
	const char* Prefix = Category == ESoundCategory::Background ? "bg" : "sfx";
	return FString(Prefix) + "_" + std::to_string(NextAliasId++);
}

bool FAudioManager::EnsureInitialized()
{
	if (!Backend)
	{
		Backend = std::make_unique<FAudioBackend>();
	}

	if (Backend->bInitialized)
	{
		return true;
	}

	ma_engine_config Config = ma_engine_config_init();
	const ma_result Result = ma_engine_init(&Config, &Backend->Engine);
	if (Result != MA_SUCCESS)
	{
		UE_LOG_CATEGORY(Audio, Error, "Failed to initialize miniaudio engine: %s", ma_result_description(Result));
		Backend.reset();
		return false;
	}

	Backend->bInitialized = true;
	UE_LOG_CATEGORY(Audio, Info, "miniaudio engine initialized");
	return true;
}

bool FAudioManager::CloseSoundInternal(const FString& Alias, bool bErase)
{
	FPlayingSound* PlayingSound = FindActiveSound(Alias);
	if (PlayingSound == nullptr)
	{
		return false;
	}

	const ma_result StopResult = ma_sound_stop(&PlayingSound->Sound);
	if (StopResult != MA_SUCCESS && StopResult != MA_INVALID_OPERATION)
	{
		UE_LOG_CATEGORY(Audio, Error, "Failed to stop sound: %s | %s", Alias.c_str(), ma_result_description(StopResult));
	}

	ma_sound_uninit(&PlayingSound->Sound);

	if (bErase)
	{
		ActiveSounds.erase(Alias);
		if (CurrentBackgroundAlias == Alias)
		{
			CurrentBackgroundAlias.clear();
		}
	}

	return true;
}

bool FAudioManager::IsAliasPlaying(const FString& Alias) const
{
	const FPlayingSound* PlayingSound = FindActiveSound(Alias);
	if (PlayingSound == nullptr)
	{
		return false;
	}

	return ma_sound_is_playing(&PlayingSound->Sound) == MA_TRUE;
}

FAudioManager::FPlayingSound* FAudioManager::FindActiveSound(const FString& Alias)
{
	const auto It = ActiveSounds.find(Alias);
	return It != ActiveSounds.end() ? It->second.get() : nullptr;
}

const FAudioManager::FPlayingSound* FAudioManager::FindActiveSound(const FString& Alias) const
{
	const auto It = ActiveSounds.find(Alias);
	return It != ActiveSounds.end() ? It->second.get() : nullptr;
}

void FAudioManager::CleanupFinishedSounds()
{
	TArray<FString> FinishedAliases;

	for (const auto& Pair : ActiveSounds)
	{
		const FPlayingSound& Sound = *Pair.second;
		if (Sound.bLoop || Sound.bPaused)
		{
			continue;
		}

		if (ma_sound_is_playing(&Sound.Sound) == MA_FALSE)
		{
			FinishedAliases.push_back(Pair.first);
		}
	}

	for (const FString& Alias : FinishedAliases)
	{
		CloseSoundInternal(Alias, true);
	}
}

float FAudioManager::GetClampedVolume(float Volume) const
{
	return (std::clamp)(Volume, 0.0f, 1.0f);
}
