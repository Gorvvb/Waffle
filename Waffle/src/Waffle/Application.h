#pragma once

#include "Core.h"

#include "Window.h"
#include "Waffle/LayerStack.h"
#include "Waffle/Events/Event.h"
#include "Waffle/Events/ApplicationEvent.h"

#include "Waffle/ImGui/ImGuiLayer.h"

namespace Waffle {

	class Application
	{
	private:
		std::unique_ptr<Window> m_Window;
		ImGuiLayer* m_ImGuiLayer;
		bool m_Running = true;
		LayerStack m_LayerStack;

		static Application* s_Instance;
	public:
		Application();
		virtual ~Application();

		void Run();

		void OnEvent(Event& e);

		void PushLayer(Layer* layer);
		void PushOverlay(Layer* layer);

		inline Window& GetWindow() { return *m_Window; }
		inline static Application& Get() { return *s_Instance; }
	private:
		bool OnWindowClose(WindowCloseEvent& e);
	};

	// Remember to define in client
	Application* CreateApplication();
}