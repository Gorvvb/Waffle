#include "ProjectExporter.h"
#include "AssetPacker.h"
#include "Waffle/Core/Log.h"
#include "Waffle/Project/RuntimeProjectConfig.h"
#include "Waffle/Renderer/PostProcessing.h"
#include "Waffle/Renderer/Renderer.h"
#include "Waffle/Renderer/RendererAPI.h"
#include "Waffle/Scripting/CSharpScriptEngine.h"

#if defined(WF_PLATFORM_WINDOWS)
	#include <windows.h>
	#include <shlobj.h>
#endif

#include <vector>
#include <fstream>
#include <iostream>
#include <cmath>
#include <algorithm>

#include <yaml-cpp/yaml.h>

#include "stb_image/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "GLFW/deps/stb_image_write.h"

namespace Waffle {

#if defined(WF_PLATFORM_WINDOWS)

#pragma pack(push, 1)
struct ICONDIRHEADER {
	uint16_t idReserved;
	uint16_t idType;
	uint16_t idCount;
};

struct ICONDIRENTRY {
	uint8_t  bWidth;
	uint8_t  bHeight;
	uint8_t  bColorCount;
	uint8_t  bReserved;
	uint16_t wPlanes;
	uint16_t wBitsPerPixel;
	uint32_t dwBytesInRes;
	uint32_t dwImageOffset;
};

struct GRPICONDIRENTRY {
	uint8_t  bWidth;
	uint8_t  bHeight;
	uint8_t  bColorCount;
	uint8_t  bReserved;
	uint16_t wPlanes;
	uint16_t wBitsPerPixel;
	uint32_t dwBytesInRes;
	uint16_t nID;
};

struct GRPICONDIR {
	uint16_t idReserved;
	uint16_t idType;
	uint16_t idCount;
};
#pragma pack(pop)

struct ResItem {
	// Enum callback type/name pointers die with FreeLibrary - keep int IDs as numbers, copy string names.
	bool typeIsInt = false, nameIsInt = false;
	uintptr_t typeInt = 0, nameInt = 0;
	std::wstring typeStr, nameStr;
	WORD lang = 0;

