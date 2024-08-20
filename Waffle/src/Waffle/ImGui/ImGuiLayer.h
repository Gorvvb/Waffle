#pragma once

#include "Waffle/Layer.h"

#include "Waffle/Events/ApplicationEvent.h"
#include "Waffle/Events/KeyEvent.h"
#include "Waffle/Events/MouseEvent.h"

namespace Waffle {
	class WAFFLE_API ImGuiLayer : public Layer
	{
	private:
		float m_Time = 0.0f;
	public:
		ImGuiLayer();
		~ImGuiLayer();

		virtual void OnAttach() override;
		virtual void OnDetach() override;
		virtual void OnImGuiRender() override;

		void Begin();
		void End();
	};
}