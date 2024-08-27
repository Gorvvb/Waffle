#pragma once

#include "Waffle/Core/Base.h"

#include "Waffle/Core/Window.h"
#include "Waffle/Core/LayerStack.h"
#include "Waffle/Events/Event.h"
#include "Waffle/Events/ApplicationEvent.h"

#include "Waffle/Core/Timestep.h"

#include "Waffle/ImGui/ImGuiLayer.h"

namespace Waffle {

	class Application
	{
	private:
		std::unique_ptr<Window> m_Window;
		ImGuiLayer* m_ImGuiLayer;
		bool m_Running = true;
		bool m_Minimized = false;
		LayerStack m_LayerStack;

		float m_lastFrameTime = 0.0f;

		static Application* s_Instance;
	public:
		Application();
		virtual ~Application();

		void Run();

		void OnEvent(Event& e);

		void PushLayer(Layer* layer);
		void PushOverlay(Layer* layer);

		Window& GetWindow() { return *m_Window; }
		static Application& Get() { return *s_Instance; }
	private:
		bool OnWindowClose(WindowCloseEvent& e);
		bool OnWindowRisize(WindowResizeEvent& e);
	};

	// Remember to define in client
	Application* CreateApplication();
}