#pragma once

#include "Scene.h"

namespace Waffle {

	class SceneSerializer
	{
	private:
		Ref<Scene> m_Scene;
	public:
		SceneSerializer(const Ref<Scene>& scene);

		// Returns false when the file could not be opened or written.
		bool Serialize(const std::string& filepath);

		bool Deserialize(const std::string& filepath);

		static bool SerializeEntityToPrefab(Entity entity, const std::string& filepath);

		// Undo snapshots: full entity (all components + tag + UUID) as one YAML string.
		static std::string SerializeEntityToString(Entity entity);
		// Restores a snapshotted entity into the scene: destroys any existing entity with the same
		// UUID and recreates it from the snapshot (hierarchy references stay valid - UUID-stable).
		static Entity RestoreEntityFromSnapshot(Scene* scene, const std::string& yaml);
		static Entity DeserializePrefabToEntity(Scene* scene, const std::string& filepath, float x = 0.0f, float y = 0.0f);
	};
}