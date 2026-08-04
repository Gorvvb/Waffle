#include "RuntimeLayer.h"
#include "Waffle/Scene/SceneSerializer.h"

#include <yaml-cpp/yaml.h>

namespace Waffle {

	RuntimeLayer::RuntimeLayer()
		: Layer("RuntimeLayer")
	{}

	void RuntimeLayer::OnAttach()
	{
		WF_PROFILE_FUNCTION();

		if (std::filesystem::exists("Assets/project.wfp"))
		{
			try
			{
				YAML::Node data = YAML::LoadFile("Assets/project.wfp");
				auto project = data["Project"];
				if (project)
				{
					if (project["Gravity"])
						m_Gravity = project["Gravity"].as<float>();

					// Read ordered scene list - index 0 is always the start scene
					if (project["Scenes"])
					{
						for (auto node : project["Scenes"])
						{
							std::string path = node.as<std::string>();
							// Normalize backslashes
							std::replace(path.begin(), path.end(), '\\', '/');
							if (std::filesystem::exists(path))
								m_SceneList.push_back(path);
							else
								WF_WARN("RuntimeLayer: Scene not found: {0}", path);
						}
					}

					// Fallback to StartScene if Scenes list is empty
					if (m_SceneList.empty() && project["StartScene"])
					{
						std::string startScene = project["StartScene"].as<std::string>();
						std::replace(startScene.begin(), startScene.end(), '\\', '/');
						if (std::filesystem::exists(startScene))
							m_SceneList.push_back(startScene);
					}
				}
			}
			catch (const std::exception& e)
			{
				WF_ERROR("RuntimeLayer: Failed to parse project.wfp: {0}", e.what());
			}
		}

		// Last resort: scan disk (alphabetical, no guaranteed order)
		if (m_SceneList.empty())
		{
			if (std::filesystem::exists("Assets/Scenes"))
			{
				for (auto& entry : std::filesystem::directory_iterator("Assets/Scenes"))
					if (entry.is_regular_file() && entry.path().extension() == ".waffle")
						m_SceneList.push_back(entry.path().string());
			}

			if (m_SceneList.empty() && std::filesystem::exists("Assets"))
			{
				for (auto& entry : std::filesystem::recursive_directory_iterator("Assets"))
					if (entry.is_regular_file() && entry.path().extension() == ".waffle")
						m_SceneList.push_back(entry.path().string());
			}
		}

		LoadScene(0);
	}

	void RuntimeLayer::LoadScene(int index)
	{
		if (m_Scene)
			m_Scene->OnRuntimeStop();

		if (m_SceneList.empty() || index < 0 || index >= (int)m_SceneList.size())
		{
			WF_WARN("RuntimeLayer: No scene at index {0}", index);
			m_Scene = CreateRef<Scene>();
			m_CurrentSceneIndex = index;
			return;
		}

		const std::string& scenePath = m_SceneList[index];
		WF_INFO("RuntimeLayer: Loading scene [{0}]: {1}", index, scenePath);

		m_Scene = CreateRef<Scene>();
		m_Scene->SetGravity(m_Gravity);

		SceneSerializer serializer(m_Scene);
		if (!serializer.Deserialize(scenePath))
			WF_ERROR("RuntimeLayer: Failed to deserialize scene: {0}", scenePath);

		uint32_t w = Application::Get().GetWindow().GetWidth();
		uint32_t h = Application::Get().GetWindow().GetHeight();
		m_Scene->OnViewportResize(w, h);
		m_Scene->OnRuntimeStart();

		m_CurrentSceneIndex = index;
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

		if (!m_Scene)
			return;

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

		int pendingScene = m_Scene->OnUpdateRuntime(ts);
		if (pendingScene != -1)
			LoadScene(pendingScene);
	}

	void RuntimeLayer::OnImGuiRender()
	{}

	void RuntimeLayer::OnEvent(Event& e)
	{
		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<WindowResizeEvent>(WF_BIND_EVENT_FN(RuntimeLayer::OnWindowResize));
	}

	bool RuntimeLayer::OnWindowResize(WindowResizeEvent& e)
	{
		if (m_Scene && e.GetWidth() > 0 && e.GetHeight() > 0)
			m_Scene->OnViewportResize(e.GetWidth(), e.GetHeight());
		return false;
	}

}