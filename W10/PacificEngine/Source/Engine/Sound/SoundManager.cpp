#include "SoundManager.h"

#include "Core/Logging/LogMacros.h"
#include "Platform/Paths.h"
#include "fmod_errors.h"

#include <algorithm>
#include <cctype>
#include <system_error>

namespace
{
constexpr int MaxFmodChannels = 512;

size_t GetBusIndex(ESoundBus Bus)
{
    return static_cast<size_t>(Bus);
}

bool IsValidBus(ESoundBus Bus)
{
    return GetBusIndex(Bus) < static_cast<size_t>(ESoundBus::Count);
}

const char* GetBusName(ESoundBus Bus)
{
    switch (Bus)
    {
    case ESoundBus::Master:
        return "Master";
    case ESoundBus::BGM:
        return "BGM";
    case ESoundBus::SFX:
        return "SFX";
    case ESoundBus::UI:
        return "UI";
    case ESoundBus::Player:
        return "Player";
    case ESoundBus::Ambience:
        return "Ambience";
    default:
        return "Unknown";
    }
}

bool CheckFmod(FMOD_RESULT Result, const char* Action)
{
    if (Result == FMOD_OK)
    {
        return true;
    }

    UE_LOG(Sound, Error, "%s failed: %s", Action, FMOD_ErrorString(Result));
    return false;
}

FString ToLowerString(FString Value)
{
    for (char& C : Value)
    {
        C = static_cast<char>(std::tolower(static_cast<unsigned char>(C)));
    }
    return Value;
}

std::filesystem::path MakeRelativePath(const std::filesystem::path& Root, const std::filesystem::path& Path)
{
    std::error_code Ec;
    std::filesystem::path Relative = std::filesystem::relative(Path, Root, Ec);
    if (Ec || Relative.empty())
    {
        Relative = Path.lexically_relative(Root);
    }
    return Relative;
}

bool IsLongSoundDirectory(const std::filesystem::path& SoundRoot, const std::filesystem::path& FilePath)
{
    const std::filesystem::path Relative = MakeRelativePath(SoundRoot, FilePath);
    auto It = Relative.begin();
    if (It == Relative.end())
    {
        return false;
    }

    const FString FirstDirectory = ToLowerString(FPaths::FromPath(*It));
    return FirstDirectory == "bgm" || FirstDirectory == "ambience";
}

float ClampVolume(float Volume)
{
    return std::clamp(Volume, 0.0f, 1.0f);
}
} // namespace

bool FSoundManager::Initialize()
{
    if (bInitialized)
    {
        return true;
    }

    BusGroups.fill(nullptr);

    if (!CheckFmod(FMOD::System_Create(&System), "FMOD::System_Create"))
    {
        System = nullptr;
        return false;
    }

    if (!CheckFmod(System->init(MaxFmodChannels, FMOD_INIT_NORMAL, nullptr), "FMOD::System::init"))
    {
        Shutdown();
        return false;
    }

    FMOD::ChannelGroup* MasterGroup = nullptr;
    if (!CheckFmod(System->getMasterChannelGroup(&MasterGroup), "FMOD::System::getMasterChannelGroup"))
    {
        Shutdown();
        return false;
    }

    BusGroups[GetBusIndex(ESoundBus::Master)] = MasterGroup;

    for (size_t Index = 0; Index < static_cast<size_t>(ESoundBus::Count); ++Index)
    {
        const ESoundBus Bus = static_cast<ESoundBus>(Index);
        if (Bus == ESoundBus::Master)
        {
            continue;
        }

        FMOD::ChannelGroup* Group = nullptr;
        if (!CheckFmod(System->createChannelGroup(GetBusName(Bus), &Group), "FMOD::System::createChannelGroup"))
        {
            Shutdown();
            return false;
        }

        if (!CheckFmod(MasterGroup->addGroup(Group), "FMOD::ChannelGroup::addGroup"))
        {
            if (Group)
            {
                Group->release();
            }
            Shutdown();
            return false;
        }

        BusGroups[Index] = Group;
    }

    ApplyStoredVolumes();
    bInitialized = true;

    if (!LoadAllSounds())
    {
        UE_LOG(Sound, Warning, "Sound folder scan completed with errors.");
    }

    UE_LOG(Sound, Info, "Sound manager initialized. Loaded %zu sounds.", Sounds.size());
    return true;
}

