#pragma once

#include "Waffle/Renderer/Texture.h"

#include <glad/glad.h>

namespace Waffle {

	class OpenGlTexture2D : public Texture2D
	{
	private:
		std::string m_Path;
		uint32_t m_Width, m_Height;
		uint32_t m_RendererID;

		GLenum m_InternalFormat, m_DataFormat;
	public:
		OpenGlTexture2D(uint32_t width, uint32_t height);
		OpenGlTexture2D(const std::string& path);

		virtual ~OpenGlTexture2D();

		virtual uint32_t GetWidth() const override { return m_Width; }
		virtual uint32_t GetHeight() const override { return m_Height; }

		virtual void SetData(void* data, uint32_t size) override;

		virtual void Bind(uint32_t slot = 0) const override;

		virtual bool operator==(const Texture& other) const override 
		{ 
			return m_RendererID == ((OpenGlTexture2D&)other).m_RendererID;
		};
	};
}