#include "RuntimeLayer.h"
#include "Waffle/Scene/SceneSerializer.h"
#include "Waffle/Core/VFS.h"

#include <yaml-cpp/yaml.h>

namespace Waffle {

	RuntimeLayer::RuntimeLayer()
		: Layer("RuntimeLayer")
	{}

	void RuntimeLayer::OnAttach()
	{
		WF_PROFILE_FUNCTION();

		std::string wfpContent = VFS::ReadFileAsString("Assets/project.wfp");
		if (!wfpContent.empty())
		{
			try
			{
				YAML::Node data = YAML::Load(wfpContent);
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
							if (VFS::Exists(path))
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
						if (VFS::Exists(startScene))
							m_SceneList.push_back(startScene);
					}
				}
			}
			catch (const std::exception& e)
			{
				WF_ERROR("RuntimeLayer: Failed to parse project.wfp: {0}", e.what());
			}
		}

		// Last resort: scan VFS & physical disk for .waffle files
		if (m_SceneList.empty())
		{
			if (VFS::IsMounted())
			{
				for (const auto& path : VFS::GetMountedFilePaths())
				{
					if (path.length() >= 7 && path.substr(path.length() - 7) == ".waffle")
					{
						m_SceneList.push_back(path);
					}
				}
			}

			if (m_SceneList.empty() && std::filesystem::exists("Assets"))
			{
				std::error_code ec;
				for (auto& entry : std::filesystem::recursive_directory_iterator("Assets", ec))
					if (entry.is_regular_file(ec) && entry.path().extension() == ".waffle")
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