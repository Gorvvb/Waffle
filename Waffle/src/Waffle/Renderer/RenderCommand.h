#pragma once

#include "RendererAPI.h"

namespace Waffle {

	class RenderCommand
	{
	private:
		static RendererAPI* s_RendererAPI;
	public:
		static void Init()
		{
			WF_PROFILE_FUNCTION();

			s_RendererAPI->Init();
		}

		static void SetViewPort(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
		{
			s_RendererAPI->SetViewPort(x, y, width, height);
		}

		static void SetClearColor(const glm::vec4& color)
		{
			s_RendererAPI->SetClearColor(color);
		}

		static void Clear()
		{
			s_RendererAPI->Clear();
		}

		static void DrawIndexed(const Ref<VertexArray>& vertexArray, uint32_t count = 0)
		{
			s_RendererAPI->DrawIndexed(vertexArray, count);
		}
	};
}