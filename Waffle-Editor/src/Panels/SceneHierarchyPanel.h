#pragma once

#include <functional>

#include "Waffle/Core/Base.h"
#include "Waffle/Core/Log.h"
#include "Waffle/Scene/Scene.h"
#include "Waffle/Scene/Entity.h"

namespace Waffle {

	class SceneHierarchyPanel
	{
	private:
		Ref<Scene> m_Context;
		Entity m_SelectionContext;

		Entity m_RenamingEntity;
		char m_RenameBuffer[256] = "";

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