	LPCWSTR Type() const { return typeIsInt ? (LPCWSTR)typeInt : typeStr.c_str(); }
	LPCWSTR Name() const { return nameIsInt ? (LPCWSTR)nameInt : nameStr.c_str(); }
};

static void StoreResID(LPCWSTR p, bool& isInt, uintptr_t& intVal, std::wstring& str)
{
	if (IS_INTRESOURCE(p))
	{
		isInt = true;
		intVal = (uintptr_t)p;
	}
	else
	{
		isInt = false;
		str = p;
	}
}

static BOOL CALLBACK EnumLangsCB(HMODULE hModule, LPCWSTR lpType, LPCWSTR lpName, WORD wLanguage, LONG_PTR lParam)
{
	auto* list = reinterpret_cast<std::vector<ResItem>*>(lParam);
	ResItem item;
	item.lang = wLanguage;
	StoreResID(lpType, item.typeIsInt, item.typeInt, item.typeStr);
	StoreResID(lpName, item.nameIsInt, item.nameInt, item.nameStr);
	list->push_back(std::move(item));
	return TRUE;
}

static BOOL CALLBACK EnumNamesCB(HMODULE hModule, LPCWSTR lpType, LPWSTR lpName, LONG_PTR lParam)
{
	EnumResourceLanguagesW(hModule, lpType, lpName, EnumLangsCB, lParam);
	return TRUE;
}

static std::vector<uint8_t> FlipVertical(const std::vector<uint8_t>& src, int width, int height)
{
	std::vector<uint8_t> flipped(src.size());
	size_t rowBytes = (size_t)width * 4;
	for (int y = 0; y < height; y++)
	{
		const uint8_t* srcRow = src.data() + (size_t)(height - 1 - y) * rowBytes;
		uint8_t*       dstRow = flipped.data() + (size_t)y * rowBytes;
		memcpy(dstRow, srcRow, rowBytes);
	}
	return flipped;
}

static std::vector<uint8_t> ResizeRGBA(const uint8_t* src, int srcW, int srcH, int dstW, int dstH)
{
	std::vector<uint8_t> dst((size_t)dstW * dstH * 4);
	float xRatio = (float)srcW / dstW;
	float yRatio = (float)srcH / dstH;

	for (int y = 0; y < dstH; y++)
	{
		float srcY = (y + 0.5f) * yRatio - 0.5f;
		int y0 = (int)std::floor(srcY);
		int y1 = std::min(y0 + 1, srcH - 1);
		y0 = std::max(y0, 0);
		float yWeight = srcY - y0;

		for (int x = 0; x < dstW; x++)
		{
			float srcX = (x + 0.5f) * xRatio - 0.5f;
			int x0 = (int)std::floor(srcX);
			int x1 = std::min(x0 + 1, srcW - 1);
			x0 = std::max(x0, 0);
			float xWeight = srcX - x0;

			for (int c = 0; c < 4; c++)
			{
				float p00 = src[(y0 * srcW + x0) * 4 + c];
				float p10 = src[(y0 * srcW + x1) * 4 + c];
				float p01 = src[(y1 * srcW + x0) * 4 + c];
				float p11 = src[(y1 * srcW + x1) * 4 + c];

				float top    = p00 * (1.0f - xWeight) + p10 * xWeight;
				float bottom = p01 * (1.0f - xWeight) + p11 * xWeight;
				float val    = top  * (1.0f - yWeight) + bottom * yWeight;

				dst[(y * dstW + x) * 4 + c] = (uint8_t)std::clamp(val, 0.0f, 255.0f);
			}
		}
	}
	return dst;
}

// Collects existing RT_ICON/RT_GROUP_ICON resources so they can be purged in the same update session.
static std::vector<ResItem> CollectExistingIconResources(const std::filesystem::path& exePath)
{
	std::vector<ResItem> items;
	HMODULE hMod = LoadLibraryExW(exePath.wstring().c_str(), NULL, LOAD_LIBRARY_AS_DATAFILE);
	if (hMod)
	{
		EnumResourceNamesW(hMod, RT_GROUP_ICON, EnumNamesCB, (LONG_PTR)&items);
		EnumResourceNamesW(hMod, RT_ICON,       EnumNamesCB, (LONG_PTR)&items);
		FreeLibrary(hMod);
	}
	return items;
}

static bool EmbedIconInExecutable(const std::filesystem::path& exePath, const std::filesystem::path& iconPath)
{
	std::error_code ec;
	if (!std::filesystem::exists(exePath, ec) || !std::filesystem::exists(iconPath, ec))
		return false;

	// Collect existing icons BEFORE opening the update handle; purge + write happen in one session.
	std::vector<ResItem> existingItems = CollectExistingIconResources(exePath);

	std::ifstream iconFile(iconPath, std::ios::binary | std::ios::ate);
	if (!iconFile.is_open())
		return false;

	std::streamsize fileSize = iconFile.tellg();
	iconFile.seekg(0, std::ios::beg);

	std::vector<uint8_t> fileData((size_t)fileSize);
	if (!iconFile.read(reinterpret_cast<char*>(fileData.data()), fileSize))
		return false;

	iconFile.close();

	std::string ext = iconPath.extension().string();
	for (char& c : ext) { c = (char)tolower(c); }

	WORD langIDs[] = {
		MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),
		MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US)
	};

	// Build the icon payload (grpBuffer + per-image data) up front so purge + write stay atomic.

	struct IconResData {
		int width, height;
		std::vector<uint8_t> bytes;
	};
	std::vector<IconResData> iconResList;
	std::vector<uint8_t>     grpBuffer;
	std::vector<GRPICONDIRENTRY> grpEntriesScratch;
	bool                     isIco = false;

	if (ext == ".ico" && fileSize >= (std::streamsize)sizeof(ICONDIRHEADER))
	{
		const ICONDIRHEADER* header = reinterpret_cast<const ICONDIRHEADER*>(fileData.data());
		if (header->idReserved == 0 && header->idType == 1 && header->idCount > 0)
		{
			uint16_t count = header->idCount;
			if ((size_t)fileSize >= sizeof(ICONDIRHEADER) + count * sizeof(ICONDIRENTRY))
			{
				const ICONDIRENTRY* entries = reinterpret_cast<const ICONDIRENTRY*>(
					fileData.data() + sizeof(ICONDIRHEADER));

				// Keep only in-file entries (uint32 offset+size wraps); GRP IDs match the written RT_ICON IDs.
				for (uint16_t i = 0; i < count; i++)
				{
					uint64_t dataEnd = (uint64_t)entries[i].dwImageOffset + entries[i].dwBytesInRes;
					if (dataEnd == 0 || dataEnd > (uint64_t)fileData.size())
					{
						WF_CORE_WARN("Export: ICO entry {0} points outside the file; skipped.", i);
						continue;
					}

					iconResList.push_back({
						entries[i].bWidth  ? entries[i].bWidth  : 256,
						entries[i].bHeight ? entries[i].bHeight : 256,
						std::vector<uint8_t>(
							fileData.data() + entries[i].dwImageOffset,
							fileData.data() + dataEnd)
					});

					GRPICONDIRENTRY ge{};
					ge.bWidth        = entries[i].bWidth;
					ge.bHeight       = entries[i].bHeight;
					ge.bColorCount   = entries[i].bColorCount;
					ge.bReserved     = 0;
					ge.wPlanes       = entries[i].wPlanes;
					ge.wBitsPerPixel = entries[i].wBitsPerPixel;
					ge.dwBytesInRes  = entries[i].dwBytesInRes;
					ge.nID           = (uint16_t)iconResList.size();
					grpEntriesScratch.push_back(ge);
				}

				if (!grpEntriesScratch.empty())
				{
					uint16_t numEntries = (uint16_t)iconResList.size();
					size_t gs = sizeof(GRPICONDIR) + numEntries * sizeof(GRPICONDIRENTRY);
					grpBuffer.assign(gs, 0);

					GRPICONDIR* grpHdr = reinterpret_cast<GRPICONDIR*>(grpBuffer.data());
					grpHdr->idReserved = 0;
					grpHdr->idType     = 1;
					grpHdr->idCount    = numEntries;

					GRPICONDIRENTRY* grpEntries = reinterpret_cast<GRPICONDIRENTRY*>(
						grpBuffer.data() + sizeof(GRPICONDIR));
					for (uint16_t i = 0; i < numEntries; i++)
						grpEntries[i] = grpEntriesScratch[i];
				}

				isIco = !iconResList.empty();
			}
		}
	}

	if (!isIco)
	{
		// Raster path: resize to standard sizes + PNG; decode from memory (stbi_load fails on non-ASCII paths).
		int width = 0, height = 0, channels = 0;
		stbi_uc* srcPixels = stbi_load_from_memory(fileData.data(), (int)fileSize, &width, &height, &channels, 4);
		if (!srcPixels)
			return false;

		std::vector<int> targetSizes = { 256, 48, 32, 16 };
		for (int sz : targetSizes)
		{
			if (sz > width && sz > height && !iconResList.empty())
				continue;

			std::vector<uint8_t> resized = (sz == width && sz == height)
				? std::vector<uint8_t>(srcPixels, srcPixels + width * height * 4)
				: ResizeRGBA(srcPixels, width, height, sz, sz);

			// Windows DIB format expects bottom-up row order; flip before PNG encoding
			std::vector<uint8_t> flipped = FlipVertical(resized, sz, sz);

			std::vector<uint8_t> pngBytes;
			stbi_write_png_to_func(
				[](void* ctx, void* data, int size) {
					auto* vec = static_cast<std::vector<uint8_t>*>(ctx);
					auto* b   = static_cast<uint8_t*>(data);
					vec->insert(vec->end(), b, b + size);
				},
				&pngBytes, sz, sz, 4, flipped.data(), sz * 4);

			if (!pngBytes.empty())
				iconResList.push_back({ sz, sz, std::move(pngBytes) });
		}
		stbi_image_free(srcPixels);

		if (iconResList.empty())
			return false;

		uint16_t numEntries = (uint16_t)iconResList.size();
		size_t gs = sizeof(GRPICONDIR) + numEntries * sizeof(GRPICONDIRENTRY);
		grpBuffer.assign(gs, 0);

		GRPICONDIR* grpHdr = reinterpret_cast<GRPICONDIR*>(grpBuffer.data());
		grpHdr->idReserved = 0;
		grpHdr->idType     = 1;
		grpHdr->idCount    = numEntries;

		GRPICONDIRENTRY* grpEntries = reinterpret_cast<GRPICONDIRENTRY*>(
			grpBuffer.data() + sizeof(GRPICONDIR));

		for (uint16_t i = 0; i < numEntries; i++)
		{
			const auto& res        = iconResList[i];
			grpEntries[i].bWidth       = (res.width  >= 256) ? 0 : (uint8_t)res.width;
			grpEntries[i].bHeight      = (res.height >= 256) ? 0 : (uint8_t)res.height;
			grpEntries[i].bColorCount  = 0;
			grpEntries[i].bReserved    = 0;
			grpEntries[i].wPlanes      = 1;
			grpEntries[i].wBitsPerPixel= 32;
			grpEntries[i].dwBytesInRes = (uint32_t)res.bytes.size();
			grpEntries[i].nID          = i + 1;
		}
	}

	// Single update session: purge old icons then write new ones
	HANDLE hUpdate = BeginUpdateResourceW(exePath.wstring().c_str(), FALSE);
	if (!hUpdate)
	{
		WF_CORE_WARN("BeginUpdateResourceW failed for {0}", exePath.string());
		return false;
	}

	// Step A: purge every existing RT_ICON and RT_GROUP_ICON in this same session
	for (const auto& item : existingItems)
	{
		UpdateResourceW(hUpdate, item.Type(), item.Name(), item.lang, NULL, 0);
	}

	// Step B: write the new icon images
	for (uint16_t i = 0; i < (uint16_t)iconResList.size(); i++)
	{
		uint16_t iconID = i + 1;
		for (WORD lang : langIDs)
		{
			UpdateResourceW(hUpdate, RT_ICON, MAKEINTRESOURCEW(iconID), lang,
				(void*)iconResList[i].bytes.data(), (DWORD)iconResList[i].bytes.size());
		}
	}

	// Step C: write the group icon directory
	for (WORD lang : langIDs)
	{
		UpdateResourceW(hUpdate, RT_GROUP_ICON, MAKEINTRESOURCEW(1), lang,
			grpBuffer.data(), (DWORD)grpBuffer.size());
	}

	bool success = EndUpdateResourceW(hUpdate, FALSE);
	if (success)
	{
		SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
		std::filesystem::last_write_time(exePath, std::filesystem::file_time_type::clock::now(), ec);
		WF_CORE_INFO("Successfully embedded icon '{0}' into '{1}'",
			iconPath.string(), exePath.string());
	}
	else
	{
		WF_CORE_WARN("EndUpdateResourceW failed when embedding icon for {0}", exePath.string());
	}
	return success;
}

