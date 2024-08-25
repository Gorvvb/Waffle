#include "wfpch.h"
#include "OpenGLContext.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

namespace Waffle {
	OpenGLContext::OpenGLContext(GLFWwindow* windowHandle)
		: m_WindowHandle(windowHandle)
	{
		WF_CORE_ASSERT(windowHandle, "Window handle is null!");
	}

	void OpenGLContext::Init()
	{
		glfwMakeContextCurrent(m_WindowHandle);
		int status = gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
		WF_CORE_ASSERT(status, "Failed to initialize Glad!");

		WF_CORE_INFO("OpenGL Info:");
		WF_CORE_INFO("  Vendor: {0}", (const char*)glGetString(GL_VENDOR));
		WF_CORE_INFO("  Renderer: {0}", (const char*)glGetString(GL_RENDERER));
		WF_CORE_INFO("  Version: {0}", (const char*)glGetString(GL_VERSION));

	#ifdef WF_ENABLE_ASSERTS
		int versionMajor, versionMinor;
		glGetIntegerv(GL_MAJOR_VERSION, &versionMajor);
		glGetIntegerv(GL_MINOR_VERSION, &versionMinor);

		WF_CORE_ASSERT(versionMajor > 4 || (versionMajor == 4 && versionMinor >= 5), "Waffle requires at least OpenGL version 4.5!")
	#endif
	}

	void OpenGLContext::SwapBuffers()
	{
		glfwSwapBuffers(m_WindowHandle);
	}
}