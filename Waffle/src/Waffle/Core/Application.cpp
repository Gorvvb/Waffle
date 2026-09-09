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

		// Release renderer GPU resources HERE, while the window/context
		// still exists - static destructors run after main() on a dead
		// context (leaks at best, crashes at worst).
		Renderer::Shutdown();

		SubsystemManager::Shutdown();
		EventQueue::Shutdown();
		JobSystem::Shutdown();
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

		// Iteration guard: a handler may push/pop layers; the stack defers
		// those mutations until the walk finishes.
		m_LayerStack.BeginIteration();
		for (auto it = m_LayerStack.rbegin(); it != m_LayerStack.rend(); ++it)
		{
			if (e.handled)
				break;
			(*it)->OnEvent(e);
		}
		m_LayerStack.EndIteration();
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

			// Advance input edge-detection state machines (keyboard/mouse/
			// gamepad Pressed/Released transitions) for this frame.
			Input::Update();

			// Dispatch queued deferred events from background threads / systems
			EventQueue::DispatchPendingEvents(WF_BIND_EVENT_FN(Application::OnEvent));

			if (!m_Minimized)
			{
				// 1. Fixed Timestep Accumulator Step (Physics & Fixed Logic)
				m_Accumulator += timestep.GetSeconds() * m_TimeScale;
				if (m_FixedTimestep > 0.0f)
				{
					while (m_Accumulator >= m_FixedTimestep)
					{
						WF_PROFILE_SCOPE("LayerStack OnFixedUpdate");
						SubsystemManager::OnFixedUpdate(m_FixedTimestep);

						m_LayerStack.BeginIteration();
						for (const auto& layer : m_LayerStack)
							layer->OnFixedUpdate(m_FixedTimestep);
						m_LayerStack.EndIteration();

						m_Accumulator -= m_FixedTimestep;
					}
				}
				else
				{
					// A zero/negative fixed timestep would hang the loop -
					// clamp instead of draining.
					m_Accumulator = 0.0f;
				}

				// 2. Variable Frame Update Step (Render & Frame Logic)
				{
					WF_PROFILE_SCOPE("LayerStack OnUpdate");
					SubsystemManager::OnUpdate(timestep);

					m_LayerStack.BeginIteration();
					for (const auto& layer : m_LayerStack)
						layer->OnUpdate(timestep);
					m_LayerStack.EndIteration();
				}

				m_ImGuiLayer->Begin();
				{
					WF_PROFILE_SCOPE("LayerStack OnImGuiRender");
					m_LayerStack.BeginIteration();
					for (const auto& layer : m_LayerStack)
						layer->OnImGuiRender();
					m_LayerStack.EndIteration();
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