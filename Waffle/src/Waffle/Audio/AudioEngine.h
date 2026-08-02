#pragma once

#include "Waffle/Audio/AudioBackend.h"
#include <memory>
#include <string>

namespace Waffle {

	class AudioEngine
	{
	public:
		static void Init();
		static void Shutdown();

		static bool PlaySound(const std::string& filepath, float volume = 1.0f, bool loop = false);
		static void StopSound(const std::string& filepath);
		static void StopAllSounds();

		static void SetSoundVolume(const std::string& filepath, float volume);
		static void SetMasterVolume(float volume);

		static AudioBackend* GetBackend() { return s_Backend.get(); }

	private:
		static std::unique_ptr<AudioBackend> s_Backend;
	};

}
