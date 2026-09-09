#pragma once

#include "Waffle/Renderer/UniformBuffer.h"

namespace Waffle {

	class OpenGLUniformBuffer : public UniformBuffer
	{
	private:
		uint32_t m_RendererID = 0;
	public:
		OpenGLUniformBuffer(uint32_t size, uint32_t binding);
		virtual ~OpenGLUniformBuffer();

		// Owns a raw GL handle - copying would double-delete it.
		OpenGLUniformBuffer(const OpenGLUniformBuffer&) = delete;
		OpenGLUniformBuffer& operator=(const OpenGLUniformBuffer&) = delete;

		virtual void SetData(const void* data, uint32_t size, uint32_t offset = 0) override;
	};
}