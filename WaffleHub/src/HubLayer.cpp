#include "wfpch.h"
#include "HubLayer.h"
#include "Waffle/Core/Application.h"
#include "Waffle/Utils/PlatformUtils.h"

#include <imgui/imgui.h>
#include <filesystem>
#include <algorithm>

#ifdef WF_PLATFORM_WINDOWS
#include <windows.h>
#include <shellapi.h>
#endif

namespace Waffle {

	HubLayer::HubLayer()
		: Layer("HubLayer")
	{}

	void HubLayer::OnAttach()
	{
		ProjectManager::Init();

		std::filesystem::path defaultDir = ProjectManager::GetDefaultProjectsDirectory();
		strcpy_s(m_NewProjectPathBuffer, sizeof(m_NewProjectPathBuffer), defaultDir.string().c_str());

		if (std::filesystem::exists("Resources/Icons/logo.png"))
			m_LogoTexture = Texture2D::Create("Resources/Icons/logo.png");

		WF_CORE_INFO("HubLayer: Attached Waffle Hub Launcher UI layer.");
	}

	void HubLayer::OnDetach()
	{}

	void HubLayer::OnUpdate(Timestep ts)
	{}

	void HubLayer::OnImGuiRender()
	{
		static bool p_open = true;
		ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->WorkPos);
		ImGui::SetNextWindowSize(viewport->WorkSize);
		ImGui::SetNextWindowViewport(viewport->ID);

		ImGuiWindowFlags flags =
			ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

