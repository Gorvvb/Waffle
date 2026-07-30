#include "wfpch.h"
#include "Math.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>

namespace Waffle {
	namespace Math {

		bool DecomposeTransform(const glm::mat4& transform, glm::vec3& translation, glm::vec3& rotation, glm::vec3& scale)
		{
			using namespace glm;
			using T = float;

			mat4 LocalMatrix(transform);

			// Normalize the matrix.
			if (epsilonEqual(LocalMatrix[3][3], static_cast<float>(0), epsilon<T>()))
				return false;

			// First, isolate perspective.  This is the messiest.
			if (
				epsilonNotEqual(LocalMatrix[0][3], static_cast<T>(0), epsilon<T>()) ||
				epsilonNotEqual(LocalMatrix[1][3], static_cast<T>(0), epsilon<T>()) ||
				epsilonNotEqual(LocalMatrix[2][3], static_cast<T>(0), epsilon<T>()))
			{
				// Clear the perspective partition
				LocalMatrix[0][3] = LocalMatrix[1][3] = LocalMatrix[2][3] = static_cast<T>(0);
				LocalMatrix[3][3] = static_cast<T>(1);
			}

			// Next take care of translation (easy).
			translation = vec3(LocalMatrix[3]);
			LocalMatrix[3] = vec4(0, 0, 0, LocalMatrix[3].w);

			vec3 Row[3];

			// Now get scale and shear.
			for (length_t i = 0; i < 3; ++i)
				for (length_t j = 0; j < 3; ++j)
					Row[i][j] = LocalMatrix[i][j];

			// Compute scale factors
			scale.x = length(Row[0]);
			scale.y = length(Row[1]);
			scale.z = length(Row[2]);

			const T eps = static_cast<T>(1e-6);

			if (scale.x > eps) Row[0] /= scale.x; else Row[0] = vec3(1, 0, 0);
			if (scale.y > eps) Row[1] /= scale.y; else Row[1] = vec3(0, 1, 0);
			if (scale.z > eps) Row[2] /= scale.z; else Row[2] = vec3(0, 0, 1);

			// Clamp scale away from 0 to prevent zero-matrix lockup
			if (scale.x < 0.0001f) scale.x = 0.0001f;
			if (scale.y < 0.0001f) scale.y = 0.0001f;
			if (scale.z < 0.0001f) scale.z = 0.0001f;

			// Clamp value inside [-1, 1] to prevent asin domain NaN errors
			T sinY = glm::clamp(-Row[0][2], static_cast<T>(-1), static_cast<T>(1));
			rotation.y = asin(sinY);

			if (abs(cos(rotation.y)) > eps) {
				rotation.x = atan2(Row[1][2], Row[2][2]);
				rotation.z = atan2(Row[0][1], Row[0][0]);
			}
			else {
				rotation.x = atan2(-Row[2][0], Row[1][1]);
				rotation.z = 0;
			}

			if (std::isnan(translation.x) || std::isnan(translation.y) || std::isnan(translation.z) ||
				std::isnan(rotation.x) || std::isnan(rotation.y) || std::isnan(rotation.z) ||
				std::isnan(scale.x) || std::isnan(scale.y) || std::isnan(scale.z))
			{
				return false;
			}

			return true;
		}
	}
}