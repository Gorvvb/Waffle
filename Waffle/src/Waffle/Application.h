#pragma once

//#include "wfpch.h"
#include "Core.h"
#include "Events/Event.h"
#include "Window.h"

namespace Waffle {

	class WAFFLE_API Application
	{
	public:
		Application();
		virtual ~Application();

		void Run();
	private:
		std::unique_ptr<Window> m_Window;
		bool m_Running = true;
	};

	// Define in client
	Application* CreateApplication();
}