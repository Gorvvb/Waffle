#include "wfpch.h"
#include "Application.h"

#include "Waffle/Core/Input.h"
#include "Waffle/Renderer/Renderer.h"

#include "Waffle/Core/Log.h"

#include <GLFW/glfw3.h>

namespace Waffle {

	Application* Application::s_Instance = nullptr;

	Application::Application()
	{
		WF_PROFILE_FUNCTION();

		WF_CORE_ASSERT(!s_Instance, "Application already exists!");
		s_Instance = this;

		m_Window = std::unique_ptr<Window>(Window::Create());
		m_Window->SetEventCallback(WF_BIND_EVENT_FN(Application::OnEvent));
		//m_Window->SetVSync(false);

		Renderer::Init();

		m_ImGuiLayer = new ImGuiLayer;
		PushOverlay(m_ImGuiLayer);
	}

	Application::~Application()
	{
		WF_PROFILE_FUNCTION();

		//Renderer::Shutdown();
	}

	void Application::PushLayer(Layer* layer)
	{
		WF_PROFILE_FUNCTION();

		m_LayerStack.PushLayer(layer);
		layer->OnAttach();
	}

	void Application::PushOverlay(Layer* layer)
	{
		WF_PROFILE_FUNCTION();

		m_LayerStack.PushOverlay(layer);
		layer->OnAttach();
	}

	void Application::OnEvent(Event& e)
	{
		WF_PROFILE_FUNCTION();

		EventDispatcher dispacher(e);
		dispacher.Dispatch<WindowCloseEvent>(WF_BIND_EVENT_FN(Application::OnWindowClose));
		dispacher.Dispatch<WindowResizeEvent>(WF_BIND_EVENT_FN(Application::OnWindowRisize));

		for (auto it = m_LayerStack.end(); it != m_LayerStack.begin();)
		{
			(*--it)->OnEvent(e);
			if (e.handled)
				break;
		}
	}

	void Application::Run()
	{
		WF_PROFILE_FUNCTION();

		while (m_Running)
		{
			WF_PROFILE_SCOPE("RunLoop");

			float time = (float)glfwGetTime(); // Platform::GetTime()
			Timestep timestep = time - m_lastFrameTime;
			m_lastFrameTime = time;

			if (!m_Minimized)
			{
				{
					WF_PROFILE_SCOPE("LayerStack OnUpdate");

					for (Layer* layer : m_LayerStack)
						layer->OnUpdate(timestep);
				}

				m_ImGuiLayer->Begin();
				{
					WF_PROFILE_SCOPE("LayerStack OnImGuiRender");
					for (Layer* layer : m_LayerStack)
						layer->OnImGuiRender();
				}
				m_ImGuiLayer->End();
			}

			m_Window->OnUpdate();
		}
	}

	bool Application::OnWindowClose(WindowCloseEvent& e)
	{
		m_Running = false;
		return true;
	}
	bool Application::OnWindowRisize(WindowResizeEvent& e)
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