#include "wfpch.h"
#include "ImGuiLayer.h"

#include <imgui.h>
#include "ImGuizmo.h"

#include "Waffle/Core/Application.h"
#include "Waffle/Renderer/RendererAPI.h"

// ---- OpenGL path ----
#include "backends/imgui_impl_opengl3.h"
#include "backends/imgui_impl_glfw.h"
#include "GLFW/glfw3.h"
#include "glad/glad.h"

// ---- Vulkan path ----
#include "backends/imgui_impl_vulkan.h"
#include "Platform/Vulkan/VulkanContext.h"

namespace Waffle {

	static bool   s_IsVulkan = false;
	static GLuint s_NearestSampler = 0; // forces GL_NEAREST for all ImGui texture draws

	ImGuiLayer::ImGuiLayer()
		: Layer("ImGuiLayer")
	{}

	ImGuiLayer::~ImGuiLayer() {}

	void ImGuiLayer::OnAttach()
	{
		WF_PROFILE_FUNCTION();

		s_IsVulkan = (RendererAPI::GetAPI() == RendererAPI::API::Vulkan);

		// Setup ImGui context
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO(); (void)io;
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
		io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

		if (std::filesystem::exists("assets/fonts/OpenSans/OpenSans-Bold.ttf"))
			io.Fonts->AddFontFromFileTTF("assets/fonts/OpenSans/OpenSans-Bold.ttf", 18.0f);

		if (std::filesystem::exists("assets/fonts/OpenSans/OpenSans-Regular.ttf"))
			io.FontDefault = io.Fonts->AddFontFromFileTTF("assets/fonts/OpenSans/OpenSans-Regular.ttf", 18.0f);
		else
			io.FontDefault = io.Fonts->AddFontDefault();

		ImGui::StyleColorsDark();

		ImGuiStyle& style = ImGui::GetStyle();
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			style.WindowRounding = 0.0f;
			style.Colors[ImGuiCol_WindowBg].w = 1.0f;
		}

		SetDarkThemeColors();

		Application& app = Application::Get();
		GLFWwindow* window = static_cast<GLFWwindow*>(app.GetWindow().GetNativeWindow());

