#include "wfpch.h"
#include "ContentBrowserPanel.h"
#include "Waffle/Scene/SceneSerializer.h"
#include "Waffle/Utils/PlatformUtils.h"

#include <imgui/imgui.h>
#include <fstream>

namespace Waffle {

	std::filesystem::path g_AssetPath = "Assets";

	ContentBrowserPanel::ContentBrowserPanel()
		: m_CurrentDirectory(g_AssetPath)
	{
		m_DirectoryIcon = Texture2D::Create("Resources/Icons/ContentBrowser/DirectoryIcon.png");
		m_FileIcon = Texture2D::Create("Resources/Icons/ContentBrowser/FileIcon.png");
	}

	void ContentBrowserPanel::SetAssetDirectory(const std::filesystem::path& path)
	{
		g_AssetPath = path;
		m_CurrentDirectory = g_AssetPath;
	}

	void ContentBrowserPanel::OnImGuiRender()
	{
		ImGui::Begin("Content Browser");

		// Header navigation bar
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 4.0f));
		if (m_CurrentDirectory != g_AssetPath)
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
				bool isSelected = (m_SelectedItem == path);
				ImGui::PushStyleColor(ImGuiCol_Button, isSelected ? ImVec4{ 0.2f, 0.4f, 0.8f, 0.5f } : ImVec4{ 0, 0, 0, 0 });
				ImGui::ImageButton("##", (ImTextureID)(uintptr_t)icon->GetRendererID(), { thumbnailSize, thumbnailSize }, { 0, 1 }, { 1, 0 });

				if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
				{
					m_SelectedItem = path;
				}

				if (ImGui::BeginDragDropSource())
				{
					const wchar_t* itemPath = relativePath.c_str();
					ImGui::SetDragDropPayload("CONTENT_BROWSER_ITEM", itemPath, (wcslen(itemPath) + 1) * sizeof(wchar_t), ImGuiCond_Once);
					ImGui::EndDragDropSource();
				}

				// Right click context menu on individual item (File / Folder)
				if (ImGui::BeginPopupContextItem())
				{
					m_SelectedItem = path;
					if (ImGui::MenuItem("Rename"))
					{
						m_ItemToRename = path;
						memset(m_RenameItemBuffer, 0, sizeof(m_RenameItemBuffer));
						strcpy_s(m_RenameItemBuffer, sizeof(m_RenameItemBuffer), path.filename().string().c_str());
						m_OpenRenameModal = true;
					}
					if (ImGui::MenuItem("Delete"))
					{
						m_ItemToDelete = path;
						m_OpenDeleteModal = true;
					}
					ImGui::EndPopup();
				}

				ImGui::PopStyleColor();
				if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
				{
					if (directoryEntry.is_directory())
					{
						m_CurrentDirectory /= path.filename();
						m_SelectedItem.clear();
					}
					else if (path.extension() == ".waffle")
					{
						if (m_OpenSceneCallback)
							m_OpenSceneCallback(path);
					}
					else if (path.extension() == ".lua" || path.extension() == ".h" || path.extension() == ".cpp" || path.extension() == ".txt")
					{
						PlatformUtils::OpenFileInEditor(path.string());
					}
				}

				ImGui::TextWrapped(filenameString.c_str());

				ImGui::NextColumn();

				ImGui::PopID();
			}
		}

		ImGui::Columns(1);

		if (ImGui::IsMouseDown(0) && ImGui::IsWindowHovered())
		{
			m_SelectedItem.clear();
		}

		if (!m_SelectedItem.empty() && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && ImGui::IsKeyPressed(ImGuiKey_Delete))
		{
			m_ItemToDelete = m_SelectedItem;
			m_OpenDeleteModal = true;
		}

		// Right-click on blank space in Content Browser
		if (ImGui::BeginPopupContextWindow(0, 1 | ImGuiPopupFlags_NoOpenOverItems))
		{
			if (ImGui::BeginMenu("Create"))
			{
				if (ImGui::MenuItem("Folder"))
				{
					std::filesystem::path folderPath = m_CurrentDirectory / "NewFolder";
					int counter = 1;
					while (std::filesystem::exists(folderPath))
					{
						folderPath = m_CurrentDirectory / ("NewFolder" + std::to_string(counter++));
					}
					std::filesystem::create_directory(folderPath);
				}

				if (ImGui::MenuItem("Create Scene"))
				{
					std::filesystem::path scenePath = m_CurrentDirectory / "NewScene.waffle";
					int counter = 1;
					while (std::filesystem::exists(scenePath))
					{
						scenePath = m_CurrentDirectory / ("NewScene" + std::to_string(counter++) + ".waffle");
					}

					Ref<Scene> newScene = CreateRef<Scene>();
					newScene->SetName(scenePath.stem().string());
					SceneSerializer serializer(newScene);
					serializer.Serialize(scenePath.string());
				}

				if (ImGui::MenuItem("Lua Script"))
				{
					std::filesystem::path scriptPath = m_CurrentDirectory / "NewScript.lua";
					int counter = 1;
					while (std::filesystem::exists(scriptPath))
					{
						scriptPath = m_CurrentDirectory / ("NewScript" + std::to_string(counter++) + ".lua");
					}

					std::ofstream scriptFile(scriptPath);
					scriptFile << "-- Waffle Lua Script\n\n"
							   << "function OnCreate(entity)\n"
							   << "    -- Called when the script starts\n"
							   << "end\n\n"
							   << "function OnUpdate(entity, ts)\n"
							   << "    -- Called every frame during gameplay\n"
							   << "end\n\n"
							   << "function OnDestroy(entity)\n"
							   << "    -- Called when the script is destroyed\n"
							   << "end\n";
					scriptFile.close();
				}
				ImGui::EndMenu();
			}
			ImGui::EndPopup();
		}

		// Modal Dialog: Delete Item Confirmation
		if (m_OpenDeleteModal)
		{
			ImGui::OpenPopup("Delete Confirmation");
			m_OpenDeleteModal = false;
		}

		if (ImGui::BeginPopupModal("Delete Confirmation", NULL, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::Text("Are you sure you want to delete '%s'?", m_ItemToDelete.filename().string().c_str());
			ImGui::TextDisabled("This action cannot be undone.");
			ImGui::Separator();

			if (ImGui::Button("Delete", ImVec2(120, 0)))
			{
				if (std::filesystem::exists(m_ItemToDelete))
				{
					std::filesystem::remove_all(m_ItemToDelete);
				}
				if (m_SelectedItem == m_ItemToDelete)
					m_SelectedItem.clear();
				m_ItemToDelete.clear();
				ImGui::CloseCurrentPopup();
			}
			ImGui::SetItemDefaultFocus();
			ImGui::SameLine();
			if (ImGui::Button("Cancel", ImVec2(120, 0)))
			{
				m_ItemToDelete.clear();
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}

		// Modal Dialog: Rename Item
		if (m_OpenRenameModal)
		{
			ImGui::OpenPopup("Rename Item");
			m_OpenRenameModal = false;
		}

		if (ImGui::BeginPopupModal("Rename Item", NULL, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::Text("Enter new name:");
			ImGui::InputText("##NewItemName", m_RenameItemBuffer, sizeof(m_RenameItemBuffer));

			if (ImGui::Button("Rename", ImVec2(120, 0)) || ImGui::IsKeyPressed(ImGuiKey_Enter))
			{
				std::string newName = m_RenameItemBuffer;
				if (!newName.empty() && std::filesystem::exists(m_ItemToRename))
				{
					std::filesystem::path newPath = m_ItemToRename.parent_path() / newName;
					std::filesystem::rename(m_ItemToRename, newPath);
				}
				m_ItemToRename.clear();
				ImGui::CloseCurrentPopup();
			}
			ImGui::SetItemDefaultFocus();
			ImGui::SameLine();
			if (ImGui::Button("Cancel", ImVec2(120, 0)))
			{
				m_ItemToRename.clear();
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}

		// Status Bar
		ImGui::SetCursorPosY(ImGui::GetWindowHeight() - 24.0f);
		ImGui::Separator();
		ImGui::TextDisabled("Items: %u", itemCount);

		ImGui::End();
	}
}