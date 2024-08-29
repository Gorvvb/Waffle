#pragma once

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
	public:
		SceneHierarchyPanel() = default;
		SceneHierarchyPanel(const Ref<Scene>& context);

		void SetContext(const Ref<Scene>& context);
		
		void OnImGuiRender();
	private:
		void DrawEntityNode(Entity entity);
		void DrawComponents(Entity entity);
	};
}