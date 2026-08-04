#include "wfpch.h"
#include "AudioEngine.h"
#include "MiniAudioBackend.h"

#if defined(PlaySound)
#undef PlaySound
#endif

namespace Waffle {

	std::unique_ptr<AudioBackend> AudioEngine::s_Backend = nullptr;

	void AudioEngine::Init()
	{
		if (!s_Backend)
		{
			s_Backend = std::make_unique<MiniAudioBackend>();
		}
	}

	void AudioEngine::Shutdown()
	{
		if (s_Backend)
		{
			s_Backend->Shutdown();
			s_Backend.reset();
		}
	}

	bool AudioEngine::PlaySound(const std::string& filepath, float volume, float pitch, bool loop, bool spatial, float x, float y, float minDistance, float maxDistance)
	{
		if (!s_Backend) Init();
		return s_Backend ? s_Backend->PlaySound(filepath, volume, pitch, loop, spatial, x, y, minDistance, maxDistance) : false;
	}

	void AudioEngine::StopSound(const std::string& filepath)
	{
		if (s_Backend) s_Backend->StopSound(filepath);
	}

	void AudioEngine::PauseSound(const std::string& filepath)
	{
		if (s_Backend) s_Backend->PauseSound(filepath);
	}

	void AudioEngine::ResumeSound(const std::string& filepath)
	{
		if (s_Backend) s_Backend->ResumeSound(filepath);
	}

	bool AudioEngine::IsSoundPlaying(const std::string& filepath)
	{
		return s_Backend ? s_Backend->IsSoundPlaying(filepath) : false;
	}

	void AudioEngine::StopAllSounds()
	{
		if (s_Backend) s_Backend->StopAllSounds();
	}

	void AudioEngine::PauseAllSounds()
	{
		if (s_Backend) s_Backend->PauseAllSounds();
	}

	void AudioEngine::ResumeAllSounds()
	{
		if (s_Backend) s_Backend->ResumeAllSounds();
	}

	void AudioEngine::SetSoundVolume(const std::string& filepath, float volume)
	{
		if (s_Backend) s_Backend->SetSoundVolume(filepath, volume);
	}

	void AudioEngine::SetSoundPitch(const std::string& filepath, float pitch)
	{
		if (s_Backend) s_Backend->SetSoundPitch(filepath, pitch);
	}

	void AudioEngine::SetMasterVolume(float volume)
	{
		if (s_Backend) s_Backend->SetMasterVolume(volume);
	}

	void AudioEngine::SetListenerPosition(float x, float y, float z)
	{
		if (s_Backend) s_Backend->SetListenerPosition(x, y, z);
	}

	void AudioEngine::SetSoundPosition(const std::string& filepath, float x, float y, float z)
	{
		if (s_Backend) s_Backend->SetSoundPosition(filepath, x, y, z);
	}

}
