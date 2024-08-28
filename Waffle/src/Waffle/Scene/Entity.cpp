#include "wfpch.h"
#include "Entity.h"

namespace Waffle {

	Entity::Entity(entt::entity handle, Scene* scene)
		: m_EntityHandle(handle), m_Scene(scene)
	{

	}
}