#pragma once

#include "Waffle/Core/Timestep.h"

#include "entt.hpp"

namespace Waffle {

	class Entity;

	class Scene
	{
	private:
		entt::registry m_Registry;

		friend class Entity;
	public:
		Scene();
		~Scene();

		Entity CreateEntity(const std::string& name = std::string());

		void OnUpdate(Timestep ts);
	};
}