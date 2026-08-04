#include "wfpch.h"
#include "WindowsAudioBackend.h"

#include <windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")

#if defined(PlaySound)
#undef PlaySound
#endif

namespace Waffle {

	void WindowsAudioBackend::Init() {}
	void WindowsAudioBackend::Shutdown() {}

	bool WindowsAudioBackend::PlaySound(const std::string& filepath, float volume, float pitch, bool loop, bool spatial, float x, float y, float minDistance, float maxDistance)
	{
		DWORD flags = SND_FILENAME | SND_ASYNC;
		if (loop) flags |= SND_LOOP;
		return PlaySoundA(filepath.c_str(), NULL, flags) != FALSE;
	}

	void WindowsAudioBackend::StopSound(const std::string& filepath)
	{
		PlaySoundA(NULL, NULL, 0);
	}

	void WindowsAudioBackend::PauseSound(const std::string& filepath)
	{
		StopSound(filepath);
	}

	void WindowsAudioBackend::ResumeSound(const std::string& filepath)
	{
		PlaySound(filepath);
	}

	bool WindowsAudioBackend::IsSoundPlaying(const std::string& filepath)
	{
		return false;
	}

	void WindowsAudioBackend::StopAllSounds()
	{
		PlaySoundA(NULL, NULL, 0);
	}

	void WindowsAudioBackend::PauseAllSounds()
	{
		StopAllSounds();
	}

	void WindowsAudioBackend::ResumeAllSounds() {}

	void WindowsAudioBackend::SetSoundVolume(const std::string& filepath, float volume) {}
	void WindowsAudioBackend::SetSoundPitch(const std::string& filepath, float pitch) {}
	void WindowsAudioBackend::SetMasterVolume(float volume) {}

	void WindowsAudioBackend::SetListenerPosition(float x, float y, float z) {}
	void WindowsAudioBackend::SetSoundPosition(const std::string& filepath, float x, float y, float z) {}

}
