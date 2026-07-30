#pragma once

#include <glm/glm.hpp>

namespace Waffle {
	namespace Math {

		// Extracts Translation, Euler Rotation (radians), and Scale from a 4x4 Transformation Matrix
		bool DecomposeTransform(const glm::mat4& transform, glm::vec3& translation, glm::vec3& rotation, glm::vec3& scale);
	}
}