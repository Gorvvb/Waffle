#include "SceneHierarchyPanel.h"
#include "Waffle/Scene/Components.h"

#include <imgui/imgui.h>

namespace Waffle {

	SceneHierarchyPanel::SceneHierarchyPanel(const Ref<Scene>& context)
	{
		SetContext(context);

	}

	void SceneHierarchyPanel::SetContext(const Ref<Scene>& context)
	{
		m_Context = context;
	}

	void SceneHierarchyPanel::OnImGuiRender()
	{
        ImGui::Begin("Scene Hierarchy");

        /*auto view = m_Context->m_Registry.view<TagComponent>();

		view.each([&](auto entity, auto& tagComponent) {
			ImGui::Text("%s", tagComponent.Tag.c_str());
		});*/

		auto view = m_Context->m_Registry.view<TagComponent>();

		for (auto entityID : view)
		{
			Entity entity{ entityID, m_Context.get() };

			DrawEntityNode(entity);
		}

        ImGui::End();
	}

	void SceneHierarchyPanel::DrawEntityNode(Entity entity)
	{
		auto& tagComponent = entity.GetComponent<TagComponent>().Tag;
		//ImGui::Text("%s", tagComponent.c_str());
		ImGuiTreeNodeFlags flags = ((m_SelectionContext == entity) ? ImGuiTreeNodeFlags_Selected : 0) | ImGuiTreeNodeFlags_OpenOnArrow;
		bool opened = ImGui::TreeNodeEx((void*)(uint64_t)(uint32_t)entity, flags, tagComponent.c_str());

		if (ImGui::IsItemClicked())
		{
			m_SelectionContext = entity;
		}

		if (opened)
		{
			ImGuiTreeNodeFlags flags = ((m_SelectionContext == entity) ? ImGuiTreeNodeFlags_Selected : 0) | ImGuiTreeNodeFlags_OpenOnArrow;
			bool opened = ImGui::TreeNodeEx((void*)9817239, flags, tagComponent.c_str());
			if (opened)
				ImGui::TreePop();
			ImGui::TreePop();
		}
	}
}