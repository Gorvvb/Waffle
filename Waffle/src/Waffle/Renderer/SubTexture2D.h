#pragma once

#include "Waffle/Renderer/Texture.h"
#include <glm/glm.hpp>
#include <memory>

#include "Waffle/Core/Ref.h"

namespace Waffle {

	class SubTexture2D : public RefCounted
	{
	public:
		SubTexture2D(const Ref<Texture2D>& texture, const glm::vec2& min, const glm::vec2& max);

		const Ref<Texture2D>& GetTexture() const { return m_Texture; }
		const glm::vec2* GetTexCoords() const { return m_TexCoords; }

		// Factory function to slice a spritesheet by grid column/row index or pixel coordinates
		static Ref<SubTexture2D> CreateFromCoords(const Ref<Texture2D>& texture, const glm::vec2& coords, const glm::vec2& cellSize, const glm::vec2& spriteSize = { 1.0f, 1.0f });

	private:
		Ref<Texture2D> m_Texture;
		glm::vec2 m_TexCoords[4];
	};

}
