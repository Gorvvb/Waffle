#pragma once

#include <glm/glm.hpp>

namespace Waffle {

	class Camera
	{
	protected:
		glm::mat4 m_Projection = glm::mat4(1.0f);
	public:
		Camera() = default;
		Camera(const glm::mat4& projection)
			: m_Projection(projection) {}

		const glm::mat4& GetProjection() const { return m_Projection; }
	};
}