#pragma once
#include "soloud/include/soloud.h"
#include "soloud/include/soloud_wav.h"
#include "soloud/include/soloud_wavstream.h"
#include <iostream>
#include <string>
#include <unordered_map>

class AudioManager
{
public:
	static AudioManager* getInstance()
	{
		static AudioManager* instance = new AudioManager();
		return instance;
	}

	bool LoadSFX(const std::string& name, const std::string& filePath)
	{
		auto it = wavCache.find(filePath);
		if (it != wavCache.end())
		{
			return true;	// 이미 로드되어있음
		}
		else
		{
			auto wav = std::make_unique<SoLoud::Wav>();
			if (wav->load(filePath.c_str()) == SoLoud::SO_NO_ERROR)
			{
				wavCache[name] = std::move(wav);
				return true;
			}
			else
				return false;
		}
	}

	void PlaySFX(const std::string& name, const float volume = 1.0f)
	{
		auto it = wavCache.find(name);
		if (it == wavCache.end())
			return;

		int sfxCount = gSoloud.countAudioSource(*(it->second));
		if (sfxCount > AUDIO_MAX_PLAY_FOR_EACH_CLIP)
			return;

		unsigned int handle = gSoloud.play(*it->second);
		gSoloud.setVolume(handle, volume);
	}

	bool LoadAndPlayBGM(const std::string& filePath) {
		auto stream = std::make_unique<SoLoud::WavStream>();
		if (stream->load(filePath.c_str()) == SoLoud::SO_NO_ERROR) {
			bgmWavStream = std::move(stream);
			
			unsigned int handle = gSoloud.play(*bgmWavStream);
			gSoloud.setLooping(handle, true);
		}
		return false;
	}

private:
	SoLoud::Soloud gSoloud; // SoLoud engine
	SoLoud::Wav wav;
	std::unordered_map<std::string, std::unique_ptr<SoLoud::Wav>> wavCache;
	std::unique_ptr<SoLoud::WavStream> bgmWavStream;

	void LoadAllSfxWav()
	{
		// TODO: 여기에 필요한 모든 효과음들을 로드
		// e.g. 
		LoadSFX("whistle", "Assets/whistle.wav");
		LoadSFX("ball_bounce", "Assets/ball_bounce.wav");
		LoadSFX("skill_common", "Assets/skill_charge.wav");
		LoadSFX("skillA_launch", "Assets/laser-gun-shot.wav");
		LoadSFX("crowd_reaction", "Assets/crowd-reaction.wav");
		// NOTE: Visual Studio에서 실행할 때에는 pwd가 프로젝트 파일(.vcxproj)이 위치한 곳임
	}

	AudioManager() 
	{
		gSoloud.init();
		gSoloud.setMaxActiveVoiceCount(32);
		LoadAllSfxWav();
	}
	~AudioManager()
	{
		gSoloud.deinit();
	}
};

