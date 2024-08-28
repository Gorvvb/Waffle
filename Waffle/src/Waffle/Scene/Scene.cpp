#include "wfpch.h"
#include "Scene.h"

#include "Components.h"

#include "Waffle/Renderer/Renderer2D.h"

#include <glm/glm.hpp>

namespace Waffle {

	Scene::Scene()
	{
	}

	Scene::~Scene()
	{

	}

	entt::entity Scene::CreateEntity()
	{
		return entt::entity();
	}

	void Scene::OnUpdate(Timestep ts)
	{
		auto group = m_Registry.group<TransformComponent>(entt::get<SpriteRendererComponent>);
		for (auto entity : group)
		{
			auto components = group.get<TransformComponent, SpriteRendererComponent>(entity);
			auto& [transform, sprite] = components;

			Renderer2D::DrawQuad(transform, sprite.Color);
		}
	}
}