void FSoundManager::Shutdown()
{
    StopAll();

    for (auto& Pair : Sounds)
    {
        if (Pair.second)
        {
            Pair.second->release();
        }
    }
    Sounds.clear();

    for (size_t Index = 0; Index < static_cast<size_t>(ESoundBus::Count); ++Index)
    {
        if (Index == GetBusIndex(ESoundBus::Master))
        {
            BusGroups[Index] = nullptr;
            continue;
        }

        if (BusGroups[Index])
        {
            BusGroups[Index]->release();
            BusGroups[Index] = nullptr;
        }
    }

    if (System)
    {
        System->close();
        System->release();
        System = nullptr;
    }

    PlayingSounds.clear();
    NextPlayingSoundId = 1;
    bInitialized = false;
}

void FSoundManager::Tick(float DeltaTime)
{
    (void)DeltaTime;

    if (!bInitialized || !System)
    {
        return;
    }

    CheckFmod(System->update(), "FMOD::System::update");

    for (auto It = PlayingSounds.begin(); It != PlayingSounds.end();)
    {
        FPlayingSound& PlayingSound = It->second;
        bool bIsPlaying = false;
        const FMOD_RESULT Result = PlayingSound.Channel ? PlayingSound.Channel->isPlaying(&bIsPlaying) : FMOD_ERR_INVALID_HANDLE;
        if (Result != FMOD_OK || !bIsPlaying)
        {
            It = PlayingSounds.erase(It);
            continue;
        }

        ++It;
    }
}

bool FSoundManager::LoadSound(const FName& Key, const FSoundDesc& Desc)
{
    if (!System || !Key.IsValid() || Desc.Path.empty())
    {
        return false;
    }

    if (Sounds.find(Key) != Sounds.end())
    {
        UE_LOG(Sound, Warning, "Sound key already loaded: %s", Key.ToString().c_str());
        return true;
    }

    FMOD::Sound* Sound = nullptr;
    const FMOD_MODE Mode = FMOD_DEFAULT | FMOD_2D | (Desc.bLoop ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF);
    const FMOD_RESULT Result = Desc.bStream
        ? System->createStream(Desc.Path.c_str(), Mode, nullptr, &Sound)
        : System->createSound(Desc.Path.c_str(), Mode, nullptr, &Sound);

    if (Result != FMOD_OK)
    {
        UE_LOG(Sound, Error, "Failed to load sound '%s' from '%s': %s",
               Key.ToString().c_str(), Desc.Path.c_str(), FMOD_ErrorString(Result));
        return false;
    }

    Sounds[Key] = Sound;
    UE_LOG(Sound, Verbose, "Loaded sound '%s' from '%s'.", Key.ToString().c_str(), Desc.Path.c_str());
    return true;
}

FSoundHandle FSoundManager::Play(const FName& Key, ESoundBus Bus, float Volume)
{
    return PlayInternal(Key, Bus, Volume, false);
}

FSoundHandle FSoundManager::PlayLoop(const FName& Key, ESoundBus Bus, float Volume)
{
    return PlayInternal(Key, Bus, Volume, true);
}

void FSoundManager::Stop(FSoundHandle Handle)
{
    if (!Handle.IsValid())
    {
        return;
    }

    auto It = PlayingSounds.find(Handle.Id);
    if (It == PlayingSounds.end() || It->second.Generation != Handle.Generation)
    {
        return;
    }

    if (It->second.Channel)
    {
        It->second.Channel->stop();
    }
    PlayingSounds.erase(It);
}

void FSoundManager::StopBus(ESoundBus Bus)
{
    if (!IsValidBus(Bus))
    {
        return;
    }

    const bool bStopAll = Bus == ESoundBus::Master;
    FMOD::ChannelGroup* Group = BusGroups[GetBusIndex(Bus)];
    if (Group)
    {
        Group->stop();
    }

    for (auto It = PlayingSounds.begin(); It != PlayingSounds.end();)
    {
        if (bStopAll || It->second.Bus == Bus)
        {
            It = PlayingSounds.erase(It);
            continue;
        }

        ++It;
    }
}

void FSoundManager::StopAll()
{
    FMOD::ChannelGroup* MasterGroup = BusGroups[GetBusIndex(ESoundBus::Master)];
    if (MasterGroup)
    {
        MasterGroup->stop();
    }
    PlayingSounds.clear();
}

void FSoundManager::SetBusVolume(ESoundBus Bus, float Volume)
{
    if (!IsValidBus(Bus))
    {
        return;
    }

    if (Bus == ESoundBus::Master)
    {
        SetMasterVolume(Volume);
        return;
    }

    VolumeSettings.BusVolumes[GetBusIndex(Bus)] = ClampVolume(Volume);
    ApplyBusVolume(Bus);
}

