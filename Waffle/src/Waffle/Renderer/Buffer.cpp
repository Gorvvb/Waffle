#include "wfpch.h"
#include "Buffer.h"

#include "Renderer.h"

#include "Platform/OpenGL/OpenGLBuffer.h"

namespace Waffle {

	VertexBuffer* VertexBuffer::Create(float* verticies, uint32_t size)
	{
		switch (Renderer::GetAPI())
		{
		case RendererAPI::None:
			WF_CORE_ASSERT(false, "RendererAPI::None is currently not supported");
			return nullptr;
		case RendererAPI::OpenGL:
			return new OpenGLVertexBuffer(verticies, size);
		}

		WF_CORE_ASSERT(false, "Unknown RendererAPI");
		return nullptr;
	}

	IndexBuffer* IndexBuffer::Create(uint32_t* indecies, uint32_t count)
	{
		switch (Renderer::GetAPI())
		{
		case RendererAPI::None:
			WF_CORE_ASSERT(false, "RendererAPI::None is currently not supported");
			return nullptr;
		case RendererAPI::OpenGL:
			return new OpenGLIndexBuffer(indecies, count);
		}

		WF_CORE_ASSERT(false, "Unknown RendererAPI");
		return nullptr;
	}

}