#pragma once

#include "Waffle.h"

namespace Waffle {

	class RuntimeLayer : public Layer
	{
	public:
		RuntimeLayer();
		virtual ~RuntimeLayer() = default;

		virtual void OnAttach() override;
		virtual void OnDetach() override;

		virtual void OnUpdate(Timestep dt) override;
		virtual void OnImGuiRender() override;
		virtual void OnEvent(Event& e) override;

	private:
		bool OnWindowResize(WindowResizeEvent& e);

	private:
		Ref<Scene> m_Scene;
		std::filesystem::path m_ScenePath;
	};

}
