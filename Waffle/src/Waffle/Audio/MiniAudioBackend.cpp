#include "wfpch.h"
#include "MiniAudioBackend.h"
#include "Waffle/Core/Log.h"
#include "Waffle/Core/VFS.h"
#include "Waffle/Scripting/LuaScriptEngine.h"

#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

#include <filesystem>
#include <unordered_map>
#include <mutex>

namespace Waffle {

	struct SoundInstance
	{
		ma_sound Sound;
		ma_decoder Decoder;
		Buffer MemoryBuffer;
		bool HasDecoder = false;
		bool Initialized = false;
		bool PausedByEngine = false;
	};

	struct MiniAudioData
	{
		ma_engine Engine;
		bool EngineInitialized = false;
		float MasterVolume = 1.0f;

		std::unordered_map<std::string, SoundInstance> Sounds;
		std::mutex AudioMutex;
	};

	static std::filesystem::path ResolveAudioPath(const std::string& filepath)
	{
		std::filesystem::path p(filepath);

		// 1. Direct path check (as-is via VFS)
		if (VFS::Exists(p))
			return p;

		// 2. Relative to active project AssetPath
		std::filesystem::path assetPath = LuaScriptEngine::GetAssetPath() / p;
		if (VFS::Exists(assetPath))
			return assetPath;

		return p;
	}

	MiniAudioBackend::MiniAudioBackend()
		: m_Data(std::make_unique<MiniAudioData>())
	{
		Init();
	}

	MiniAudioBackend::~MiniAudioBackend()
	{
		Shutdown();
	}

	void MiniAudioBackend::Init()
	{
		ma_result result = ma_engine_init(NULL, &m_Data->Engine);
		if (result != MA_SUCCESS)
		{
			WF_CORE_ERROR("MiniAudioBackend: Failed to initialize miniaudio engine! Error code: {0}", (int)result);
			m_Data->EngineInitialized = false;
			return;
		}

		m_Data->EngineInitialized = true;
		WF_CORE_INFO("MiniAudioBackend initialized successfully (Low-latency 2D Spatial Audio)");
	}

	void MiniAudioBackend::Shutdown()
	{
		if (!m_Data || !m_Data->EngineInitialized)
			return;

		StopAllSounds();

		std::lock_guard<std::mutex> lock(m_Data->AudioMutex);
		for (auto& [path, instance] : m_Data->Sounds)
		{
			if (instance.Initialized)
			{
				ma_sound_uninit(&instance.Sound);
				instance.Initialized = false;
			}
		}
		m_Data->Sounds.clear();

		ma_engine_uninit(&m_Data->Engine);
		m_Data->EngineInitialized = false;
	}

