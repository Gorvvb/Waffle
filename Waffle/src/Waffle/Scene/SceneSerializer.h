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
	};
}