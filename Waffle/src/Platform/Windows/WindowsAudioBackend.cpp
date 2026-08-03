#include "wfpch.h"
#include "WindowsAudioBackend.h"
#include "Waffle/Core/Log.h"

#include <filesystem>
#include <windows.h>
#include <mmsystem.h>

#pragma comment(lib, "winmm.lib")

#if defined(PlaySound)
#undef PlaySound
#endif

namespace Waffle {

	WindowsAudioBackend::WindowsAudioBackend()
	{
		Init();
	}

	WindowsAudioBackend::~WindowsAudioBackend()
	{
		Shutdown();
	}

	void WindowsAudioBackend::Init()
	{
		WF_CORE_INFO("WindowsAudioBackend initialized (WinMM MCI Engine)");
	}

	void WindowsAudioBackend::Shutdown()
	{
		StopAllSounds();
	}

	bool WindowsAudioBackend::PlaySound(const std::string& filepath, float volume, bool loop)
	{
		std::lock_guard<std::mutex> lock(m_AudioMutex);

		if (!std::filesystem::exists(filepath))
		{
			WF_CORE_WARN("WindowsAudioBackend: Audio file not found '{0}'", filepath);
			return false;
		}

		std::string alias = "audio_" + std::to_string(std::hash<std::string>{}(filepath));

		std::string closeCmd = "close " + alias;
		mciSendStringA(closeCmd.c_str(), NULL, 0, NULL);

		std::string openCmd = "open \"" + filepath + "\" type mpegvideo alias " + alias;
		MCIERROR err = mciSendStringA(openCmd.c_str(), NULL, 0, NULL);
		if (err != 0)
		{
			openCmd = "open \"" + filepath + "\" alias " + alias;
			err = mciSendStringA(openCmd.c_str(), NULL, 0, NULL);
		}

		if (err != 0)
		{
			WF_CORE_ERROR("WindowsAudioBackend: Failed to open audio '{0}', MCI error code: {1}", filepath, err);
			return false;
		}

		std::string playCmd = "play " + alias + " from 0";
		if (loop) playCmd += " repeat";
		mciSendStringA(playCmd.c_str(), NULL, 0, NULL);

		m_SoundVolumes[filepath] = volume;
		SetSoundVolume(filepath, volume);

		return true;
	}

	void WindowsAudioBackend::StopSound(const std::string& filepath)
	{
		std::lock_guard<std::mutex> lock(m_AudioMutex);
		std::string alias = "audio_" + std::to_string(std::hash<std::string>{}(filepath));
		std::string stopCmd = "stop " + alias;
		mciSendStringA(stopCmd.c_str(), NULL, 0, NULL);
		std::string closeCmd = "close " + alias;
		mciSendStringA(closeCmd.c_str(), NULL, 0, NULL);
	}

	void WindowsAudioBackend::StopAllSounds()
	{
		std::lock_guard<std::mutex> lock(m_AudioMutex);
		mciSendStringA("close all", NULL, 0, NULL);
	}

	void WindowsAudioBackend::SetSoundVolume(const std::string& filepath, float volume)
	{
		m_SoundVolumes[filepath] = volume;
		float effectiveVolume = volume * m_MasterVolume;
		int mciVol = (int)(effectiveVolume * 1000.0f);
		if (mciVol < 0) mciVol = 0;
		if (mciVol > 1000) mciVol = 1000;

		std::string alias = "audio_" + std::to_string(std::hash<std::string>{}(filepath));
		std::string setVolCmd = "setaudio " + alias + " volume to " + std::to_string(mciVol);
		mciSendStringA(setVolCmd.c_str(), NULL, 0, NULL);
	}

	void WindowsAudioBackend::SetMasterVolume(float volume)
	{
		m_MasterVolume = volume;
		for (auto& [path, vol] : m_SoundVolumes)
		{
			SetSoundVolume(path, vol);
		}
	}

}