	bool MiniAudioBackend::PlaySound(const std::string& filepath, float volume, float pitch, bool loop, bool spatial, float x, float y, float minDistance, float maxDistance)
	{
		if (!m_Data || !m_Data->EngineInitialized)
			return false;

		std::lock_guard<std::mutex> lock(m_Data->AudioMutex);

		std::filesystem::path resolved = ResolveAudioPath(filepath);
		if (!VFS::Exists(resolved))
		{
			WF_CORE_WARN("MiniAudioBackend: Audio file not found '{0}' (Resolved: '{1}')", filepath, resolved.string());
			return false;
		}

		std::string resolvedStr = resolved.string();

		auto it = m_Data->Sounds.find(filepath);
		if (it != m_Data->Sounds.end() && it->second.Initialized)
		{
			ma_sound_uninit(&it->second.Sound);
			if (it->second.HasDecoder)
			{
				ma_decoder_uninit(&it->second.Decoder);
				it->second.HasDecoder = false;
			}
			it->second.MemoryBuffer.Release();
			it->second.Initialized = false;
		}

		SoundInstance& instance = m_Data->Sounds[filepath];

		ma_uint32 flags = MA_SOUND_FLAG_ASYNC;
		if (spatial)
			flags |= 0;

		ma_result result = ma_sound_init_from_file(&m_Data->Engine, resolvedStr.c_str(), flags, NULL, NULL, &instance.Sound);
		if (result != MA_SUCCESS && VFS::Exists(resolvedStr))
		{
			Buffer buf = VFS::ReadFile(resolvedStr);
			if (!buf && resolvedStr != filepath)
				buf = VFS::ReadFile(filepath);

			if (buf)
			{
				instance.MemoryBuffer = std::move(buf);
				ma_decoder_config decCfg = ma_decoder_config_init_default();
				result = ma_decoder_init_memory(instance.MemoryBuffer.Data, instance.MemoryBuffer.Size, &decCfg, &instance.Decoder);
				if (result == MA_SUCCESS)
				{
					instance.HasDecoder = true;
					result = ma_sound_init_from_data_source(&m_Data->Engine, &instance.Decoder, flags, NULL, &instance.Sound);
				}
			}
		}

		if (result != MA_SUCCESS)
		{
			WF_CORE_ERROR("MiniAudioBackend: Failed to load sound '{0}'! Error code: {1}", filepath, (int)result);
			return false;
		}

		instance.Initialized = true;
		instance.PausedByEngine = false;

		ma_sound_set_volume(&instance.Sound, volume * m_Data->MasterVolume);
		ma_sound_set_pitch(&instance.Sound, pitch);
		ma_sound_set_looping(&instance.Sound, loop ? MA_TRUE : MA_FALSE);

		if (spatial)
		{
			ma_sound_set_spatialization_enabled(&instance.Sound, MA_TRUE);
			ma_sound_set_position(&instance.Sound, x, y, 0.0f);
			ma_sound_set_min_distance(&instance.Sound, minDistance);
			ma_sound_set_max_distance(&instance.Sound, maxDistance);
			ma_sound_set_attenuation_model(&instance.Sound, ma_attenuation_model_inverse);
		}
		else
		{
			ma_sound_set_spatialization_enabled(&instance.Sound, MA_FALSE);
		}

		ma_result playResult = ma_sound_start(&instance.Sound);
		return playResult == MA_SUCCESS;
	}

	void MiniAudioBackend::StopSound(const std::string& filepath)
	{
		if (!m_Data || !m_Data->EngineInitialized) return;
		std::lock_guard<std::mutex> lock(m_Data->AudioMutex);

		auto it = m_Data->Sounds.find(filepath);
		if (it != m_Data->Sounds.end() && it->second.Initialized)
		{
			ma_sound_stop(&it->second.Sound);
			it->second.PausedByEngine = false;
		}
	}

	void MiniAudioBackend::PauseSound(const std::string& filepath)
	{
		if (!m_Data || !m_Data->EngineInitialized) return;
		std::lock_guard<std::mutex> lock(m_Data->AudioMutex);

		auto it = m_Data->Sounds.find(filepath);
		if (it != m_Data->Sounds.end() && it->second.Initialized)
		{
			ma_sound_stop(&it->second.Sound);
			it->second.PausedByEngine = true;
		}
	}

	void MiniAudioBackend::ResumeSound(const std::string& filepath)
	{
		if (!m_Data || !m_Data->EngineInitialized) return;
		std::lock_guard<std::mutex> lock(m_Data->AudioMutex);

		auto it = m_Data->Sounds.find(filepath);
		if (it != m_Data->Sounds.end() && it->second.Initialized)
		{
			ma_sound_start(&it->second.Sound);
			it->second.PausedByEngine = false;
		}
	}

	bool MiniAudioBackend::IsSoundPlaying(const std::string& filepath)
	{
		if (!m_Data || !m_Data->EngineInitialized) return false;
		std::lock_guard<std::mutex> lock(m_Data->AudioMutex);

		auto it = m_Data->Sounds.find(filepath);
		if (it != m_Data->Sounds.end() && it->second.Initialized)
		{
			return ma_sound_is_playing(&it->second.Sound) == MA_TRUE;
		}
		return false;
	}

