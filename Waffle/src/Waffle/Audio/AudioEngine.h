#pragma once

#include "Waffle/Audio/AudioBackend.h"
#include <memory>
#include <string>

#if defined(PlaySound)
#undef PlaySound
#endif

namespace Waffle {

	class AudioEngine
	{
	public:
		static void Init();
		static void Shutdown();

		static bool PlaySound(const std::string& filepath, float volume = 1.0f, float pitch = 1.0f, bool loop = false, bool spatial = false, float x = 0.0f, float y = 0.0f, float minDistance = 1.0f, float maxDistance = 20.0f);
		static void StopSound(const std::string& filepath);
		static void PauseSound(const std::string& filepath);
		static void ResumeSound(const std::string& filepath);
		static bool IsSoundPlaying(const std::string& filepath);
		static void StopAllSounds();
		static void PauseAllSounds();
		static void ResumeAllSounds();

		static void SetSoundVolume(const std::string& filepath, float volume);
		static void SetSoundPitch(const std::string& filepath, float pitch);
		static void SetMasterVolume(float volume);

		static void SetListenerPosition(float x, float y, float z = 0.0f);
		static void SetSoundPosition(const std::string& filepath, float x, float y, float z = 0.0f);

		static AudioBackend* GetBackend() { return s_Backend.get(); }

	private:
		static std::unique_ptr<AudioBackend> s_Backend;
	};

}
