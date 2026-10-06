#pragma once

#include "RendererAPI.h"

// Thin facade over the backend's RendererAPI for a handful of global-ish calls that survived the RHI migration (clear color, viewport, device limits); all drawing and state binding goes through Waffle::CommandBuffer (see Waffle/RHI/CommandBuffer.h).

namespace Waffle {

	class VulkanRendererAPI;
	class OpenGLRendererAPI;

	class RenderCommand
	{
	private:
		static RendererAPI* s_RendererAPI;
	public:
		static void Init();

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

		static uint32_t GetMaxTextureSlots()
		{
			return s_RendererAPI->GetMaxTextureSlots();
		}
	};
}
