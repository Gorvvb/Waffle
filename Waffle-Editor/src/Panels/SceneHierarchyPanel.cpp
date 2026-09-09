#include "SceneHierarchyPanel.h"

#include "Waffle/Scene/Components.h"
#include "Waffle/Scene/SceneSerializer.h"
#include "Waffle/Utils/PlatformUtils.h"

#include "Waffle/Scripting/LuaScriptEngine.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include "Waffle/ImGui/ImGuiUtilities.h"
#include "Waffle/ImGui/ImGuiWidgets.h"

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
		// A stale m_RenamingEntity keeps the Delete-key handler disabled
		// after a scene switch mid-rename.
		m_RenamingEntity = {};
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

			// Don't delete while a rename/text edit is in progress - Delete is a text-editing key there
			if (m_SelectionContext && !m_RenamingEntity && !ImGui::IsAnyItemActive() && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && ImGui::IsKeyPressed(ImGuiKey_Delete))
			{
				m_Context->DestroyEntity(m_SelectionContext);
				m_SelectionContext = {};
			}

			if (ImGui::IsMouseDown(0) && ImGui::IsWindowHovered())
				m_SelectionContext = {};

			// Drag and drop onto empty hierarchy space:
			// - entities are unparented
			// - prefabs from the Content Browser are instantiated as root entities
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
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
				{
					const wchar_t* pathStr = (const wchar_t*)payload->Data;
					std::filesystem::path path = std::filesystem::path(g_AssetPath) / pathStr;
					std::string ext = path.extension().string();
					for (auto& c : ext) c = (char)tolower(c);
					if (ext == ".prefab")
					{
						Entity instantiated = SceneSerializer::DeserializePrefabToEntity(
							m_Context.get(), path.string());
						if (instantiated)
						{
							m_Context->CreateRuntimePhysicsBody(instantiated); // no-op in edit mode
							m_SelectionContext = instantiated;
						}
					}
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
			strncpy_s(m_RenameBuffer, sizeof(m_RenameBuffer), tag.c_str(), _TRUNCATE);
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
					std::filesystem::path fullPath = std::filesystem::path(g_AssetPath) / path;
					std::filesystem::path relPath = std::filesystem::relative(fullPath, g_AssetPath);
					std::string relStr = relPath.string();
					sc.ScriptPaths.push_back(relStr);

					// Scrape fields so they appear immediately without reload
					LuaScriptEngine::ScrapeFieldsFromScript(fullPath, relStr, sc);
				}
				else if (ext == ".prefab")
				{
					// Instantiate the prefab as a child of the drop-target entity
					Entity instantiated = SceneSerializer::DeserializePrefabToEntity(
						m_Context.get(), path.string());
					if (instantiated)
					{
						m_Context->CreateRuntimePhysicsBody(instantiated); // no-op in edit mode
						m_Context->ParentEntity(instantiated, entity);
						m_SelectionContext = instantiated;
					}
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
				strncpy_s(m_RenameBuffer, sizeof(m_RenameBuffer), tag.c_str(), _TRUNCATE);
			}
			if (ImGui::MenuItem("Duplicate Entity"))
				entityDuplicated = true;
			if (ImGui::MenuItem("Save as Prefab"))
			{
				std::string entityName = tag.empty() ? "Entity" : tag;
				std::filesystem::path prefabsDir = g_AssetPath / "Prefabs";
				if (!std::filesystem::exists(prefabsDir))
					std::filesystem::create_directories(prefabsDir);

				std::filesystem::path prefabPath = prefabsDir / (entityName + ".prefab");
				int counter = 1;
				while (std::filesystem::exists(prefabPath))
				{
					prefabPath = prefabsDir / (entityName + std::to_string(counter++) + ".prefab");
				}

				SceneSerializer::SerializeEntityToPrefab(entity, prefabPath.string());
				WF_CORE_INFO("Saved entity '{0}' as prefab '{1}'", entityName, prefabPath.string());
			}
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

	template<typename T, typename UIFunction>
	static void DrawComponent(const std::string& name, Entity entity, UIFunction uiFunction)
	{
		const ImGuiTreeNodeFlags treeNodeFlags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding;
		if (entity.HasComponent<T>())
		{
			auto& component = entity.GetComponent<T>();
			ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();

			ImGuiEx::ScopedStyle framePadding(ImGuiStyleVar_FramePadding, ImVec2{ 4, 4 });
			float lineHeight = ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y * 2.0f;

			ImGui::Separator();
			bool open = ImGui::TreeNodeEx((void*)typeid(T).hash_code(), treeNodeFlags, "%s", name.c_str());

			ImGui::SameLine(contentRegionAvailable.x - (lineHeight * 0.5f));
			if (ImGui::Button("+", ImVec2{ lineHeight, lineHeight }))
			{
				ImGui::OpenPopup("ComponentSettings");
			}

			bool removeComponent = false;
			if (ImGui::BeginPopup("ComponentSettings"))
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
		ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();

		if (entity.HasComponent<TagComponent>())
		{
			auto& tag = entity.GetComponent<TagComponent>().Tag;

			char buffer[256];
			memset(buffer, 0, sizeof(buffer));
			strncpy_s(buffer, sizeof(buffer), tag.c_str(), _TRUNCATE);

			float buttonWidth = 120.0f;
			float labelWidth = 35.0f;
			float tagWidth = contentRegionAvailable.x - labelWidth - buttonWidth - 15.0f;
			if (tagWidth < 50.0f) tagWidth = 50.0f;

			ImGui::Text("Tag");
			ImGui::SameLine();
			ImGui::PushItemWidth(tagWidth);
			if (ImGui::InputText("##Tag", buffer, sizeof(buffer)))
			{
				tag = std::string(buffer);
				if (tag.empty())
					tag = "Empty Entity";
			}
			ImGui::PopItemWidth();
		}

		ImGui::SameLine(contentRegionAvailable.x - 120.0f);
		if (ImGui::Button("Add Component", ImVec2(120.0f, 0.0f)))
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
			DisplayAddComponentEntry<AnimatorComponent>("Animator (2D)");

			ImGui::EndPopup();
		}

		ImGui::Spacing();

		DrawComponent<TransformComponent>("Transform", entity, [](auto& component)
		{
			UI::DrawVec3Control("Translation", component.Translation);
			glm::vec3 rotation = glm::degrees(component.Rotation);
			UI::DrawVec3Control("Rotation", rotation);
			component.Rotation = glm::radians(rotation);
			UI::DrawVec3Control("Scale", component.Scale, 1.0f);
		});

		DrawComponent<CameraComponent>("Camera", entity, [](auto& component)
		{
			auto& camera = component.Camera;

			UI::BeginPropertyGrid();
			UI::PropertyCheckbox("Primary", component.Primary);

			const char* projectionTypeString[] = { "Perspective", "Orthographic" };
			int currentProjection = (int)camera.GetProjectionType();
			if (UI::PropertyDropdown("Projection", projectionTypeString, 2, &currentProjection))
			{
				camera.SetProjectionType((SceneCamera::ProjectionType)currentProjection);
			}

			if (camera.GetProjectionType() == SceneCamera::ProjectionType::Perspective)
			{
				float verticalFOV = glm::degrees(camera.GetPerspectiveVerticalFOV());
				if (UI::PropertyFloat("Vertical FOV", verticalFOV))
					camera.SetPerspectiveVerticalFOV(glm::radians(verticalFOV));

				float perspectiveNear = camera.GetPerspectiveNearClip();
				if (UI::PropertyFloat("Near", perspectiveNear))
					camera.SetPerspectiveNearClip(perspectiveNear);

				float perspectiveFar = camera.GetPerspectiveFarClip();
				if (UI::PropertyFloat("Far", perspectiveFar))
					camera.SetPerspectiveFarClip(perspectiveFar);
			}

			if (camera.GetProjectionType() == SceneCamera::ProjectionType::Orthograpic)
			{
				float orthoSize = camera.GetOrthographicSize();
				if (UI::PropertyFloat("Size", orthoSize))
					camera.SetOrthographicSize(orthoSize);

				float orthoNear = camera.GetOrthographicNearClip();
				if (UI::PropertyFloat("Near", orthoNear))
					camera.SetOrthographicNearClip(orthoNear);

				float orthoFar = camera.GetOrthographicFarClip();
				if (UI::PropertyFloat("Far", orthoFar))
					camera.SetOrthographicFarClip(orthoFar);

				UI::PropertyCheckbox("Fixed Aspect Ratio", component.FixedAspectRatio);
			}

			UI::DrawColorEdit4("Background Color", component.BackgroundColor);
			UI::EndPropertyGrid();

			ImGui::Text("Background Image (Unfinished)");
			if (component.BackgroundImage)
			{
				if (component.BackgroundFilterMode == TextureFilter::Nearest)
				{
					if (ImGui::GetPlatformIO().DrawCallback_SetSamplerNearest)
						ImGui::GetWindowDrawList()->AddCallback(ImGui::GetPlatformIO().DrawCallback_SetSamplerNearest, nullptr);
				}
				else
				{
					if (ImGui::GetPlatformIO().DrawCallback_SetSamplerLinear)
						ImGui::GetWindowDrawList()->AddCallback(ImGui::GetPlatformIO().DrawCallback_SetSamplerLinear, nullptr);
				}

				ImGui::ImageButton("##BgTexture", (void*)(intptr_t)component.BackgroundImage->GetRendererID(), ImVec2(100.0f, 100.0f), ImVec2(0, 1), ImVec2(1, 0));

				if (ImGui::GetPlatformIO().DrawCallback_SetSamplerLinear)
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
			int currentFilterInt = static_cast<int>(component.BackgroundFilterMode);
			UI::BeginPropertyGrid();
			if (UI::PropertyDropdown("Filter Mode", filterOptions, 2, &currentFilterInt))
			{
				component.BackgroundFilterMode = static_cast<Waffle::TextureFilter>(currentFilterInt);
				if (component.BackgroundImage)
					component.BackgroundImage->SetFilter(component.BackgroundFilterMode);
			}
			UI::EndPropertyGrid();
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

				// Gather all .lua files for the dropdown from a CACHE -
				// this ran a full recursive asset-tree walk per scripted
				// entity PER FRAME. Refresh every 2 seconds.
				static std::vector<std::filesystem::path> s_LuaFileCache;
				static float s_LuaFileCacheAge = 1e9f;
				s_LuaFileCacheAge += ImGui::GetIO().DeltaTime;
				if (s_LuaFileCacheAge >= 2.0f && std::filesystem::exists(g_AssetPath))
				{
					s_LuaFileCacheAge = 0.0f;
					s_LuaFileCache.clear();
					std::error_code itEc;
					for (auto& entry : std::filesystem::recursive_directory_iterator(g_AssetPath, itEc))
					{
						if (itEc) break;
						if (entry.is_regular_file(itEc) && entry.path().extension() == ".lua")
						{
							s_LuaFileCache.push_back(std::filesystem::relative(entry.path(), g_AssetPath));
						}
					}
				}
				const std::vector<std::filesystem::path>& luaFiles = s_LuaFileCache;

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
							std::filesystem::path path(pathStr); // relative to the asset root
							if (path.extension() == ".lua")
							{
								// Keep the full asset-relative path - filename alone breaks scripts in subfolders
								std::string relStr = path.string();
								if (relStr != currentScript)
									component.Fields.erase(currentScript);
								component.ScriptPaths[i] = relStr;

								// Scrape fields immediately so they show in the inspector before Play
								std::filesystem::path fullPath = std::filesystem::path(g_AssetPath) / path;
								LuaScriptEngine::ScrapeFieldsFromScript(fullPath, relStr, component);
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
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SPRITESHEET_FRAME_ITEM"))
				{
					// Payload: "absoluteTexturePath|minX,minY,maxX,maxY"
					const char* dataStr = (const char*)payload->Data;
					if (dataStr && payload->DataSize > 1)
					{
						std::string payloadStr(dataStr);
						size_t pipePos = payloadStr.find('|');
						if (pipePos != std::string::npos)
						{
							std::string texPath = payloadStr.substr(0, pipePos);
							if (std::filesystem::exists(texPath))
								component.Texture = Texture2D::Create(texPath, component.FilterMode);
						}
					}
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
			ImGui::DragInt("Sorting Layer", &component.SortingLayer, 1.0f, -100, 100);
			ImGui::DragInt("Order in Layer", &component.SortingOrder, 1.0f, -1000, 1000);
		});

		DrawComponent<CircleRendererComponent>("Circle Renderer (2D)", entity, [](auto& component)
		{
			UI::BeginPropertyGrid();
			UI::DrawColorEdit4("Color", component.Color);
			UI::PropertyFloat("Thickness", component.Thickness, 0.025f, 0.0f, 1.0f);
			UI::PropertyFloat("Fade", component.Fade, 0.00025f, 0.0f, 1.0f);
			UI::PropertyInt("Sorting Layer", component.SortingLayer);
			UI::PropertyInt("Order in Layer", component.SortingOrder);
			UI::EndPropertyGrid();
		});

		DrawComponent<Rigidbody2DComponent>("Rigidbody (2D)", entity, [](auto& component)
		{
			UI::BeginPropertyGrid();
			const char* bodyTypeStrings[] = { "Static", "Dynamic", "Kinematic" };
			int currentBodyType = (int)component.Type;
			if (UI::PropertyDropdown("Body Type", bodyTypeStrings, 3, &currentBodyType))
			{
				component.Type = (Rigidbody2DComponent::BodyType)currentBodyType;
			}

			UI::PropertyCheckbox("Fixed Rotation", component.FixedRotation);
			UI::PropertyFloat("Mass", component.Mass, 0.1f, 0.01f, 1000.0f);
			UI::EndPropertyGrid();
		});

		DrawComponent<BoxCollider2DComponent>("Box Collider (2D)", entity, [](auto& component)
		{
			UI::BeginPropertyGrid();
			UI::PropertyFloat("Offset X", component.Offset.x, 0.05f);
			UI::PropertyFloat("Offset Y", component.Offset.y, 0.05f);
			UI::PropertyFloat("Size X", component.Size.x, 0.05f);
			UI::PropertyFloat("Size Y", component.Size.y, 0.05f);
			UI::PropertyCheckbox("Is Trigger", component.IsTrigger);
			UI::PropertyFloat("Density", component.Density, 0.01f, 0.0f, 5.0f);
			UI::PropertyFloat("Friction", component.Friction, 0.01f, 0.0f, 1.0f);
			UI::PropertyFloat("Restitution", component.Restitution, 0.01f, 0.0f, 1.0f);
			UI::PropertyFloat("Restitution Thresh.", component.RestitutionThreshold, 0.01f, 0.0f);
			UI::EndPropertyGrid();
		});

		DrawComponent<CircleCollider2DComponent>("Circle Collider (2D)", entity, [](auto& component)
		{
			UI::BeginPropertyGrid();
			UI::PropertyFloat("Offset X", component.Offset.x, 0.05f);
			UI::PropertyFloat("Offset Y", component.Offset.y, 0.05f);
			UI::PropertyFloat("Radius", component.Radius, 0.05f);
			UI::PropertyCheckbox("Is Trigger", component.IsTrigger);
			UI::PropertyFloat("Density", component.Density, 0.01f, 0.0f, 5.0f);
			UI::PropertyFloat("Friction", component.Friction, 0.01f, 0.0f, 1.0f);
			UI::PropertyFloat("Restitution", component.Restitution, 0.01f, 0.0f, 1.0f);
			UI::PropertyFloat("Restitution Thresh.", component.RestitutionThreshold, 0.01f, 0.0f);
			UI::EndPropertyGrid();
		});

		DrawComponent<PolygonCollider2DComponent>("Polygon Collider (2D)", entity, [](auto& component)
		{
			UI::BeginPropertyGrid();
			UI::PropertyFloat("Offset X", component.Offset.x, 0.05f);
			UI::PropertyFloat("Offset Y", component.Offset.y, 0.05f);
			UI::PropertyCheckbox("Is Trigger", component.IsTrigger);
			UI::PropertyFloat("Density", component.Density, 0.01f, 0.0f, 5.0f);
			UI::PropertyFloat("Friction", component.Friction, 0.01f, 0.0f, 1.0f);
			UI::PropertyFloat("Restitution", component.Restitution, 0.01f, 0.0f, 1.0f);
			UI::PropertyFloat("Restitution Thresh.", component.RestitutionThreshold, 0.01f, 0.0f);
			UI::EndPropertyGrid();

			ImGui::Text("Vertices Count: %zu", component.Vertices.size());
			if (ImGui::Button("Add Vertex"))
				component.Vertices.push_back({ 0.0f, 0.0f });
		});

		DrawComponent<AnimatorComponent>("Animator (2D)", entity, [](auto& component)
		{
			ImGui::Text("Current Clip: %s", component.CurrentClip.c_str());
			ImGui::Text("Frame: %d | Playing: %s", component.CurrentFrameIndex, component.IsPlaying ? "Yes" : "No");

			if (ImGui::Button(component.IsPlaying ? "Pause" : "Play"))
			{
				component.IsPlaying = !component.IsPlaying;
			}
			ImGui::SameLine();
			if (ImGui::Button("Stop"))
			{
				component.Stop();
			}

			ImGui::Separator();
			static char clipNameBuf[64] = "NewClip";
			ImGui::InputText("##NewClipName", clipNameBuf, sizeof(clipNameBuf));
			ImGui::SameLine();
			if (ImGui::Button("Add Clip"))
			{
				std::string name(clipNameBuf);
				if (!name.empty() && component.Clips.find(name) == component.Clips.end())
				{
					AnimationClip clip;
					clip.Name = name;
					component.Clips[name] = clip;
					if (component.CurrentClip.empty())
						component.CurrentClip = name;
				}
			}

			ImGui::Separator();
			std::vector<std::string> clipNames;
			for (auto& [name, clip] : component.Clips) clipNames.push_back(name);

			for (auto& clipName : clipNames)
			{
				auto& clip = component.Clips[clipName];
				bool isCurrent = (component.CurrentClip == clipName);
				std::string headerLabel = clipName + (isCurrent ? " (Active)" : "");
				if (ImGui::TreeNodeEx(headerLabel.c_str(), isCurrent ? ImGuiTreeNodeFlags_Selected : 0))
				{
					if (!isCurrent && ImGui::Button("Make Active Clip"))
					{
						component.Play(clipName);
					}

					ImGui::Text("Texture: %s", clip.TexturePath.empty() ? "None (Drag PNG Here)" : clip.TexturePath.c_str());
					if (ImGui::BeginDragDropTarget())
					{
						if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
						{
							const wchar_t* pathStr = (const wchar_t*)payload->Data;
							std::filesystem::path path = std::filesystem::path(g_AssetPath) / pathStr;
							std::string ext = path.extension().string();
							for (auto& c : ext) c = (char)tolower(c);
							if (ext == ".png" || ext == ".jpg" || ext == ".jpeg")
							{
								clip.TexturePath = std::filesystem::relative(path, g_AssetPath).string();
								clip.Texture = Texture2D::Create(path.string());
								clip.RefreshSubTextures();
							}
						}
						ImGui::EndDragDropTarget();
					}

					if (ImGui::DragInt("Columns", &clip.Columns, 1.0f, 1, 64)) clip.RefreshSubTextures();
					if (ImGui::DragInt("Rows", &clip.Rows, 1.0f, 1, 64)) clip.RefreshSubTextures();
					if (ImGui::DragInt("Start Frame", &clip.StartFrame, 1.0f, 0, std::max(0, clip.Columns * clip.Rows - 1))) clip.RefreshSubTextures();
					if (ImGui::DragInt("End Frame", &clip.EndFrame, 1.0f, 0, std::max(0, clip.Columns * clip.Rows - 1))) clip.RefreshSubTextures();
					ImGui::DragFloat("FPS", &clip.FPS, 0.5f, 0.1f, 120.0f);
					ImGui::Checkbox("Loop", &clip.Loop);

					if (ImGui::Button("Delete Clip"))
					{
						component.Clips.erase(clipName);
						if (component.CurrentClip == clipName) component.CurrentClip.clear();
					}

					ImGui::TreePop();
				}
			}
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