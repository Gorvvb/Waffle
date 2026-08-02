#include "SceneHierarchyPanel.h"

#include "Waffle/Scene/Components.h"
#include "Waffle/Utils/PlatformUtils.h"

#include "Waffle/Scripting/LuaScriptEngine.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <glm/gtc/type_ptr.hpp>

namespace Waffle {

	extern std::filesystem::path g_AssetPath;

	SceneHierarchyPanel::SceneHierarchyPanel(const Ref<Scene>& context)
	{
		SetContext(context);
	}

	void SceneHierarchyPanel::SetContext(const Ref<Scene>& context)
	{
		m_Context = context;
		m_SelectionContext = {};
	}

	void SceneHierarchyPanel::OnImGuiRender()
	{
		ImGui::Begin("Scene Hierarchy");

		if (m_Context)
		{
			std::string sceneName = m_Context->GetName();
			if (sceneName.empty())
				sceneName = "Untitled";
			ImGui::Text("Active Scene: %s", sceneName.c_str());
			ImGui::Separator();

			auto view = m_Context->m_Registry.view<TagComponent>();

			for (auto entityID : view)
			{
				Entity entity{ entityID, m_Context.get() };
				if (!entity.HasComponent<RelationshipComponent>() || entity.GetComponent<RelationshipComponent>().Parent == 0)
				{
					DrawEntityNode(entity);
				}
			}

			if (m_SelectionContext && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && ImGui::IsKeyPressed(ImGuiKey_Delete))
			{
				m_Context->DestroyEntity(m_SelectionContext);
				m_SelectionContext = {};
			}

			if (ImGui::IsMouseDown(0) && ImGui::IsWindowHovered())
				m_SelectionContext = {};

			// Drag and drop onto empty hierarchy space to unparent
			ImGui::Dummy(ImGui::GetContentRegionAvail());
			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SCENE_HIERARCHY_ENTITY"))
				{
					UUID droppedEntityUUID = *(const UUID*)payload->Data;
					Entity droppedEntity = m_Context->GetEntityByUUID(droppedEntityUUID);
					if (droppedEntity)
						m_Context->UnparentEntity(droppedEntity);
				}
				ImGui::EndDragDropTarget();
			}

			// Right-click on a blank space
			if (ImGui::BeginPopupContextWindow(0, 1 | ImGuiPopupFlags_NoOpenOverItems))
			{
				if (ImGui::MenuItem("Create Empty Entity"))
				{
					Entity newEntity = m_Context->CreateEntity("Empty Entity");
					m_SelectionContext = newEntity;
				}
				ImGui::EndPopup();
			}

			ImGui::End();

			ImGui::Begin("Properties");
			if (m_SelectionContext)
			{
				DrawComponents(m_SelectionContext);
			}
			ImGui::End();
		}
	}

	void SceneHierarchyPanel::SetSelectedEntity(Entity entity)
	{
		m_SelectionContext = entity;
	}

	void SceneHierarchyPanel::DrawEntityNode(Entity entity)
	{
		auto& tag = entity.GetComponent<TagComponent>().Tag;
		
		bool hasChildren = entity.HasComponent<RelationshipComponent>() && !entity.GetComponent<RelationshipComponent>().Children.empty();
		ImGuiTreeNodeFlags flags = ((m_SelectionContext == entity) ? ImGuiTreeNodeFlags_Selected : 0) | ImGuiTreeNodeFlags_OpenOnArrow;
		flags |= ImGuiTreeNodeFlags_SpanAvailWidth;
		if (!hasChildren)
			flags |= ImGuiTreeNodeFlags_Leaf;

		bool isRenaming = (m_RenamingEntity == entity);
		bool opened = false;

		if (isRenaming)
		{
			ImGui::PushID((void*)(uint64_t)(uint32_t)entity);
			ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
			if (ImGui::InputText("##TagInlineRename", m_RenameBuffer, sizeof(m_RenameBuffer), ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll))
			{
				std::string newTag = std::string(m_RenameBuffer);
				tag = newTag.empty() ? "Empty Entity" : newTag;
				m_RenamingEntity = {};
			}

			if (ImGui::IsItemDeactivated() && !ImGui::IsKeyPressed(ImGuiKey_Escape))
			{
				std::string newTag = std::string(m_RenameBuffer);
				tag = newTag.empty() ? "Empty Entity" : newTag;
				m_RenamingEntity = {};
			}

			if (ImGui::IsKeyPressed(ImGuiKey_Escape))
			{
				m_RenamingEntity = {};
			}

			ImGui::PopID();
		}
		else
		{
			opened = ImGui::TreeNodeEx((void*)(uint64_t)(uint32_t)entity, flags, tag.c_str());
		}

		if (!isRenaming && ImGui::IsItemClicked())
		{
			m_SelectionContext = entity;
		}

		if (!isRenaming && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
		{
			m_RenamingEntity = entity;
			memset(m_RenameBuffer, 0, sizeof(m_RenameBuffer));
			strcpy_s(m_RenameBuffer, sizeof(m_RenameBuffer), tag.c_str());
		}

		// Drag Source
		if (ImGui::BeginDragDropSource())
		{
			UUID uuid = entity.GetUUID();
			ImGui::SetDragDropPayload("SCENE_HIERARCHY_ENTITY", &uuid, sizeof(UUID));
			ImGui::Text("%s", tag.c_str());
			ImGui::EndDragDropSource();
		}

		// Drag Target (Parenting & Assets)
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SCENE_HIERARCHY_ENTITY"))
			{
				UUID droppedEntityUUID = *(const UUID*)payload->Data;
				Entity droppedEntity = m_Context->GetEntityByUUID(droppedEntityUUID);
				if (droppedEntity && droppedEntity != entity)
				{
					m_Context->ParentEntity(droppedEntity, entity);
				}
			}
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
			{
				const wchar_t* pathStr = (const wchar_t*)payload->Data;
				std::filesystem::path path = std::filesystem::path(g_AssetPath) / pathStr;
				std::string ext = path.extension().string();
				for (auto& c : ext) c = (char)tolower(c);

				if (ext == ".png" || ext == ".jpg" || ext == ".jpeg")
				{
					if (!entity.HasComponent<SpriteRendererComponent>())
						entity.AddComponent<SpriteRendererComponent>();

					auto& src = entity.GetComponent<SpriteRendererComponent>();
					src.Texture = Texture2D::Create(path.string(), src.FilterMode);
				}
				else if (ext == ".lua")
				{
					if (!entity.HasComponent<ScriptComponent>())
						entity.AddComponent<ScriptComponent>();

					auto& sc = entity.GetComponent<ScriptComponent>();
					sc.ScriptPaths.push_back(path.string());
				}
			}
			ImGui::EndDragDropTarget();
		}

		bool entityDeleted = false;
		bool entityDuplicated = false;
		if (ImGui::BeginPopupContextItem())
		{
			if (ImGui::MenuItem("Rename"))
			{
				m_RenamingEntity = entity;
				memset(m_RenameBuffer, 0, sizeof(m_RenameBuffer));
				strcpy_s(m_RenameBuffer, sizeof(m_RenameBuffer), tag.c_str());
			}
			if (ImGui::MenuItem("Duplicate Entity"))
				entityDuplicated = true;
			if (ImGui::MenuItem("Delete Entity"))
				entityDeleted = true;
			ImGui::EndPopup();
		}

		if (opened)
		{
			if (hasChildren)
			{
				auto children = entity.GetComponent<RelationshipComponent>().Children;
				for (auto childUUID : children)
				{
					Entity child = m_Context->GetEntityByUUID(childUUID);
					if (child)
						DrawEntityNode(child);
				}
			}
			ImGui::TreePop();
		}

		if (entityDeleted)
		{
			m_Context->DestroyEntity(entity);
			if (m_SelectionContext == entity)
				m_SelectionContext = {};
		}

		if (entityDuplicated)
		{
			m_Context->DuplicateEntity(entity);
		}
	}

	static void DrawVec3Control(const std::string& label, glm::vec3& values, float resetValue = 0.0f, float columnWidth = 100.0f)
	{
		ImGuiIO& io = ImGui::GetIO();
		auto boldFont = io.Fonts->Fonts[0];

		ImGui::PushID(label.c_str());

		ImGui::Columns(2);

		ImGui::SetColumnWidth(0, columnWidth);
		ImGui::Text(label.c_str());
		ImGui::NextColumn();

		ImGui::PushMultiItemsWidths(3, ImGui::CalcItemWidth());
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 0,0 });

		float lineHeight = ImGui::GetTextLineHeight() + ImGui::GetStyle().FramePadding.y * 2.0f;
		ImVec2 buttonSize = { lineHeight + 3.0f, lineHeight };

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.9f, 0.2f, 0.2f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
		ImGui::PushFont(boldFont);
		if (ImGui::Button("X", buttonSize))
			values.x = resetValue;
		ImGui::PopFont();
		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		ImGui::DragFloat("##X", &values.x, 0.1f, 0.0f, 0.0f, "%.2f");
		ImGui::PopItemWidth();
		ImGui::SameLine();

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.2f, 0.7f, 0.3f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.3f, 0.8f, 0.3f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
		ImGui::PushFont(boldFont);
		if (ImGui::Button("Y", buttonSize))
			values.y = resetValue;
		ImGui::PopFont();
		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		ImGui::DragFloat("##Y", &values.y, 0.1f, 0.0f, 0.0f, "%.2f");
		ImGui::PopItemWidth();
		ImGui::SameLine();

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.1f, 0.25, 0.8f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.2f, 0.35f, 0.9f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
		ImGui::PushFont(boldFont);
		if (ImGui::Button("Z", buttonSize))
			values.z = resetValue;
		ImGui::PopFont();
		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		ImGui::DragFloat("##Z", &values.z, 0.1f, 0.0f, 0.0f, "%.2f");
		ImGui::PopItemWidth();

		ImGui::PopStyleVar();

		ImGui::Columns(1);

		ImGui::PopID();
	}
	
	template<typename T, typename UIFunction>
	static void DrawComponent(const std::string& name, Entity entity, UIFunction uiFunction)
	{
		const ImGuiTreeNodeFlags treeNodeFlags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_AllowOverlap | ImGuiTreeNodeFlags_FramePadding;
		if (entity.HasComponent<T>())
		{
			auto& component = entity.GetComponent<T>();
			ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();

			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 4, 4 });
			float lineHeight = ImGui::GetTextLineHeight() + ImGui::GetStyle().FramePadding.y * 2.0f;

			ImGui::Separator();
			bool open = ImGui::TreeNodeEx((void*)typeid(T).hash_code(), treeNodeFlags, name.c_str());
			ImGui::PopStyleVar();

			ImGui::SameLine(contentRegionAvailable.x - (lineHeight * 0.5f));
			if (ImGui::Button("+", ImVec2{ lineHeight, lineHeight }))
			{
				ImGui::OpenPopup("ComponentsSettings");
			}

			bool removeComponent = false;
			if (ImGui::BeginPopup("ComponentsSettings"))
			{
				if (ImGui::MenuItem("Remove Component"))
					removeComponent = true;
				ImGui::EndPopup();
			}

			if (open)
			{
				uiFunction(component);
				ImGui::TreePop();
			}

			if (removeComponent)
				entity.RemoveComponent<T>();
		}
	}

	void SceneHierarchyPanel::DrawComponents(Entity entity)
	{
		if (entity.HasComponent<TagComponent>())
		{
			auto& tag = entity.GetComponent<TagComponent>().Tag;

			char buffer[256];
			memset(buffer, 0, sizeof(buffer));
			strcpy_s(buffer, sizeof(buffer), tag.c_str());
			if (ImGui::InputText("##Tag", buffer, sizeof(buffer)))
			{
				tag = std::string(buffer);
				if (tag.empty())
					tag = "Empty Entity";
			}
		}

		ImGui::SameLine();
		ImGui::PushItemWidth(-1);
		if (ImGui::Button("Add Component"))
			ImGui::OpenPopup("AddComponent");

		if (ImGui::BeginPopup("AddComponent"))
		{
			DisplayAddComponentEntry<CameraComponent>("Camera");
			DisplayAddComponentEntry<ScriptComponent>("Script");
			DisplayAddComponentEntry<LifetimeComponent>("Lifetime");
			DisplayAddComponentEntry<SpriteRendererComponent>("Sprite Renderer (2D)");
			DisplayAddComponentEntry<CircleRendererComponent>("Circle Renderer (2D)");
			DisplayAddComponentEntry<Rigidbody2DComponent>("Rigidbody (2D)");
			DisplayAddComponentEntry<BoxCollider2DComponent>("Box Collider (2D)");
			DisplayAddComponentEntry<CircleCollider2DComponent>("Circle Collider (2D)");
			DisplayAddComponentEntry<PolygonCollider2DComponent>("Polygon Collider (2D)");

			ImGui::EndPopup();
		}

		ImGui::PopItemWidth();

		DrawComponent<TransformComponent>("Transform", entity, [](auto& component)
		{
			DrawVec3Control("Translation", component.Translation);
			glm::vec3 rotation = glm::degrees(component.Rotation);
			DrawVec3Control("Rotation", rotation);
			component.Rotation = glm::radians(rotation);
			DrawVec3Control("Scale", component.Scale, 1.0f);
		});

		DrawComponent<CameraComponent>("Camera", entity, [](auto& component)
		{
			auto& camera = component.Camera;

			ImGui::Checkbox("Primary", &component.Primary);

			const char* projectionTypeString[] = { "Perspective", "Orthographic" };
			const char* currentProjectionTypeString = projectionTypeString[(int)camera.GetProjectionType()];

			if (ImGui::BeginCombo("Projection", currentProjectionTypeString))
			{
				for (int i = 0; i < 2; i++)
				{
					bool isSelected = (currentProjectionTypeString == projectionTypeString[i]);
					if (ImGui::Selectable(projectionTypeString[i], isSelected))
					{
						currentProjectionTypeString = projectionTypeString[i];
						camera.SetProjectionType((SceneCamera::ProjectionType)i);
					}

					if (isSelected)
						ImGui::SetItemDefaultFocus();
				}

				ImGui::EndCombo();
			}

			if (camera.GetProjectionType() == SceneCamera::ProjectionType::Perspective)
			{
				float verticalFOV = glm::degrees(camera.GetPerspectiveVerticalFOV());
				if (ImGui::DragFloat("Vertical FOV", &verticalFOV))
					camera.SetPerspectiveVerticalFOV(glm::radians(verticalFOV));

				float perspectiveNear = camera.GetPerspectiveNearClip();
				if (ImGui::DragFloat("Near", &perspectiveNear))
					camera.SetPerspectiveNearClip(perspectiveNear);

				float perspectiveFar = camera.GetPerspectiveFarClip();
				if (ImGui::DragFloat("Far", &perspectiveFar))
					camera.SetPerspectiveFarClip(perspectiveFar);
			}

			if (camera.GetProjectionType() == SceneCamera::ProjectionType::Orthograpic)
			{
				float orthoSize = camera.GetOrthographicSize();
				if (ImGui::DragFloat("Size", &orthoSize))
					camera.SetOrthographicSize(orthoSize);

				float orthoNear = camera.GetOrthographicNearClip();
				if (ImGui::DragFloat("Near", &orthoNear))
					camera.SetOrthographicNearClip(orthoNear);

				float orthoFar = camera.GetOrthographicFarClip();
				if (ImGui::DragFloat("Far", &orthoFar))
					camera.SetOrthographicFarClip(orthoFar);

				ImGui::Checkbox("Fixed Aspect Ratio", &component.FixedAspectRatio);
			}

			ImGui::ColorEdit4("Background Color", glm::value_ptr(component.BackgroundColor));

			ImGui::Text("Background Image (Unfinished)");
			if (component.BackgroundImage)
			{
				if (component.BackgroundFilterMode == TextureFilter::Nearest)
					ImGui::GetWindowDrawList()->AddCallback(ImGui::GetPlatformIO().DrawCallback_SetSamplerNearest, nullptr);
				else
					ImGui::GetWindowDrawList()->AddCallback(ImGui::GetPlatformIO().DrawCallback_SetSamplerLinear, nullptr);

				ImGui::ImageButton("##BgTexture", (void*)(intptr_t)component.BackgroundImage->GetRendererID(), ImVec2(100.0f, 100.0f), ImVec2(0, 1), ImVec2(1, 0));

				ImGui::GetWindowDrawList()->AddCallback(ImGui::GetPlatformIO().DrawCallback_SetSamplerLinear, nullptr);
			}
			else
			{
				ImGui::Button("No Texture##Bg", ImVec2(100.0f, 0.0f));
			}

			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
				{
					const wchar_t* path = (const wchar_t*)payload->Data;
					std::filesystem::path texturePath = std::filesystem::path(g_AssetPath) / path;
					component.BackgroundImagePath = GetNormalizedAssetPath(texturePath.string());
					component.BackgroundImage = Texture2D::Create(texturePath.string(), component.BackgroundFilterMode);
				}
				ImGui::EndDragDropTarget();
			}

			ImGui::SameLine();
			if (ImGui::Button("Browse##BgImage"))
			{
				std::string filepath = FileDialogs::OpenFile("Texture Files (*.png;*.jpg;*.jpeg)\0*.png;*.jpg;*.jpeg\0All Files (*.*)\0*.*\0");
				if (!filepath.empty())
				{
					component.BackgroundImagePath = GetNormalizedAssetPath(filepath);
					component.BackgroundImage = Texture2D::Create(filepath, component.BackgroundFilterMode);
				}
			}

			if (component.BackgroundImage)
			{
				ImGui::SameLine();
				if (ImGui::Button("Remove Texture##Bg"))
				{
					component.BackgroundImagePath = "";
					component.BackgroundImage = nullptr;
				}
			}

			const char* filterOptions[] = { "Nearest", "Linear" };
			const char* currentFilter = filterOptions[static_cast<int>(component.BackgroundFilterMode)];
			if (ImGui::BeginCombo("Filter Mode##Bg", currentFilter))
			{
				for (int i = 0; i < IM_ARRAYSIZE(filterOptions); i++)
				{
					bool isSelected = (currentFilter == filterOptions[i]);
					if (ImGui::Selectable(filterOptions[i], isSelected))
					{
						component.BackgroundFilterMode = static_cast<Waffle::TextureFilter>(i);

						if (component.BackgroundImage)
						{
							component.BackgroundImage->SetFilter(component.BackgroundFilterMode);
						}
					}
					if (isSelected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}

			ImGui::DragFloat2("Tiling Factor##Bg", glm::value_ptr(component.BackgroundTilingFactor), 0.1f, 0.0f, 100.0f);
		});

		DrawComponent<ScriptComponent>("Script", entity, [](auto& component)
			{
				if (component.ScriptPaths.empty() && !component.ClassName.empty())
				{
					component.ScriptPaths.push_back(component.ClassName);
				}
				if (component.ScriptPaths.empty())
				{
					component.ScriptPaths.push_back("");
				}

				// Gather all .lua files from Assets directory for the dropdown
				std::vector<std::filesystem::path> luaFiles;
				if (std::filesystem::exists(g_AssetPath))
				{
					for (auto& entry : std::filesystem::recursive_directory_iterator(g_AssetPath))
					{
						if (entry.is_regular_file() && entry.path().extension() == ".lua")
						{
							luaFiles.push_back(std::filesystem::relative(entry.path(), g_AssetPath));
						}
					}
				}

				for (size_t i = 0; i < component.ScriptPaths.size(); i++)
				{
					ImGui::PushID((int)i);
					std::string label = "##Script" + std::to_string(i + 1);

					std::string currentScript = component.ScriptPaths[i];
					std::string filenameOnly = currentScript.empty() ? "None (Select .lua Script)" : std::filesystem::path(currentScript).filename().string();

					ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 120.0f);
					if (ImGui::BeginCombo(label.c_str(), filenameOnly.c_str()))
					{
						if (ImGui::Selectable("None", currentScript.empty()))
						{
							component.ScriptPaths[i] = "";
							component.Fields.erase(currentScript);
						}

						for (const auto& luaFile : luaFiles)
						{
							std::string fileStr = luaFile.string();
							std::string nameStr = luaFile.filename().string();
							bool isSelected = (currentScript == fileStr || currentScript == nameStr);
							if (ImGui::Selectable(nameStr.c_str(), isSelected))
							{
								// If the script changed, clear old fields for this slot
								if (fileStr != currentScript)
									component.Fields.erase(currentScript);
								component.ScriptPaths[i] = fileStr;

								// Scrape fields immediately so they show in the inspector before Play
								std::filesystem::path fullPath = std::filesystem::path(g_AssetPath) / fileStr;
								LuaScriptEngine::ScrapeFieldsFromScript(fullPath, fileStr, component);
							}
							if (isSelected)
								ImGui::SetItemDefaultFocus();
						}
						ImGui::EndCombo();
					}

					if (!currentScript.empty() && ImGui::IsItemHovered())
					{
						ImGui::SetTooltip("%s", currentScript.c_str());
					}

					if (ImGui::BeginDragDropTarget())
					{
						if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
						{
							const wchar_t* pathStr = (const wchar_t*)payload->Data;
							std::filesystem::path path(pathStr);
							if (path.extension() == ".lua")
							{
								if (path.filename().string() != std::filesystem::path(currentScript).filename().string())
									component.Fields.erase(currentScript);
								component.ScriptPaths[i] = path.filename().string();

								// Scrape fields immediately so they show in the inspector before Play
								std::filesystem::path fullPath = std::filesystem::path(g_AssetPath) / path.filename();
								LuaScriptEngine::ScrapeFieldsFromScript(fullPath, path.filename().string(), component);
							}
						}

						ImGui::EndDragDropTarget();
					}

					ImGui::SameLine();
					if (ImGui::Button("Edit"))
					{
						if (!component.ScriptPaths[i].empty())
						{
							std::filesystem::path fullPath = std::filesystem::path(g_AssetPath) / component.ScriptPaths[i];
							if (!std::filesystem::exists(fullPath) && std::filesystem::exists(g_AssetPath))
							{
								for (auto& entry : std::filesystem::recursive_directory_iterator(g_AssetPath))
								{
									if (entry.is_regular_file() && entry.path().filename() == std::filesystem::path(component.ScriptPaths[i]).filename())
									{
										fullPath = entry.path();
										break;
									}
								}
							}
							PlatformUtils::OpenFileInEditor(fullPath.string());
						}
					}

					ImGui::SameLine();
					if (ImGui::Button("Remove"))
					{
						component.Fields.erase(currentScript);
						component.ScriptPaths.erase(component.ScriptPaths.begin() + i);
						ImGui::PopID();
						break;
					}

					ImGui::PopID();
				}

				if (ImGui::Button("+ Add Another Script"))
				{
					component.ScriptPaths.push_back("");
				}

				// --- Public Fields ---
				for (auto& [scriptPath, fieldList] : component.Fields)
				{
					if (fieldList.empty()) continue;

					// Only show fields for scripts still in the ScriptPaths list
					bool scriptStillActive = std::any_of(component.ScriptPaths.begin(), component.ScriptPaths.end(),
						[&scriptPath](const std::string& p) { return p == scriptPath; });
					if (!scriptStillActive) continue;

					ImGui::Spacing();
					ImGui::Separator();
					std::string stem = std::filesystem::path(scriptPath).stem().string();
					ImGui::TextDisabled("%s", stem.c_str());
					ImGui::Spacing();

					for (auto& field : fieldList)
					{
						ImGui::PushID((scriptPath + field.Name).c_str());

						ImGui::Columns(2, nullptr, false);
						ImGui::SetColumnWidth(0, 120.0f);
						ImGui::Text("%s", field.Name.c_str());
						ImGui::NextColumn();
						ImGui::SetNextItemWidth(-1);

						switch (field.Type)
						{
						case LuaFieldType::Float:
							ImGui::DragFloat("##v", &field.FloatVal, 0.1f);
							if (ImGui::IsItemDeactivatedAfterEdit()) field.UserModified = true;
							break;
						case LuaFieldType::Int:
							ImGui::DragInt("##v", &field.IntVal);
							if (ImGui::IsItemDeactivatedAfterEdit()) field.UserModified = true;
							break;
						case LuaFieldType::Bool:
							ImGui::Checkbox("##v", &field.BoolVal);
							if (ImGui::IsItemDeactivatedAfterEdit()) field.UserModified = true;
							break;
						case LuaFieldType::String:
						{
							char buf[256] = {};
							strncpy_s(buf, field.StringVal.c_str(), sizeof(buf) - 1);
							if (ImGui::InputText("##v", buf, sizeof(buf)))
								field.StringVal = buf;
							if (ImGui::IsItemDeactivatedAfterEdit()) field.UserModified = true;
							break;
						}
						}

						ImGui::Columns(1);
						ImGui::PopID();
					}
				}
			});

		DrawComponent<LifetimeComponent>("Lifetime", entity, [](auto& component)
		{
			ImGui::DragFloat("Lifetime", &component.Lifetime, 0.1f, 0.0f, 1000.0f);
		});

		DrawComponent<SpriteRendererComponent>("Sprite Renderer (2D)", entity, [](auto& component)
		{
			ImGui::ColorEdit4("Color", glm::value_ptr(component.Color));

			if (component.Texture)
			{
				ImGui::ImageButton("##SpriteTexturePreview", (ImTextureID)(uintptr_t)component.Texture->GetRendererID(), { 64, 64 }, { 0, 1 }, { 1, 0 });
			}
			else
			{
				ImGui::Button("No Texture", { 64, 64 });
			}

			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
				{
					const wchar_t* path = (const wchar_t*)payload->Data;
					std::filesystem::path texturePath = std::filesystem::path(g_AssetPath) / path;
					component.Texture = Texture2D::Create(texturePath.string(), component.FilterMode);
				}
				ImGui::EndDragDropTarget();
			}

			ImGui::SameLine();
			if (ImGui::Button("Browse Texture"))
			{
				std::string filepath = FileDialogs::OpenFile("Texture Files (*.png *.jpg *.jpeg)\0*.png;*.jpg;*.jpeg\0All Files (*.*)\0*.*\0");
				if (!filepath.empty())
				{
					component.Texture = Texture2D::Create(filepath, component.FilterMode);
				}
			}

			if (component.Texture)
			{
				ImGui::SameLine();
				if (ImGui::Button("Remove Texture"))
				{
					component.Texture = nullptr;
				}
			}

			const char* filterOptions[] = { "Nearest", "Linear" };
			const char* currentFilter = filterOptions[static_cast<int>(component.FilterMode)];
			if (ImGui::BeginCombo("Filter Mode", currentFilter))
			{
				for (int i = 0; i < IM_ARRAYSIZE(filterOptions); i++)
				{
					bool isSelected = (currentFilter == filterOptions[i]);
					if (ImGui::Selectable(filterOptions[i], isSelected))
					{
						component.FilterMode = static_cast<TextureFilter>(i);
						if (component.Texture)
							component.Texture->SetFilter(component.FilterMode);
					}

					if (isSelected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}

			ImGui::DragFloat2("Tiling Factor", glm::value_ptr(component.TilingFactor), 0.1f, 0.0f, 100.0f);
		});

		DrawComponent<CircleRendererComponent>("Circle Renderer (2D)", entity, [](auto& component)
		{
			ImGui::ColorEdit4("Color", glm::value_ptr(component.Color));
			ImGui::DragFloat("Thickness", &component.Thickness, 0.025f, 0.0f, 1.0f);
			ImGui::DragFloat("Fade", &component.Fade, 0.00025f, 0.0f, 1.0f);
		});

		DrawComponent<Rigidbody2DComponent>("Rigidbody (2D)", entity, [](auto& component)
		{
			const char* bodyTypeStrings[] = { "Static", "Dynamic", "Kinematic" };
			const char* currentBodyTypeString = bodyTypeStrings[(int)component.Type];

			if (ImGui::BeginCombo("Body type", currentBodyTypeString))
			{
				for (int i = 0; i < 3; i++)
				{
					bool isSelected = (currentBodyTypeString == bodyTypeStrings[i]);
					if (ImGui::Selectable(bodyTypeStrings[i], isSelected))
					{
						currentBodyTypeString = bodyTypeStrings[i];
						component.Type = (Rigidbody2DComponent::BodyType)i;
					}

					if (isSelected)
						ImGui::SetItemDefaultFocus();
				}

				ImGui::EndCombo();
			}

			ImGui::Checkbox("Fixed Rotation", &component.FixedRotation);
		});

		DrawComponent<BoxCollider2DComponent>("Box Collider (2D)", entity, [](auto& component)
		{
			ImGui::DragFloat2("Offset", glm::value_ptr(component.Offset));
			ImGui::DragFloat2("Size", glm::value_ptr(component.Size));
			ImGui::DragFloat("Density", &component.Density, 0.01f, 0.0f, 5.0f);
			ImGui::DragFloat("Friction", &component.Friction, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat("Restitution", &component.Restitution, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat("Restitution Threshold", &component.RestitutionThreshold, 0.01f, 0.0f);
		});

		DrawComponent<CircleCollider2DComponent>("Circle Collider (2D)", entity, [](auto& component)
		{
			ImGui::DragFloat2("Offset", glm::value_ptr(component.Offset));
			ImGui::DragFloat("Radius", &component.Radius);
			ImGui::DragFloat("Density", &component.Density, 0.01f, 0.0f, 5.0f);
			ImGui::DragFloat("Friction", &component.Friction, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat("Restitution", &component.Restitution, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat("Restitution Threshold", &component.RestitutionThreshold, 0.01f, 0.0f);
		});

		DrawComponent<PolygonCollider2DComponent>("Polygon Collider (2D)", entity, [](auto& component)
		{
			ImGui::DragFloat2("Offset", glm::value_ptr(component.Offset));
			ImGui::DragFloat("Density", &component.Density, 0.01f, 0.0f, 5.0f);
			ImGui::DragFloat("Friction", &component.Friction, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat("Restitution", &component.Restitution, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat("Restitution Threshold", &component.RestitutionThreshold, 0.01f, 0.0f);
			ImGui::Text("Vertices Count: %zu", component.Vertices.size());
			if (ImGui::Button("Add Vertex"))
				component.Vertices.push_back({ 0.0f, 0.0f });
		});
	}

	template<typename T>
	void SceneHierarchyPanel::DisplayAddComponentEntry(const std::string& entryName) {
		if (std::is_same_v<T, ScriptComponent>)
		{
			if (ImGui::MenuItem(entryName.c_str()))
			{
				if (!m_SelectionContext.HasComponent<ScriptComponent>())
					m_SelectionContext.AddComponent<ScriptComponent>();

				auto& sc = m_SelectionContext.GetComponent<ScriptComponent>();
				sc.ScriptPaths.push_back("");
				ImGui::CloseCurrentPopup();
			}
		}
		else if (!m_SelectionContext.HasComponent<T>())
		{
			if (ImGui::MenuItem(entryName.c_str()))
			{
				m_SelectionContext.AddComponent<T>();
				ImGui::CloseCurrentPopup();
			}
		}
	}
}