float FSoundManager::GetBusVolume(ESoundBus Bus) const
{
    if (!IsValidBus(Bus))
    {
        return 0.0f;
    }

    if (Bus == ESoundBus::Master)
    {
        return VolumeSettings.Master;
    }

    return VolumeSettings.BusVolumes[GetBusIndex(Bus)];
}

void FSoundManager::SetMasterVolume(float Volume)
{
    VolumeSettings.Master = ClampVolume(Volume);
    ApplyBusVolume(ESoundBus::Master);
}

float FSoundManager::GetMasterVolume() const
{
    return VolumeSettings.Master;
}

void FSoundManager::SetSoundVolume(FSoundHandle Handle, float Volume)
{
    if (!Handle.IsValid())
    {
        return;
    }

    auto It = PlayingSounds.find(Handle.Id);
    if (It == PlayingSounds.end() || It->second.Generation != Handle.Generation)
    {
        return;
    }

    FPlayingSound& PlayingSound = It->second;
    PlayingSound.Volume = ClampVolume(Volume);
    if (PlayingSound.Channel)
    {
        PlayingSound.Channel->setVolume(PlayingSound.Volume);
    }
}

void FSoundManager::SetSoundPosition(FSoundHandle Handle, uint32 PositionMs)
{
    if (!Handle.IsValid())
    {
        return;
    }

    auto It = PlayingSounds.find(Handle.Id);
    if (It == PlayingSounds.end() || It->second.Generation != Handle.Generation || !It->second.Channel)
    {
        return;
    }

    const FMOD_RESULT Result = It->second.Channel->setPosition(PositionMs, FMOD_TIMEUNIT_MS);
    if (Result != FMOD_OK)
    {
        UE_LOG(Sound, Warning, "Failed to set sound position: %s", FMOD_ErrorString(Result));
    }
}

uint32 FSoundManager::GetSoundPosition(FSoundHandle Handle) const
{
    if (!Handle.IsValid())
    {
        return 0;
    }

    auto It = PlayingSounds.find(Handle.Id);
    if (It == PlayingSounds.end() || It->second.Generation != Handle.Generation || !It->second.Channel)
    {
        return 0;
    }

    uint32 PositionMs = 0;
    const FMOD_RESULT Result = It->second.Channel->getPosition(&PositionMs, FMOD_TIMEUNIT_MS);
    if (Result != FMOD_OK)
    {
        return 0;
    }

    return PositionMs;
}

uint32 FSoundManager::GetSoundLength(const FName& Key) const
{
    auto It = Sounds.find(Key);
    if (It == Sounds.end() || !It->second)
    {
        return 0;
    }

    uint32 LengthMs = 0;
    const FMOD_RESULT Result = It->second->getLength(&LengthMs, FMOD_TIMEUNIT_MS);
    if (Result != FMOD_OK)
    {
        return 0;
    }

    return LengthMs;
}

bool FSoundManager::IsPlaying(FSoundHandle Handle) const
{
    if (!Handle.IsValid())
    {
        return false;
    }

    auto It = PlayingSounds.find(Handle.Id);
    if (It == PlayingSounds.end() || It->second.Generation != Handle.Generation || !It->second.Channel)
    {
        return false;
    }

    bool bIsPlaying = false;
    return It->second.Channel->isPlaying(&bIsPlaying) == FMOD_OK && bIsPlaying;
}

