#include "wfpch.h"
#include "ContentBrowserPanel.h"

#include <imgui/imgui.h>

namespace Waffle {

	extern const std::filesystem::path g_AssetPath = "assets";

	ContentBrowserPanel::ContentBrowserPanel()
		: m_CurrentDirectory(g_AssetPath)
	{
		m_DirectoryIcon = Texture2D::Create("Resources/Icons/ContentBrowser/DirectoryIcon.png");
		m_FileIcon = Texture2D::Create("Resources/Icons/ContentBrowser/FileIcon.png");
	}

	void ContentBrowserPanel::OnImGuiRender()
	{
		ImGui::Begin("Content Browser");

		// Header navigation bar
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 4.0f));
		if (m_CurrentDirectory != std::filesystem::path(g_AssetPath))
		{
			if (ImGui::Button(" <- Back "))
			{
				m_CurrentDirectory = m_CurrentDirectory.parent_path();
			}
			ImGui::SameLine();
		}

		// Breadcrumb path display
		std::string pathString = m_CurrentDirectory.string();
		ImGui::TextDisabled("Location: %s", pathString.c_str());
		ImGui::PopStyleVar();

		ImGui::Separator();

		static float padding = 16.0f;
		static float thumbnailSize = 72.0f;
		float cellSize = thumbnailSize + padding;

		float panelWidth = ImGui::GetContentRegionAvail().x;
		int columnCount = (int)(panelWidth / cellSize);
		if (columnCount < 1)
			columnCount = 1;

		ImGui::Columns(columnCount, 0, false);

		uint32_t itemCount = 0;
		if (std::filesystem::exists(m_CurrentDirectory))
		{
			for (auto& directoryEntry : std::filesystem::directory_iterator(m_CurrentDirectory))
			{
				itemCount++;
				const auto& path = directoryEntry.path();
				auto relativePath = std::filesystem::relative(path, g_AssetPath);
				std::string filenameString = relativePath.filename().string();

				ImGui::PushID(filenameString.c_str());

				Ref<Texture2D> icon = directoryEntry.is_directory() ? m_DirectoryIcon : m_FileIcon;
				ImGui::PushStyleColor(ImGuiCol_Button, { 0, 0, 0, 0 });
				ImGui::ImageButton("##", (ImTextureID)(uintptr_t)icon->GetRendererID(), { thumbnailSize, thumbnailSize }, { 0, 1 }, { 1, 0 });

				if (ImGui::BeginDragDropSource())
				{
					const wchar_t* itemPath = relativePath.c_str();
					ImGui::SetDragDropPayload("CONTENT_BROWSER_ITEM", itemPath, (wcslen(itemPath) + 1) * sizeof(wchar_t), ImGuiCond_Once);
					ImGui::EndDragDropSource();
				}

				ImGui::PopStyleColor();
				if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
				{
					if (directoryEntry.is_directory())
						m_CurrentDirectory /= path.filename();
				}

				ImGui::TextWrapped(filenameString.c_str());

				ImGui::NextColumn();

				ImGui::PopID();
			}
		}

		ImGui::Columns(1);

		// Status Bar
		ImGui::SetCursorPosY(ImGui::GetWindowHeight() - 24.0f);
		ImGui::Separator();
		ImGui::TextDisabled("Items: %u", itemCount);

		ImGui::End();
	}
}