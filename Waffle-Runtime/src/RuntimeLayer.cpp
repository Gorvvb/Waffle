#include "RuntimeLayer.h"
#include "Waffle/Scene/SceneSerializer.h"

#include <yaml-cpp/yaml.h>

namespace Waffle {

	RuntimeLayer::RuntimeLayer()
		: Layer("RuntimeLayer")
	{
	}

	void RuntimeLayer::OnAttach()
	{
		WF_PROFILE_FUNCTION();

		m_Scene = CreateRef<Scene>();

		std::filesystem::path scenePath;

		// 1. Check Assets/project.wfp for configured StartScene
		if (std::filesystem::exists("Assets/project.wfp"))
		{
			try
			{
				YAML::Node data = YAML::LoadFile("Assets/project.wfp");
				auto project = data["Project"];
				if (project && project["StartScene"])
				{
					std::string specifiedScene = project["StartScene"].as<std::string>();
					if (!specifiedScene.empty() && std::filesystem::exists(specifiedScene))
						scenePath = specifiedScene;
				}
			}
			catch (...)
			{
			}
		}

		// 2. Fallback search for scene file in Assets/Scenes or Assets/
		if (scenePath.empty() && std::filesystem::exists("Assets/Scenes"))
		{
			for (auto& entry : std::filesystem::directory_iterator("Assets/Scenes"))
			{
				if (entry.is_regular_file() && entry.path().extension() == ".waffle")
				{
					scenePath = entry.path();
					break;
				}
			}
		}

		if (scenePath.empty() && std::filesystem::exists("Assets"))
		{
			for (auto& entry : std::filesystem::recursive_directory_iterator("Assets"))
			{
				if (entry.is_regular_file() && entry.path().extension() == ".waffle")
				{
					scenePath = entry.path();
					break;
				}
			}
		}

		if (!scenePath.empty())
		{
			WF_INFO("Loading runtime scene: {0}", scenePath.string());
			SceneSerializer serializer(m_Scene);
			if (serializer.Deserialize(scenePath.string()))
			{
				m_ScenePath = scenePath;
			}
			else
			{
				WF_ERROR("Failed to deserialize scene: {0}", scenePath.string());
			}
		}
		else
		{
			WF_WARN("No .waffle scene file found in Assets folder!");
		}

		uint32_t windowWidth = Application::Get().GetWindow().GetWidth();
		uint32_t windowHeight = Application::Get().GetWindow().GetHeight();
		m_Scene->OnViewportResize(windowWidth, windowHeight);
		m_Scene->OnRuntimeStart();
	}

	void RuntimeLayer::OnDetach()
	{
		WF_PROFILE_FUNCTION();
		if (m_Scene)
			m_Scene->OnRuntimeStop();
	}

	void RuntimeLayer::OnUpdate(Timestep ts)
	{
		WF_PROFILE_FUNCTION();

		if (m_Scene)
		{
			Entity primaryCam = m_Scene->GetPrimaryCameraEntity();
			if (primaryCam)
			{
				glm::vec4 clearColor = primaryCam.GetComponent<CameraComponent>().BackgroundColor;
				RenderCommand::SetClearColor(clearColor);
				RenderCommand::Clear();
			}
			else
			{
				RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1.0f });
				RenderCommand::Clear();
			}

			m_Scene->OnUpdateRuntime(ts);
		}
	}

	void RuntimeLayer::OnImGuiRender()
	{
	}

	void RuntimeLayer::OnEvent(Event& e)
	{
		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<WindowResizeEvent>(WF_BIND_EVENT_FN(RuntimeLayer::OnWindowResize));
	}

	bool RuntimeLayer::OnWindowResize(WindowResizeEvent& e)
	{
		if (m_Scene && e.GetWidth() > 0 && e.GetHeight() > 0)
		{
			m_Scene->OnViewportResize(e.GetWidth(), e.GetHeight());
		}
		return false;
	}

}
