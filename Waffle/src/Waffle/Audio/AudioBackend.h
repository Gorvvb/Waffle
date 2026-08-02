#pragma once

#include <string>

namespace Waffle {

	class AudioBackend
	{
	public:
		virtual ~AudioBackend() = default;

		virtual void Init() = 0;
		virtual void Shutdown() = 0;

		virtual bool PlaySound(const std::string& filepath, float volume = 1.0f, bool loop = false) = 0;
		virtual void StopSound(const std::string& filepath) = 0;
		virtual void StopAllSounds() = 0;

		virtual void SetSoundVolume(const std::string& filepath, float volume) = 0;
		virtual void SetMasterVolume(float volume) = 0;
	};

}
