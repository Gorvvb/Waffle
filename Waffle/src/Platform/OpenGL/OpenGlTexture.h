#pragma once

#include "Waffle/Renderer/Texture.h"

namespace Waffle {

	class OpenGlTexture2D : public Texture2D
	{
	private:
		std::string m_Path;
		uint32_t m_Width, m_Height;
		uint32_t m_RendererID;
	public:
		OpenGlTexture2D(const std::string& path);

		virtual ~OpenGlTexture2D();

		virtual uint32_t GetWidth() const override { return m_Width; }
		virtual uint32_t GetHeight() const override { return m_Height; }

		virtual void Bind(uint32_t slot = 0) const override;
	};
}