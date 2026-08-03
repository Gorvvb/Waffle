#include "wfpch.h"
#include "ContentBrowserPanel.h"
#include "Waffle/Core/PlatformDetection.h"
#include "Waffle/Utils/PlatformUtils.h"
#include "Waffle/Scene/Scene.h"
#include "Waffle/Scene/Entity.h"
#include "Waffle/Scene/Components.h"
#include "Waffle/Scene/SceneSerializer.h"

#include <imgui/imgui.h>
#include <yaml-cpp/yaml.h>
#include <fstream>
#include <glad/glad.h>
#include <glm/glm.hpp>

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
				const auto& path = directoryEntry.path();
				std::string ext = path.extension().string();
				for (auto& c : ext) c = (char)tolower(c);
				std::string fn = path.filename().string();

				// Hide internal engine/project files (.wfp, .wfk, .ini, .log, .yaml, dotfiles) from the user
				if (ext == ".wfp" || ext == ".wfk" || ext == ".ini" || ext == ".log" || ext == ".yaml" || ext == ".yml" || (!fn.empty() && fn[0] == '.'))
					continue;

				itemCount++;
				auto relativePath = std::filesystem::relative(path, g_AssetPath);
				std::string filenameString = relativePath.filename().string();

				ImGui::PushID(filenameString.c_str());

				Ref<Texture2D> icon = directoryEntry.is_directory() ? m_DirectoryIcon : m_FileIcon;
				if (!directoryEntry.is_directory())
				{
					if (ext == ".png" || ext == ".jpg" || ext == ".jpeg")
					{
						std::string pathStr = path.string();
						auto it = m_TextureCache.find(pathStr);
						if (it != m_TextureCache.end() && it->second)
						{
							icon = it->second;
						}
						else
						{
							// Always load thumbnails as Nearest — clear any stale Linear entry first
							m_TextureCache.erase(pathStr);
							Ref<Texture2D> loadedTex = Texture2D::Create(pathStr, TextureFilter::Nearest);
							if (loadedTex)
							{
								m_TextureCache[pathStr] = loadedTex;
								icon = loadedTex;
							}
						}
					}
					else if (ext == ".prefab")
					{
						std::string pathStr = path.string();
						auto it = m_TextureCache.find(pathStr);
						if (it != m_TextureCache.end() && it->second)
						{
							icon = it->second;
						}
						else
						{
							try {
								YAML::Node data = YAML::LoadFile(pathStr);
								auto entityNode = data["Entity"];
								if (entityNode && entityNode["SpriteRendererComponent"] && entityNode["SpriteRendererComponent"]["TexturePath"])
								{
									std::string texRelPath = entityNode["SpriteRendererComponent"]["TexturePath"].as<std::string>();
									if (!texRelPath.empty())
										texRelPath = (g_AssetPath / texRelPath).string();

									if (std::filesystem::exists(texRelPath))
									{
										Ref<Texture2D> loadedTex = Texture2D::Create(texRelPath, TextureFilter::Nearest);
										if (loadedTex)
										{
											m_TextureCache[pathStr] = loadedTex;
											icon = loadedTex;
										}
									}
								}
							} catch (...) {}
						}
					}
				}

				ImVec2 iconSize = { thumbnailSize, thumbnailSize };
				if (icon && icon != m_DirectoryIcon && icon != m_FileIcon)
				{
					icon->SetFilter(TextureFilter::Nearest); // reapply every frame before ImGui draws it

					float aspect = (float)icon->GetWidth() / (float)icon->GetHeight();
					if (aspect > 0.0f)
					{
						if (aspect >= 1.0f)
							iconSize = ImVec2(thumbnailSize, thumbnailSize / aspect);
						else
							iconSize = ImVec2(thumbnailSize * aspect, thumbnailSize);
					}
				}

				bool isSelected = (m_SelectedItem == path);
				ImGui::PushStyleColor(ImGuiCol_Button, isSelected ? ImVec4{ 0.2f, 0.4f, 0.8f, 0.5f } : ImVec4{ 0, 0, 0, 0 });
				ImGui::ImageButton("##", (ImTextureID)(uintptr_t)icon->GetRendererID(), iconSize, { 0, 1 }, { 1, 0 });

				if (icon && icon != m_DirectoryIcon && icon != m_FileIcon)
				{
					ImDrawList* dl = ImGui::GetWindowDrawList();
					dl->AddCallback(ImDrawCallback_ResetRenderState, nullptr);
				}

				// Drop entity onto item icon to create prefab
				if (ImGui::BeginDragDropTarget())
				{
					if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SCENE_HIERARCHY_ENTITY"))
					{
						UUID entityUUID = *(const UUID*)payload->Data;
						if (m_SceneContext)
						{
							Entity entity = m_SceneContext->GetEntityByUUID(entityUUID);
							if (entity)
							{
								std::string entityName = entity.GetComponent<TagComponent>().Tag;
								if (entityName.empty()) entityName = "Entity";

								std::filesystem::path prefabsDir = m_CurrentDirectory;
								if (std::filesystem::exists(g_AssetPath / "Prefabs"))
									prefabsDir = g_AssetPath / "Prefabs";

								std::filesystem::path prefabPath = prefabsDir / (entityName + ".prefab");
								int counter = 1;
								while (std::filesystem::exists(prefabPath))
								{
									prefabPath = prefabsDir / (entityName + std::to_string(counter++) + ".prefab");
								}

								SceneSerializer::SerializeEntityToPrefab(entity, prefabPath.string());
								WF_CORE_INFO("Saved entity '{0}' to prefab '{1}'", entityName, prefabPath.string());
							}
						}
					}
					ImGui::EndDragDropTarget();
				}

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
					else if (path.extension() == ".spritesheet")
					{
						m_SelectedSpritesheetPath = path;
						try {
							YAML::Node data = YAML::LoadFile(path.string());
							std::string texName = data["Spritesheet"].as<std::string>("");

							std::filesystem::path fullTexPath = path.parent_path() / texName;
							if (!std::filesystem::exists(fullTexPath)) fullTexPath = g_AssetPath / texName;
							if (std::filesystem::exists(fullTexPath))
							{
								m_SpritesheetTexture = Texture2D::Create(fullTexPath.string(), TextureFilter::Nearest);
								m_SpritesheetSubTextures.clear();
								m_SpritesheetRegions.clear();
								m_SpritesheetTexAbsPath = std::filesystem::absolute(fullTexPath).string();

								// New named-region format (from Spritesheet Editor)
								if (data["Regions"] && data["Regions"].IsSequence())
								{
									const float texW = (float)m_SpritesheetTexture->GetWidth();
									const float texH = (float)m_SpritesheetTexture->GetHeight();
									for (auto regNode : data["Regions"])
									{
										SpritesheetRegionInfo info;
										info.Name = regNode["Name"].as<std::string>("Sprite");
										if (regNode["Min"].IsSequence() && regNode["Max"].IsSequence())
										{
											info.Min = { regNode["Min"][0].as<float>(0.0f), regNode["Min"][1].as<float>(0.0f) };
											info.Max = { regNode["Max"][0].as<float>(0.0f), regNode["Max"][1].as<float>(0.0f) };
										}
										m_SpritesheetRegions.push_back(info);

										// Build sub-texture from pixel rect (flip Y for OpenGL)
										glm::vec2 uvMin = { info.Min.x / texW, 1.0f - info.Max.y / texH };
										glm::vec2 uvMax = { info.Max.x / texW, 1.0f - info.Min.y / texH };
										auto sub = CreateRef<SubTexture2D>(m_SpritesheetTexture, uvMin, uvMax);
										if (sub) m_SpritesheetSubTextures.push_back(sub);
									}
								}
								else
								{
									// Legacy grid format
									m_SpritesheetCols = data["Columns"].as<int>(1);
									m_SpritesheetRows = data["Rows"].as<int>(1);
									float cellW = (float)m_SpritesheetTexture->GetWidth() / (float)m_SpritesheetCols;
									float cellH = (float)m_SpritesheetTexture->GetHeight() / (float)m_SpritesheetRows;
									int total = m_SpritesheetCols * m_SpritesheetRows;
									for (int i = 0; i < total; i++)
									{
										int col = i % m_SpritesheetCols;
										int row = m_SpritesheetRows - 1 - (i / m_SpritesheetCols);
										SpritesheetRegionInfo info;
										info.Name = "Sprite_" + std::to_string(i);
										info.Min = { (float)col * cellW, (float)(m_SpritesheetRows - 1 - row) * cellH };
										info.Max = { info.Min.x + cellW, info.Min.y + cellH };
										m_SpritesheetRegions.push_back(info);
										auto sub = SubTexture2D::CreateFromCoords(m_SpritesheetTexture, { (float)col, (float)row }, { cellW, cellH });
										if (sub) m_SpritesheetSubTextures.push_back(sub);
									}
								}
							}
							m_ShowSpritesheetViewer = true;
						} catch (...) {}
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
					if (!newPath.has_extension() && m_ItemToRename.has_extension())
					{
						newPath += m_ItemToRename.extension();
					}

					if (newPath != m_ItemToRename)
					{
						std::filesystem::path oldPath = m_ItemToRename;
						std::filesystem::rename(m_ItemToRename, newPath);

						if (oldPath.extension() == ".waffle" && m_SceneRenamedCallback)
						{
							m_SceneRenamedCallback(oldPath, newPath);
						}
					}
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

		// Modal Dialog: Create Spritesheet
		if (m_OpenSliceModal)
		{
			ImGui::OpenPopup("Create Spritesheet");
			m_OpenSliceModal = false;
		}

		if (ImGui::BeginPopupModal("Create Spritesheet", NULL, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::Text("Slicing image: %s", m_ItemToSlice.filename().string().c_str());
			ImGui::Separator();

			ImGui::DragInt("Columns (Grid Width)", &m_SliceColumns, 1.0f, 1, 64);
			ImGui::DragInt("Rows (Grid Height)", &m_SliceRows, 1.0f, 1, 64);

			ImGui::Spacing();
			if (ImGui::Button("Slice & Create Asset", ImVec2(180, 0)))
			{
				std::filesystem::path sheetYamlPath = m_ItemToSlice.parent_path() / (m_ItemToSlice.stem().string() + ".spritesheet");
				YAML::Emitter out;
				out << YAML::BeginMap;
				out << YAML::Key << "Spritesheet" << YAML::Value << m_ItemToSlice.filename().string();
				out << YAML::Key << "Columns" << YAML::Value << m_SliceColumns;
				out << YAML::Key << "Rows" << YAML::Value << m_SliceRows;
				out << YAML::EndMap;

				std::ofstream fout(sheetYamlPath);
				fout << out.c_str();
				WF_CORE_INFO("Saved spritesheet asset: {0}", sheetYamlPath.string());

				m_ItemToSlice.clear();
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel", ImVec2(100, 0)))
			{
				m_ItemToSlice.clear();
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}

		// =========================================================================
		//  SPRITESHEET SPRITE VIEWER  (bottom filmstrip)
		// =========================================================================
		if (m_ShowSpritesheetViewer && !m_SpritesheetSubTextures.empty() && m_SpritesheetTexture)
		{
			ImGui::Separator();

			// Header bar
			ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.12f, 0.12f, 1.0f));
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 4));

			ImGui::Text("Sprites  —  %s  (%zu sprites)",
				m_SelectedSpritesheetPath.filename().string().c_str(),
				m_SpritesheetSubTextures.size());
			ImGui::SameLine(ImGui::GetContentRegionAvail().x - 20.0f);
			if (ImGui::Button("x##CloseViewer"))
			{
				m_ShowSpritesheetViewer = false;
				m_SpritesheetSubTextures.clear();
				m_SpritesheetRegions.clear();
				m_SpritesheetTexture = nullptr;
				m_SpritesheetTexAbsPath.clear();
			}

			ImGui::PopStyleVar();

			// Filmstrip scrollable child
			ImGui::BeginChild("##SpriteViewer", ImVec2(0, 130.0f), true,
				ImGuiWindowFlags_HorizontalScrollbar);

			const float sprThumb = 64.0f;
			// Texture path for drag payloads — cached when the spritesheet was
			// opened, so we don't re-parse the YAML from disk every frame.
			const std::string& texPathForPayload = m_SpritesheetTexAbsPath;

			for (int i = 0; i < (int)m_SpritesheetSubTextures.size(); i++)
			{
				if (i > 0) ImGui::SameLine();
				ImGui::PushID(i);

				ImGui::BeginGroup();

				const auto& sub = m_SpritesheetSubTextures[i];
				const auto& region = (i < (int)m_SpritesheetRegions.size()) ? m_SpritesheetRegions[i] : SpritesheetRegionInfo{};

				GLuint texID = (GLuint)m_SpritesheetTexture->GetRendererID();
				const glm::vec2* uvs = sub->GetTexCoords();

				// Show actual sprite aspect inside a fixed square slot
				ImVec2 dispSize = { sprThumb, sprThumb };
				float pw = region.Max.x - region.Min.x;
				float ph = region.Max.y - region.Min.y;
				if (pw > 0.0f && ph > 0.0f)
				{
					float asp = pw / ph;
					if (asp >= 1.0f)
						dispSize = { sprThumb, sprThumb / asp };
					else
						dispSize = { sprThumb * asp, sprThumb };
				}

				// Sampler reset so Nearest filter takes effect
				ImDrawList* dl = ImGui::GetWindowDrawList();
				dl->AddCallback([](const ImDrawList*, const ImDrawCmd*) {
					glBindSampler(0, 0);
				}, nullptr);

				ImGui::ImageButton("##SprBtn",
					(ImTextureID)(uintptr_t)texID,
					dispSize,
					ImVec2(uvs[3].x, uvs[3].y),
					ImVec2(uvs[1].x, uvs[1].y));

				dl->AddCallback(ImDrawCallback_ResetRenderState, nullptr);

				// Drag source: payload = "texturePath|minX,minY,maxX,maxY"
				if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
				{
					char payloadBuf[1024];
					snprintf(payloadBuf, sizeof(payloadBuf), "%s|%.0f,%.0f,%.0f,%.0f",
						texPathForPayload.c_str(),
						region.Min.x, region.Min.y, region.Max.x, region.Max.y);
					ImGui::SetDragDropPayload("SPRITESHEET_FRAME_ITEM",
						payloadBuf, strlen(payloadBuf) + 1);

					// Drag preview thumbnail
					dl->AddCallback([](const ImDrawList*, const ImDrawCmd*) {
						glBindSampler(0, 0);
					}, nullptr);
					ImGui::Image((ImTextureID)(uintptr_t)texID,
						ImVec2(sprThumb, sprThumb),
						ImVec2(uvs[3].x, uvs[3].y),
						ImVec2(uvs[1].x, uvs[1].y));
					dl->AddCallback(ImDrawCallback_ResetRenderState, nullptr);

					ImGui::Text("%s", region.Name.c_str());
					ImGui::EndDragDropSource();
				}

				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("%s\n%.0f x %.0f px", region.Name.c_str(), pw, ph);

				ImGui::TextDisabled("%s", region.Name.c_str());

				ImGui::EndGroup();
				ImGui::PopID();
			}

			ImGui::EndChild();
			ImGui::PopStyleColor();
		}


		ImGui::SetCursorPos(ImVec2(0, 0));
		ImGui::Dummy(ImVec2(0, 0));
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SCENE_HIERARCHY_ENTITY"))
			{
				UUID entityUUID = *(const UUID*)payload->Data;
				if (m_SceneContext)
				{
					Entity entity = m_SceneContext->GetEntityByUUID(entityUUID);
					if (entity)
					{
						std::string entityName = entity.GetComponent<TagComponent>().Tag;
						if (entityName.empty()) entityName = "Entity";

						std::filesystem::path prefabsDir = m_CurrentDirectory;
						if (std::filesystem::exists(g_AssetPath / "Prefabs"))
							prefabsDir = g_AssetPath / "Prefabs";

						std::filesystem::path prefabPath = prefabsDir / (entityName + ".prefab");
						int counter = 1;
						while (std::filesystem::exists(prefabPath))
						{
							prefabPath = prefabsDir / (entityName + std::to_string(counter++) + ".prefab");
						}

						SceneSerializer::SerializeEntityToPrefab(entity, prefabPath.string());
						WF_CORE_INFO("Saved entity '{0}' to prefab '{1}'", entityName, prefabPath.string());
					}
				}
			}
			ImGui::EndDragDropTarget();
		}

		ImGui::End();
	}
}