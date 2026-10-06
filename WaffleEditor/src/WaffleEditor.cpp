#include <Waffle.h>
#include <Waffle/Core/EntryPoint.h>
#include <Waffle/Renderer/RendererAPI.h>

#include <yaml-cpp/yaml.h>

#include <filesystem>
#ifdef WF_PLATFORM_WINDOWS
#include <windows.h>
#endif

#include "imgui/imgui.h"

#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"

#include "EditorLayer.h"

namespace Waffle {

	class WaffleEditor : public Application
	{
	public:
		WaffleEditor(const ApplicationSpecification& specification)
			: Application(specification)
		{
			PushLayer(new EditorLayer());
		}

		~WaffleEditor()
		{
		}
	};

	Application* CreateApplication(ApplicationCommandLineArgs args)
	{
		ApplicationSpecification spec;
		spec.Name = "Waffle Editor";
		spec.IconPath = "Resources/Icons/logo.png";
		spec.CommandLineArgs = args;

		// Render backend must be known BEFORE Renderer::Init. Priority: -backend argument
		// (used by the switch-and-relaunch flow) > editor_config.yaml.
		for (int i = 1; i < args.Count; i++)
		{
			if (std::string(args[i]) == "-backend" && i + 1 < args.Count)
			{
				std::string backend = args[i + 1];
				if (backend == "OpenGL")
					RendererAPI::SetAPI(RendererAPI::API::OpenGL);
				else if (backend == "Vulkan")
					RendererAPI::SetAPI(RendererAPI::API::Vulkan);
				break;
			}
		}

		std::filesystem::path configPath = std::filesystem::path(args[0]).parent_path() / "editor_config.yaml";
		std::error_code ec;
		if (std::filesystem::exists(configPath, ec))
		{
			try
			{
				YAML::Node config = YAML::LoadFile(configPath.string());
				if (config["Application"] && config["Application"]["Backend"])
				{
					std::string backend = config["Application"]["Backend"].as<std::string>();
					if (backend == "OpenGL")
						RendererAPI::SetAPI(RendererAPI::API::OpenGL);
					else if (backend == "Vulkan")
						RendererAPI::SetAPI(RendererAPI::API::Vulkan);
				}
			}
			catch (...)
			{
			}
		}

		return new WaffleEditor(spec);
	}
}
