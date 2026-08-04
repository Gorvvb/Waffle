#pragma once

#include "Waffle/Audio/AudioBackend.h"
#include <unordered_map>
#include <mutex>
#include <memory>

namespace Waffle {

	struct MiniAudioData;

	class MiniAudioBackend : public AudioBackend
	{
	public:
		MiniAudioBackend();
		virtual ~MiniAudioBackend();

		virtual void Init() override;
		virtual void Shutdown() override;

		virtual bool PlaySound(const std::string& filepath, float volume = 1.0f, float pitch = 1.0f, bool loop = false, bool spatial = false, float x = 0.0f, float y = 0.0f, float minDistance = 1.0f, float maxDistance = 20.0f) override;
		virtual void StopSound(const std::string& filepath) override;
		virtual void PauseSound(const std::string& filepath) override;
		virtual void ResumeSound(const std::string& filepath) override;
		virtual bool IsSoundPlaying(const std::string& filepath) override;
		virtual void StopAllSounds() override;
		virtual void PauseAllSounds() override;
		virtual void ResumeAllSounds() override;

		virtual void SetSoundVolume(const std::string& filepath, float volume) override;
		virtual void SetSoundPitch(const std::string& filepath, float pitch) override;
		virtual void SetMasterVolume(float volume) override;

		virtual void SetListenerPosition(float x, float y, float z = 0.0f) override;
		virtual void SetSoundPosition(const std::string& filepath, float x, float y, float z = 0.0f) override;

	private:
		std::unique_ptr<MiniAudioData> m_Data;
	};

}
