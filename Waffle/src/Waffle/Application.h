#pragma once

#include "Core.h"

#include "Window.h"
#include "Waffle/LayerStack.h"
#include "Waffle/Events/Event.h"
#include "Waffle/Events/ApplicationEvent.h"

#include "Waffle/ImGui/ImGuiLayer.h"

#include "Waffle/Renderer/Shader.h"
#include "Waffle/Renderer/Buffer.h"
#include "Waffle/Renderer/VertexArray.h"

#include "Waffle/Renderer/OrthographicCamera.h"

namespace Waffle {

	class WAFFLE_API Application
	{
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

		std::unique_ptr<Window> m_Window;
		ImGuiLayer* m_ImGuiLayer;
		bool m_Running = true;
		LayerStack m_LayerStack;

		std::shared_ptr<Shader> m_Shader;
		std::shared_ptr<VertexArray> m_VertexArray;

		std::shared_ptr<Shader> m_BlueShader;
		std::shared_ptr<VertexArray> m_SquareVA;

		OrthographicCamera m_Camera;
	private:
		static Application* s_Instance;
	};

	// Remember to define in client
	Application* CreateApplication();
}