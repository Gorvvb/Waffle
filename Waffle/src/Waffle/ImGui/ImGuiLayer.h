#pragma once

#include "Waffle/Core/Layer.h"

#include "Waffle/Events/ApplicationEvent.h"
#include "Waffle/Events/KeyEvent.h"
#include "Waffle/Events/MouseEvent.h"

namespace Waffle {
	class ImGuiLayer : public Layer
	{
	private:
		float m_Time = 0.0f;
		bool m_BlockEvents = true;
	public:
		ImGuiLayer();
		~ImGuiLayer();

		virtual void OnAttach() override;
		virtual void OnDetach() override;
		virtual void OnEvent(Event& e) override;

		void Begin();
		void End();

		void BlockEvents(bool block) { m_BlockEvents = block; }
	};
}