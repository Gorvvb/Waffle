#include "wfpch.h"
#include "Frustum2D.h"
#include <glm/gtc/matrix_transform.hpp>

namespace Waffle {

	Frustum2D Frustum2D::FromViewProjection(const glm::mat4& viewProj)
	{
		Frustum2D frustum;

		frustum.m_Planes[0] = glm::vec4(viewProj[0][3] + viewProj[0][0], viewProj[1][3] + viewProj[1][0], viewProj[2][3] + viewProj[2][0], viewProj[3][3] + viewProj[3][0]);
		frustum.m_Planes[1] = glm::vec4(viewProj[0][3] - viewProj[0][0], viewProj[1][3] - viewProj[1][0], viewProj[2][3] - viewProj[2][0], viewProj[3][3] - viewProj[3][0]);
		frustum.m_Planes[2] = glm::vec4(viewProj[0][3] + viewProj[0][1], viewProj[1][3] + viewProj[1][1], viewProj[2][3] + viewProj[2][1], viewProj[3][3] + viewProj[3][1]);
		frustum.m_Planes[3] = glm::vec4(viewProj[0][3] - viewProj[0][1], viewProj[1][3] - viewProj[1][1], viewProj[2][3] - viewProj[2][1], viewProj[3][3] - viewProj[3][1]);
		frustum.m_Planes[4] = glm::vec4(viewProj[0][3] + viewProj[0][2], viewProj[1][3] + viewProj[1][2], viewProj[2][3] + viewProj[2][2], viewProj[3][3] + viewProj[3][2]);
		frustum.m_Planes[5] = glm::vec4(viewProj[0][3] - viewProj[0][2], viewProj[1][3] - viewProj[1][2], viewProj[2][3] - viewProj[2][2], viewProj[3][3] - viewProj[3][2]);

		for (int i = 0; i < 6; i++)
		{
			float len = glm::length(glm::vec3(frustum.m_Planes[i]));
			if (len > 0.00001f)
				frustum.m_Planes[i] /= len;
		}

		return frustum;
	}

	Frustum2D Frustum2D::FromOrthographic(float orthoSize, float aspectRatio, const glm::mat4& transform)
	{
		float orthoLeft = -orthoSize * aspectRatio * 0.5f;
		float orthoRight = orthoSize * aspectRatio * 0.5f;
		float orthoBottom = -orthoSize * 0.5f;
		float orthoTop = orthoSize * 0.5f;

		glm::mat4 proj = glm::ortho(orthoLeft, orthoRight, orthoBottom, orthoTop, -1000.0f, 1000.0f);
		glm::mat4 view = glm::inverse(transform);
		return FromViewProjection(proj * view);
	}

	Frustum2D Frustum2D::FromProjectionAndView(const glm::mat4& projection, const glm::mat4& view)
	{
		return FromViewProjection(projection * view);
	}

	bool Frustum2D::IsVisible(const glm::vec3& min, const glm::vec3& max) const
	{
		for (int i = 0; i < 4; i++)
		{
			const glm::vec4& plane = m_Planes[i];
			glm::vec3 normal(plane.x, plane.y, plane.z);

			glm::vec3 positiveVertex(
				plane.x >= 0.0f ? max.x : min.x,
				plane.y >= 0.0f ? max.y : min.y,
				plane.z >= 0.0f ? max.z : min.z
			);

			if (glm::dot(normal, positiveVertex) + plane.w < 0.0f)
				return false;
		}
		return true;
	}

	bool Frustum2D::IsVisible(const AABB2D& bounds) const
	{
		return IsVisible(glm::vec3(bounds.Min, -1.0f), glm::vec3(bounds.Max, 1.0f));
	}

	bool Frustum2D::IsVisible(const glm::vec2& position, const glm::vec2& size) const
	{
		glm::vec2 halfSize = size * 0.5f;
		return IsVisible(glm::vec3(position - halfSize, -1.0f), glm::vec3(position + halfSize, 1.0f));
	}

}