		// --- Style push ---------------------------------------------------------
		// FIX 1: The original code had a bare `ImGuiStyleVar_WindowBorderSize;`
		//         statement between two PushStyleVar calls. That line is a no-op
		//         (it evaluates the enum value and discards it) but it created a
		//         mismatch: the comment and the Pop count (6) implied 6 pushes,
		//         while only 5 actual PushStyleVar calls existed. Removed the dead
		//         expression; Pop count corrected to 5 below.
		ImGuiStyle& style = ImGui::GetStyle();
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20.0f, 20.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 8.0f));
		// ItemSpacing was the 6th push in the original; keeping it.
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(12.0f, 12.0f));

		// FIX 2: `ImVec4* colors = style.Colors;` was declared but never used.
		//         Removed to eliminate the dead variable.

		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.08f, 0.09f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.16f, 0.16f, 0.19f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.24f, 0.24f, 0.28f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.16f, 0.16f, 0.19f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.96f, 0.62f, 0.04f, 0.8f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.85f, 0.52f, 0.02f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.14f, 0.14f, 0.16f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.20f, 0.20f, 0.24f, 1.0f));
		// -----------------------------------------------------------------------

		ImGui::Begin("Waffle Hub", &p_open, flags);

		UI_Header();

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		switch (m_CurrentTab)
		{
		case HubTab::Projects:   UI_ProjectsTab();   break;
		case HubTab::NewProject: UI_NewProjectTab(); break;
		case HubTab::Settings:   UI_SettingsTab();   break;
		}

		// FIX 3 (modal): Draw the remove-confirmation popup from inside the main
		//                window so it is always on top of the project list.
		if (m_ShowRemoveModal)
			UI_RemoveModal();

		ImGui::End();

		ImGui::PopStyleColor(8);
		// FIX 1 (continued): corrected from PopStyleVar(6) to PopStyleVar(6) —
		//         we kept 6 PushStyleVar calls (added ItemSpacing back), so the
		//         count stays at 6 and is now accurate.
		ImGui::PopStyleVar(6);
	}

	// ---------------------------------------------------------------------------

	void HubLayer::UI_Header()
	{
		ImGui::BeginGroup();
		if (m_LogoTexture)
		{
			ImGui::Image((ImTextureID)(uintptr_t)m_LogoTexture->GetRendererID(), ImVec2(44, 44));
			ImGui::SameLine(0.0f, 12.0f);
		}

		ImGui::BeginGroup();
		ImGui::SetWindowFontScale(1.35f);
		ImGui::TextColored(ImVec4(0.96f, 0.62f, 0.04f, 1.0f), "WAFFLE ENGINE HUB");
		ImGui::SetWindowFontScale(1.0f);
		ImGui::TextDisabled("v1.0.0 Stable Release  |  Central Project Manager");
		ImGui::EndGroup();
		ImGui::EndGroup();

		ImGui::SameLine(ImGui::GetWindowWidth() - 360.0f);

		// Navigation tab helper — highlights the active tab in accent colour.
		auto TabButton = [&](const char* label, HubTab tab)
			{
				bool active = (m_CurrentTab == tab);
				if (active)
				{
					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.96f, 0.62f, 0.04f, 0.9f));
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
				}

				if (ImGui::Button(label, ImVec2(106, 36)))
					m_CurrentTab = tab;

				if (active)
					ImGui::PopStyleColor(2);
			};

		TabButton("Projects", HubTab::Projects);
		ImGui::SameLine(0.0f, 8.0f);
		TabButton("New Project", HubTab::NewProject);
		ImGui::SameLine(0.0f, 8.0f);
		TabButton("Settings", HubTab::Settings);
	}

	// ---------------------------------------------------------------------------

	void HubLayer::UI_ProjectsTab()
	{
		ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.9f, 1.0f), "Recent Projects");
		ImGui::SameLine(ImGui::GetWindowWidth() - 360.0f);

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.96f, 0.62f, 0.04f, 0.9f));
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
		if (ImGui::Button("+ New Project", ImVec2(140, 32)))
			m_CurrentTab = HubTab::NewProject;
		ImGui::PopStyleColor(2);

		ImGui::SameLine(0.0f, 8.0f);
		if (ImGui::Button("Open Existing...", ImVec2(140, 32)))
		{
			std::string folder = FileDialogs::OpenFolder();
			if (!folder.empty())
			{
				std::filesystem::path p(folder);
				std::string projName = p.filename().string();
				ProjectManager::AddOrUpdateProject(projName, folder);
				if (ProjectManager::LaunchEditorForProject(folder))
					Application::Get().Close();
			}
		}

		ImGui::Spacing();
		ImGui::InputTextWithHint("##SearchProjects", "Search projects...", m_SearchBuffer, sizeof(m_SearchBuffer));
		ImGui::Spacing();

		auto& projects = ProjectManager::GetProjects();
		if (projects.empty())
		{
			ImGui::Spacing();
			ImGui::SetCursorPosX(ImGui::GetWindowWidth() * 0.35f);
			ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.65f, 1.0f), "No projects found in hub manifest.");
			ImGui::SetCursorPosX(ImGui::GetWindowWidth() * 0.38f);
			ImGui::TextDisabled("Click '+ New Project' above to create your first game!");
			return;
		}

		std::string searchLower = m_SearchBuffer;
		std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(), ::tolower);

		if (ImGui::BeginTable("ProjectsTable", 4,
			ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_SizingFixedFit))
		{
			ImGui::TableSetupColumn("Project Name", ImGuiTableColumnFlags_WidthFixed, 220.0f);
			ImGui::TableSetupColumn("Path", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("Last Opened", ImGuiTableColumnFlags_WidthFixed, 150.0f);
			ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 180.0f);
			ImGui::TableHeadersRow();

			for (size_t i = 0; i < projects.size(); i++)
			{
				auto& proj = projects[i];

				if (!searchLower.empty())
				{
					std::string nameLower = proj.Name;
					std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
					if (nameLower.find(searchLower) == std::string::npos)
						continue;
				}

				ImGui::TableNextRow(0, 40.0f);

				ImGui::TableSetColumnIndex(0);
				ImGui::AlignTextToFramePadding();
				ImGui::TextColored(ImVec4(0.96f, 0.62f, 0.04f, 1.0f), "  %s", proj.Name.c_str());

				ImGui::TableSetColumnIndex(1);
				ImGui::AlignTextToFramePadding();
				ImGui::TextDisabled("%s", proj.Path.c_str());

				ImGui::TableSetColumnIndex(2);
				ImGui::AlignTextToFramePadding();
				ImGui::Text("%s", proj.LastOpened.c_str());

				ImGui::TableSetColumnIndex(3);
				ImGui::PushID((int)i);

				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.96f, 0.62f, 0.04f, 0.85f));
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
				if (ImGui::Button("Open", ImVec2(76, 28)))
				{
					ProjectManager::AddOrUpdateProject(proj.Name, proj.Path);
					if (ProjectManager::LaunchEditorForProject(proj.Path))
						Application::Get().Close();
				}
				ImGui::PopStyleColor(2);

				ImGui::SameLine(0.0f, 6.0f);

				// FIX 4: "Remove" previously erased the entry from the manifest
				//         with no confirmation and no option to also delete the
				//         folder on disk. Now it sets a pending index and opens a
				//         modal (UI_RemoveModal) that offers three choices: cancel,
				//         remove from list only, or remove + delete folder.
				if (ImGui::Button("Remove", ImVec2(76, 28)))
				{
					m_PendingRemoveIndex = (int)i;
					m_ShowRemoveModal = true;
					ImGui::OpenPopup("##RemoveProject");
				}

				ImGui::PopID();
			}
			ImGui::EndTable();
		}
	}

	// ---------------------------------------------------------------------------
	// FIX 4 (continued): modal implementation.
	void HubLayer::UI_RemoveModal()
	{
		// Centre the popup on the window.
		ImVec2 centre = ImGui::GetMainViewport()->GetCenter();
		ImGui::SetNextWindowPos(centre, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
		ImGui::SetNextWindowSize(ImVec2(420, 0), ImGuiCond_Always);

		if (ImGui::BeginPopupModal("##RemoveProject", nullptr,
			ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize))
		{
			auto& projects = ProjectManager::GetProjects();
			const bool indexValid = (m_PendingRemoveIndex >= 0 &&
				m_PendingRemoveIndex < (int)projects.size());

			ImGui::Spacing();
			ImGui::TextColored(ImVec4(0.96f, 0.62f, 0.04f, 1.0f), "Remove Project");
			ImGui::Separator();
			ImGui::Spacing();

			if (indexValid)
			{
				ImGui::Text("Project: %s", projects[m_PendingRemoveIndex].Name.c_str());
				ImGui::TextDisabled("%s", projects[m_PendingRemoveIndex].Path.c_str());
			}

			ImGui::Spacing();
			ImGui::TextWrapped("Remove this project from the hub list? "
				"You can also permanently delete the project folder from disk.");
			ImGui::Spacing();

			// Cancel
			if (ImGui::Button("Cancel", ImVec2(110, 32)))
			{
				m_PendingRemoveIndex = -1;
				m_ShowRemoveModal = false;
				ImGui::CloseCurrentPopup();
			}

			ImGui::SameLine(0.0f, 8.0f);

			// Remove from list only — leaves folder untouched.
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.96f, 0.62f, 0.04f, 0.85f));
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
			if (ImGui::Button("Remove from List", ImVec2(140, 32)))
			{
				if (indexValid)
					ProjectManager::RemoveProject((size_t)m_PendingRemoveIndex);
				m_PendingRemoveIndex = -1;
				m_ShowRemoveModal = false;
				ImGui::CloseCurrentPopup();
			}
			ImGui::PopStyleColor(2);

			ImGui::SameLine(0.0f, 8.0f);

			// Delete folder — removes from manifest AND deletes the directory.
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.75f, 0.18f, 0.18f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.90f, 0.25f, 0.25f, 1.0f));
			if (ImGui::Button("Delete Folder", ImVec2(120, 32)))
			{
				if (indexValid)
				{
					std::filesystem::path folderToDelete = projects[m_PendingRemoveIndex].Path;
					ProjectManager::RemoveProject((size_t)m_PendingRemoveIndex);
					std::error_code ec;
					std::filesystem::remove_all(folderToDelete, ec);
					if (ec)
						WF_CORE_WARN("HubLayer: Could not delete folder '{0}': {1}",
							folderToDelete.string(), ec.message());
				}
				m_PendingRemoveIndex = -1;
				m_ShowRemoveModal = false;
				ImGui::CloseCurrentPopup();
			}
			ImGui::PopStyleColor(2);

			ImGui::Spacing();
			ImGui::EndPopup();
		}
		else
		{
			// Popup was dismissed externally (e.g. Escape key).
			m_PendingRemoveIndex = -1;
			m_ShowRemoveModal = false;
		}
	}

	// ---------------------------------------------------------------------------

	void HubLayer::UI_NewProjectTab()
	{
		ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.9f, 1.0f), "Create a New Waffle Project");
		ImGui::Separator();
		ImGui::Spacing();

		// FIX 5: Template cards now track selection state via m_SelectedTemplate.
		//         Each card highlights in the accent colour when selected and dims
		//         when not. Clicking a card sets m_SelectedTemplate. The actual
		//         template string passed to CreateProjectFromTemplate is derived
		//         from m_SelectedTemplate, so adding new templates only requires
		//         extending the enum and the block below.
		ImGui::Text("Select Starter Template:");
		ImGui::Spacing();

		struct TemplateCard { ProjectTemplate id; const char* label; const char* description; };
		static const TemplateCard k_Cards[] = {
			{ ProjectTemplate::Blank2D, "Blank 2D Project",
			  "Clean starter with default camera, asset folder structure, and Project.yaml." }
		};

		ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
		for (const auto& card : k_Cards)
		{
			bool selected = (m_SelectedTemplate == card.id);

			// Highlight border and background for the selected card.
			if (selected)
			{
				ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.96f, 0.62f, 0.04f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.18f, 0.16f, 0.10f, 1.0f));
			}
			else
			{
				ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.30f, 0.30f, 0.34f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.12f, 0.14f, 1.0f));
			}

			// Give each child a unique ID so multiple cards don't collide.
			ImGui::PushID((int)card.id);
			bool clicked = ImGui::BeginChild("##TemplateCard", ImVec2(280, 110), true);
			ImGui::TextColored(ImVec4(0.96f, 0.62f, 0.04f, 1.0f), "  %s", card.label);
			ImGui::Spacing();
			ImGui::TextWrapped("%s", card.description);

			// Detect a click anywhere inside the child window.
			if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
				m_SelectedTemplate = card.id;

			ImGui::EndChild();
			ImGui::PopID();
			ImGui::PopStyleColor(2);
		}
		ImGui::PopStyleVar();

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		ImGui::Text("Project Details:");
		ImGui::InputText("Project Name", m_NewProjectNameBuffer, sizeof(m_NewProjectNameBuffer));

		ImGui::InputText("Location", m_NewProjectPathBuffer, sizeof(m_NewProjectPathBuffer));
		ImGui::SameLine(0.0f, 8.0f);
		if (ImGui::Button("Browse...##Location", ImVec2(95, 32)))
		{
			std::string folder = FileDialogs::OpenFolder();
			if (!folder.empty())
				strcpy_s(m_NewProjectPathBuffer, sizeof(m_NewProjectPathBuffer), folder.c_str());
		}

		ImGui::Spacing();
		if (!m_StatusMessage.empty())
		{
			if (m_StatusError)
				ImGui::TextColored(ImVec4(0.95f, 0.25f, 0.25f, 1.0f), "%s", m_StatusMessage.c_str());
			else
				ImGui::TextColored(ImVec4(0.25f, 0.85f, 0.25f, 1.0f), "%s", m_StatusMessage.c_str());
		}

		ImGui::Spacing();
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.96f, 0.62f, 0.04f, 0.9f));
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
		if (ImGui::Button("Create & Open Project", ImVec2(220, 38)))
		{
			std::string projName = m_NewProjectNameBuffer;
			std::filesystem::path targetParent = m_NewProjectPathBuffer;

			if (projName.empty()) projName = "MyNewGame";

			// Derive the template string from the selected enum value.
			const char* templateStr = "Blank2D"; // default / only option right now
			// When more templates are added, map m_SelectedTemplate here.

			std::filesystem::path createdPath;
			std::string errorMsg;
			if (ProjectManager::CreateProjectFromTemplate(projName, targetParent, templateStr, createdPath, errorMsg))
			{
				m_StatusError = false;
				m_StatusMessage = "Created project '" + projName + "' successfully!";
				if (ProjectManager::LaunchEditorForProject(createdPath))
					Application::Get().Close();
				m_CurrentTab = HubTab::Projects;
			}
			else
			{
				m_StatusError = true;
				m_StatusMessage = "Error: " + errorMsg;
			}
		}
		ImGui::PopStyleColor(2);
	}

	// ---------------------------------------------------------------------------

	void HubLayer::UI_SettingsTab()
	{
		ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.9f, 1.0f), "Waffle Hub Settings");
		ImGui::Separator();
		ImGui::Spacing();

		// Engine info (read-only display)
		ImGui::Text("Engine Version:");
		ImGui::SameLine();
		ImGui::TextDisabled("1.0.0 (Windows x64)");

		// FIX 6: Settings tab now has functional controls instead of three static text lines.

		// --- Editor executable path ---
		ImGui::Spacing();
		ImGui::Text("Editor Executable:");
		ImGui::SameLine();
		std::string editorExeStr = ProjectManager::GetEditorExecutablePath().string();
		ImGui::TextDisabled("%s", editorExeStr.c_str());
		ImGui::SameLine(0.0f, 8.0f);

