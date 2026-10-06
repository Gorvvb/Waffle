#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace Waffle {

	// The runtime project config ("Assets/project.wproj"): the single file describing a shipped
	// game. Written by the editor's project settings, the exporter and the Hub's template
	// creator; read by the player. Scene paths are project-root-relative and slash-normalized.
	struct RuntimeProjectConfig
	{
		std::string Name;                             // display / app name
		std::string StartScene;                       // optional - empty means Scenes[0] is the start
		std::string IconPath;                         // optional, project-root-relative
		std::string Backend;                          // render backend: "Vulkan"/"OpenGL"; empty = engine default
		float GravityY = -9.8f;
		std::vector<std::string> Scenes;              // ordered; index 0 starts when StartScene is empty
	};

	// Writes the config as YAML into assetsDir/project.wproj. Returns false if the file could not be written.
	bool WriteRuntimeProjectConfig(const std::filesystem::path& assetsDir, const RuntimeProjectConfig& config);

	// Reads Assets/project.wproj through the VFS (mount archives first) and parses it. The result is
	// cached per process, so the player app and RuntimeLayer share one parse. Missing file -> defaulted struct.
	const RuntimeProjectConfig& GetRuntimeProjectConfig();

}
