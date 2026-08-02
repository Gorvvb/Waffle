#pragma once

#include "Scene.h"

namespace Waffle {

	class SceneSerializer
	{
	private:
		Ref<Scene> m_Scene;
	public:
		SceneSerializer(const Ref<Scene>& scene);

		void Serialize(const std::string& filepath);
		void SerializeRuntime(const std::string& filepath);

		bool Deserialize(const std::string& filepath);
		bool DeserializeRuntime(const std::string& filepath);

		static bool SerializeEntityToPrefab(Entity entity, const std::string& filepath);
		static Entity DeserializePrefabToEntity(Scene* scene, const std::string& filepath, float x = 0.0f, float y = 0.0f);
	};
}