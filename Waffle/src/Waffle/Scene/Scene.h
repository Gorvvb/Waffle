#pragma once

#include "entt.hpp"

namespace Waffle {

	class Scene
	{
	private:
		entt::registry m_Registry;
	public:
		Scene();
		~Scene();
	};
}