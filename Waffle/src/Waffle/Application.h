#pragma once

//#include "wfpch.h"
#include "Core.h"
#include "Events/Event.h"
#include "Window.h"

#include "Waffle/Events/ApplicationEvent.h"


namespace Waffle {

	class WAFFLE_API Application
	{
	public:
		Application();
		virtual ~Application();

		void Run();

		void OnEvent(Event& e);
	private:
		bool OnWindowClose(WindowCloseEvent& e);

		std::unique_ptr<Window> m_Window;
		bool m_Running = true;
	};

	// Define in client
	Application* CreateApplication();
}