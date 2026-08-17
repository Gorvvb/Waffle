#include "EditorPanelManager.h"

namespace Waffle {

	void EditorPanelManager::OnImGuiRender()
	{
		for (auto& [name, panelData] : m_Panels)
		{
			if (panelData.IsOpen && panelData.Panel)
			{
				panelData.Panel->OnImGuiRender();
			}
		}
	}

	void EditorPanelManager::OnEvent(Event& e)
	{
		for (auto& [name, panelData] : m_Panels)
		{
			if (panelData.IsOpen && panelData.Panel)
			{
				panelData.Panel->OnEvent(e);
			}
		}
	}

	void EditorPanelManager::SetContext(const Ref<Scene>& scene)
	{
		for (auto& [name, panelData] : m_Panels)
		{
			if (panelData.Panel)
			{
				panelData.Panel->SetContext(scene);
			}
		}
	}

	bool* EditorPanelManager::GetPanelOpenFlag(const std::string& name)
	{
		auto it = m_Panels.find(name);
		if (it != m_Panels.end())
			return &it->second.IsOpen;
		return nullptr;
	}

	std::shared_ptr<EditorPanel> EditorPanelManager::GetPanel(const std::string& name)
	{
		auto it = m_Panels.find(name);
		if (it != m_Panels.end())
			return it->second.Panel;
		return nullptr;
	}

}
