#pragma once

#include "Waffle/Core/Layer.h"
#include "Waffle/Renderer/Texture.h"
#include "ProjectManager.h"

namespace Waffle {

	class HubLayer : public Layer
	{
	public:
		HubLayer();
		virtual ~HubLayer() = default;

		virtual void OnAttach() override;
		virtual void OnDetach() override;
		virtual void OnUpdate(Timestep ts) override;
		virtual void OnImGuiRender() override;

	private:
		void UI_Header();
		void UI_ProjectsTab();
		void UI_NewProjectTab();
		void UI_SettingsTab();
		void UI_RemoveModal();

	private:
		enum class HubTab
		{
			Projects = 0,
			NewProject,
			Settings
		};

		// Template choices for the New Project tab.
		// Extend this enum and the card-rendering loop when more templates are added.
		enum class ProjectTemplate
		{
			Blank2D = 0
		};

		HubTab m_CurrentTab = HubTab::Projects;

		Ref<Texture2D> m_LogoTexture;

		char m_SearchBuffer[128]         = { 0 };
		char m_NewProjectNameBuffer[128] = "MyNewGame";
		char m_NewProjectPathBuffer[256] = { 0 };

		std::string m_StatusMessage = "";
		bool        m_StatusError   = false;

		// Tracks which starter template the user has selected on the New Project tab.
		ProjectTemplate m_SelectedTemplate = ProjectTemplate::Blank2D;

		// Remove-confirmation modal state.
		// -1 means no removal is pending.
		int  m_PendingRemoveIndex = -1;
		bool m_ShowRemoveModal    = false;
	};

}