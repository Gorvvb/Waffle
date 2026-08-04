#pragma once

#include <filesystem>
#include <string>

namespace Waffle {

	struct AssetPackerOptions
	{
		std::filesystem::path SourceDirectory;
		std::filesystem::path OutputWpackPath;
		uint32_t EncryptionKey = 0x57414646; // "WAFF" default key
		bool Compress = false;
	};

	class AssetPacker
	{
	public:
		static bool CreateArchive(const AssetPackerOptions& options, std::string& outErrorMessage);
	};

}
