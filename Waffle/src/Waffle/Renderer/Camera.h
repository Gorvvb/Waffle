#pragma once

#include <glm/glm.hpp>

namespace Waffle {

	class Camera
	{
	private:
		glm::mat4 m_Projection;
	public:
		Camera(const glm::mat4& projection)
			: m_Projection(projection) {}

		const glm::mat4& GetProjection() const { return m_Projection; }
	};
}