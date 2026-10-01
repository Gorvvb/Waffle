#include "RuntimeLayer.h"
#include "Waffle/Scene/SceneSerializer.h"
#include "Waffle/Core/VFS.h"
#include "Waffle/Renderer/PostProcessing.h"
#include "Waffle/Renderer/Renderer.h"
#include "Waffle/Scripting/LuaScriptEngine.h"

#include <yaml-cpp/yaml.h>
#include <glm/gtc/matrix_transform.hpp>

namespace Waffle {

	RuntimeLayer::RuntimeLayer()
		: Layer("RuntimeLayer")
	{}

	void RuntimeLayer::OnAttach()
	{
		WF_PROFILE_FUNCTION();

		FramebufferSpecification spec;
		spec.Width = Application::Get().GetWindow().GetWidth();
		spec.Height = Application::Get().GetWindow().GetHeight();
		spec.Attachments = { FramebufferTextureFormat::RGBA8, FramebufferTextureFormat::RED_INTEGER, FramebufferTextureFormat::Depth };
		m_Framebuffer = Framebuffer::Create(spec);

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

					if (project["Scenes"])
					{
						for (auto node : project["Scenes"])
						{
							std::string path = node.as<std::string>();
							std::replace(path.begin(), path.end(), '\\', '/');
							if (VFS::Exists(path))
								m_SceneList.push_back(path);
							else
								WF_WARN("RuntimeLayer: Scene not found: {0}", path);
						}
					}

					if (m_SceneList.empty() && project["StartScene"])
					{
						std::string startScene = project["StartScene"].as<std::string>();
						std::replace(startScene.begin(), startScene.end(), '\\', '/');
						if (VFS::Exists(startScene))
							m_SceneList.push_back(startScene);
					}

					// Post-processing settings live on each scene's camera
					// (CameraComponent::PostProcessing) - read per frame below.
				}
			}
			catch (const std::exception& e)
			{
				WF_ERROR("RuntimeLayer: Failed to parse project.wfp: {0}", e.what());
			}
		}

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

			// Both fallbacks iterate unordered maps / raw directory order -
			// sort so ChangeScene(N) indices are deterministic across runs.
			std::sort(m_SceneList.begin(), m_SceneList.end());
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
		// Scripts read this via GetCurrentSceneIndex - without it every
		// ChangeScene computed from a stale index (stuck at 0).
		LuaScriptEngine::SetCurrentSceneIndex(index);
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

		uint32_t width = Application::Get().GetWindow().GetWidth();
		uint32_t height = Application::Get().GetWindow().GetHeight();
		if (width == 0 || height == 0) return;

		CommandBuffer* cmd = Renderer::GetCommandBuffer();

		Entity primaryCam = m_Scene->GetPrimaryCameraEntity();
		glm::vec4 clearColor{ 0.1f, 0.1f, 0.1f, 1.0f };
		PostProcessingSettings postSettings;
		if (primaryCam)
		{
			const auto& cc = primaryCam.GetComponent<CameraComponent>();
			clearColor = cc.BackgroundColor;
			postSettings = cc.PostProcessing;
		}
		cmd->SetClearColor(clearColor);

		if (postSettings.EnablePostProcessing)
		{
			if (width == 0 || height == 0) return;

			const auto& spec = m_Framebuffer->GetSpecification();
			if (spec.Width != width || spec.Height != height)
			{
				m_Framebuffer->Resize(width, height);
			}

			// Render into the offscreen target, then post-process and
			// present it to the screen.
			cmd->BeginRenderPass(m_Framebuffer);

			int pendingScene = m_Scene->OnUpdateRuntime(ts);
			cmd->EndRenderPass();

			// Deferred Quit: tearing the scene down inside a click callback
			// corrupts the registry, so it runs after the frame.
			if (LuaScriptEngine::IsQuitRequested())
			{
				LuaScriptEngine::ClearQuitRequest();
				m_Scene->OnRuntimeStop();
				Application::Get().Close();
				return;
			}

			PostProcessing::ProcessAndPresent(m_Framebuffer, 0, width, height, postSettings);

			if (pendingScene != -1)
				LoadScene(pendingScene);
		}
		else
		{
			// No post chain - render straight to the present surface.
			cmd->BeginSwapchainPass(width, height);

			int pendingScene = m_Scene->OnUpdateRuntime(ts);
			cmd->EndRenderPass();

			if (LuaScriptEngine::IsQuitRequested())
			{
				LuaScriptEngine::ClearQuitRequest();
				m_Scene->OnRuntimeStop();
				Application::Get().Close();
				return;
			}
			if (pendingScene != -1)
				LoadScene(pendingScene);
		}
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