#pragma once

#include "Waffle/Core/Buffer.h"
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace Waffle {

	struct VFSEntry
	{
		std::string Path;
		uint64_t Offset = 0;
		uint64_t Size = 0;
		uint64_t UncompressedSize = 0;
		uint32_t Flags = 0;
		uint32_t XORKey = 0;
	};

	class VFS
	{
	public:
		static bool MountArchive(const std::filesystem::path& wpackPath);
		static void UnmountArchive();
		static bool IsMounted();

		static bool Exists(const std::filesystem::path& filepath);
		static Buffer ReadFile(const std::filesystem::path& filepath);
		static std::string ReadFileAsString(const std::filesystem::path& filepath);

		static std::vector<std::string> GetMountedFilePaths();

	private:
		static std::string NormalizePath(const std::filesystem::path& path);
		static void ObfuscateBuffer(uint8_t* data, uint64_t size, uint32_t key);

	private:
		static std::filesystem::path s_MountedWpackPath;
		static std::unordered_map<std::string, VFSEntry> s_Entries;
		static bool s_IsMounted;
	};

}
