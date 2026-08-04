#pragma once

#include <string>

#if defined(PlaySound)
#undef PlaySound
#endif

namespace Waffle {

	class AudioBackend
	{
	public:
		virtual ~AudioBackend() = default;

		virtual void Init() = 0;
		virtual void Shutdown() = 0;

		virtual bool PlaySound(const std::string& filepath, float volume = 1.0f, float pitch = 1.0f, bool loop = false, bool spatial = false, float x = 0.0f, float y = 0.0f, float minDistance = 1.0f, float maxDistance = 20.0f) = 0;
		virtual void StopSound(const std::string& filepath) = 0;
		virtual void PauseSound(const std::string& filepath) = 0;
		virtual void ResumeSound(const std::string& filepath) = 0;
		virtual bool IsSoundPlaying(const std::string& filepath) = 0;
		virtual void StopAllSounds() = 0;
		virtual void PauseAllSounds() = 0;
		virtual void ResumeAllSounds() = 0;

		virtual void SetSoundVolume(const std::string& filepath, float volume) = 0;
		virtual void SetSoundPitch(const std::string& filepath, float pitch) = 0;
		virtual void SetMasterVolume(float volume) = 0;

		virtual void SetListenerPosition(float x, float y, float z = 0.0f) = 0;
		virtual void SetSoundPosition(const std::string& filepath, float x, float y, float z = 0.0f) = 0;
	};

}