#endif

// Byte offset of the LAST "Assets" path segment, or npos - a plain find matched "D:/GameAssets/..." and broke runtime scene paths.
static size_t FindLastAssetsSegment(const std::string& s)
{
	size_t best = std::string::npos;
	size_t pos = 0;
	while ((pos = s.find("Assets", pos)) != std::string::npos)
	{
		bool leftOk = (pos == 0) || s[pos - 1] == '/';
		size_t end = pos + 6;
		bool rightOk = (end == s.size()) || s[end] == '/';
		if (leftOk && rightOk)
			best = pos;
		pos = end;
	}
	return best;
}

static std::filesystem::path FindRuntimeExecutable()
{
	std::vector<std::filesystem::path> candidates;

#if defined(WF_PLATFORM_WINDOWS)
	char buffer[MAX_PATH];
	GetModuleFileNameA(NULL, buffer, MAX_PATH);
	std::filesystem::path exeDir = std::filesystem::path(buffer).parent_path();
#else
	std::filesystem::path exeDir = std::filesystem::current_path();
#endif

	// Order matters: Dist/Release before Debug so exports never ship the Debug runtime; the "../WafflePlayer" sibling only fits deployed layouts.
	candidates = {
		exeDir / "../../bin/Dist-windows-x86_64/WafflePlayer/WafflePlayer.exe",
		exeDir / "../../bin/Release-windows-x86_64/WafflePlayer/WafflePlayer.exe",
		exeDir / "../../bin/Debug-windows-x86_64/WafflePlayer/WafflePlayer.exe",
		exeDir / "WafflePlayer.exe",
		exeDir / "../WafflePlayer/WafflePlayer.exe",
		"bin/Dist-windows-x86_64/WafflePlayer/WafflePlayer.exe",
		"bin/Release-windows-x86_64/WafflePlayer/WafflePlayer.exe",
		"bin/Debug-windows-x86_64/WafflePlayer/WafflePlayer.exe",
		"../bin/Dist-windows-x86_64/WafflePlayer/WafflePlayer.exe",
		"../bin/Release-windows-x86_64/WafflePlayer/WafflePlayer.exe",
		"../bin/Debug-windows-x86_64/WafflePlayer/WafflePlayer.exe",
		"WafflePlayer.exe"
	};

	for (const auto& path : candidates)
	{
		std::error_code ec;
		if (std::filesystem::exists(path, ec))
		{
			auto canon = std::filesystem::canonical(path, ec);
			std::filesystem::path chosen = ec ? path : canon;

			// Debug runtime = console window + validation spam in the player's face - warn loudly.
			std::string chosenStr = chosen.string();
			if (chosenStr.find("Debug-windows") != std::string::npos)
				WF_CORE_WARN("ProjectExporter: shipping the DEBUG runtime - "
					"build the Dist configuration for a clean export ({0})", chosenStr);

			return chosen;
		}
	}

	// Last resort: bounded upward search that skips vendor/.git/build dirs so it can't burn minutes scanning the drive.
	try
	{
		auto isPruned = [](const std::filesystem::path& dir)
		{
			std::string name = dir.filename().string();
			for (auto& c : name) c = (char)tolower((unsigned char)c);
			return name == "vendor" || name == ".git" || name == ".vs" ||
				name == "intermediate" || name == "node_modules" ||
				name == "wafflehub" || name == "projects";
		};

		std::filesystem::path current = exeDir;
		for (int depth = 0; depth < 4 && current.has_parent_path(); ++depth)
		{
			std::error_code ec;
			// operator* yields a CONST entry; pruning is an iterator operation, so walk the iterator explicitly.
			for (auto it = std::filesystem::recursive_directory_iterator(current, ec);
				it != std::filesystem::recursive_directory_iterator(); ++it)
			{
				if (ec)
					break;
				if (it->is_regular_file(ec) && it->path().filename() == "WafflePlayer.exe")
					return it->path();
				if (it->is_directory(ec) && isPruned(it->path()))
					it.disable_recursion_pending();
			}
			current = current.parent_path();
		}
	}
	catch (...)
	{
	}

	return "";
}

	static std::string GetModuleFileNameA_()
	{
		char buffer[MAX_PATH] = {};
		GetModuleFileNameA(nullptr, buffer, MAX_PATH);
		return buffer;
	}

	bool ProjectExporter::ExportProject(const ExportOptions& options, std::string& outErrorMessage)
	{
		outErrorMessage.clear();

		std::string appName = options.AppName.empty() ? options.ProjectName : options.AppName;
		if (appName.empty())
			appName = "WaffleGame";

		std::filesystem::path activeProjectPath = options.ProjectPath;
		if (activeProjectPath.empty())
		{
			activeProjectPath = std::filesystem::current_path();
		}
		else
		{
			activeProjectPath = std::filesystem::absolute(activeProjectPath);
		}

		std::filesystem::path exportsDir = activeProjectPath / "Exports";
		std::error_code ec;

		std::filesystem::create_directories(exportsDir, ec);
		if (ec)
		{
			outErrorMessage = "Failed to create export directory: " + exportsDir.string() + " (" + ec.message() + ")";
			return false;
		}

		// 1. Locate WafflePlayer.exe
		std::filesystem::path runtimeExe = FindRuntimeExecutable();
		if (runtimeExe.empty() || !std::filesystem::exists(runtimeExe))
		{
			outErrorMessage = "Could not find WafflePlayer.exe template. Please build the WafflePlayer project first.";
			return false;
		}

		// 2. Copy WafflePlayer.exe -> Exports/[AppName].exe
		std::filesystem::path targetExePath = exportsDir / (appName + ".exe");
		std::filesystem::copy_file(runtimeExe, targetExePath,
			std::filesystem::copy_options::overwrite_existing, ec);
		if (ec)
		{
			outErrorMessage = "Failed to copy game executable: " + ec.message();
			return false;
		}

		// 2b. Ship runtime DLLs the exe links against; shaderc_shared.dll resolves via the Vulkan SDK, which players lack.
		{
			const char* requiredDlls[] = { "shaderc_shared.dll" };
			std::filesystem::path sdkBin;
			if (const char* sdk = getenv("VULKAN_SDK"))
				sdkBin = std::filesystem::path(sdk) / "Bin";

			// The editor's own directory often carries the DLL (it links the same shaderc).
			char editorModuleBuffer[MAX_PATH];
			GetModuleFileNameA(nullptr, editorModuleBuffer, MAX_PATH);
			std::filesystem::path editorDir = std::filesystem::path(editorModuleBuffer).parent_path();

			for (const char* dll : requiredDlls)
			{
				std::vector<std::filesystem::path> dllCandidates = {
					runtimeExe.parent_path() / dll,
					editorDir / dll,
					sdkBin / dll
				};
				bool shipped = false;
				for (const auto& dllPath : dllCandidates)
				{
					if (std::filesystem::exists(dllPath, ec))
					{
						std::filesystem::copy_file(dllPath, exportsDir / dll,
							std::filesystem::copy_options::overwrite_existing, ec);
						if (ec)
						{
							outErrorMessage = std::string("Failed to copy runtime DLL ") + dll + ": " + ec.message();
							return false;
						}
						shipped = true;
						break;
					}
				}

				// A missing DLL must fail the export LOUDLY: the packaged game wouldn't start without the Vulkan SDK.
				if (!shipped)
				{
					outErrorMessage = std::string("Could not find ") + dll +
						" to ship with the game. Build the WafflePlayer project "
						"(the DLL is copied next to its exe), make sure the VULKAN_SDK "
						"environment variable is set, and export again.";
					return false;
				}
			}
		}

		// 3. Bundle Assets
		std::filesystem::path sourceAssets = activeProjectPath / "Assets";
		std::filesystem::path targetAssets = exportsDir / "Assets";
		std::filesystem::create_directories(targetAssets, ec);

		if (std::filesystem::exists(sourceAssets))
		{
			std::filesystem::copy(sourceAssets, targetAssets,
				std::filesystem::copy_options::overwrite_existing |
				std::filesystem::copy_options::recursive, ec);
			if (ec)
			{
				outErrorMessage = "Failed to copy project assets: " + ec.message();
				return false;
			}
		}

#if defined(WF_PLATFORM_WINDOWS)
		char modBuffer[MAX_PATH];
		GetModuleFileNameA(NULL, modBuffer, MAX_PATH);
		std::filesystem::path exeDir = std::filesystem::path(modBuffer).parent_path();
#else
		std::filesystem::path exeDir = std::filesystem::current_path();
#endif

		std::vector<std::filesystem::path> engineAssetCandidates = {
			exeDir / "Assets",
			exeDir / "../../WaffleEditor/Assets",
			exeDir / "../../../WaffleEditor/Assets"
		};

		// 4. Ensure engine base shaders exist in export Assets/shaders
		std::filesystem::path targetShaders = targetAssets / "shaders";
		std::filesystem::create_directories(targetShaders, ec);

		for (const auto& baseDir : engineAssetCandidates)
		{
			std::filesystem::path shaderDir = baseDir / "shaders";
			if (std::filesystem::exists(shaderDir, ec))
			{
				std::filesystem::copy(shaderDir, targetShaders,
					std::filesystem::copy_options::overwrite_existing |
					std::filesystem::copy_options::recursive, ec);
				break;
			}
		}

		// 5. Ensure engine base fonts exist in export Assets/fonts
		std::filesystem::path targetFonts = targetAssets / "fonts";
		std::filesystem::create_directories(targetFonts, ec);

		for (const auto& baseDir : engineAssetCandidates)
		{
			std::filesystem::path fontDir = baseDir / "fonts";
			if (std::filesystem::exists(fontDir, ec))
			{
				std::filesystem::copy(fontDir, targetFonts,
					std::filesystem::copy_options::overwrite_existing |
					std::filesystem::copy_options::recursive, ec);
				break;
			}
		}
		if (!std::filesystem::exists(targetFonts / "OpenSans", ec))
			WF_CORE_WARN("ProjectExporter: no fonts staged into the export - UI text will be invisible! (looked for an fonts/OpenSans folder next to the editor)");

		// 6. Ensure shader cache exists in export Assets/cache
		std::filesystem::path targetCache = targetAssets / "cache";
		std::filesystem::create_directories(targetCache, ec);

		for (const auto& baseDir : engineAssetCandidates)
		{
			std::filesystem::path cacheDir = baseDir / "cache";
			if (std::filesystem::exists(cacheDir, ec))
			{
				std::filesystem::copy(cacheDir, targetCache,
					std::filesystem::copy_options::overwrite_existing |
					std::filesystem::copy_options::recursive, ec);
				break;
			}
		}

		// 7. Copy extra .cs scripts outside Assets/ into targetAssets/Scripts, keeping relative dirs (flat copy clobbers same names).
		if (std::filesystem::exists(activeProjectPath))
		{
			std::filesystem::path targetScripts = targetAssets / "Scripts";
			std::filesystem::create_directories(targetScripts, ec);

			std::error_code iterEc;
			for (const auto& entry : std::filesystem::recursive_directory_iterator(activeProjectPath, iterEc))
			{
				if (iterEc)
					break;
				if (!entry.is_regular_file(iterEc))
					continue;

				std::string ext = entry.path().extension().string();
				for (auto& c : ext) c = (char)tolower((unsigned char)c);
				if (ext != ".cs")
					continue;

				std::error_code relEc;
				std::filesystem::path rel = std::filesystem::relative(entry.path(), activeProjectPath, relEc);
				if (relEc || rel.empty())
					continue;

				// Skip anything under an Assets/Exports/.git segment (any case).
				bool skip = false;
				for (const auto& part : rel)
				{
					std::string p = part.string();
					for (auto& c : p) c = (char)tolower((unsigned char)c);
					if (p == "assets" || p == "exports" || p == ".git")
					{
						skip = true;
						break;
					}
				}
				if (skip)
					continue;

				std::filesystem::path dest = targetScripts / rel;
				std::filesystem::create_directories(dest.parent_path(), ec);
				std::filesystem::copy_file(entry.path(), dest,
					std::filesystem::copy_options::overwrite_existing, ec);
			}
		}

		// 7b. Stage the embedded .NET runtime (<exe dir>/host/fxr + shared/Microsoft.NETCore.App) + DotnetRuntime folder so exports need no .NET install.
		{
			// Locate the dev dotnet root the same way the engine host does.
			std::filesystem::path dotnetRoot;
			if (const char* pf = std::getenv("ProgramFiles"); pf && *pf)
				dotnetRoot = std::filesystem::path(pf) / "dotnet";
			if (const char* env = std::getenv("DOTNET_ROOT"); env && *env)
				dotnetRoot = env;

			std::error_code stageEc;
			std::filesystem::path fxrDir = dotnetRoot / "host" / "fxr";
			std::filesystem::path sharedDir = dotnetRoot / "shared" / "Microsoft.NETCore.App";
			if (!dotnetRoot.empty() && std::filesystem::exists(fxrDir, stageEc) && std::filesystem::exists(sharedDir, stageEc))
			{
				// Highest-versioned host fxr + shared runtime.
				auto highestVersion = [](const std::filesystem::path& dir)
				{
					std::filesystem::path best;
					unsigned long long bestVer[4] = { 0, 0, 0, 0 };
					std::error_code itEc;
					for (auto& entry : std::filesystem::directory_iterator(dir, itEc))
					{
						if (!entry.is_directory(itEc))
							continue;
						unsigned long long v[4] = { 0, 0, 0, 0 };
						int index = 0, valid = 0;
						for (char c : entry.path().filename().string())
						{
							if (c == '.') { if (++index > 3) break; continue; }
							if (c < '0' || c > '9') { valid = 0; break; }
							valid = 1;
							v[index] = v[index] * 10 + (c - '0');
						}
						if (!valid || index == 0)
							continue;
						bool greater = false;
						for (int i = 0; i < 4; i++)
						{
							if (v[i] != bestVer[i]) { greater = v[i] > bestVer[i]; break; }
						}
						if (best.empty() || greater)
						{
							std::copy(std::begin(v), std::end(v), std::begin(bestVer));
							best = entry.path();
						}
					}
					return best;
				};

				std::filesystem::path fxrVersion = highestVersion(fxrDir);
				std::filesystem::path sharedVersion = highestVersion(sharedDir);
				if (!fxrVersion.empty() && !sharedVersion.empty())
				{
					// std::filesystem::copy does NOT create missing parents - without these the copies fail silently into ec.
					std::filesystem::create_directories(exportsDir / "host" / "fxr", stageEc);
					std::filesystem::create_directories(exportsDir / "shared" / "Microsoft.NETCore.App", stageEc);
					std::filesystem::copy(fxrVersion, exportsDir / "host" / "fxr" / fxrVersion.filename(),
						std::filesystem::copy_options::overwrite_existing | std::filesystem::copy_options::recursive, stageEc);
					std::filesystem::copy(sharedVersion, exportsDir / "shared" / "Microsoft.NETCore.App" / sharedVersion.filename(),
						std::filesystem::copy_options::overwrite_existing | std::filesystem::copy_options::recursive, stageEc);
				}
				else
				{
					WF_CORE_WARN("ProjectExporter: .NET runtime not found under '{0}' - the export will only run on machines with .NET installed!",
						dotnetRoot.string());
				}
			}

			// DotnetRuntime contract folder: next to the editor exe (packaged), then the repo bin/DotnetRuntime.
			std::filesystem::path editorExe = std::filesystem::path(GetModuleFileNameA_()).parent_path();
			std::vector<std::filesystem::path> candidates = {
				editorExe / "DotnetRuntime",
				editorExe / ".." / ".." / "DotnetRuntime",
			};
			for (const auto& candidate : candidates)
			{
				std::error_code candEc;
				std::filesystem::path dir = std::filesystem::weakly_canonical(candidate, candEc);
				if (std::filesystem::exists(dir / "Waffle.Scripting.dll", candEc))
				{
					std::filesystem::copy(dir, exportsDir / "DotnetRuntime",
						std::filesystem::copy_options::overwrite_existing | std::filesystem::copy_options::recursive, stageEc);
					break;
				}
			}

			// Stage GameScripts.dll where the engine's packed-export fallback looks: packed exports have no Assets/Scripts to compile.
			std::filesystem::path scriptDll = activeProjectPath / "Assets" / "cache" / "Scripting" / "GameScripts.dll";
			if (std::filesystem::exists(scriptDll, stageEc))
			{
				std::filesystem::path gameScriptsDir = exportsDir / "DotnetRuntime" / "GameScripts";
				std::filesystem::create_directories(gameScriptsDir, stageEc);
				std::filesystem::copy_file(scriptDll, gameScriptsDir / "GameScripts.dll",
					std::filesystem::copy_options::overwrite_existing, stageEc);
				std::filesystem::path scriptPdb = scriptDll;
				scriptPdb.replace_extension(".pdb");
				if (std::filesystem::exists(scriptPdb, stageEc))
					std::filesystem::copy_file(scriptPdb, gameScriptsDir / "GameScripts.pdb",
						std::filesystem::copy_options::overwrite_existing, stageEc);
			}
			else
			{
				WF_CORE_WARN("ProjectExporter: no compiled GameScripts.dll in Assets/cache/Scripting - the exported game will have NO scripts! Open the project in the editor and press Play once to compile them.");
			}
		}

		// 8. Resolve icon: custom > project logo > editor logo; the exe ships with the Waffle logo compiled in, only re-embed when one is found.
		std::string           relativeIconPath;
		std::filesystem::path chosenIconPath;

		if (!options.CustomIconPath.empty() && std::filesystem::exists(options.CustomIconPath, ec))
		{
			chosenIconPath = options.CustomIconPath;
		}
		else if (std::filesystem::exists(activeProjectPath / "Assets/icon.ico", ec))
		{
			chosenIconPath = activeProjectPath / "Assets/icon.ico";
		}
		else if (std::filesystem::exists(activeProjectPath / "Assets/Icon.ico", ec))
		{
			chosenIconPath = activeProjectPath / "Assets/Icon.ico";
		}
		else if (std::filesystem::exists(activeProjectPath / "Assets/icon.png", ec))
		{
			chosenIconPath = activeProjectPath / "Assets/icon.png";
		}
		else if (std::filesystem::exists(activeProjectPath / "Assets/Icon.png", ec))
		{
			chosenIconPath = activeProjectPath / "Assets/Icon.png";
		}
		else if (std::filesystem::exists(activeProjectPath / "Assets/images/logo.png", ec))
		{
			chosenIconPath = activeProjectPath / "Assets/images/logo.png";
		}
		else if (std::filesystem::exists(activeProjectPath / "Assets/Images/logo.png", ec))
		{
			chosenIconPath = activeProjectPath / "Assets/Images/logo.png";
		}
		// No match: chosenIconPath stays empty and the compiled-in Waffle logo is kept as-is.

		if (!chosenIconPath.empty())
		{
			// Copy icon into the export bundle so the runtime can use it for the window
			std::filesystem::path iconDest = targetAssets /
				("app_icon" + chosenIconPath.extension().string());
			std::filesystem::copy_file(chosenIconPath, iconDest,
				std::filesystem::copy_options::overwrite_existing, ec);
			if (!ec)
				relativeIconPath = "Assets/app_icon" + chosenIconPath.extension().string();

#if defined(WF_PLATFORM_WINDOWS)
			// Replace the PE icon resource so Explorer/taskbar show the right icon; purge + write in one BeginUpdateResource session.
			if (!EmbedIconInExecutable(targetExePath, chosenIconPath))
			{
				WF_CORE_WARN("Icon embedding failed for '{0}' - exported exe will keep the Waffle logo.",
					targetExePath.string());
			}
#endif
		}

		// 9. Determine relative StartScene path for runtime
		std::string relativeStartScene;
		if (!options.SelectedScenePath.empty())
		{
			std::filesystem::path scenePath(options.SelectedScenePath);
			std::string scenePathStr = scenePath.string();
			std::replace(scenePathStr.begin(), scenePathStr.end(), '\\', '/');
			auto assetsPos = FindLastAssetsSegment(scenePathStr);
			if (assetsPos != std::string::npos)
				relativeStartScene = scenePathStr.substr(assetsPos);
			else
				relativeStartScene = (std::filesystem::path("Assets/Scenes") / scenePath.filename()).string();
		}

		// Normalize all scene paths to be relative to Assets/
		auto MakeRelativeScene = [](const std::string& rawPath) -> std::string
			{
				std::string s = rawPath;
				// Normalize slashes
				std::replace(s.begin(), s.end(), '\\', '/');
				auto pos = FindLastAssetsSegment(s);
				if (pos != std::string::npos)
					return s.substr(pos);
				// Fallback: just use the filename under Assets/Scenes
				return "Assets/Scenes/" + std::filesystem::path(s).filename().string();
			};

		// 10. Write Assets/project.wproj runtime config
		RuntimeProjectConfig config;
		config.Name = appName;
		config.StartScene = relativeStartScene;
		config.IconPath = relativeIconPath;
		config.GravityY = options.Gravity;
		// Exports run on whatever backend the editor was launched with.
		config.Backend = Renderer::GetAPI() == RendererAPI::API::OpenGL ? "OpenGL" : "Vulkan";

		// Write the full ordered scene list so ChangeScene(n) works at runtime
		if (!options.SceneList.empty())
		{
			for (const auto& scenePath : options.SceneList)
				config.Scenes.push_back(MakeRelativeScene(scenePath));
		}
		else if (!relativeStartScene.empty())
		{
			// Fallback: at minimum write the start scene so index 0 works
			config.Scenes.push_back(relativeStartScene);
		}

		// Post-processing settings live on the scene's cameras (CameraComponent::PostProcessing).
		WriteRuntimeProjectConfig(targetAssets, config);

		// 11. Pack all exported assets into game.wpack; compile scripts first (a failed compile is non-fatal - staging below warns if nothing to ship).
		CSharpScriptEngine::CompileProjectScripts();

		AssetPackerOptions packOptions;
		packOptions.SourceDirectory = targetAssets;
		packOptions.OutputWpackPath = exportsDir / "game.wpack";

		std::string packError;
		if (AssetPacker::CreateArchive(packOptions, packError))
		{
			WF_CORE_INFO("ProjectExporter: Assets successfully packed into game.wpack.");
			// Clean up raw loose Assets directory so release export contains only game.wpack
			std::filesystem::remove_all(targetAssets, ec);
		}
		else
		{
			WF_CORE_WARN("ProjectExporter: Asset packing failed: {0}", packError);
		}

		WF_CORE_INFO("Successfully exported project '{0}' to {1}", appName, targetExePath.string());
		return true;
	}

}