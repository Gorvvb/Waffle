#pragma once

#include <glm/glm.hpp>

namespace Waffle {

	struct AABB2D
	{
		glm::vec2 Min = glm::vec2(0.0f);
		glm::vec2 Max = glm::vec2(0.0f);

		AABB2D() = default;
		AABB2D(const glm::vec2& min, const glm::vec2& max)
			: Min(min), Max(max) {}

		bool Intersects(const AABB2D& other) const
		{
			return (Min.x <= other.Max.x && Max.x >= other.Min.x) &&
			       (Min.y <= other.Max.y && Max.y >= other.Min.y);
		}

		bool Contains(const glm::vec2& point) const
		{
			return (point.x >= Min.x && point.x <= Max.x) &&
			       (point.y >= Min.y && point.y <= Max.y);
		}
	};

	class Frustum2D
	{
	public:
		Frustum2D() = default;

		static Frustum2D FromOrthographic(float orthoSize, float aspectRatio, const glm::mat4& transform);
		static Frustum2D FromProjectionAndView(const glm::mat4& projection, const glm::mat4& view);

		bool IsVisible(const AABB2D& bounds) const;
		bool IsVisible(const glm::vec2& position, const glm::vec2& size) const;

		const AABB2D& GetBounds() const { return m_Bounds; }

	private:
		AABB2D m_Bounds;
	};

}
