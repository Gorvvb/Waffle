#include "wfpch.h"
#include "Frustum2D.h"

namespace Waffle {

	Frustum2D Frustum2D::FromOrthographic(float orthoSize, float aspectRatio, const glm::mat4& transform)
	{
		Frustum2D frustum;
		float orthoLeft = -orthoSize * aspectRatio * 0.5f;
		float orthoRight = orthoSize * aspectRatio * 0.5f;
		float orthoBottom = -orthoSize * 0.5f;
		float orthoTop = orthoSize * 0.5f;

		glm::vec3 center = transform[3];

		frustum.m_Bounds.Min = glm::vec2(center.x + orthoLeft, center.y + orthoBottom);
		frustum.m_Bounds.Max = glm::vec2(center.x + orthoRight, center.y + orthoTop);

		return frustum;
	}

	Frustum2D Frustum2D::FromProjectionAndView(const glm::mat4& projection, const glm::mat4& view)
	{
		Frustum2D frustum;
		glm::mat4 invVP = glm::inverse(projection * view);

		glm::vec4 ndcNear[4] = {
			{ -1.0f, -1.0f, -1.0f, 1.0f },
			{  1.0f, -1.0f, -1.0f, 1.0f },
			{  1.0f,  1.0f, -1.0f, 1.0f },
			{ -1.0f,  1.0f, -1.0f, 1.0f }
		};

		glm::vec4 ndcFar[4] = {
			{ -1.0f, -1.0f, 1.0f, 1.0f },
			{  1.0f, -1.0f, 1.0f, 1.0f },
			{  1.0f,  1.0f, 1.0f, 1.0f },
			{ -1.0f,  1.0f, 1.0f, 1.0f }
		};

		glm::vec2 minPt( 1e9f);
		glm::vec2 maxPt(-1e9f);

		for (int i = 0; i < 4; i++)
		{
			glm::vec4 nearW = invVP * ndcNear[i];
			glm::vec4 farW = invVP * ndcFar[i];

			if (nearW.w != 0.0f) nearW /= nearW.w;
			if (farW.w != 0.0f) farW /= farW.w;

			glm::vec3 p0 = glm::vec3(nearW);
			glm::vec3 dir = glm::vec3(farW) - p0;

			// Intersect ray with z = 0 world plane for 2D sprites
			if (std::abs(dir.z) > 0.00001f)
			{
				float t = -p0.z / dir.z;
				if (t >= 0.0f && t <= 1.0f)
				{
					glm::vec3 planePt = p0 + t * dir;
					minPt.x = glm::min(minPt.x, planePt.x);
					minPt.y = glm::min(minPt.y, planePt.y);
					maxPt.x = glm::max(maxPt.x, planePt.x);
					maxPt.y = glm::max(maxPt.y, planePt.y);
					continue;
				}
			}

			// Fallback: near plane point
			minPt.x = glm::min(minPt.x, nearW.x);
			minPt.y = glm::min(minPt.y, nearW.y);
			maxPt.x = glm::max(maxPt.x, nearW.x);
			maxPt.y = glm::max(maxPt.y, nearW.y);
		}

		frustum.m_Bounds.Min = minPt;
		frustum.m_Bounds.Max = maxPt;
		return frustum;
	}

	bool Frustum2D::IsVisible(const AABB2D& bounds) const
	{
		return m_Bounds.Intersects(bounds);
	}

	bool Frustum2D::IsVisible(const glm::vec2& position, const glm::vec2& size) const
	{
		glm::vec2 halfSize = size * 0.5f;
		AABB2D bounds(position - halfSize, position + halfSize);
		return m_Bounds.Intersects(bounds);
	}

}
