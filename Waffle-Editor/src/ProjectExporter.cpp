#include "ProjectExporter.h"
#include "Waffle/Core/Log.h"

#if defined(_WIN32)
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

#if defined(_WIN32)

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
	LPWSTR type;
	LPWSTR name;
	WORD   lang;
};

static BOOL CALLBACK EnumLangsCB(HMODULE hModule, LPCWSTR lpType, LPCWSTR lpName, WORD wLanguage, LONG_PTR lParam)
{
	auto* list = reinterpret_cast<std::vector<ResItem>*>(lParam);
	list->push_back({ (LPWSTR)lpType, (LPWSTR)lpName, wLanguage });
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

// Collects all existing RT_ICON and RT_GROUP_ICON resources from the exe
// so we can purge them inside the same update session that writes the new icon.
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

	// Collect existing icon resources BEFORE opening the update handle,
	// then purge + write in ONE session so nothing gets re-applied between sessions.
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

	// --- Build the icon payload (grpBuffer + per-image data) ---
	// We need this ready before opening the update handle so we can
	// do purge + write atomically in one session.

	struct IconResData {
		int width, height;
		std::vector<uint8_t> bytes;
	};
	std::vector<IconResData> iconResList;
	std::vector<uint8_t>     grpBuffer;
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

				size_t gs = sizeof(GRPICONDIR) + count * sizeof(GRPICONDIRENTRY);
				grpBuffer.assign(gs, 0);

				GRPICONDIR* grpHdr = reinterpret_cast<GRPICONDIR*>(grpBuffer.data());
				grpHdr->idReserved = 0;
				grpHdr->idType     = 1;
				grpHdr->idCount    = count;

				GRPICONDIRENTRY* grpEntries = reinterpret_cast<GRPICONDIRENTRY*>(
					grpBuffer.data() + sizeof(GRPICONDIR));

				for (uint16_t i = 0; i < count; i++)
				{
					grpEntries[i].bWidth       = entries[i].bWidth;
					grpEntries[i].bHeight      = entries[i].bHeight;
					grpEntries[i].bColorCount  = entries[i].bColorCount;
					grpEntries[i].bReserved    = 0;
					grpEntries[i].wPlanes      = entries[i].wPlanes;
					grpEntries[i].wBitsPerPixel= entries[i].wBitsPerPixel;
					grpEntries[i].dwBytesInRes = entries[i].dwBytesInRes;
					grpEntries[i].nID          = i + 1;

					if (entries[i].dwImageOffset + entries[i].dwBytesInRes <= fileData.size())
					{
						iconResList.push_back({
							entries[i].bWidth  ? entries[i].bWidth  : 256,
							entries[i].bHeight ? entries[i].bHeight : 256,
							std::vector<uint8_t>(
								fileData.data() + entries[i].dwImageOffset,
								fileData.data() + entries[i].dwImageOffset + entries[i].dwBytesInRes)
						});
					}
				}

				isIco = !iconResList.empty();
			}
		}
	}

	if (!isIco)
	{
		// Raster image path: resize to standard sizes and encode as PNG
		int width = 0, height = 0, channels = 0;
		stbi_uc* srcPixels = stbi_load(iconPath.string().c_str(), &width, &height, &channels, 4);
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

	// --- Single update session: purge old icons then write new ones ---
	HANDLE hUpdate = BeginUpdateResourceW(exePath.wstring().c_str(), FALSE);
	if (!hUpdate)
	{
		WF_CORE_WARN("BeginUpdateResourceW failed for {0}", exePath.string());
		return false;
	}

	// Step A: purge every existing RT_ICON and RT_GROUP_ICON in this same session
	for (const auto& item : existingItems)
	{
		UpdateResourceW(hUpdate, item.type, item.name, item.lang, NULL, 0);
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

#endif // _WIN32

	static std::filesystem::path FindRuntimeExecutable()
	{
		std::vector<std::filesystem::path> candidates;

#if defined(WF_DIST)
		candidates = {
			"../bin/Dist-windows-x86_64/Waffle-Runtime/Waffle-Runtime.exe",
			"bin/Dist-windows-x86_64/Waffle-Runtime/Waffle-Runtime.exe",
			"../bin/Release-windows-x86_64/Waffle-Runtime/Waffle-Runtime.exe",
			"bin/Release-windows-x86_64/Waffle-Runtime/Waffle-Runtime.exe",
			"../bin/Debug-windows-x86_64/Waffle-Runtime/Waffle-Runtime.exe",
			"bin/Debug-windows-x86_64/Waffle-Runtime/Waffle-Runtime.exe",
			"Waffle-Runtime.exe"
		};
#elif defined(WF_RELEASE)
		candidates = {
			"../bin/Release-windows-x86_64/Waffle-Runtime/Waffle-Runtime.exe",
			"bin/Release-windows-x86_64/Waffle-Runtime/Waffle-Runtime.exe",
			"../bin/Dist-windows-x86_64/Waffle-Runtime/Waffle-Runtime.exe",
			"bin/Dist-windows-x86_64/Waffle-Runtime/Waffle-Runtime.exe",
			"../bin/Debug-windows-x86_64/Waffle-Runtime/Waffle-Runtime.exe",
			"bin/Debug-windows-x86_64/Waffle-Runtime/Waffle-Runtime.exe",
			"Waffle-Runtime.exe"
		};
#else // Debug
		candidates = {
			"../bin/Debug-windows-x86_64/Waffle-Runtime/Waffle-Runtime.exe",
			"bin/Debug-windows-x86_64/Waffle-Runtime/Waffle-Runtime.exe",
			"../bin/Release-windows-x86_64/Waffle-Runtime/Waffle-Runtime.exe",
			"bin/Release-windows-x86_64/Waffle-Runtime/Waffle-Runtime.exe",
			"../bin/Dist-windows-x86_64/Waffle-Runtime/Waffle-Runtime.exe",
			"bin/Dist-windows-x86_64/Waffle-Runtime/Waffle-Runtime.exe",
			"Waffle-Runtime.exe"
		};
#endif

		for (const auto& path : candidates)
		{
			if (std::filesystem::exists(path))
				return std::filesystem::canonical(path);
		}

		try
		{
			std::filesystem::path current = std::filesystem::current_path();
			for (int depth = 0; depth < 3 && current.has_parent_path(); ++depth)
			{
				for (const auto& entry : std::filesystem::recursive_directory_iterator(current))
				{
					if (entry.is_regular_file() && entry.path().filename() == "Waffle-Runtime.exe")
						return entry.path();
				}
				current = current.parent_path();
			}
		}
		catch (...)
		{
		}

		return "";
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
			activeProjectPath = std::filesystem::path("Projects") / options.ProjectName;
		}

		std::filesystem::path exportsDir = activeProjectPath / "Exports";
		std::error_code ec;

		std::filesystem::create_directories(exportsDir, ec);
		if (ec)
		{
			outErrorMessage = "Failed to create export directory: " + exportsDir.string() + " (" + ec.message() + ")";
			return false;
		}

		// 1. Locate Waffle-Runtime.exe
		std::filesystem::path runtimeExe = FindRuntimeExecutable();
		if (runtimeExe.empty() || !std::filesystem::exists(runtimeExe))
		{
			outErrorMessage = "Could not find Waffle-Runtime.exe template. Please build the Waffle-Runtime project first.";
			return false;
		}

		// 2. Copy Waffle-Runtime.exe -> Exports/[AppName].exe
		std::filesystem::path targetExePath = exportsDir / (appName + ".exe");
		std::filesystem::copy_file(runtimeExe, targetExePath,
			std::filesystem::copy_options::overwrite_existing, ec);
		if (ec)
		{
			outErrorMessage = "Failed to copy game executable: " + ec.message();
			return false;
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

		// 4. Ensure engine base shaders exist in export Assets/shaders
		std::filesystem::path targetShaders = targetAssets / "shaders";
		std::filesystem::create_directories(targetShaders, ec);

		for (const auto& shaderDir : std::vector<std::filesystem::path>{
			"Assets/shaders",
			"Waffle-Editor/Assets/shaders",
			"../Waffle-Editor/Assets/shaders" })
		{
			if (std::filesystem::exists(shaderDir))
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

		for (const auto& fontDir : std::vector<std::filesystem::path>{
			"Assets/fonts",
			"Waffle-Editor/Assets/fonts",
			"../Waffle-Editor/Assets/fonts" })
		{
			if (std::filesystem::exists(fontDir))
			{
				std::filesystem::copy(fontDir, targetFonts,
					std::filesystem::copy_options::overwrite_existing |
					std::filesystem::copy_options::recursive, ec);
				break;
			}
		}

		// 6. Ensure shader cache exists in export Assets/cache
		std::filesystem::path targetCache = targetAssets / "cache";
		std::filesystem::create_directories(targetCache, ec);

		for (const auto& cacheDir : std::vector<std::filesystem::path>{
			"Assets/cache",
			"Waffle-Editor/Assets/cache",
			"../Waffle-Editor/Assets/cache" })
		{
			if (std::filesystem::exists(cacheDir))
			{
				std::filesystem::copy(cacheDir, targetCache,
					std::filesystem::copy_options::overwrite_existing |
					std::filesystem::copy_options::recursive, ec);
				break;
			}
		}

		// 7. Copy extra .lua scripts outside Assets/ into targetAssets/Scripts
		if (std::filesystem::exists(activeProjectPath))
		{
			std::filesystem::path targetScripts = targetAssets / "Scripts";
			std::filesystem::create_directories(targetScripts, ec);

			for (const auto& entry : std::filesystem::recursive_directory_iterator(activeProjectPath))
			{
				if (entry.is_regular_file() && entry.path().extension() == ".lua")
				{
					std::string pathStr = entry.path().string();
					if (pathStr.find("Assets")  == std::string::npos &&
						pathStr.find("Exports") == std::string::npos)
					{
						std::filesystem::copy_file(entry.path(),
							targetScripts / entry.path().filename(),
							std::filesystem::copy_options::overwrite_existing, ec);
					}
				}
			}
		}

		// 8. Resolve icon: custom > project logo > editor logo > nothing
		//    The Waffle-Runtime.exe already has the Waffle logo compiled in as a
		//    PE resource.  We only call EmbedIconInExecutable when there is
		//    actually a custom icon to use - otherwise the compiled-in logo stays.
		std::string           relativeIconPath;
		std::filesystem::path chosenIconPath;

		if (!options.CustomIconPath.empty() && std::filesystem::exists(options.CustomIconPath, ec))
		{
			// User explicitly picked an icon - use it
			chosenIconPath = options.CustomIconPath;
		}
		else if (std::filesystem::exists(activeProjectPath / "Assets/images/logo.png", ec))
		{
			chosenIconPath = activeProjectPath / "Assets/images/logo.png";
		}
		// If neither condition matched, chosenIconPath stays empty and the
		// Waffle logo that was compiled into Waffle-Runtime.exe is kept as-is.

		if (!chosenIconPath.empty())
		{
			// Copy icon into the export bundle so the runtime can use it for the window
			std::filesystem::path iconDest = targetAssets /
				("app_icon" + chosenIconPath.extension().string());
			std::filesystem::copy_file(chosenIconPath, iconDest,
				std::filesystem::copy_options::overwrite_existing, ec);
			if (!ec)
				relativeIconPath = "Assets/app_icon" + chosenIconPath.extension().string();

#if defined(_WIN32)
			// Replace the PE icon resource so Explorer / taskbar show the right icon.
			// Purge and write happen inside one BeginUpdateResource session.
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
			auto assetsPos = scenePathStr.find("Assets");
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
				auto pos = s.find("Assets");
				if (pos != std::string::npos)
					return s.substr(pos);
				// Fallback: just use the filename under Assets/Scenes
				return "Assets/Scenes/" + std::filesystem::path(s).filename().string();
			};

		// 10. Write Assets/project.wfp runtime config
		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Project" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "Name" << YAML::Value << appName;
		out << YAML::Key << "StartScene" << YAML::Value << relativeStartScene;
		out << YAML::Key << "IconPath" << YAML::Value << relativeIconPath;
		out << YAML::Key << "Gravity" << YAML::Value << options.Gravity;

		// Write the full ordered scene list so ChangeScene(n) works at runtime
		out << YAML::Key << "Scenes" << YAML::Value << YAML::BeginSeq;
		if (!options.SceneList.empty())
		{
			for (const auto& scenePath : options.SceneList)
				out << MakeRelativeScene(scenePath);
		}
		else if (!relativeStartScene.empty())
		{
			// Fallback: at minimum write the start scene so index 0 works
			out << relativeStartScene;
		}
		out << YAML::EndSeq;

		out << YAML::EndMap;
		out << YAML::EndMap;

		std::ofstream fout(targetAssets / "project.wfp");
		fout << out.c_str();

		WF_CORE_INFO("Successfully exported project '{0}' to {1}", appName, targetExePath.string());
		return true;
	}

}