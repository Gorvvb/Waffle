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

	// -------------------------------------------------------------------------
	// VulkanGraphicsPipeline - explicit pipeline object. The VkPipeline itself
	// is resolved (and cached) from the bound vertex array's input layout, the
	// topology, and the ACTIVE render target's formats at first draw - the
	// formats are a property of the target being rendered into, so they cannot
	// be known at Create() time. Resolution goes through the shader's pipeline
	// cache, so each distinct combination is created exactly once.
	// -------------------------------------------------------------------------
	class VulkanGraphicsPipeline : public GraphicsPipeline
	{
	public:
		VulkanGraphicsPipeline(const Desc& desc)
			: GraphicsPipeline(desc) {}

		VkPipeline Resolve(const VulkanVertexArray* vertexArray);
	};

	// -------------------------------------------------------------------------
	// VulkanCommandBuffer - records into the current frame's command buffer.
	// Draw state comes from this object's explicit binds, not from the
	// context's global state.
	// -------------------------------------------------------------------------
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
