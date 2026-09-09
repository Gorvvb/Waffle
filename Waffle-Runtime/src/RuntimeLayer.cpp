#include "RuntimeLayer.h"
#include "Waffle/Scene/SceneSerializer.h"
#include "Waffle/Core/VFS.h"
#include "Waffle/Renderer/PostProcessing.h"

#include <yaml-cpp/yaml.h>
#include <glad/glad.h>
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

					auto ppNode = project["PostProcessing"];
					if (ppNode)
					{
						auto& pp = PostProcessing::GetSettings();
						if (ppNode["EnablePostProcessing"]) pp.EnablePostProcessing = ppNode["EnablePostProcessing"].as<bool>();
						if (ppNode["EnableBloom"]) pp.EnableBloom = ppNode["EnableBloom"].as<bool>();
						if (ppNode["BloomThreshold"]) pp.BloomThreshold = ppNode["BloomThreshold"].as<float>();
						if (ppNode["BloomIntensity"]) pp.BloomIntensity = ppNode["BloomIntensity"].as<float>();
						if (ppNode["BloomColor"])
						{
							pp.BloomColor.r = ppNode["BloomColor"][0].as<float>();
							pp.BloomColor.g = ppNode["BloomColor"][1].as<float>();
							pp.BloomColor.b = ppNode["BloomColor"][2].as<float>();
						}

						if (ppNode["EnableVignette"]) pp.EnableVignette = ppNode["EnableVignette"].as<bool>();
						if (ppNode["VignetteIntensity"]) pp.VignetteIntensity = ppNode["VignetteIntensity"].as<float>();
						if (ppNode["VignetteSmoothness"]) pp.VignetteSmoothness = ppNode["VignetteSmoothness"].as<float>();
						if (ppNode["VignetteColor"])
						{
							pp.VignetteColor.r = ppNode["VignetteColor"][0].as<float>();
							pp.VignetteColor.g = ppNode["VignetteColor"][1].as<float>();
							pp.VignetteColor.b = ppNode["VignetteColor"][2].as<float>();
						}

						if (ppNode["EnableTonemapping"]) pp.EnableTonemapping = ppNode["EnableTonemapping"].as<bool>();
						if (ppNode["Exposure"]) pp.Exposure = ppNode["Exposure"].as<float>();
						if (ppNode["Contrast"]) pp.Contrast = ppNode["Contrast"].as<float>();
						if (ppNode["Saturation"]) pp.Saturation = ppNode["Saturation"].as<float>();
						if (ppNode["ColorGradingTint"])
						{
							pp.ColorGradingTint.r = ppNode["ColorGradingTint"][0].as<float>();
							pp.ColorGradingTint.g = ppNode["ColorGradingTint"][1].as<float>();
							pp.ColorGradingTint.b = ppNode["ColorGradingTint"][2].as<float>();
						}
					}
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

		if (PostProcessing::GetSettings().EnablePostProcessing)
		{
			uint32_t width = Application::Get().GetWindow().GetWidth();
			uint32_t height = Application::Get().GetWindow().GetHeight();
			if (width == 0 || height == 0) return;

			const auto& spec = m_Framebuffer->GetSpecification();
			if (spec.Width != width || spec.Height != height)
			{
				m_Framebuffer->Resize(width, height);
			}

			m_Framebuffer->Bind();

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
			m_Framebuffer->Unbind();

			uint32_t processedTex = PostProcessing::Process((uint32_t)m_Framebuffer->GetColorAttachmentRendererID(0), width, height);

			PostProcessing::PresentToScreen(processedTex, width, height);

			if (pendingScene != -1)
				LoadScene(pendingScene);
		}
		else
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

			int pendingScene = m_Scene->OnUpdateRuntime(ts);
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