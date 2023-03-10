#include "wfpch.h"
#include "Application.h"
#include "Waffle/Events/ApplicationEvent.h"
#include "Waffle/Log.h"


namespace Waffle {
	Application::Application()
	{

	}

	Application::~Application()
	{

	}

	void Application::Run()
	{
		WindowResizeEvent e(1280, 720);
		WF_TRACE(e);

		while (true);
	}
}