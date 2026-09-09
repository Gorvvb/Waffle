#include "wfpch.h"
#include "Waffle/Core/VFS.h"
#include "Waffle/Core/Log.h"

#include <fstream>
#include <algorithm>
#include <cctype>

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

		// Match an "Assets" path SEGMENT (case-insensitive) - a plain
		// substring search also matched inside names like "MyAssets/..." and
		// truncated the path at the wrong position.
		std::string lower = s;
		std::transform(lower.begin(), lower.end(), lower.begin(),
			[](unsigned char c) { return (char)std::tolower(c); });

		size_t pos = 0;
		bool found = false;
		while ((pos = lower.find("assets", pos)) != std::string::npos)
		{
			bool leftOk = (pos == 0) || lower[pos - 1] == '/';
			size_t end = pos + 6;
			bool rightOk = (end == lower.size()) || lower[end] == '/';
			if (leftOk && rightOk)
			{
				found = true;
				break;
			}
			pos = end;
		}

		if (found)
		{
			s = "Assets" + s.substr(pos + 6);
		}
		else if (s_IsMounted)
		{
			std::string candidate = "Assets/" + s;
			if (s_Entries.find(candidate) != s_Entries.end())
				return candidate;
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

		std::error_code ec;
		if (!std::filesystem::exists(wpackPath, ec))
		{
			WF_CORE_WARN("VFS::MountArchive - File '{0}' does not exist.", wpackPath.string());
			return false;
		}

		uint64_t archiveSize = std::filesystem::file_size(wpackPath, ec);
		if (ec || archiveSize < 20) // header alone is magic+version+count
		{
			WF_CORE_ERROR("VFS::MountArchive - '{0}' is not a valid archive (size {1}).",
				wpackPath.string(), ec ? 0 : archiveSize);
			return false;
		}

		std::ifstream stream(wpackPath, std::ios::binary);
		if (!stream.is_open())
		{
			WF_CORE_ERROR("VFS::MountArchive - Could not open '{0}'.", wpackPath.string());
			return false;
		}

		// Reads a POD header field; every failure means a truncated/corrupt
		// pack and must abort the mount, not feed garbage into allocations.
		auto readField = [&stream](void* dst, uint32_t bytes) -> bool
		{
			stream.read(reinterpret_cast<char*>(dst), bytes);
			return (bool)stream;
		};

		uint32_t magic = 0;
		uint32_t version = 0;
		uint32_t fileCount = 0;

		if (!readField(&magic, sizeof(magic)) || magic != WFPK_MAGIC)
		{
			WF_CORE_ERROR("VFS::MountArchive - Invalid magic header in '{0}'. Expected 'WFPK'.", wpackPath.string());
			return false;
		}

		if (!readField(&version, sizeof(version)) || !readField(&fileCount, sizeof(fileCount)))
		{
			WF_CORE_ERROR("VFS::MountArchive - Truncated header in '{0}'.", wpackPath.string());
			return false;
		}

		// Sanity bound: a corrupted length field must not turn into a
		// multi-gigabyte allocation or a 4-billion-iteration loop.
		constexpr uint32_t MaxPathLength = 4096;
		constexpr uint32_t MaxFileCount  = 100000;
		if (fileCount > MaxFileCount)
		{
			WF_CORE_ERROR("VFS::MountArchive - Implausible file count ({0}) in '{1}' - corrupt archive?",
				fileCount, wpackPath.string());
			return false;
		}

		uint32_t mountedCount = 0;
		for (uint32_t i = 0; i < fileCount; ++i)
		{
			uint32_t pathLength = 0;
			if (!readField(&pathLength, sizeof(pathLength)))
			{
				WF_CORE_ERROR("VFS::MountArchive - Truncated entry #{0} in '{1}'.", i, wpackPath.string());
				break;
			}
			if (pathLength == 0 || pathLength > MaxPathLength)
			{
				WF_CORE_ERROR("VFS::MountArchive - Invalid path length ({0}) in entry #{1} of '{2}'.",
					pathLength, i, wpackPath.string());
				break;
			}

			std::string pathStr(pathLength, '\0');
			stream.read(&pathStr[0], pathLength);
			if (!stream)
			{
				WF_CORE_ERROR("VFS::MountArchive - Truncated path in entry #{0} of '{1}'.", i, wpackPath.string());
				break;
			}

			VFSEntry entry;
			if (!readField(&entry.Offset, sizeof(entry.Offset)) ||
				!readField(&entry.Size, sizeof(entry.Size)) ||
				!readField(&entry.UncompressedSize, sizeof(entry.UncompressedSize)) ||
				!readField(&entry.Flags, sizeof(entry.Flags)) ||
				!readField(&entry.XORKey, sizeof(entry.XORKey)))
			{
				WF_CORE_ERROR("VFS::MountArchive - Truncated entry data #{0} in '{1}'.", i, wpackPath.string());
				break;
			}

			// Entry window must lie inside the archive file.
			if (entry.Offset >= archiveSize ||
				entry.Size > archiveSize - entry.Offset)
			{
				WF_CORE_WARN("VFS::MountArchive - Entry '{0}' points outside the archive; skipped.", pathStr);
				continue;
			}

			std::string norm = NormalizePath(pathStr);
			s_Entries[norm] = entry;
			mountedCount++;
		}

		if (mountedCount == 0)
		{
			WF_CORE_ERROR("VFS::MountArchive - No usable entries in '{0}'.", wpackPath.string());
			return false;
		}

		s_MountedWpackPath = wpackPath;
		s_IsMounted = true;
		WF_CORE_INFO("VFS::MountArchive - Mounted '{0}' ({1} files indexed).", wpackPath.string(), mountedCount);
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