		if (s_IsVulkan)
		{
			auto* ctx = VulkanContext::Get();

			ImGui_ImplGlfw_InitForVulkan(window, true);

			ImGui_ImplVulkan_InitInfo initInfo
			{
				.ApiVersion = VK_API_VERSION_1_4,
				.Instance = ctx->GetInstance(),
				.PhysicalDevice = ctx->GetPhysicalDevice(),
				.Device = ctx->GetDevice(),
				.QueueFamily = ctx->GetGraphicsQueueFamily(),
				.Queue = ctx->GetGraphicsQueue(),
				.DescriptorPool = ctx->GetDescriptorPool(),
				// ImGui requires MinImageCount >= ImageCount. With 3 swapchain
				// images and only 2 frames in flight, the backend rotated its font
				// descriptor sets through destroyed/uninitialized handles.
				.MinImageCount = ctx->GetSwapChainImageCount(),
				.ImageCount = ctx->GetSwapChainImageCount(),
				.UseDynamicRendering = true
			};

			initInfo.PipelineInfoMain.RenderPass = VK_NULL_HANDLE;
			initInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

#ifdef IMGUI_IMPL_VULKAN_HAS_DYNAMIC_RENDERING
			VkFormat colorFmt = ctx->GetSwapChainImageFormat();
			initInfo.PipelineInfoMain.PipelineRenderingCreateInfo =
			{
				.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
				.colorAttachmentCount = 1,
				.pColorAttachmentFormats = &colorFmt,
				.depthAttachmentFormat = ctx->GetDepthFormat()
			};
#endif

			ImGui_ImplVulkan_LoadFunctions(
				VK_API_VERSION_1_4,
				[](const char* function_name, void* user_data) -> PFN_vkVoidFunction {
					return vkGetInstanceProcAddr(static_cast<VkInstance>(user_data), function_name);
				},
				static_cast<void*>(ctx->GetInstance())
			);

			ImGui_ImplVulkan_Init(&initInfo);
		}
		else
		{
			ImGui_ImplGlfw_InitForOpenGL(window, true);
			ImGui_ImplOpenGL3_Init("#version 460");

			// Create a persistent nearest-neighbour sampler object.
			// ImGui's OpenGL backend calls glBindTexture directly without
			// touching sampler state. Binding this sampler to unit 0 before
			// RenderDrawData forces GL_NEAREST for every texture ImGui draws,
			// overriding whatever the texture object's own parameters say.
			// A sampler object always takes precedence over glTexParameteri.
			glGenSamplers(1, &s_NearestSampler);
			glSamplerParameteri(s_NearestSampler, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glSamplerParameteri(s_NearestSampler, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glSamplerParameteri(s_NearestSampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glSamplerParameteri(s_NearestSampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		}
	}

	void ImGuiLayer::OnDetach()
	{
		WF_PROFILE_FUNCTION();

		if (s_IsVulkan)
		{
			auto* ctx = VulkanContext::Get();
			if (ctx) vkDeviceWaitIdle(ctx->GetDevice());
			ImGui_ImplVulkan_Shutdown();
			ImGui_ImplGlfw_Shutdown();
		}
		else
		{
			ImGui_ImplOpenGL3_Shutdown();
			ImGui_ImplGlfw_Shutdown();

			if (s_NearestSampler)
			{
				glDeleteSamplers(1, &s_NearestSampler);
				s_NearestSampler = 0;
			}
		}
		ImGui::DestroyContext();
	}

	void ImGuiLayer::Begin()
	{
		WF_PROFILE_FUNCTION();

		if (s_IsVulkan)
		{
			ImGui_ImplVulkan_NewFrame();
			ImGui_ImplGlfw_NewFrame();
		}
		else
		{
			ImGui_ImplOpenGL3_NewFrame();
			ImGui_ImplGlfw_NewFrame();
		}
		ImGui::NewFrame();
		ImGuizmo::BeginFrame();
	}

	void ImGuiLayer::OnEvent(Event& e)
	{
		if (m_BlockEvents)
		{
			ImGuiIO& io = ImGui::GetIO();
			e.handled |= e.IsInCategory(EventCategoryMouse) && io.WantCaptureMouse;
			e.handled |= e.IsInCategory(EventCategoryKeyboard) && io.WantCaptureKeyboard;
		}
	}

	void ImGuiLayer::End()
	{
		WF_PROFILE_FUNCTION();

		ImGuiIO& io = ImGui::GetIO();
		Application& app = Application::Get();
		io.DisplaySize = ImVec2((float)app.GetWindow().GetWidth(),
			(float)app.GetWindow().GetHeight());

		ImGui::Render();

		if (s_IsVulkan)
		{
			auto* ctx = VulkanContext::Get();
			VkCommandBuffer cmd = ctx->GetCurrentCommandBuffer();

			if (!ctx->IsRenderingActive())
			{
				VkClearColorValue        clearCol = ctx->GetClearColor();
				VkClearDepthStencilValue clearDepth{ 1.0f, 0 };
				ctx->BeginSwapChainRendering(clearCol, clearDepth);

				VkViewport vp = ctx->GetCurrentViewport();
				VkRect2D   sc = ctx->GetCurrentScissor();
				vkCmdSetViewport(cmd, 0, 1, &vp);
				vkCmdSetScissor(cmd, 0, 1, &sc);
			}

			ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

			if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
			{
				ImGui::UpdatePlatformWindows();
				ImGui::RenderPlatformWindowsDefault();
			}
		}
		else
		{
			// Bind the nearest-neighbour sampler to unit 0 so that every
			// texture ImGui draws (content browser thumbnails, spritesheet
			// canvas, etc.) uses GL_NEAREST regardless of what ImGui's
			// internal SetupRenderState does to the GL state.
			// The sampler object takes precedence over per-texture parameters.
			glBindSampler(0, s_NearestSampler);

			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

			// Unbind the sampler so the scene renderer's Bind() calls
			// (which also call glBindSampler(slot, 0)) work as expected.
			glBindSampler(0, 0);

			if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
			{
				GLFWwindow* backup_current_context = glfwGetCurrentContext();
				ImGui::UpdatePlatformWindows();
				ImGui::RenderPlatformWindowsDefault();
				glfwMakeContextCurrent(backup_current_context);
			}
		}
	}

	void ImGuiLayer::BeginTextureSamplerPassthrough(ImDrawList* drawList)
	{
		if (s_IsVulkan) return;

		drawList->AddCallback([](const ImDrawList*, const ImDrawCmd*) {
			glBindSampler(0, 0);
		}, nullptr);
	}

	void ImGuiLayer::EndTextureSamplerPassthrough(ImDrawList* drawList)
	{
		if (s_IsVulkan) return;

		drawList->AddCallback(ImDrawCallback_ResetRenderState, nullptr);
	}

	void ImGuiLayer::SetDarkThemeColors()
	{
		auto& style = ImGui::GetStyle();

		// ── Layout & spacing ─────────────────────────────────────────────
		style.WindowMinSize = ImVec2(160.0f, 100.0f);
		style.WindowPadding = ImVec2(8.0f, 8.0f);
		style.FramePadding = ImVec2(8.0f, 5.0f);
		style.CellPadding = ImVec2(6.0f, 4.0f);
		style.ItemSpacing = ImVec2(8.0f, 6.0f);
		style.ItemInnerSpacing = ImVec2(6.0f, 4.0f);
		style.IndentSpacing = 18.0f;
		style.ScrollbarSize = 13.0f;
		style.GrabMinSize = 10.0f;

		// ── Rounding ─────────────────────────────────────────────────────
		style.WindowRounding = 6.0f;
		style.ChildRounding = 6.0f;
		style.FrameRounding = 6.0f;
		style.PopupRounding = 8.0f;
		style.ScrollbarRounding = 9.0f;
		style.GrabRounding = 3.0f;
		style.TabRounding = 6.0f;

		// ── Borders & alignment ──────────────────────────────────────────
		style.WindowBorderSize = 1.0f;
		style.ChildBorderSize = 1.0f;
		style.FrameBorderSize = 0.0f;
		style.PopupBorderSize = 1.0f;
		style.TabBorderSize = 0.0f;
		style.WindowTitleAlign = ImVec2(0.5f, 0.5f);
		style.ButtonTextAlign = ImVec2(0.5f, 0.5f);

		auto& colors = style.Colors;

		// Brand accent (waffle amber) + darker warm states.
		const ImVec4 accent = ImVec4{ 0.914f, 0.608f, 0.176f, 1.0f };
		const ImVec4 accentHovered = ImVec4{ 0.965f, 0.690f, 0.278f, 1.0f };
		const ImVec4 accentActive = ImVec4{ 0.835f, 0.540f, 0.145f, 1.0f };

		// ── Text ─────────────────────────────────────────────────────────
		colors[ImGuiCol_Text] = ImVec4{ 0.91f, 0.92f, 0.94f, 1.0f };
		colors[ImGuiCol_TextDisabled] = ImVec4{ 0.46f, 0.49f, 0.54f, 1.0f };
		colors[ImGuiCol_TextSelectedBg] = ImVec4{ accent.x, accent.y, accent.z, 0.30f };

		// ── Backgrounds (charcoal, slightly blue-tinted) ──────────────────
		colors[ImGuiCol_WindowBg] = ImVec4{ 0.086f, 0.094f, 0.114f, 1.0f };
		colors[ImGuiCol_ChildBg] = ImVec4{ 0.102f, 0.110f, 0.133f, 1.0f };
		colors[ImGuiCol_PopupBg] = ImVec4{ 0.118f, 0.128f, 0.153f, 0.985f };
		colors[ImGuiCol_MenuBarBg] = ImVec4{ 0.070f, 0.076f, 0.092f, 1.0f };
		colors[ImGuiCol_Border] = ImVec4{ 0.21f, 0.23f, 0.27f, 0.60f };
		colors[ImGuiCol_BorderShadow] = ImVec4{ 0.0f, 0.0f, 0.0f, 0.0f };
		colors[ImGuiCol_ModalWindowDimBg] = ImVec4{ 0.02f, 0.02f, 0.04f, 0.60f };

		// ── Headers (tree nodes, selectables, collapsing headers) ────────
		colors[ImGuiCol_Header] = ImVec4{ 0.155f, 0.168f, 0.195f, 1.0f };
		colors[ImGuiCol_HeaderHovered] = ImVec4{ 0.20f, 0.215f, 0.25f, 1.0f };
		colors[ImGuiCol_HeaderActive] = ImVec4{ 0.235f, 0.25f, 0.29f, 1.0f };

		// ── Buttons ──────────────────────────────────────────────────────
		colors[ImGuiCol_Button] = ImVec4{ 0.155f, 0.168f, 0.195f, 1.0f };
		colors[ImGuiCol_ButtonHovered] = ImVec4{ 0.21f, 0.225f, 0.26f, 1.0f };
		colors[ImGuiCol_ButtonActive] = ImVec4{ 0.24f, 0.255f, 0.30f, 1.0f };

		// ── Frames (inputs, drags, combos) ───────────────────────────────
		colors[ImGuiCol_FrameBg] = ImVec4{ 0.13f, 0.14f, 0.165f, 1.0f };
		colors[ImGuiCol_FrameBgHovered] = ImVec4{ 0.18f, 0.195f, 0.225f, 1.0f };
		colors[ImGuiCol_FrameBgActive] = ImVec4{ 0.20f, 0.215f, 0.25f, 1.0f };

		// ── Widgets ──────────────────────────────────────────────────────
		colors[ImGuiCol_CheckMark] = accent;
		colors[ImGuiCol_SliderGrab] = accent;
		colors[ImGuiCol_SliderGrabActive] = accentHovered;
		colors[ImGuiCol_DragDropTarget] = ImVec4{ accent.x, accent.y, accent.z, 0.90f };
		colors[ImGuiCol_NavHighlight] = accent;
		colors[ImGuiCol_NavWindowingHighlight] = ImVec4{ 1.0f, 1.0f, 1.0f, 0.7f };
		colors[ImGuiCol_NavWindowingDimBg] = ImVec4{ 0.2f, 0.2f, 0.2f, 0.5f };

		// ── Tabs ─────────────────────────────────────────────────────────
		colors[ImGuiCol_Tab] = ImVec4{ 0.110f, 0.120f, 0.143f, 1.0f };
		colors[ImGuiCol_TabHovered] = ImVec4{ 0.20f, 0.215f, 0.25f, 1.0f };
		colors[ImGuiCol_TabActive] = ImVec4{ 0.165f, 0.178f, 0.207f, 1.0f };
		colors[ImGuiCol_TabUnfocused] = ImVec4{ 0.110f, 0.120f, 0.143f, 1.0f };
		colors[ImGuiCol_TabUnfocusedActive] = ImVec4{ 0.150f, 0.162f, 0.190f, 1.0f };

		// ── Title bars ───────────────────────────────────────────────────
		colors[ImGuiCol_TitleBg] = ImVec4{ 0.070f, 0.076f, 0.092f, 1.0f };
		colors[ImGuiCol_TitleBgActive] = ImVec4{ 0.078f, 0.085f, 0.102f, 1.0f };
		colors[ImGuiCol_TitleBgCollapsed] = ImVec4{ 0.070f, 0.076f, 0.092f, 1.0f };

		// ── Separators & resize grips ────────────────────────────────────
		colors[ImGuiCol_Separator] = ImVec4{ 0.21f, 0.23f, 0.27f, 0.8f };
		colors[ImGuiCol_SeparatorHovered] = ImVec4{ 0.32f, 0.34f, 0.40f, 1.0f };
		colors[ImGuiCol_SeparatorActive] = accentActive;
		colors[ImGuiCol_ResizeGrip] = ImVec4{ 0.21f, 0.23f, 0.27f, 0.6f };
		colors[ImGuiCol_ResizeGripHovered] = ImVec4{ 0.32f, 0.34f, 0.40f, 0.8f };
		colors[ImGuiCol_ResizeGripActive] = accentActive;

		// ── Scrollbars ───────────────────────────────────────────────────
		colors[ImGuiCol_ScrollbarBg] = ImVec4{ 0.086f, 0.094f, 0.114f, 0.6f };
		colors[ImGuiCol_ScrollbarGrab] = ImVec4{ 0.20f, 0.215f, 0.25f, 1.0f };
		colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4{ 0.28f, 0.30f, 0.35f, 1.0f };
		colors[ImGuiCol_ScrollbarGrabActive] = accentActive;

		// ── Docking ──────────────────────────────────────────────────────
		colors[ImGuiCol_DockingPreview] = ImVec4{ accent.x, accent.y, accent.z, 0.45f };
		colors[ImGuiCol_DockingEmptyBg] = ImVec4{ 0.065f, 0.070f, 0.085f, 1.0f };

		// ── Tables & plots ───────────────────────────────────────────────
		colors[ImGuiCol_TableHeaderBg] = ImVec4{ 0.13f, 0.14f, 0.165f, 1.0f };
		colors[ImGuiCol_TableBorderStrong] = ImVec4{ 0.21f, 0.23f, 0.27f, 1.0f };
		colors[ImGuiCol_TableBorderLight] = ImVec4{ 0.17f, 0.185f, 0.215f, 1.0f };
		colors[ImGuiCol_TableRowBg] = ImVec4{ 0.0f, 0.0f, 0.0f, 0.0f };
		colors[ImGuiCol_TableRowBgAlt] = ImVec4{ 1.0f, 1.0f, 1.0f, 0.025f };
		colors[ImGuiCol_PlotLines] = accentHovered;
		colors[ImGuiCol_PlotLinesHovered] = ImVec4{ 0.45f, 0.62f, 1.0f, 1.0f };
		colors[ImGuiCol_PlotHistogram] = accent;
		colors[ImGuiCol_PlotHistogramHovered] = accentHovered;
	}
}