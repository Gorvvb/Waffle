#pragma once

#include "Waffle/Renderer/Buffer.h"

namespace Waffle {

	class OpenGLVertexBuffer : public VertexBuffer
	{
	private:
		uint32_t m_RendererID;
		BufferLayout m_Layout;
	public:
		OpenGLVertexBuffer(uint32_t size);
		OpenGLVertexBuffer(float* vertecies, uint32_t size);
		virtual ~OpenGLVertexBuffer();

		// Owns a raw GL handle - copying would double-delete it.
		OpenGLVertexBuffer(const OpenGLVertexBuffer&) = delete;
		OpenGLVertexBuffer& operator=(const OpenGLVertexBuffer&) = delete;

		virtual void Bind() const override;
		virtual void Unbind() const override;

		virtual void SetData(const void* data, uint32_t size) override;

		virtual const BufferLayout& GetLayout() const override { return m_Layout; }
		virtual void SetLayout(const BufferLayout& layout) override { m_Layout = layout; }
	};

	class OpenGLIndexBuffer : public IndexBuffer
	{
	private:
		uint32_t m_RendererID;
		uint32_t m_Count;
	public:
		OpenGLIndexBuffer(uint32_t* indecies, uint32_t count);
		virtual ~OpenGLIndexBuffer();

		// Owns a raw GL handle - copying would double-delete it.
		OpenGLIndexBuffer(const OpenGLIndexBuffer&) = delete;
		OpenGLIndexBuffer& operator=(const OpenGLIndexBuffer&) = delete;

		virtual void Bind() const override;
		virtual void Unbind() const override;

		virtual uint32_t GetCount() const { return m_Count; }
	};
}