	void MiniAudioBackend::StopAllSounds()
	{
		if (!m_Data || !m_Data->EngineInitialized) return;
		std::lock_guard<std::mutex> lock(m_Data->AudioMutex);

		for (auto& [path, instance] : m_Data->Sounds)
		{
			if (instance.Initialized)
			{
				ma_sound_stop(&instance.Sound);
				instance.PausedByEngine = false;
			}
		}
	}

	void MiniAudioBackend::PauseAllSounds()
	{
		if (!m_Data || !m_Data->EngineInitialized) return;
		std::lock_guard<std::mutex> lock(m_Data->AudioMutex);

		int count = 0;
		for (auto& [path, instance] : m_Data->Sounds)
		{
			if (instance.Initialized && ma_sound_is_playing(&instance.Sound))
			{
				ma_sound_stop(&instance.Sound);
				instance.PausedByEngine = true;
				count++;
			}
		}
		WF_CORE_INFO("[AudioPauseLog] MiniAudioBackend::PauseAllSounds - Paused {0} active sound(s) out of {1} loaded.", count, m_Data->Sounds.size());
	}

	void MiniAudioBackend::ResumeAllSounds()
	{
		if (!m_Data || !m_Data->EngineInitialized) return;
		std::lock_guard<std::mutex> lock(m_Data->AudioMutex);

		int count = 0;
		for (auto& [path, instance] : m_Data->Sounds)
		{
			if (instance.Initialized && instance.PausedByEngine)
			{
				ma_sound_start(&instance.Sound);
				instance.PausedByEngine = false;
				count++;
			}
		}
		WF_CORE_INFO("[AudioPauseLog] MiniAudioBackend::ResumeAllSounds - Resumed {0} sound(s).", count);
	}

	void MiniAudioBackend::SetSoundVolume(const std::string& filepath, float volume)
	{
		if (!m_Data || !m_Data->EngineInitialized) return;
		std::lock_guard<std::mutex> lock(m_Data->AudioMutex);

		auto it = m_Data->Sounds.find(filepath);
		if (it != m_Data->Sounds.end() && it->second.Initialized)
		{
			ma_sound_set_volume(&it->second.Sound, volume * m_Data->MasterVolume);
		}
	}

	void MiniAudioBackend::SetSoundPitch(const std::string& filepath, float pitch)
	{
		if (!m_Data || !m_Data->EngineInitialized) return;
		std::lock_guard<std::mutex> lock(m_Data->AudioMutex);

		auto it = m_Data->Sounds.find(filepath);
		if (it != m_Data->Sounds.end() && it->second.Initialized)
		{
			ma_sound_set_pitch(&it->second.Sound, pitch);
		}
	}

	void MiniAudioBackend::SetMasterVolume(float volume)
	{
		if (!m_Data || !m_Data->EngineInitialized) return;
		std::lock_guard<std::mutex> lock(m_Data->AudioMutex);

		m_Data->MasterVolume = volume;
		ma_engine_set_volume(&m_Data->Engine, volume);
	}

	void MiniAudioBackend::SetListenerPosition(float x, float y, float z)
	{
		if (!m_Data || !m_Data->EngineInitialized) return;
		std::lock_guard<std::mutex> lock(m_Data->AudioMutex);

		ma_engine_listener_set_position(&m_Data->Engine, 0, x, y, z);
		ma_engine_listener_set_direction(&m_Data->Engine, 0, 0.0f, 0.0f, -1.0f);
	}

	void MiniAudioBackend::SetSoundPosition(const std::string& filepath, float x, float y, float z)
	{
		if (!m_Data || !m_Data->EngineInitialized) return;
		std::lock_guard<std::mutex> lock(m_Data->AudioMutex);

		auto it = m_Data->Sounds.find(filepath);
		if (it != m_Data->Sounds.end() && it->second.Initialized)
		{
			ma_sound_set_position(&it->second.Sound, x, y, z);
		}
	}

}
