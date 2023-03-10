#pragma once

#include "wfpch.h"
#include "Core.h"
#include "Events/Event.h"

namespace Waffle {

	class WAFFLE_API Application
	{
	public:
		Application();
		virtual ~Application();

		void Run();
	};

	// Define in client
	Application* CreateApplication();
}