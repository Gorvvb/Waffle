#include "wfpch.h"
#include "Frustum2D.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_access.hpp>

namespace Waffle {

	// Gribb-Hartmann plane extraction. glm::row() reads through GLM's
	// column-major storage, so this stays correct regardless of index-order
	// conventions - a hand-rolled m[i][j] variant silently extracts planes
	// from the TRANSPOSED matrix and mis-culls everything once the camera
	// moves away from the origin.
	Frustum2D Frustum2D::FromViewProjection(const glm::mat4& viewProj)
	{
		Frustum2D frustum;

		const glm::vec4 row0 = glm::row(viewProj, 0);
		const glm::vec4 row1 = glm::row(viewProj, 1);
		const glm::vec4 row2 = glm::row(viewProj, 2);
		const glm::vec4 row3 = glm::row(viewProj, 3);

		frustum.m_Planes[0] = row3 + row0; // left
		frustum.m_Planes[1] = row3 - row0; // right
		frustum.m_Planes[2] = row3 + row1; // bottom
		frustum.m_Planes[3] = row3 - row1; // top
		frustum.m_Planes[4] = row3 + row2; // near
		frustum.m_Planes[5] = row3 - row2; // far

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
		for (int i = 0; i < 6; i++)
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

	bool Frustum2D::IsVisible(const glm::vec2& position, const glm::vec2& size, float z) const
	{
		glm::vec2 halfSize = size * 0.5f;
		return IsVisible(glm::vec3(position - halfSize, z), glm::vec3(position + halfSize, z));
	}

}
