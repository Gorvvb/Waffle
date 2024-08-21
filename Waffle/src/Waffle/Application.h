#pragma once

#include "Core.h"

#include "Window.h"
#include "Waffle/LayerStack.h"
#include "Waffle/Events/Event.h"
#include "Waffle/Events/ApplicationEvent.h"

#include "Waffle/ImGui/ImGuiLayer.h"

#include "Waffle/Renderer/Shader.h"
#include "Waffle/Renderer/Buffer.h"

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

		unsigned int m_VertexArray;
		std::unique_ptr<Shader> m_Shader;
		std::unique_ptr<VertexBuffer> m_VertexBuffer;
		std::unique_ptr<IndexBuffer> m_IndexBuffer;
	private:
		static Application* s_Instance;
	};

	// Remember to define in client
	Application* CreateApplication();
}