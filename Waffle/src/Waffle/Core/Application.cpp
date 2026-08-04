#include "wfpch.h"
#include "Application.h"

#include "Waffle/Core/Input.h"
#include "Waffle/Core/JobSystem.h"
#include "Waffle/Core/Subsystem.h"
#include "Waffle/Core/EventQueue.h"
#include "Waffle/Renderer/Renderer.h"
#include "Waffle/Core/Log.h"

#include <GLFW/glfw3.h>

namespace Waffle {

	Application* Application::s_Instance = nullptr;

	Application::Application(const ApplicationSpecification& specification)
		: m_Specification(specification)
	{
		WF_PROFILE_FUNCTION();

		WF_CORE_ASSERT(!s_Instance, "Application already exists!");
		s_Instance = this;

		// Set working directory
		if (!m_Specification.WorkingDirectory.empty())
			std::filesystem::current_path(m_Specification.WorkingDirectory);

		// Core engine services initialization
		JobSystem::Init();
		EventQueue::Init();
		SubsystemManager::Init();

		m_Window = Window::Create(WindowProps(m_Specification.Name, 1600, 900, m_Specification.IconPath));
		m_Window->SetEventCallback(WF_BIND_EVENT_FN(Application::OnEvent));
		m_Window->SetVSync(false);

		Renderer::Init();

		m_ImGuiLayer = new ImGuiLayer;
		PushOverlay(m_ImGuiLayer);
	}

	Application::~Application()
	{
		WF_PROFILE_FUNCTION();

		// Detach and destroy layers in REVERSE order (top-to-bottom: overlays first down to base layers)
		m_LayerStack.Clear();

		SubsystemManager::Shutdown();
		EventQueue::Shutdown();
		JobSystem::Shutdown();

		//Renderer::Shutdown();
	}

	void Application::PushLayer(Layer* layer)
	{
		WF_PROFILE_FUNCTION();
		m_LayerStack.PushLayer(layer);
	}

	void Application::PushOverlay(Layer* layer)
	{
		WF_PROFILE_FUNCTION();
		m_LayerStack.PushOverlay(layer);
	}

	void Application::OnEvent(Event& e)
	{
		WF_PROFILE_FUNCTION();

		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<WindowCloseEvent>(WF_BIND_EVENT_FN(Application::OnWindowClose));
		dispatcher.Dispatch<WindowResizeEvent>(WF_BIND_EVENT_FN(Application::OnWindowResize));

		for (auto it = m_LayerStack.rbegin(); it != m_LayerStack.rend(); ++it)
		{
			if (e.handled)
				break;
			(*it)->OnEvent(e);
		}
	}

	void Application::Run()
	{
		WF_PROFILE_FUNCTION();

		while (m_Running)
		{
			WF_PROFILE_SCOPE("RunLoop");

			float time = (float)glfwGetTime(); // Platform::GetTime()
			float rawDelta = time - m_lastFrameTime;
			m_lastFrameTime = time;
			Timestep timestep = std::min(rawDelta, 0.1f); // Cap timestep to 100ms to prevent dt explosion

			// Dispatch queued deferred events from background threads / systems
			EventQueue::DispatchPendingEvents(WF_BIND_EVENT_FN(Application::OnEvent));

			if (!m_Minimized)
			{
				// 1. Fixed Timestep Accumulator Step (Physics & Fixed Logic)
				m_Accumulator += timestep.GetSeconds() * m_TimeScale;
				while (m_Accumulator >= m_FixedTimestep)
				{
					WF_PROFILE_SCOPE("LayerStack OnFixedUpdate");
					SubsystemManager::OnFixedUpdate(m_FixedTimestep);

					for (const auto& layer : m_LayerStack)
						layer->OnFixedUpdate(m_FixedTimestep);

					m_Accumulator -= m_FixedTimestep;
				}

				// 2. Variable Frame Update Step (Render & Frame Logic)
				{
					WF_PROFILE_SCOPE("LayerStack OnUpdate");
					SubsystemManager::OnUpdate(timestep);

					for (const auto& layer : m_LayerStack)
						layer->OnUpdate(timestep);
				}

				m_ImGuiLayer->Begin();
				{
					WF_PROFILE_SCOPE("LayerStack OnImGuiRender");
					for (const auto& layer : m_LayerStack)
						layer->OnImGuiRender();
				}
				m_ImGuiLayer->End();
			}

			m_Window->OnUpdate();
		}
	}

	void Application::Close()
	{
		m_Running = false;
	}

	bool Application::OnWindowClose(WindowCloseEvent& e)
	{
		m_Running = false;
		return true;
	}
	bool Application::OnWindowResize(WindowResizeEvent& e)
	{
		WF_PROFILE_FUNCTION();

		if (e.GetWidth() == 0 || e.GetHeight() == 0)
		{
			m_Minimized = true;
			return false;
		}

		m_Minimized = false;
		Renderer::OnWindowResize(e.GetWidth(), e.GetHeight());

		return false;
	}
}