bool FSoundManager::LoadAllSounds()
{
    const std::filesystem::path SoundRoot = std::filesystem::path(FPaths::ContentDir()) / L"Sound";

    std::error_code Ec;
    if (!std::filesystem::exists(SoundRoot, Ec) || !std::filesystem::is_directory(SoundRoot, Ec))
    {
        UE_LOG(Sound, Warning, "Sound directory does not exist: %s", FPaths::FromPath(SoundRoot).c_str());
        return true;
    }

    bool bScanSucceeded = true;
    uint32 LoadedCount = 0;
    std::filesystem::recursive_directory_iterator It(SoundRoot, std::filesystem::directory_options::skip_permission_denied, Ec);
    const std::filesystem::recursive_directory_iterator End;

    if (Ec)
    {
        UE_LOG(Sound, Warning, "Failed to scan sound directory '%s'.", FPaths::FromPath(SoundRoot).c_str());
        return false;
    }

    for (; It != End; It.increment(Ec))
    {
        if (Ec)
        {
            UE_LOG(Sound, Warning, "Sound directory scan skipped an entry: %s", Ec.message().c_str());
            Ec.clear();
            bScanSucceeded = false;
            continue;
        }

        if (!It->is_regular_file(Ec) || !IsSupportedSoundFile(It->path()))
        {
            continue;
        }

        const FName Key = MakeSoundKeyFromPath(SoundRoot, It->path());
        FSoundDesc Desc;
        Desc.Path = FPaths::FromPath(It->path().lexically_normal());
        Desc.bStream = ShouldStreamByDefault(SoundRoot, It->path());
        Desc.bLoop = ShouldLoopByDefault(SoundRoot, It->path());
        Desc.b3D = false;

        if (LoadSound(Key, Desc))
        {
            ++LoadedCount;
        }
        else
        {
            bScanSucceeded = false;
        }
    }

    UE_LOG(Sound, Info, "Auto-loaded %u sounds from %s.", LoadedCount, FPaths::FromPath(SoundRoot).c_str());
    return bScanSucceeded;
}

FSoundHandle FSoundManager::PlayInternal(const FName& Key, ESoundBus Bus, float Volume, bool bForceLoop)
{
    if (!bInitialized || !System || !IsValidBus(Bus))
    {
        return {};
    }

    auto It = Sounds.find(Key);
    if (It == Sounds.end() || !It->second)
    {
        UE_LOG(Sound, Warning, "Sound key not found: %s", Key.ToString().c_str());
        return {};
    }

    FMOD::Channel* Channel = nullptr;
    FMOD::ChannelGroup* Group = BusGroups[GetBusIndex(Bus)];
    FMOD_RESULT Result = System->playSound(It->second, Group, true, &Channel);
    if (Result != FMOD_OK || !Channel)
    {
        UE_LOG(Sound, Error, "Failed to play sound '%s': %s", Key.ToString().c_str(), FMOD_ErrorString(Result));
        return {};
    }

    Channel->setVolume(ClampVolume(Volume));
    if (bForceLoop)
    {
        Channel->setMode(FMOD_LOOP_NORMAL);
    }
    Channel->setPaused(false);

    uint32 Id = NextPlayingSoundId++;
    if (Id == 0)
    {
        Id = NextPlayingSoundId++;
    }

    const uint32 Generation = 1;
    PlayingSounds[Id] = FPlayingSound{ Generation, Key, Bus, Channel, ClampVolume(Volume), bForceLoop };
    return FSoundHandle{ Id, Generation };
}

void FSoundManager::ApplyStoredVolumes()
{
    ApplyBusVolume(ESoundBus::Master);
    for (size_t Index = 0; Index < static_cast<size_t>(ESoundBus::Count); ++Index)
    {
        const ESoundBus Bus = static_cast<ESoundBus>(Index);
        if (Bus != ESoundBus::Master)
        {
            ApplyBusVolume(Bus);
        }
    }
}

void FSoundManager::ApplyBusVolume(ESoundBus Bus)
{
    if (!IsValidBus(Bus))
    {
        return;
    }

    FMOD::ChannelGroup* Group = BusGroups[GetBusIndex(Bus)];
    if (!Group)
    {
        return;
    }

    const float Volume = Bus == ESoundBus::Master
        ? VolumeSettings.Master
        : VolumeSettings.BusVolumes[GetBusIndex(Bus)];
    Group->setVolume(Volume);
}

bool FSoundManager::IsSupportedSoundFile(const std::filesystem::path& Path)
{
    const FString Extension = ToLowerString(FPaths::FromPath(Path.extension()));
    return Extension == ".wav" || Extension == ".mp3" || Extension == ".ogg";
}

FName FSoundManager::MakeSoundKeyFromPath(const std::filesystem::path& SoundRoot, const std::filesystem::path& FilePath)
{
    std::filesystem::path Relative = MakeRelativePath(SoundRoot, FilePath);
    Relative.replace_extension();
    return FName(FPaths::FromPath(Relative));
}

bool FSoundManager::ShouldStreamByDefault(const std::filesystem::path& SoundRoot, const std::filesystem::path& FilePath)
{
    return IsLongSoundDirectory(SoundRoot, FilePath);
}

bool FSoundManager::ShouldLoopByDefault(const std::filesystem::path& SoundRoot, const std::filesystem::path& FilePath)
{
    return IsLongSoundDirectory(SoundRoot, FilePath);
}
