#pragma once

#include <functional>

#include "Waffle/Core/Base.h"
#include "Waffle/Core/Log.h"
#include "Waffle/Scene/Scene.h"
#include "Waffle/Scene/Entity.h"

namespace Waffle {

	// Which collider the viewport "edit collider" gizmo acts on.
	enum class ColliderEditTarget { Box = 0, Circle = 1 };

	class SceneHierarchyPanel
	{
	private:
		Ref<Scene> m_Context;
		Entity m_SelectionContext;

		Entity m_RenamingEntity;
		char m_RenameBuffer[256] = "";

		std::function<bool(int)> m_IsEditingCollider;
		std::function<void(int, bool)> m_SetEditingCollider;

		bool m_PrefabEditMode = false;
		std::string m_PrefabName;
		std::string m_PrefabSourceSceneName;
		std::function<void()> m_OnPrefabBack;
	public:
		SceneHierarchyPanel() = default;
		SceneHierarchyPanel(const Ref<Scene>& context);

		void SetContext(const Ref<Scene>& context);
		
		void OnImGuiRender();

		Entity GetSelectedEntity() const { return m_SelectionContext; }
		void SetSelectedEntity(Entity entity);

		// Collider edit mode hooks - the toggle state lives in EditorLayer
		// because the viewport gizmo acts on it (Unity-style "Edit Collider").
		void SetColliderEditHooks(
			std::function<bool(int)> isEditing,
			std::function<void(int, bool)> setEditing)
		{
			m_IsEditingCollider = std::move(isEditing);
			m_SetEditingCollider = std::move(setEditing);
		}

		// Prefab edit mode: the header shows a Back button + which prefab is
		// being edited (and the scene it came from) instead of the scene name.
		void SetPrefabEditMode(bool active, const std::string& prefabName = "",
			const std::string& sourceSceneName = "",
			std::function<void()> onBack = nullptr)
		{
			m_PrefabEditMode = active;
			m_PrefabName = prefabName;
			m_PrefabSourceSceneName = sourceSceneName;
			m_OnPrefabBack = std::move(onBack);
		}
	private:
		template<typename T>
		void DisplayAddComponentEntry(const std::string& entryName);

		void DrawEntityNode(Entity entity);
		void DrawComponents(Entity entity);

		// Creates a UI element entity (RectTransform + mirrored transform),
		// parented to the scene's UI canvas when one exists.
		Entity CreateUIElement(const std::string& name);
	};
}