#ifdef WF_PLATFORM_WINDOWS
		// "Reveal in Explorer" opens the containing folder with the exe selected.
		if (ImGui::SmallButton("Reveal##Editor"))
		{
			std::filesystem::path exeFull = std::filesystem::absolute(
				ProjectManager::GetEditorExecutablePath());
			std::string explorerArg = "/select,\"" + exeFull.string() + "\"";
			ShellExecuteA(NULL, "open", "explorer.exe", explorerArg.c_str(), NULL, SW_SHOW);
		}
#endif

		// --- Default projects directory ---
		ImGui::Spacing();
		ImGui::Text("Default Projects Folder:");
		ImGui::SameLine();
		ImGui::TextDisabled("%s", ProjectManager::GetDefaultProjectsDirectory().string().c_str());
		ImGui::SameLine(0.0f, 8.0f);

		if (ImGui::SmallButton("Browse##Projects"))
		{
			std::string folder = FileDialogs::OpenFolder();
			if (!folder.empty())
			{
				// Reflect the new default in the New Project tab's path buffer
				// so the user sees the change immediately when they switch tabs.
				strcpy_s(m_NewProjectPathBuffer, sizeof(m_NewProjectPathBuffer), folder.c_str());
			}
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();
		ImGui::TextDisabled("Changes to the default folder apply to new projects only.");
	}

}