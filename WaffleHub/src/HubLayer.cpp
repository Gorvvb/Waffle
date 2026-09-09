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

	// =========================================================================
	// Design system - one cohesive palette so every tab shares the same voice.
	// =========================================================================
	namespace HubStyle {

		// Surface colors (charcoal, slightly blue-tinted - modern IDE feel).
		const ImVec4 Bg        = ImVec4(0.055f, 0.062f, 0.078f, 1.00f); // window
		const ImVec4 Panel     = ImVec4(0.086f, 0.094f, 0.114f, 1.00f); // cards / inputs
		const ImVec4 PanelAlt  = ImVec4(0.118f, 0.128f, 0.155f, 1.00f); // hovered card
		const ImVec4 Border    = ImVec4(0.16f,  0.17f,  0.21f,  0.60f);
		const ImVec4 BorderLit = ImVec4(0.26f,  0.28f,  0.34f,  0.90f);

		// Text.
		const ImVec4 Text      = ImVec4(0.91f,  0.92f,  0.94f,  1.00f);
		const ImVec4 TextDim   = ImVec4(0.56f,  0.59f,  0.64f,  1.00f);
		const ImVec4 TextFaint = ImVec4(0.38f,  0.41f,  0.46f,  1.00f);

		// Brand accent (refined waffle amber) + states.
		const ImVec4 Accent      = ImVec4(0.914f, 0.608f, 0.176f, 1.00f);
		const ImVec4 AccentHover = ImVec4(0.965f, 0.690f, 0.278f, 1.00f);
		const ImVec4 AccentWash  = ImVec4(0.914f, 0.608f, 0.176f, 0.10f); // selection wash
		const ImVec4 OnAccent    = ImVec4(0.10f,  0.08f,  0.04f,  1.00f); // text on accent

		// Semantic.
		const ImVec4 Danger      = ImVec4(0.72f, 0.24f, 0.24f, 1.00f);
		const ImVec4 DangerHover = ImVec4(0.88f, 0.32f, 0.32f, 1.00f);
		const ImVec4 Success     = ImVec4(0.32f, 0.80f, 0.44f, 1.00f);

		const float  Rounding    = 8.0f;
		const float  RoundingBig = 12.0f;
	}

	// =========================================================================
	// Small widget helpers built on the palette above.
	// =========================================================================
	static bool HubAccentButton(const char* label, const ImVec2& size)
	{
		ImGui::PushStyleColor(ImGuiCol_Button, HubStyle::Accent);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, HubStyle::AccentHover);
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, HubStyle::AccentHover);
		ImGui::PushStyleColor(ImGuiCol_Text, HubStyle::OnAccent);
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, HubStyle::Rounding);
		bool clicked = ImGui::Button(label, size);
		ImGui::PopStyleVar();
		ImGui::PopStyleColor(4);
		return clicked;
	}

	static bool HubGhostButton(const char* label, const ImVec2& size)
	{
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, HubStyle::PanelAlt);
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, HubStyle::Border);
		ImGui::PushStyleColor(ImGuiCol_Text, HubStyle::TextDim);
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, HubStyle::Rounding);
		ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
		ImGui::PushStyleColor(ImGuiCol_Border, HubStyle::Border);
		bool clicked = ImGui::Button(label, size);
		ImGui::PopStyleColor(5);
		ImGui::PopStyleVar(2);
		return clicked;
	}

	static bool HubDangerButton(const char* label, const ImVec2& size)
	{
		ImGui::PushStyleColor(ImGuiCol_Button, HubStyle::Danger);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, HubStyle::DangerHover);
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, HubStyle::DangerHover);
		ImGui::PushStyleColor(ImGuiCol_Text, HubStyle::Text);
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, HubStyle::Rounding);
		bool clicked = ImGui::Button(label, size);
		ImGui::PopStyleVar();
		ImGui::PopStyleColor(4);
		return clicked;
	}

	// Rounded section heading: small caps-ish label + thin rule.
	static void HubSectionLabel(const char* label, float afterGap = 6.0f)
	{
		ImGui::Dummy(ImVec2(0.0f, 4.0f));
		ImGui::PushStyleColor(ImGuiCol_Text, HubStyle::TextDim);
		ImGui::SetWindowFontScale(0.92f);
		ImGui::TextUnformatted(label);
		ImGui::SetWindowFontScale(1.0f);
		ImGui::PopStyleColor();

		// Thin accent rule under the heading.
		ImVec2 p = ImGui::GetCursorScreenPos();
		ImVec2 w = ImGui::GetContentRegionAvail();
		ImGui::GetWindowDrawList()->AddRectFilled(p, ImVec2(p.x + w.x, p.y + 1.0f),
			ImGui::ColorConvertFloat4ToU32(HubStyle::Border), 0.0f);
		ImGui::Dummy(ImVec2(0.0f, 3.0f + afterGap));
	}

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
			ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
			ImGuiWindowFlags_NoScrollbar;

		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(28.0f, 24.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, HubStyle::Rounding);
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12.0f, 9.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10.0f, 10.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding, HubStyle::Rounding);
		ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, HubStyle::RoundingBig);
		ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarRounding, HubStyle::RoundingBig);
		ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 12.0f);

		ImGui::PushStyleColor(ImGuiCol_WindowBg, HubStyle::Bg);
		ImGui::PushStyleColor(ImGuiCol_Text, HubStyle::Text);
		ImGui::PushStyleColor(ImGuiCol_TextDisabled, HubStyle::TextFaint);
		ImGui::PushStyleColor(ImGuiCol_FrameBg, HubStyle::Panel);
		ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, HubStyle::PanelAlt);
		ImGui::PushStyleColor(ImGuiCol_FrameBgActive, HubStyle::PanelAlt);
		ImGui::PushStyleColor(ImGuiCol_Border, HubStyle::Border);
		ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, ImVec4(0, 0, 0, 0));
		ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, HubStyle::Border);
		ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, HubStyle::BorderLit);
		ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, HubStyle::Accent);
		ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, ImVec4(0.02f, 0.02f, 0.04f, 0.65f));
		ImGui::PushStyleColor(ImGuiCol_PopupBg, HubStyle::Panel);

		ImGui::Begin("Waffle Hub", &p_open, flags);

		UI_Header();

		// Content area inside its own subtle panel.
		ImVec2 contentSize = ImGui::GetContentRegionAvail();
		ImGui::PushStyleColor(ImGuiCol_ChildBg, HubStyle::Panel);
		ImGui::BeginChild("##Content", contentSize, ImGuiChildFlags_Borders,
			ImGuiWindowFlags_None);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24.0f, 20.0f));

		switch (m_CurrentTab)
		{
		case HubTab::Projects:   UI_ProjectsTab();   break;
		case HubTab::NewProject: UI_NewProjectTab(); break;
		case HubTab::Settings:   UI_SettingsTab();   break;
		}

		ImGui::PopStyleVar();
		ImGui::EndChild();
		ImGui::PopStyleColor();

		if (m_ShowRemoveModal)
			UI_RemoveModal();

		ImGui::End();

		ImGui::PopStyleColor(13);
		ImGui::PopStyleVar(11);
	}

	// ---------------------------------------------------------------------------

	void HubLayer::UI_Header()
	{
		ImGui::BeginGroup();
		if (m_LogoTexture)
		{
			// Soft rounded badge behind the logo.
			ImVec2 p = ImGui::GetCursorScreenPos();
			ImGui::GetWindowDrawList()->AddRectFilled(p, ImVec2(p.x + 52, p.y + 52),
				ImGui::ColorConvertFloat4ToU32(HubStyle::Panel), HubStyle::RoundingBig);
			ImGui::GetWindowDrawList()->AddRect(p, ImVec2(p.x + 52, p.y + 52),
				ImGui::ColorConvertFloat4ToU32(HubStyle::Border), HubStyle::RoundingBig);
			{
				ImVec2 cp = ImGui::GetCursorPos();
				ImGui::SetCursorPos(ImVec2(cp.x + 9.0f, cp.y + 9.0f));
				ImGui::Image((ImTextureID)(uintptr_t)m_LogoTexture->GetRendererID(), ImVec2(34, 34));
				ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 9.0f);
			}
			ImGui::SameLine(0.0f, 16.0f);
		}

		ImGui::BeginGroup();
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 3));
		ImGui::SetWindowFontScale(1.55f);
		ImGui::TextColored(HubStyle::Text, "WAFFLE");
		ImGui::SameLine();
		ImGui::TextColored(HubStyle::Accent, "HUB");
		ImGui::SetWindowFontScale(1.0f);
		ImGui::TextColored(HubStyle::TextFaint, "Launch, create and manage your games");
		ImGui::PopStyleVar();
		ImGui::EndGroup();
		ImGui::EndGroup();

		// Right-aligned segmented tab bar on the same row as the wordmark.
		const float tabW = 118.0f, tabH = 34.0f;
		const float totalW = 3.0f * tabW + 2.0f * 8.0f;

		auto TabButton = [&](const char* label, HubTab tab)
		{
			bool active = (m_CurrentTab == tab);
			if (active)
			{
				ImGui::PushStyleColor(ImGuiCol_Button, HubStyle::AccentWash);
				ImGui::PushStyleColor(ImGuiCol_Text, HubStyle::Accent);
				ImGui::PushStyleColor(ImGuiCol_Border, HubStyle::Accent);
				ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
			}
			else
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
				ImGui::PushStyleColor(ImGuiCol_Text, HubStyle::TextDim);
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, HubStyle::PanelAlt);
				ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
			}
			if (ImGui::Button(label, ImVec2(tabW, tabH)))
				m_CurrentTab = tab;
			ImGui::PopStyleVar();
			ImGui::PopStyleColor(3);
		};

		ImGui::SameLine();
		ImGui::SetCursorPosX(ImGui::GetWindowWidth() - ImGui::GetStyle().WindowPadding.x - totalW - 4.0f);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 8.0f);
		TabButton("Projects", HubTab::Projects);
		ImGui::SameLine(0.0f, 8.0f);
		TabButton("New Project", HubTab::NewProject);
		ImGui::SameLine(0.0f, 8.0f);
		TabButton("Settings", HubTab::Settings);

		ImGui::Dummy(ImVec2(0.0f, 16.0f));
	}

	// ---------------------------------------------------------------------------

	void HubLayer::UI_ProjectsTab()
	{
		// --- Toolbar: search + primary actions ------------------------------
		float searchW = ImGui::GetContentRegionAvail().x - 320.0f;
		if (searchW < 220.0f) searchW = 220.0f;
		ImGui::SetNextItemWidth(searchW);
		ImGui::InputTextWithHint("##SearchProjects", "Search projects...",
			m_SearchBuffer, sizeof(m_SearchBuffer));

		ImGui::SameLine();
		if (HubGhostButton("Open Existing...", ImVec2(150.0f, 37.0f)))
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
		ImGui::SameLine();
		if (HubAccentButton("  +  New Project", ImVec2(152.0f, 37.0f)))
			m_CurrentTab = HubTab::NewProject;

		auto& projects = ProjectManager::GetProjects();

		// Filter first (also drives the count in the heading).
		std::string searchLower = m_SearchBuffer;
		std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(), ::tolower);
		std::vector<size_t> visible;
		for (size_t i = 0; i < projects.size(); i++)
		{
			if (searchLower.empty())
			{
				visible.push_back(i);
				continue;
			}
			std::string nameLower = projects[i].Name;
			std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
			if (nameLower.find(searchLower) != std::string::npos)
				visible.push_back(i);
		}

		char heading[96];
		snprintf(heading, sizeof(heading), "PROJECTS   %zu", visible.size());
		HubSectionLabel(heading);

		if (visible.empty())
		{
			// Centered empty state.
			ImVec2 region = ImGui::GetContentRegionAvail();
			const char* line1 = projects.empty()
				? "No projects yet" : "Nothing matches your search";
			const char* line2 = projects.empty()
				? "Create your first game with + New Project" : "Try a different name";
			ImVec2 s1 = ImGui::CalcTextSize(line1), s2 = ImGui::CalcTextSize(line2);
			ImGui::Dummy(ImVec2(0, region.y * 0.3f));
			ImGui::SetCursorPosX((region.x - s1.x) * 0.5f);
			ImGui::SetWindowFontScale(1.25f);
			ImGui::TextColored(HubStyle::TextDim, "%s", line1);
			ImGui::SetWindowFontScale(1.0f);
			ImGui::SetCursorPosX((region.x - s2.x) * 0.5f);
			ImGui::TextColored(HubStyle::TextFaint, "%s", line2);
			return;
		}

		// --- Project cards ---------------------------------------------------
		const float cardH = 84.0f;
		for (size_t vi = 0; vi < visible.size(); vi++)
		{
			size_t i = visible[vi];
			auto& proj = projects[i];

			ImGui::PushID((int)i);

			float cardW = ImGui::GetContentRegionAvail().x;
			ImGui::PushStyleColor(ImGuiCol_ChildBg, HubStyle::Panel);
			ImGui::PushStyleColor(ImGuiCol_Border, HubStyle::Border);
			ImGui::BeginChild("##ProjectCard", ImVec2(cardW, cardH), ImGuiChildFlags_Borders);

			bool hovered = ImGui::IsWindowHovered();
			ImDrawList* dl = ImGui::GetWindowDrawList();
			ImVec2 pad = ImGui::GetStyle().WindowPadding;
			ImVec2 csp = ImGui::GetCursorScreenPos();
			ImVec2 cmin = ImVec2(csp.x - pad.x, csp.y - pad.y);
			ImVec2 cmax = ImVec2(cmin.x + cardW, cmin.y + cardH);

			// Left accent stripe + hover wash.
			dl->AddRectFilled(ImVec2(cmin.x + 1, cmin.y + 1), ImVec2(cmin.x + 5.0f, cmax.y - 1),
				ImGui::ColorConvertFloat4ToU32(hovered ? HubStyle::Accent : HubStyle::BorderLit));
			if (hovered)
				dl->AddRectFilled(ImVec2(cmin.x + 5, cmin.y + 1), ImVec2(cmax.x - 1, cmax.y - 1),
					ImGui::ColorConvertFloat4ToU32(HubStyle::PanelAlt), HubStyle::Rounding);

			// Title + path, left column.
			ImGui::SetCursorPos(ImVec2(20.0f, 14.0f));
			ImGui::SetWindowFontScale(1.12f);
			ImGui::TextColored(HubStyle::Text, "%s", proj.Name.c_str());
			ImGui::SetWindowFontScale(1.0f);
			ImGui::SetCursorPos(ImVec2(20.0f, 44.0f));
			if (proj.Missing)
			{
				ImGui::TextColored(HubStyle::DangerHover, "(folder missing)  ");
				ImGui::SameLine(0.0f, 6.0f);
			}
			ImGui::TextColored(HubStyle::TextFaint, "%s", proj.Path.c_str());

			// Right cluster: last opened + actions, vertically centered.
			ImGui::SetCursorPos(ImVec2(cardW - 340.0f, 25.0f));
			ImGui::TextColored(HubStyle::TextFaint, "%s", proj.LastOpened.c_str());

			ImGui::SetCursorPos(ImVec2(cardW - 206.0f, (cardH - 34.0f) * 0.5f));
			if (HubAccentButton("Open", ImVec2(92.0f, 34.0f)))
			{
				ProjectManager::AddOrUpdateProject(proj.Name, proj.Path);
				if (ProjectManager::LaunchEditorForProject(proj.Path))
					Application::Get().Close();
			}
			ImGui::SetCursorPos(ImVec2(cardW - 106.0f, (cardH - 34.0f) * 0.5f));
			if (HubGhostButton("Remove", ImVec2(92.0f, 34.0f)))
			{
				m_PendingRemoveIndex = (int)i;
				m_ShowRemoveModal = true;
			}

			ImGui::EndChild();
			ImGui::PopStyleColor(2);
			ImGui::PopID();

			ImGui::Spacing();
		}

		// The popup must be OPENED at window scope - the same ID scope the
		// modal's BeginPopupModal uses (opening it inside a per-card PushID
		// registers it under a different id and BeginPopupModal never matches).
		if (m_ShowRemoveModal)
		{
			ImGui::OpenPopup("##RemoveProject");
			m_ShowRemoveModal = false;
		}
	}

	// ---------------------------------------------------------------------------

	void HubLayer::UI_RemoveModal()
	{
		ImVec2 centre = ImGui::GetMainViewport()->GetCenter();
		ImGui::SetNextWindowPos(centre, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
		ImGui::SetNextWindowSize(ImVec2(460, 0), ImGuiCond_Always);

		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, HubStyle::RoundingBig);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24, 22));
		if (ImGui::BeginPopupModal("##RemoveProject", nullptr,
			ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoMove))
		{
			auto& projects = ProjectManager::GetProjects();
			const bool indexValid = (m_PendingRemoveIndex >= 0 &&
				m_PendingRemoveIndex < (int)projects.size());

			ImGui::SetWindowFontScale(1.2f);
			ImGui::TextColored(HubStyle::Text, "Remove project");
			ImGui::SetWindowFontScale(1.0f);
			ImGui::Spacing();

			if (indexValid)
			{
				ImGui::TextColored(HubStyle::Text, "%s", projects[m_PendingRemoveIndex].Name.c_str());
				ImGui::TextColored(HubStyle::TextFaint, "%s", projects[m_PendingRemoveIndex].Path.c_str());
			}
			ImGui::Spacing();
			ImGui::TextColored(HubStyle::TextDim,
				"Remove this project from the hub list?\nYou can also permanently delete the folder from disk.");

			ImGui::Spacing();
			ImGui::Spacing();

			if (HubGhostButton("Cancel", ImVec2(110, 36)))
			{
				m_PendingRemoveIndex = -1;
				m_ShowRemoveModal = false;
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine(0.0f, 8.0f);
			if (HubDangerButton("Delete Folder", ImVec2(140, 36)))
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
			ImGui::SameLine(0.0f, 8.0f);
			if (HubAccentButton("Remove from List", ImVec2(160, 36)))
			{
				if (indexValid)
					ProjectManager::RemoveProject((size_t)m_PendingRemoveIndex);
				m_PendingRemoveIndex = -1;
				m_ShowRemoveModal = false;
				ImGui::CloseCurrentPopup();
			}

			ImGui::Spacing();
			ImGui::EndPopup();
		}
		else
		{
			m_PendingRemoveIndex = -1;
			m_ShowRemoveModal = false;
		}
		ImGui::PopStyleVar(2);
	}

	// ---------------------------------------------------------------------------

	void HubLayer::UI_NewProjectTab()
	{
		HubSectionLabel("STARTER TEMPLATE");

		struct TemplateCard { ProjectTemplate id; const char* label; const char* blurb; };
		static const TemplateCard k_Cards[] = {
			{ ProjectTemplate::Blank2D, "Blank 2D Project",
			  "Clean starter with a default camera, prefab and asset folder structure. Perfect base for any 2D game." }
		};

		for (const auto& card : k_Cards)
		{
			bool selected = (m_SelectedTemplate == card.id);
			ImGui::PushID((int)card.id);

			ImGui::PushStyleColor(ImGuiCol_ChildBg, selected ? HubStyle::AccentWash : HubStyle::Panel);
			ImGui::PushStyleColor(ImGuiCol_Border, selected ? HubStyle::Accent : HubStyle::Border);

			ImGui::BeginChild("##TemplateCard", ImVec2(0.0f, 96.0f), ImGuiChildFlags_Borders);
			{
				// Selection dot + title.
				ImVec2 p = ImGui::GetCursorScreenPos();
				ImDrawList* dl = ImGui::GetWindowDrawList();
				dl->AddCircleFilled(ImVec2(p.x + 8, p.y + 14),
					6.0f, ImGui::ColorConvertFloat4ToU32(selected ? HubStyle::Accent : HubStyle::BorderLit));
				ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 24.0f);
				ImGui::TextColored(selected ? HubStyle::Accent : HubStyle::Text, "%s", card.label);
				ImGui::Spacing();
				ImGui::PushStyleColor(ImGuiCol_Text, HubStyle::TextDim);
				ImGui::TextWrapped("%s", card.blurb);
				ImGui::PopStyleColor();
			}
			if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) &&
				ImGui::IsMouseClicked(ImGuiMouseButton_Left))
			{
				m_SelectedTemplate = card.id;
			}
			ImGui::EndChild();
			ImGui::PopStyleColor(2);
			ImGui::PopID();
			ImGui::Spacing();
		}

		HubSectionLabel("PROJECT DETAILS");

		const float labelW = 110.0f;
		ImGui::TextColored(HubStyle::TextDim, "Name");
		ImGui::SameLine(labelW);
		ImGui::SetNextItemWidth(-1);
		ImGui::InputTextWithHint("##ProjName", "My awesome game", m_NewProjectNameBuffer, sizeof(m_NewProjectNameBuffer));

		ImGui::Spacing();
		ImGui::TextColored(HubStyle::TextDim, "Location");
		ImGui::SameLine(labelW);
		ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 110.0f);
		ImGui::InputText("##ProjPath", m_NewProjectPathBuffer, sizeof(m_NewProjectPathBuffer));
		ImGui::SameLine(0.0f, 8.0f);
		if (HubGhostButton("Browse", ImVec2(96.0f, 37.0f)))
		{
			std::string folder = FileDialogs::OpenFolder();
			if (!folder.empty())
				strcpy_s(m_NewProjectPathBuffer, sizeof(m_NewProjectPathBuffer), folder.c_str());
		}

		if (!m_StatusMessage.empty())
		{
			ImGui::Spacing();
			ImGui::TextColored(m_StatusError ? HubStyle::DangerHover : HubStyle::Success,
				"%s", m_StatusMessage.c_str());
		}

		ImGui::Spacing();
		ImGui::Spacing();
		if (HubAccentButton("Create & Open Project", ImVec2(230.0f, 42.0f)))
		{
			std::string projName = m_NewProjectNameBuffer;
			std::filesystem::path targetParent = m_NewProjectPathBuffer;

			if (projName.empty()) projName = "MyNewGame";

			const char* templateStr = "Blank2D"; // map m_SelectedTemplate here when more exist

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
	}

	// ---------------------------------------------------------------------------

	void HubLayer::UI_SettingsTab()
	{
		HubSectionLabel("ABOUT");

		auto KV = [](const char* key, const std::string& value, float labelW = 210.0f)
		{
			ImGui::TextColored(HubStyle::TextDim, "%s", key);
			ImGui::SameLine(labelW);
			ImGui::TextColored(HubStyle::Text, "%s", value.c_str());
		};

		KV("Engine version", "1.0.0 (Windows x64)");

		HubSectionLabel("PATHS");

		{
			ImGui::TextColored(HubStyle::TextDim, "Editor executable");
			ImGui::SameLine(210.0f);
			ImGui::TextColored(HubStyle::Text, "%s",
				ProjectManager::GetEditorExecutablePath().string().c_str());
			ImGui::SameLine(0.0f, 8.0f);
#ifdef WF_PLATFORM_WINDOWS
			if (HubGhostButton("Reveal", ImVec2(84.0f, 30.0f)))
			{
				std::filesystem::path exeFull = std::filesystem::absolute(
					ProjectManager::GetEditorExecutablePath());
				std::string explorerArg = "/select,\"" + exeFull.string() + "\"";
				ShellExecuteA(NULL, "open", "explorer.exe", explorerArg.c_str(), NULL, SW_SHOW);
			}
#endif
		}

		{
			ImGui::TextColored(HubStyle::TextDim, "Default projects folder");
			ImGui::SameLine(210.0f);
			ImGui::TextColored(HubStyle::Text, "%s",
				ProjectManager::GetDefaultProjectsDirectory().string().c_str());
			ImGui::SameLine(0.0f, 8.0f);
			if (HubGhostButton("Browse", ImVec2(84.0f, 30.0f)))
			{
				std::string folder = FileDialogs::OpenFolder();
				if (!folder.empty())
					strcpy_s(m_NewProjectPathBuffer, sizeof(m_NewProjectPathBuffer), folder.c_str());
			}
		}

		ImGui::Spacing();
		ImGui::Spacing();
		ImGui::TextColored(HubStyle::TextFaint,
			"The default folder applies to newly created projects only.");
	}

}
