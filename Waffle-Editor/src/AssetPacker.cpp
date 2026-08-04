#include "AssetPacker.h"
#include "Waffle/Core/Log.h"
#include <fstream>
#include <iostream>
#include <random>

namespace Waffle {

	static const uint32_t WFPK_MAGIC = 0x4B504657; // "WFPK"
	static const uint32_t WFPK_VERSION = 1;

	static void ObfuscateData(uint8_t* data, uint64_t size, uint32_t key)
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

	bool AssetPacker::CreateArchive(const AssetPackerOptions& options, std::string& outErrorMessage)
	{
		outErrorMessage.clear();

		if (!std::filesystem::exists(options.SourceDirectory))
		{
			outErrorMessage = "Source directory does not exist: " + options.SourceDirectory.string();
			return false;
		}

		std::error_code ec;
		std::vector<std::filesystem::path> files;

		for (const auto& entry : std::filesystem::recursive_directory_iterator(options.SourceDirectory, ec))
		{
			if (entry.is_regular_file(ec))
			{
				std::string ext = entry.path().extension().string();
				if (ext != ".wpack" && ext != ".exe")
				{
					files.push_back(entry.path());
				}
			}
		}

		if (files.empty())
		{
			WF_CORE_WARN("AssetPacker::CreateArchive - Source directory '{0}' contains no files to pack.",
				options.SourceDirectory.string());
		}

		std::filesystem::create_directories(options.OutputWpackPath.parent_path(), ec);
		std::ofstream out(options.OutputWpackPath, std::ios::binary);
		if (!out.is_open())
		{
			outErrorMessage = "Could not create output archive: " + options.OutputWpackPath.string();
			return false;
		}

		uint32_t fileCount = static_cast<uint32_t>(files.size());
		out.write(reinterpret_cast<const char*>(&WFPK_MAGIC), sizeof(WFPK_MAGIC));
		out.write(reinterpret_cast<const char*>(&WFPK_VERSION), sizeof(WFPK_VERSION));
		out.write(reinterpret_cast<const char*>(&fileCount), sizeof(fileCount));

		struct PackEntry
		{
			std::string RelativePath;
			std::filesystem::path FullPath;
			uint64_t Size = 0;
			uint64_t OffsetPlaceholder = 0;
			uint32_t Key = 0;
		};

		std::vector<PackEntry> entries;
		entries.reserve(files.size());

		std::mt19937 rng(1337);

		for (const auto& filePath : files)
		{
			PackEntry pe;
			pe.FullPath = filePath;

			std::filesystem::path rel = std::filesystem::relative(filePath, options.SourceDirectory, ec);
			if (ec || rel.empty())
				rel = filePath.filename();

			std::string relStr = rel.string();
			std::replace(relStr.begin(), relStr.end(), '\\', '/');
			pe.RelativePath = "Assets/" + relStr;
			pe.Size = std::filesystem::file_size(filePath, ec);
			pe.Key = options.EncryptionKey ^ static_cast<uint32_t>(rng());
			entries.push_back(pe);
		}

		uint64_t indexHeaderSize = sizeof(WFPK_MAGIC) + sizeof(WFPK_VERSION) + sizeof(fileCount);
		for (const auto& e : entries)
		{
			indexHeaderSize += sizeof(uint32_t) + e.RelativePath.size();
			indexHeaderSize += sizeof(uint64_t) * 3;
			indexHeaderSize += sizeof(uint32_t) * 2;
		}

		uint64_t currentDataOffset = indexHeaderSize;

		for (auto& e : entries)
		{
			e.OffsetPlaceholder = currentDataOffset;

			uint32_t pathLength = static_cast<uint32_t>(e.RelativePath.size());
			out.write(reinterpret_cast<const char*>(&pathLength), sizeof(pathLength));
			out.write(e.RelativePath.data(), pathLength);

			uint64_t offset = currentDataOffset;
			uint64_t size = e.Size;
			uint64_t uncompressedSize = e.Size;
			uint32_t flags = 0;

			out.write(reinterpret_cast<const char*>(&offset), sizeof(offset));
			out.write(reinterpret_cast<const char*>(&size), sizeof(size));
			out.write(reinterpret_cast<const char*>(&uncompressedSize), sizeof(uncompressedSize));
			out.write(reinterpret_cast<const char*>(&flags), sizeof(flags));
			out.write(reinterpret_cast<const char*>(&e.Key), sizeof(e.Key));

			currentDataOffset += size;
		}

		for (const auto& e : entries)
		{
			std::ifstream in(e.FullPath, std::ios::binary);
			if (!in.is_open())
			{
				outErrorMessage = "Failed to read input file: " + e.FullPath.string();
				return false;
			}

			std::vector<uint8_t> buffer(e.Size);
			if (e.Size > 0)
			{
				in.read(reinterpret_cast<char*>(buffer.data()), e.Size);
				if (e.Key != 0)
				{
					ObfuscateData(buffer.data(), e.Size, e.Key);
				}
				out.write(reinterpret_cast<const char*>(buffer.data()), e.Size);
			}
		}

		WF_CORE_INFO("AssetPacker::CreateArchive - Packed {0} files into '{1}' ({2} bytes).",
			fileCount, options.OutputWpackPath.string(), currentDataOffset);
		return true;
	}

}
