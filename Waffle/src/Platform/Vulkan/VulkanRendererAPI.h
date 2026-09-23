#pragma once

#include "Waffle/Renderer/RendererAPI.h"
#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include <Volk/volk.h>

// Drawing and pipeline binding live in VulkanCommandBuffer (RHI). This class
// only carries the legacy global-ish calls still reachable via RenderCommand.

namespace Waffle {

	class VulkanRendererAPI : public RendererAPI
	{
	public:
		virtual void Init() override;
		virtual void SetViewPort(uint32_t x, uint32_t y, uint32_t width, uint32_t height) override;

		virtual void SetClearColor(const glm::vec4& color) override;
		virtual void Clear() override;

		virtual uint32_t GetMaxTextureSlots() const override;
	};

}
