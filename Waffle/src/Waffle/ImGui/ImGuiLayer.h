#pragma once

#include "Waffle/Core/Layer.h"

#include "Waffle/Events/ApplicationEvent.h"
#include "Waffle/Events/KeyEvent.h"
#include "Waffle/Events/MouseEvent.h"

struct ImDrawList;

namespace Waffle {
	class ImGuiLayer : public Layer
	{
	private:
		float m_Time = 0.0f;
		bool m_BlockEvents = true;
		uint32_t m_NearestSampler = 0;
	public:
		ImGuiLayer();
		~ImGuiLayer();

		virtual void OnAttach() override;
		virtual void OnDetach() override;
		virtual void OnEvent(Event& e) override;

		void Begin();
		void End();

		void BlockEvents(bool block) { m_BlockEvents = block; }

		void SetDarkThemeColors();

		static void BeginTextureSamplerPassthrough(ImDrawList* drawList);
		static void EndTextureSamplerPassthrough(ImDrawList* drawList);
	};
}