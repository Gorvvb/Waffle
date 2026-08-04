#include <Waffle.h>
#include <Waffle/Core/EntryPoint.h>
#include <Waffle/Core/VFS.h>
#include "RuntimeLayer.h"

#include <yaml-cpp/yaml.h>
#include <filesystem>

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

	Application* CreateApplication(ApplicationCommandLineArgs args)
	{
		ApplicationSpecification spec;
		spec.Name = "Waffle Game";
		spec.CommandLineArgs = args;

		// 1. Check for and mount VFS archive (.wpack)
		if (std::filesystem::exists("game.wpack"))
		{
			VFS::MountArchive("game.wpack");
		}
		else if (std::filesystem::exists("Assets/game.wpack"))
		{
			VFS::MountArchive("Assets/game.wpack");
		}
		else
		{
			// Check for any .wpack file in executable root directory
			std::error_code ec;
			for (const auto& entry : std::filesystem::directory_iterator(".", ec))
			{
				if (entry.is_regular_file(ec) && entry.path().extension() == ".wpack")
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
