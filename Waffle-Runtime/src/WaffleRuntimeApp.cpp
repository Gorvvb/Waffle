#include <Waffle.h>
#include <Waffle/Core/EntryPoint.h>
#include <Waffle/Core/VFS.h>
#include "RuntimeLayer.h"

#include <yaml-cpp/yaml.h>
#include <filesystem>
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

	// Game data lives next to the EXECUTABLE, not the caller's working
	// directory - launching via a shortcut or the Hub with a different
	// "Start in" silently found no game otherwise.
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

		// 2. Read project config via VFS / file
		std::string wfpContent = VFS::ReadFileAsString("Assets/project.wfp");
		if (!wfpContent.empty())
		{
			try
			{
				YAML::Node data = YAML::Load(wfpContent);
				auto project = data["Project"];
				if (project)
				{
					if (project["Name"])
						spec.Name = project["Name"].as<std::string>();
					if (project["IconPath"])
						spec.IconPath = project["IconPath"].as<std::string>();
				}
			}
			catch (...)
			{
			}
		}

		return new WaffleRuntimeApp(spec);
	}
}
