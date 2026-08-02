#pragma once

#include "Waffle/Audio/AudioBackend.h"

#include <unordered_map>
#include <mutex>

namespace Waffle {

	class WindowsAudioBackend : public AudioBackend
	{
	public:
		WindowsAudioBackend();
		virtual ~WindowsAudioBackend();

		virtual void Init() override;
		virtual void Shutdown() override;

		virtual bool PlaySound(const std::string& filepath, float volume = 1.0f, bool loop = false) override;
		virtual void StopSound(const std::string& filepath) override;
		virtual void StopAllSounds() override;

		virtual void SetSoundVolume(const std::string& filepath, float volume) override;
		virtual void SetMasterVolume(float volume) override;

	private:
		float m_MasterVolume = 1.0f;
		std::unordered_map<std::string, float> m_SoundVolumes;
		std::mutex m_AudioMutex;
	};

}
