#include "RuntimeLayer.h"
#include "Waffle/Scene/SceneSerializer.h"
#include "Waffle/Core/VFS.h"
#include "Waffle/Project/RuntimeProjectConfig.h"
#include "Waffle/Renderer/PostProcessing.h"
#include "Waffle/Renderer/Renderer.h"
#include "Waffle/Scripting/CSharpScriptEngine.h"

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

		// Parsed once per process (WaffleRuntimeApp already asked for it) - gravity, scene list, start scene.
		const RuntimeProjectConfig& config = GetRuntimeProjectConfig();
		m_Gravity = config.GravityY;

		for (const auto& path : config.Scenes)
		{
			if (VFS::Exists(path))
				m_SceneList.push_back(path);
			else
				WF_WARN("RuntimeLayer: Scene not found: {0}", path);
		}

		if (m_SceneList.empty() && !config.StartScene.empty() && VFS::Exists(config.StartScene))
			m_SceneList.push_back(config.StartScene);

		// Post-processing settings live on each scene's camera (CameraComponent::PostProcessing) - read per frame below.

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

			// Both fallbacks iterate unordered maps / raw dir order - sort so ChangeScene(N) indices are deterministic.
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
		// Scripts read this via GetCurrentSceneIndex - without it ChangeScene computed from a stale index (stuck at 0).
		CSharpScriptEngine::SetCurrentSceneIndex(index);
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

		// Always render into the offscreen target (it has the entity-ID attachment), then present - never straight to the swapchain, which has no second attachment and breaks validation + picking.
		{
			const auto& spec = m_Framebuffer->GetSpecification();
			if (spec.Width != width || spec.Height != height)
			{
				m_Framebuffer->Resize(width, height);
			}

			cmd->BeginRenderPass(m_Framebuffer);

			int pendingScene = m_Scene->OnUpdateRuntime(ts);
			cmd->EndRenderPass();

			// Deferred Quit: tearing the scene down inside a click callback corrupts the registry, so run after the frame.
			if (CSharpScriptEngine::IsQuitRequested())
			{
				CSharpScriptEngine::ClearQuitRequest();
				m_Scene->OnRuntimeStop();
				Application::Get().Close();
				return;
			}

			PostProcessing::ProcessAndPresent(m_Framebuffer, 0, width, height, postSettings);

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