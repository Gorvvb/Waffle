#include "wfpch.h"
#include "WindowsWindow.h"

#include "Waffle/Events/ApplicationEvent.h"
#include "Waffle/Events/MouseEvent.h"
#include "Waffle/Events/KeyEvent.h"

#include "Waffle/Renderer/RendererAPI.h"
#include "Waffle/Renderer/Texture.h"

#include "Platform/OpenGL/OpenGLContext.h"
#include "Platform/Vulkan/VulkanContext.h"

#include "stb_image.h"

// Only include glad for OpenGL builds
#include <glad/glad.h>

namespace Waffle {

	static bool s_GLFWInitialized = false;

	static void GLFWErrorCallback(int error, const char* description)
	{
		WF_CORE_ERROR("GLFW Error ({0}): {1}", error, description);
	}

	WindowsWindow::WindowsWindow(const WindowProps& props)
	{
		Init(props);
	}

	WindowsWindow::~WindowsWindow()
	{
		Shutdown();
	}

	void WindowsWindow::Init(const WindowProps& props)
	{
		WF_PROFILE_FUNCTION();

		m_Data.Title  = props.Title;
		m_Data.Width  = props.Width;
		m_Data.Height = props.Height;

		WF_CORE_INFO("Creating window {0} ({1},{2})", props.Title, props.Width, props.Height);

		if (!s_GLFWInitialized)
		{
			WF_PROFILE_FUNCTION();

			int success = glfwInit();
			WF_CORE_ASSERT(success, "Could not initialize GLFW!");
			glfwSetErrorCallback(GLFWErrorCallback);

			s_GLFWInitialized = true;
		}

		// ---- Window creation hints depend on API ----
		if (RendererAPI::GetAPI() == RendererAPI::API::Vulkan)
		{
			// Tell GLFW not to create an OpenGL context
			glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
			m_Window = glfwCreateWindow((int)props.Width, (int)props.Height,
				m_Data.Title.c_str(), nullptr, nullptr);

			m_Context = new VulkanContext(m_Window);
		}
		else
		{
			// Default: OpenGL
			m_Window = glfwCreateWindow((int)props.Width, (int)props.Height,
				m_Data.Title.c_str(), nullptr, nullptr);

			m_Context = new OpenGLContext(m_Window);
		}

		m_Context->Init();

		glfwSetWindowUserPointer(m_Window, &m_Data);
		SetVSync(true);

		if (!props.IconPath.empty())
			SetIcon(props.IconPath);

		// ---- GLFW callbacks ----
		glfwSetWindowSizeCallback(m_Window, [](GLFWwindow* window, int width, int height)
		{
			WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);
			data.Width  = width;
			data.Height = height;

			WindowResizeEvent event(width, height);
			data.EventCallback(event);
		});

		glfwSetWindowCloseCallback(m_Window, [](GLFWwindow* window)
		{
			WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);
			WindowCloseEvent event;
			data.EventCallback(event);
		});

		glfwSetKeyCallback(m_Window, [](GLFWwindow* window, int key, int scancode, int action, int mods)
		{
			WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);

			switch (action)
			{
				case GLFW_PRESS:
				{
					KeyPressedEvent event(key, 0);
					data.EventCallback(event);
					break;
				}
				case GLFW_RELEASE:
				{
					KeyReleasedEvent event(key);
					data.EventCallback(event);
					break;
				}
				case GLFW_REPEAT:
				{
					KeyPressedEvent event(key, 1);
					data.EventCallback(event);
					break;
				}
			}
		});

		glfwSetCharCallback(m_Window, [](GLFWwindow* window, unsigned int keycode)
		{
			WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);
			KeyTypedEvent event(keycode);
			data.EventCallback(event);
		});

		glfwSetMouseButtonCallback(m_Window, [](GLFWwindow* window, int button, int action, int mods)
		{
			WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);

			switch (action)
			{
				case GLFW_PRESS:
				{
					MouseButtonPressedEvent event(button);
					data.EventCallback(event);
					break;
				}
				case GLFW_RELEASE:
				{
					MouseButtonReleasedEvent event(button);
					data.EventCallback(event);
					break;
				}
			}
		});

		glfwSetScrollCallback(m_Window, [](GLFWwindow* window, double xOffset, double yOffset)
		{
			WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);

			MouseScrolledEvent event((float)xOffset, (float)yOffset);
			data.EventCallback(event);
		});

		glfwSetCursorPosCallback(m_Window, [](GLFWwindow* window, double xPos, double yPos)
		{
			WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);

			MouseMovedEvent event((float)xPos, (float)yPos);
			data.EventCallback(event);
		});

		glfwSetDropCallback(m_Window, [](GLFWwindow* window, int count, const char** paths)
		{
			WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);

			std::vector<std::filesystem::path> droppedPaths;
			droppedPaths.reserve(count);
			for (int i = 0; i < count; i++)
				droppedPaths.emplace_back(paths[i]);

			WindowDropEvent event(droppedPaths);
			data.EventCallback(event);
		});
	}

	void WindowsWindow::Shutdown()
	{
		WF_PROFILE_FUNCTION();
		glfwDestroyWindow(m_Window);
	}

	void WindowsWindow::OnUpdate()
	{
		WF_PROFILE_FUNCTION();
		glfwPollEvents();
		m_Context->SwapBuffers();
	}

	void WindowsWindow::SetVSync(bool enabled)
	{
		WF_PROFILE_FUNCTION();

		// VSync via glfwSwapInterval only applies to OpenGL
		if (RendererAPI::GetAPI() != RendererAPI::API::Vulkan)
		{
			if (enabled)
				glfwSwapInterval(1);
			else
				glfwSwapInterval(0);
		}
		// For Vulkan, VSync is handled by present mode (FIFO = vsync on)

		m_Data.VSync = enabled;
	}

	bool WindowsWindow::IsVSync() const
	{
		return m_Data.VSync;
	}

	void WindowsWindow::SetTitle(const std::string& title)
	{
		m_Data.Title = title;
		glfwSetWindowTitle(m_Window, m_Data.Title.c_str());
	}

	void WindowsWindow::SetIcon(const std::string& path)
	{
		std::string iconPath = path;
		if (!std::filesystem::exists(iconPath))
		{
			std::filesystem::path resolved = ResolveTexturePath(path);
			std::error_code ec;
			if (std::filesystem::exists(resolved, ec))
				iconPath = resolved.string();
			else if (std::filesystem::exists("Resources/Icons/logo.png", ec))
				iconPath = "Resources/Icons/logo.png";
			else if (std::filesystem::exists("Waffle-Editor/Assets/images/logo.png", ec))
				iconPath = "Waffle-Editor/Resources/Icons/logo.png";
			else if (std::filesystem::exists("../Waffle-Editor/Resources/Icons/logo.png", ec))
				iconPath = "../Waffle-Editor/Resources/Icons/logo.png";
			else if (std::filesystem::exists("Resources/Icons/Icon.ico", ec))
				iconPath = "Resources/Icons/Icon.ico";
		}

		int width, height, channels;
		stbi_uc* pixels = stbi_load(iconPath.c_str(), &width, &height, &channels, 4);
		if (pixels)
		{
			GLFWimage image;
			image.width = width;
			image.height = height;
			image.pixels = pixels;
			glfwSetWindowIcon(m_Window, 1, &image);
			stbi_image_free(pixels);
			WF_CORE_INFO("Set window icon: {0}", iconPath);
		}
		else
		{
			WF_CORE_WARN("Failed to load window icon from {0}", path);
		}
	}
}