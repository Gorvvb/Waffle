#pragma once
#include <Waffle.h>
#include <vector>
#include <string>

namespace Waffle {

	class RuntimeLayer : public Layer
	{
	public:
		RuntimeLayer();
		virtual void OnAttach() override;
		virtual void OnDetach() override;
		virtual void OnUpdate(Timestep ts) override;
		virtual void OnImGuiRender() override;
		virtual void OnEvent(Event& e) override;

	private:
		bool OnWindowResize(WindowResizeEvent& e);
		void LoadScene(int index);

		Ref<Scene> m_Scene;
		std::filesystem::path m_ScenePath;
		std::vector<std::string> m_SceneList;
		int m_CurrentSceneIndex = 0;
		float m_Gravity = -9.8f;
	};

}