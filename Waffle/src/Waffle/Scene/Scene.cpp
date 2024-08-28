#include "wfpch.h"
#include "Scene.h"

#include "Components.h"

#include "Waffle/Renderer/Renderer2D.h"

#include <glm/glm.hpp>

#include "Entity.h"

namespace Waffle {

	Scene::Scene()
	{
	}

	Scene::~Scene()
	{

	}

	Entity Scene::CreateEntity(const std::string& name)
	{
		Entity entity = { m_Registry.create(), this };
		entity.AddComponent<TransformComponent>();
		auto& tag = entity.AddComponent<TagComponent>();
		tag.Tag = name.empty() ? "Unnamed Entity" : name;
		return entity;
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