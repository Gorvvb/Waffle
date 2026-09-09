#pragma once

#include "Waffle/Renderer/VertexArray.h"

namespace Waffle {

	class OpenGLVertexArray : public VertexArray
	{
	private:
		uint32_t m_RendererID;
		uint32_t m_VertexBufferIndex = 0;
		std::vector<Ref<VertexBuffer>> m_VertexBuffers;
		Ref<IndexBuffer> m_IndexBuffer;
	public:
		OpenGLVertexArray();
		virtual ~OpenGLVertexArray();

		// Owns a raw GL handle - copying would double-delete it.
		OpenGLVertexArray(const OpenGLVertexArray&) = delete;
		OpenGLVertexArray& operator=(const OpenGLVertexArray&) = delete;

		virtual void Bind() const override;
		virtual void Unbind() const override;

		virtual void AddVertexBuffer(const Ref<VertexBuffer>& vertexBuffer) override;
		virtual void SetIndexBuffer(const Ref<IndexBuffer>& indexBuffer) override;

		virtual const std::vector<Ref<VertexBuffer>>& GetVertexBuffers() const override { return m_VertexBuffers; }
		virtual const Ref<IndexBuffer>& GetIndexBuffer() const override { return m_IndexBuffer; }
	};
}