#include "wfpch.h"
#include "Waffle/Core/Window.h"

#ifdef WF_PLATFORM_WINDOWS
#include "Platform/Windows/WindowsWindow.h"
#endif

namespace Waffle {

	Scope<Window> Window::Create(const WindowProps& props)
	{
	#ifdef WF_PLATFORM_WINDOWS
		return CreateScope<WindowsWindow>(props);
	#else
		WF_CORE_ASSERT(false, "Unknown platform!");
		return nullptr;
	#endif
	}
}