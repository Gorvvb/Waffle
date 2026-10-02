#pragma once

#include "Waffle/RHI/GraphicsPipeline.h"
#include "Waffle/RHI/CommandBuffer.h"

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include <Volk/volk.h>

namespace Waffle {

	class VulkanShader;
	class VulkanVertexArray;

	// VulkanGraphicsPipeline - VkPipeline resolved and cached on first draw (target formats aren't known at Create() time).
	class VulkanGraphicsPipeline : public GraphicsPipeline
	{
	public:
		VulkanGraphicsPipeline(const Desc& desc)
			: GraphicsPipeline(desc) {}

		VkPipeline Resolve(const VulkanVertexArray* vertexArray);
	};

	// VulkanCommandBuffer - records into the current frame's command buffer; state comes from explicit binds, not the context.
	class VulkanCommandBuffer : public CommandBuffer
	{
	public:
		virtual void SetClearColor(const glm::vec4& color) override;
		virtual void Clear() override;

		virtual void BeginRenderPass(const Ref<Framebuffer>& target, bool clear = true) override;
		virtual void BeginSwapchainPass(uint32_t width, uint32_t height) override;
		virtual void EndRenderPass() override;

		virtual void BindPipeline(const Ref<GraphicsPipeline>& pipeline) override;

		virtual void BindTextures(uint32_t firstSlot, const Ref<Texture2D>* textures, uint32_t count) override;
		virtual void BindFramebufferAttachment(uint32_t slot, const Ref<Framebuffer>& target, uint32_t attachmentIndex) override;

		virtual void UpdateUniformBuffer(const Ref<UniformBuffer>& ubo, const void* data, uint64_t size, uint64_t offset) override;

		virtual void DrawIndexed(const Ref<VertexArray>& vertexArray, uint32_t indexCount, uint32_t indexOffset) override;
		virtual void DrawLines(const Ref<VertexArray>& vertexArray, uint32_t vertexCount, uint32_t vertexOffset) override;

		virtual void SetLineWidth(float width) override;
		virtual void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height, bool flipY = false) override;

	private:
		// DrawOrBegin rendering fallback shared by the draw entry points.
		void EnsureRenderingActive();

		Ref<GraphicsPipeline> m_BoundPipeline;
		Ref<Framebuffer>      m_ActiveFramebuffer;
		bool                  m_InSwapchainPass = false;
	};

}
