#include "wfpch.h"
#include "Waffle/Core/VFS.h"
#include "Waffle/Core/Log.h"

#include <fstream>
#include <algorithm>

namespace Waffle {

	std::filesystem::path VFS::s_MountedWpackPath;
	std::unordered_map<std::string, VFSEntry> VFS::s_Entries;
	bool VFS::s_IsMounted = false;

	static const uint32_t WFPK_MAGIC = 0x4B504657; // "WFPK"

	std::string VFS::NormalizePath(const std::filesystem::path& path)
	{
		std::string s = path.string();
		std::replace(s.begin(), s.end(), '\\', '/');

		if (s.rfind("./", 0) == 0)
			s = s.substr(2);

		auto pos = s.find("Assets/");
		if (pos != std::string::npos)
		{
			s = s.substr(pos);
		}
		else
		{
			auto posLower = s.find("assets/");
			if (posLower != std::string::npos)
			{
				s = "Assets/" + s.substr(posLower + 7);
			}
			else if (s_IsMounted)
			{
				std::string candidate = "Assets/" + s;
				if (s_Entries.find(candidate) != s_Entries.end())
					return candidate;
			}
		}

		return s;
	}

	void VFS::ObfuscateBuffer(uint8_t* data, uint64_t size, uint32_t key)
	{
		if (!data || size == 0 || key == 0)
			return;

		uint8_t k[4] = {
			static_cast<uint8_t>(key & 0xFF),
			static_cast<uint8_t>((key >> 8) & 0xFF),
			static_cast<uint8_t>((key >> 16) & 0xFF),
			static_cast<uint8_t>((key >> 24) & 0xFF)
		};

		for (uint64_t i = 0; i < size; ++i)
		{
			data[i] ^= k[i % 4];
		}
	}

	bool VFS::MountArchive(const std::filesystem::path& wpackPath)
	{
		UnmountArchive();

		if (!std::filesystem::exists(wpackPath))
		{
			WF_CORE_WARN("VFS::MountArchive - File '{0}' does not exist.", wpackPath.string());
			return false;
		}

		std::ifstream stream(wpackPath, std::ios::binary);
		if (!stream.is_open())
		{
			WF_CORE_ERROR("VFS::MountArchive - Could not open '{0}'.", wpackPath.string());
			return false;
		}

		uint32_t magic = 0;
		uint32_t version = 0;
		uint32_t fileCount = 0;

		stream.read(reinterpret_cast<char*>(&magic), sizeof(magic));
		if (magic != WFPK_MAGIC)
		{
			WF_CORE_ERROR("VFS::MountArchive - Invalid magic header in '{0}'. Expected 'WFPK'.", wpackPath.string());
			return false;
		}

		stream.read(reinterpret_cast<char*>(&version), sizeof(version));
		stream.read(reinterpret_cast<char*>(&fileCount), sizeof(fileCount));

		for (uint32_t i = 0; i < fileCount; ++i)
		{
			uint32_t pathLength = 0;
			stream.read(reinterpret_cast<char*>(&pathLength), sizeof(pathLength));

			std::string pathStr(pathLength, '\0');
			stream.read(&pathStr[0], pathLength);

			VFSEntry entry;
			entry.Path = pathStr;
			stream.read(reinterpret_cast<char*>(&entry.Offset), sizeof(entry.Offset));
			stream.read(reinterpret_cast<char*>(&entry.Size), sizeof(entry.Size));
			stream.read(reinterpret_cast<char*>(&entry.UncompressedSize), sizeof(entry.UncompressedSize));
			stream.read(reinterpret_cast<char*>(&entry.Flags), sizeof(entry.Flags));
			stream.read(reinterpret_cast<char*>(&entry.XORKey), sizeof(entry.XORKey));

			std::string norm = NormalizePath(entry.Path);
			s_Entries[norm] = entry;
		}

		s_MountedWpackPath = wpackPath;
		s_IsMounted = true;
		WF_CORE_INFO("VFS::MountArchive - Mounted '{0}' ({1} files indexed).", wpackPath.string(), fileCount);
		return true;
	}

	void VFS::UnmountArchive()
	{
		s_Entries.clear();
		s_MountedWpackPath.clear();
		s_IsMounted = false;
	}

	bool VFS::IsMounted()
	{
		return s_IsMounted;
	}

	bool VFS::Exists(const std::filesystem::path& filepath)
	{
		if (std::filesystem::exists(filepath))
			return true;

		if (!s_IsMounted)
			return false;

		std::string norm = NormalizePath(filepath);
		return s_Entries.find(norm) != s_Entries.end();
	}

	Buffer VFS::ReadFile(const std::filesystem::path& filepath)
	{
		if (std::filesystem::exists(filepath))
		{
			std::ifstream stream(filepath, std::ios::binary | std::ios::ate);
			if (stream.is_open())
			{
				std::streamsize size = stream.tellg();
				stream.seekg(0, std::ios::beg);

				Buffer buf(size);
				if (size == 0 || stream.read(reinterpret_cast<char*>(buf.Data), size))
					return buf;
			}
		}

		if (s_IsMounted)
		{
			std::string norm = NormalizePath(filepath);
			auto it = s_Entries.find(norm);
			if (it != s_Entries.end())
			{
				const auto& entry = it->second;
				std::ifstream stream(s_MountedWpackPath, std::ios::binary);
				if (stream.is_open())
				{
					stream.seekg(entry.Offset, std::ios::beg);
					Buffer buf(entry.Size);
					if (entry.Size == 0 || stream.read(reinterpret_cast<char*>(buf.Data), entry.Size))
					{
						if (entry.XORKey != 0 && entry.Size > 0)
						{
							ObfuscateBuffer(buf.Data, buf.Size, entry.XORKey);
						}
						return buf;
					}
				}
			}
		}

		WF_CORE_WARN("VFS::ReadFile - Could not read file '{0}'.", filepath.string());
		return Buffer();
	}

	std::string VFS::ReadFileAsString(const std::filesystem::path& filepath)
	{
		Buffer buf = ReadFile(filepath);
		if (!buf)
			return "";

		return std::string(reinterpret_cast<char*>(buf.Data), buf.Size);
	}

	std::vector<std::string> VFS::GetMountedFilePaths()
	{
		std::vector<std::string> result;
		result.reserve(s_Entries.size());
		for (const auto& kv : s_Entries)
			result.push_back(kv.first);
		return result;
	}

}
