#pragma once

#include "Waffle/Core/Ref.h"
#include "Waffle/Events/Event.h"
#include "Waffle/Scene/Scene.h"
#include <string>
#include <unordered_map>
#include <memory>

namespace Waffle {

	class EditorPanel
	{
	public:
		virtual ~EditorPanel() = default;

		virtual void OnImGuiRender() = 0;
		virtual void OnEvent(Event& e) {}
		virtual void SetContext(const Ref<Scene>& scene) {}
	};

	struct PanelData
	{
		std::string Name;
		std::shared_ptr<EditorPanel> Panel;
		bool IsOpen = true;
	};

	class EditorPanelManager
	{
	public:
		EditorPanelManager() = default;
		~EditorPanelManager() = default;

		template<typename T, typename... Args>
		std::shared_ptr<T> AddPanel(const std::string& name, bool defaultOpen, Args&&... args)
		{
			auto panel = std::make_shared<T>(std::forward<Args>(args)...);
			m_Panels[name] = { name, panel, defaultOpen };
			return panel;
		}

		void OnImGuiRender();
		void OnEvent(Event& e);
		void SetContext(const Ref<Scene>& scene);

		bool* GetPanelOpenFlag(const std::string& name);
		std::shared_ptr<EditorPanel> GetPanel(const std::string& name);

		const std::unordered_map<std::string, PanelData>& GetPanels() const { return m_Panels; }

	private:
		std::unordered_map<std::string, PanelData> m_Panels;
	};

}
