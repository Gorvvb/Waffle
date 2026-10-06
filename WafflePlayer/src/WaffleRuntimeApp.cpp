#include <Waffle.h>
#include <Waffle/Core/EntryPoint.h>
#include <Waffle/Core/VFS.h>
#include <Waffle/Project/RuntimeProjectConfig.h>
#include <Waffle/Renderer/RendererAPI.h>
#include "RuntimeLayer.h"

#include <filesystem>
#include <fstream>
#include <windows.h>

namespace Waffle {

	class WaffleRuntimeApp : public Application
	{
	public:
		WaffleRuntimeApp(const ApplicationSpecification& specification)
			: Application(specification)
		{
			PushLayer(new RuntimeLayer());
		}

		~WaffleRuntimeApp()
		{
		}
	};

	// Game data lives next to the EXECUTABLE, not the cwd - shortcut/Hub launches with a different "Start in" found no game otherwise.
	static std::filesystem::path GetExecutableDir()
	{
		char buffer[MAX_PATH];
		DWORD len = GetModuleFileNameA(NULL, buffer, MAX_PATH);
		if (len == 0 || len >= MAX_PATH)
			return std::filesystem::current_path();
		return std::filesystem::path(buffer).parent_path();
	}

	Application* CreateApplication(ApplicationCommandLineArgs args)
	{
		ApplicationSpecification spec;
		spec.Name = "Waffle Game";
		spec.CommandLineArgs = args;

		std::filesystem::path exeDir = GetExecutableDir();

		// 1. Check for and mount VFS archive (.wpack)
		std::filesystem::path packPath = exeDir / "game.wpack";
		if (!std::filesystem::exists(packPath))
			packPath = exeDir / "Assets" / "game.wpack";
		if (std::filesystem::exists(packPath))
		{
			VFS::MountArchive(packPath);
		}
		else
		{
			// Check for any .wpack file in the executable's directory
			std::error_code ec;
			for (const auto& entry : std::filesystem::directory_iterator(exeDir, ec))
			{
				if (ec) break;
				std::string ext = entry.path().extension().string();
				for (auto& c : ext) c = (char)tolower((unsigned char)c);
				if (entry.is_regular_file(ec) && ext == ".wpack")
				{
					VFS::MountArchive(entry.path());
					break;
				}
			}
		}

		// 2. Project config (display name / icon) - parsed once per process and shared with RuntimeLayer.
		const RuntimeProjectConfig& config = GetRuntimeProjectConfig();
		if (!config.Name.empty())
			spec.Name = config.Name;
		if (!config.IconPath.empty())
			spec.IconPath = config.IconPath;

		// 3. Render backend: env override (tests) > sidecar file (in-game graphics setting)
		//    > project config (what the editor/export chose).
		std::string backend = config.Backend;
		std::ifstream sidecar(exeDir / "waffle_backend.txt");
		if (sidecar)
		{
			std::string requested;
			std::getline(sidecar, requested);
			const char* whitespace = " \t\r\n";
			requested.erase(requested.find_last_not_of(whitespace) + 1);
			if (!requested.empty())
				backend = requested;
		}

		// An unspecified backend keeps whatever the engine started with (incl. env override).
		if (backend == "OpenGL")
			RendererAPI::SetAPI(RendererAPI::API::OpenGL);
		else if (backend == "Vulkan")
			RendererAPI::SetAPI(RendererAPI::API::Vulkan);

		return new WaffleRuntimeApp(spec);
	}
}
