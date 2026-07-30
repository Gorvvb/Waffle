#include "wfpch.h"
#include "ImGuiLayer.h"

#include <imgui.h>

#include "backends/imgui_impl_opengl3.h"
#include "backends/imgui_impl_glfw.h"

#include "Waffle/Core/Application.h"

//TEMPORARY
#include "GLFW/glfw3.h"
#include "glad/glad.h"

#include "ImGuizmo.h"

namespace Waffle {
	ImGuiLayer::ImGuiLayer()
		: Layer("ImGuiLayer")
	{
	}

	ImGuiLayer::~ImGuiLayer() {}

	void ImGuiLayer::OnAttach()
	{
		WF_PROFILE_FUNCTION();

		// Setup ImGui context
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO(); (void)io;
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
		//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
		io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

		io.Fonts->AddFontFromFileTTF("assets/fonts/OpenSans/OpenSans-Bold.ttf", 18.0f);
		io.FontDefault = io.Fonts->AddFontFromFileTTF("assets/fonts/OpenSans/OpenSans-Regular.ttf", 18.0f);

		// Setup ImGui style
		ImGui::StyleColorsDark();

		// Makes the window look like a normal window when using viewports
		ImGuiStyle& style = ImGui::GetStyle();
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			style.WindowRounding = 0.0f;
			style.Colors[ImGuiCol_WindowBg].w = 1.0f;
		}

		SetDarkThemeColors();

		Application& app = Application::Get();
		GLFWwindow* window = static_cast<GLFWwindow*>(app.GetWindow().GetNativeWindow());

		// Setup Platform/Renderer bindings
		ImGui_ImplGlfw_InitForOpenGL(window, true);
		ImGui_ImplOpenGL3_Init("#version 460");
	}

	void ImGuiLayer::OnDetach()
	{
		WF_PROFILE_FUNCTION();

		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
	}

	void ImGuiLayer::Begin()
	{
		WF_PROFILE_FUNCTION();

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
		ImGuizmo::BeginFrame();
	}

	void ImGuiLayer::OnEvent(Event& e)
	{
		if (m_BlockEvents)
		{
			ImGuiIO& io = ImGui::GetIO();
			e.handled |= e.IsInCategory(EventCategoryMouse) & io.WantCaptureMouse;
			e.handled |= e.IsInCategory(EventCategoryKeyboard) & io.WantCaptureKeyboard;
		}
	}

	void ImGuiLayer::End()
	{
		WF_PROFILE_FUNCTION();

		ImGuiIO& io = ImGui::GetIO();
		Application& app = Application::Get();
		io.DisplaySize = ImVec2((float)app.GetWindow().GetWidth(), (float)app.GetWindow().GetHeight());

		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			GLFWwindow* backup_current_context = glfwGetCurrentContext();
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
			glfwMakeContextCurrent(backup_current_context);
		}
	}

	void ImGuiLayer::SetDarkThemeColors()
	{
		auto& style = ImGui::GetStyle();
		style.WindowMinSize = ImVec2(160.0f, 100.0f);
		style.FramePadding = ImVec2(6.0f, 4.0f);
		style.ItemSpacing = ImVec2(6.0f, 5.0f);
		style.ItemInnerSpacing = ImVec2(6.0f, 4.0f);
		style.WindowRounding = 4.0f;
		style.FrameRounding = 4.0f;
		style.PopupRounding = 6.0f;
		style.ScrollbarRounding = 4.0f;
		style.GrabRounding = 3.0f;
		style.TabRounding = 4.0f;
		style.WindowBorderSize = 1.0f;
		style.FrameBorderSize = 0.0f;
		style.PopupBorderSize = 1.0f;

		auto& colors = style.Colors;

		// Main Backgrounds
		colors[ImGuiCol_WindowBg] = ImVec4{ 0.11f, 0.114f, 0.125f, 1.0f };
		colors[ImGuiCol_ChildBg] = ImVec4{ 0.11f, 0.114f, 0.125f, 1.0f };
		colors[ImGuiCol_PopupBg] = ImVec4{ 0.14f, 0.145f, 0.16f, 0.98f };
		colors[ImGuiCol_Border] = ImVec4{ 0.22f, 0.23f, 0.25f, 0.6f };
		colors[ImGuiCol_BorderShadow] = ImVec4{ 0.0f, 0.0f, 0.0f, 0.0f };

		// Headers
		colors[ImGuiCol_Header] = ImVec4{ 0.19f, 0.20f, 0.22f, 1.0f };
		colors[ImGuiCol_HeaderHovered] = ImVec4{ 0.26f, 0.27f, 0.30f, 1.0f };
		colors[ImGuiCol_HeaderActive] = ImVec4{ 0.22f, 0.23f, 0.25f, 1.0f };

		// Buttons
		colors[ImGuiCol_Button] = ImVec4{ 0.18f, 0.19f, 0.21f, 1.0f };
		colors[ImGuiCol_ButtonHovered] = ImVec4{ 0.26f, 0.27f, 0.30f, 1.0f };
		colors[ImGuiCol_ButtonActive] = ImVec4{ 0.15f, 0.16f, 0.18f, 1.0f };

		// Frame Background
		colors[ImGuiCol_FrameBg] = ImVec4{ 0.16f, 0.17f, 0.19f, 1.0f };
		colors[ImGuiCol_FrameBgHovered] = ImVec4{ 0.22f, 0.23f, 0.26f, 1.0f };
		colors[ImGuiCol_FrameBgActive] = ImVec4{ 0.14f, 0.15f, 0.17f, 1.0f };

		// Tabs
		colors[ImGuiCol_Tab] = ImVec4{ 0.14f, 0.145f, 0.16f, 1.0f };
		colors[ImGuiCol_TabHovered] = ImVec4{ 0.26f, 0.27f, 0.30f, 1.0f };
		colors[ImGuiCol_TabActive] = ImVec4{ 0.20f, 0.21f, 0.23f, 1.0f };
		colors[ImGuiCol_TabUnfocused] = ImVec4{ 0.14f, 0.145f, 0.16f, 1.0f };
		colors[ImGuiCol_TabUnfocusedActive] = ImVec4{ 0.18f, 0.19f, 0.21f, 1.0f };

		// Title
		colors[ImGuiCol_TitleBg] = ImVec4{ 0.14f, 0.145f, 0.16f, 1.0f };
		colors[ImGuiCol_TitleBgActive] = ImVec4{ 0.14f, 0.145f, 0.16f, 1.0f };
		colors[ImGuiCol_TitleBgCollapsed] = ImVec4{ 0.14f, 0.145f, 0.16f, 1.0f };

		// Resize Grip & Separator
		colors[ImGuiCol_Separator] = ImVec4{ 0.22f, 0.23f, 0.25f, 1.0f };
		colors[ImGuiCol_SeparatorHovered] = ImVec4{ 0.35f, 0.37f, 0.42f, 1.0f };
		colors[ImGuiCol_SeparatorActive] = ImVec4{ 0.45f, 0.47f, 0.52f, 1.0f };

		// Scrollbar
		colors[ImGuiCol_ScrollbarBg] = ImVec4{ 0.11f, 0.114f, 0.125f, 0.6f };
		colors[ImGuiCol_ScrollbarGrab] = ImVec4{ 0.20f, 0.21f, 0.23f, 1.0f };
		colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4{ 0.28f, 0.29f, 0.32f, 1.0f };
		colors[ImGuiCol_ScrollbarGrabActive] = ImVec4{ 0.35f, 0.37f, 0.40f, 1.0f };
	}
}