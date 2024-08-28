#pragma once

#include "Waffle/Core/Timestep.h"

#include "entt.hpp"

namespace Waffle {

	class Scene
	{
	private:
		entt::registry m_Registry;
	public:
		Scene();
		~Scene();

		// TEMPORARY
		entt::entity CreateEntity();
		entt::registry& Reg() { return m_Registry; }

		void OnUpdate(Timestep ts);
	};
}