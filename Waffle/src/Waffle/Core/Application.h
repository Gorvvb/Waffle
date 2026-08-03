#pragma once

#include "Waffle/Core/Base.h"

#include "Waffle/Core/Window.h"
#include "Waffle/Core/LayerStack.h"
#include "Waffle/Events/Event.h"
#include "Waffle/Events/ApplicationEvent.h"

#include "Waffle/Core/Timestep.h"

#include "Waffle/ImGui/ImGuiLayer.h"

namespace Waffle {

	struct ApplicationCommandLineArgs
	{
		int Count = 0;
		char** Args = nullptr;

		const char* operator[](int index) const
		{
			WF_CORE_ASSERT(index < Count);
			return Args[index];
		}
	};

	struct ApplicationSpecification
	{
		std::string Name = "Waffle Application";
		std::string IconPath = "";
		std::string WorkingDirectory;
		ApplicationCommandLineArgs CommandLineArgs;
	};

	class Application
	{
	private:
		Scope<Window> m_Window;
		ImGuiLayer* m_ImGuiLayer;
		bool m_Running = true;
		bool m_Minimized = false;
		LayerStack m_LayerStack;

		float m_lastFrameTime = 0.0f;

		ApplicationSpecification m_Specification;

		static Application* s_Instance;
		friend int main(int argc, char** argv);
	public:
		Application(const ApplicationSpecification& specification);
		virtual ~Application();

		void Run();

		void OnEvent(Event& e);

		void PushLayer(Layer* layer);
		void PushOverlay(Layer* layer);

		Window& GetWindow() { return *m_Window; }

		void Close();

		ImGuiLayer* GetImGuiLayer() { return m_ImGuiLayer; }

		float GetFixedTimestep() const { return m_FixedTimestep; }
		void SetFixedTimestep(float seconds) { m_FixedTimestep = seconds; }

		float GetTimeScale() const { return m_TimeScale; }
		void SetTimeScale(float scale) { m_TimeScale = scale; }

		static Application& Get() { return *s_Instance; }

		const ApplicationSpecification& GetSpecification() const { return m_Specification; }
	private:
		bool OnWindowClose(WindowCloseEvent& e);
		bool OnWindowResize(WindowResizeEvent& e);

		float m_FixedTimestep = 1.0f / 60.0f;
		float m_Accumulator = 0.0f;
		float m_TimeScale = 1.0f;
	};

	// Remember to define in client
	Application* CreateApplication(ApplicationCommandLineArgs args);
}