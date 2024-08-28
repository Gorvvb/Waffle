#pragma once

#include "Scene.h"

#include "entt.hpp"

namespace Waffle {

	class Entity
	{
	private:
		entt::entity m_EntityHandle{ 0 };
		Scene* m_Scene = nullptr;
	public:
		Entity() = default;
		Entity(entt::entity handle, Scene* scene);
		Entity(const Entity& other) = default;

		template<typename T, typename... Args>
		T& AddComponent(Args&&... args)
		{
			WF_CORE_ASSERT(!HasComponent<T>(), "Entity already has the component!");
			return m_Scene->m_Registry.emplace<T>(m_EntityHandle, std::forward<Args>(args)...);
		}

		template<typename T>
		T& GetComponent()
		{
			WF_CORE_ASSERT(HasComponent<T>(), "Entity does not have the component!");
			return m_Scene->m_Registry.get<T>(m_EntityHandle);
		}

		template<typename T>
		bool HasComponent()
		{
			return m_Scene->m_Registry.all_of<T>(m_EntityHandle); // If it doesn't work try any_of
		}

		template<typename T>
		bool RemoveComponent()
		{
			WF_CORE_ASSERT(HasComponent<T>(), "Entity does not have the component!");
			m_Scene->m_Registry.remove<T>(m_EntityHandle);
		}

		operator bool() const { return m_EntityHandle != entt::null; }
	};
}