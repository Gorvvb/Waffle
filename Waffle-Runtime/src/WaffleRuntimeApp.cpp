#include <Waffle.h>
#include <Waffle/Core/EntryPoint.h>
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

		if (std::filesystem::exists("Assets/project.wfp"))
		{
			try
			{
				YAML::Node data = YAML::LoadFile("Assets/project.wfp");
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
