#include "wfpch.h"
#include "VulkanRendererAPI.h"
#include "VulkanContext.h"

namespace Waffle {

	void VulkanRendererAPI::Init()
	{
		WF_PROFILE_FUNCTION();
		// The VulkanContext handles all initialisation.
		// Nothing extra needed here.
	}

	uint32_t VulkanRendererAPI::GetMaxTextureSlots() const
	{
		auto* ctx = VulkanContext::Get();
		if (!ctx || !ctx->GetPhysicalDevice())
			return 32;

		VkPhysicalDeviceProperties props{};
		vkGetPhysicalDeviceProperties(ctx->GetPhysicalDevice(), &props);
		// Vulkan guarantees at least 16 sampled images per stage.
		return props.limits.maxPerStageDescriptorSampledImages;
	}

	void VulkanRendererAPI::SetViewPort(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
	{
		auto* ctx = VulkanContext::Get();
		VkCommandBuffer cmd = ctx->GetCurrentCommandBuffer();

		// Negative height flips Vulkan's Y-down NDC to match OpenGL/GLM conventions.
		VkViewport vp{};
		vp.x        = (float)x;
		vp.y        = (float)(y + height);  // start at bottom of region
		vp.width    = (float)width;
		vp.height   = -(float)height;       // render upward
		vp.minDepth = 0.0f;
		vp.maxDepth = 1.0f;

		VkRect2D sc{};
		sc.offset = { (int32_t)x, (int32_t)y };
		sc.extent = { width, height };

		ctx->SetViewport(vp, sc);
		vkCmdSetViewport(cmd, 0, 1, &vp);
		vkCmdSetScissor(cmd, 0, 1, &sc);
	}

	void VulkanRendererAPI::SetClearColor(const glm::vec4& color)
	{
		VkClearColorValue c{};
		c.float32[0] = color.r;
		c.float32[1] = color.g;
		c.float32[2] = color.b;
		c.float32[3] = color.a;
		VulkanContext::Get()->SetClearColor(c);
	}

	void VulkanRendererAPI::Clear()
	{
		auto* ctx = VulkanContext::Get();
		if (ctx->IsRenderingActive()) return; // already rendering - skip

		// Begin swap-chain dynamic rendering with the stored clear color
		VkClearDepthStencilValue depth{ 1.0f, 0 };
		ctx->BeginSwapChainRendering(ctx->GetClearColor(), depth);

		// Apply viewport / scissor
		VkCommandBuffer cmd = ctx->GetCurrentCommandBuffer();
		VkViewport vp = ctx->GetCurrentViewport();
		VkRect2D   sc = ctx->GetCurrentScissor();
		vkCmdSetViewport(cmd, 0, 1, &vp);
		vkCmdSetScissor(cmd, 0, 1, &sc);
	}

}
