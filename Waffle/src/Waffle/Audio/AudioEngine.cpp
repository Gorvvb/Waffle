#include "wfpch.h"
#include "AudioEngine.h"
#include "Platform/Windows/WindowsAudioBackend.h"

#if defined(PlaySound)
#undef PlaySound
#endif

namespace Waffle {

	std::unique_ptr<AudioBackend> AudioEngine::s_Backend = nullptr;

	void AudioEngine::Init()
	{
		if (!s_Backend)
		{
#if defined(WF_PLATFORM_WINDOWS) || defined(_WIN32)
			s_Backend = std::make_unique<WindowsAudioBackend>();
#else
			// Default fallback or non-windows backend
			s_Backend = std::make_unique<WindowsAudioBackend>();
#endif
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

	bool AudioEngine::PlaySound(const std::string& filepath, float volume, bool loop)
	{
		if (!s_Backend) Init();
		return s_Backend ? s_Backend->PlaySound(filepath, volume, loop) : false;
	}

	void AudioEngine::StopSound(const std::string& filepath)
	{
		if (s_Backend) s_Backend->StopSound(filepath);
	}

	void AudioEngine::StopAllSounds()
	{
		if (s_Backend) s_Backend->StopAllSounds();
	}

	void AudioEngine::SetSoundVolume(const std::string& filepath, float volume)
	{
		if (s_Backend) s_Backend->SetSoundVolume(filepath, volume);
	}

	void AudioEngine::SetMasterVolume(float volume)
	{
		if (s_Backend) s_Backend->SetMasterVolume(volume);
	}

}
