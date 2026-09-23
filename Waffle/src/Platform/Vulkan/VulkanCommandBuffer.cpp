#include "wfpch.h"
#include "VulkanCommandBuffer.h"
#include "VulkanContext.h"
#include "VulkanShader.h"
#include "VulkanTexture.h"
#include "VulkanFramebuffer.h"
#include "VulkanVertexArray.h"
#include "VulkanUniformBuffer.h"
#include "VulkanBuffer.h"

namespace Waffle {

	// =========================================================================
	// VulkanGraphicsPipeline
	// =========================================================================
	VkPipeline VulkanGraphicsPipeline::Resolve(const VulkanVertexArray* vertexArray)
	{
		auto* ctx = VulkanContext::Get();
		auto* shader = dynamic_cast<VulkanShader*>(m_Desc.Shader.get());
		WF_CORE_ASSERT(shader, "VulkanGraphicsPipeline requires a Vulkan shader!");

		VkPrimitiveTopology topology = (m_Desc.Topology == Topology::Lines)
			? VK_PRIMITIVE_TOPOLOGY_LINE_LIST
			: VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

		return shader->GetOrCreatePipeline(vertexArray, topology,
			ctx->GetActiveColorFormats(), ctx->GetActiveDepthFormat(),
			(int)m_Desc.Blending, m_Desc.DepthTest, m_Desc.DepthWrite);
	}

		static void BindVertexBuffersAtCurrentSlice(VkCommandBuffer cmd, const VulkanVertexArray* va)
	{
		const auto& vertexBuffers = va->GetVertexBuffers();
		if (vertexBuffers.empty())
			return;

		std::vector<VkBuffer> buffers;
		std::vector<VkDeviceSize> offsets;
		buffers.reserve(vertexBuffers.size());
		offsets.reserve(vertexBuffers.size());
		for (const auto& vb : vertexBuffers)
		{
			auto* vkvb = dynamic_cast<VulkanVertexBuffer*>(vb.get());
			WF_CORE_ASSERT(vkvb, "Vertex array holds a non-Vulkan vertex buffer!");
			// Dynamic vertex buffers are ring-sliced; the draw must read the
			// slice its batch was uploaded to, not offset 0.
			buffers.push_back(vkvb->GetVulkanBuffer());
			offsets.push_back(vkvb ? vkvb->GetCurrentOffset() : 0);
		}
		vkCmdBindVertexBuffers(cmd, 0, (uint32_t)buffers.size(), buffers.data(), offsets.data());
	}

// =========================================================================
	// VulkanCommandBuffer - passes
	// =========================================================================
	void VulkanCommandBuffer::SetClearColor(const glm::vec4& color)
	{
		VkClearColorValue c{};
		c.float32[0] = color.r;
		c.float32[1] = color.g;
		c.float32[2] = color.b;
		c.float32[3] = color.a;
		VulkanContext::Get()->SetClearColor(c);
	}

	void VulkanCommandBuffer::Clear()
	{
		auto* ctx = VulkanContext::Get();
		if (ctx->IsRenderingActive())
			return; // already inside a pass - its load ops own the clearing

		BeginSwapchainPass(ctx->GetSwapChainExtent().width, ctx->GetSwapChainExtent().height);
	}

	void VulkanCommandBuffer::BeginRenderPass(const Ref<Framebuffer>& target, bool clear)
	{
		WF_CORE_ASSERT(target, "BeginRenderPass requires a framebuffer - use BeginSwapchainPass for the present surface!");
		auto* vkfb = dynamic_cast<VulkanFramebuffer*>(target.get());
		WF_CORE_ASSERT(vkfb, "BeginRenderPass - not a Vulkan framebuffer!");
		vkfb->SetClearOnBegin(clear);
		vkfb->Bind();
		m_ActiveFramebuffer = target;
		m_InSwapchainPass = false;
	}

	void VulkanCommandBuffer::BeginSwapchainPass(uint32_t width, uint32_t height)
	{
		auto* ctx = VulkanContext::Get();
		VkCommandBuffer cmd = ctx->GetCurrentCommandBuffer();

		if (ctx->IsRenderingActive())
		{
			// A pass is still open - close it so layouts stay consistent
			// before rendering to the present surface. The framebuffer's
			// Unbind owns vkCmdEndRendering AND its layout transitions;
			// ending rendering manually here as well would double-end.
			if (m_ActiveFramebuffer)
			{
				m_ActiveFramebuffer->Unbind();
				m_ActiveFramebuffer = nullptr;
			}
			else
			{
				vkCmdEndRendering(cmd);
				ctx->SetRenderingActive(false);
			}
		}

		VkClearDepthStencilValue depth{ 1.0f, 0 };
		ctx->BeginSwapChainRendering(ctx->GetClearColor(), depth);

		// Negative height maps Vulkan's Y-down NDC onto the engine's Y-up
		// camera convention (same flip the RendererAPI applies).
		VkViewport vp
		{
			.x = 0.0f, .y = (float)height,
			.width = (float)width, .height = -(float)height,
			.minDepth = 0.0f, .maxDepth = 1.0f
		};
		VkRect2D sc{ .offset{.x = 0, .y = 0}, .extent{.width = width, .height = height} };
		ctx->SetViewport(vp, sc);
		vkCmdSetViewport(cmd, 0, 1, &vp);
		vkCmdSetScissor(cmd, 0, 1, &sc);

		m_InSwapchainPass = true;
	}

	void VulkanCommandBuffer::EndRenderPass()
	{
		auto* ctx = VulkanContext::Get();
		if (m_ActiveFramebuffer)
		{
			m_ActiveFramebuffer->Unbind();
			m_ActiveFramebuffer = nullptr;
			m_InSwapchainPass = false;
		}
		else if (m_InSwapchainPass)
		{
			ctx->EndSwapChainRendering();
			m_InSwapchainPass = false;
		}
	}

	// =========================================================================
	// VulkanCommandBuffer - state
	// =========================================================================
	void VulkanCommandBuffer::BindPipeline(const Ref<GraphicsPipeline>& pipeline)
	{
		WF_CORE_ASSERT(pipeline, "BindPipeline - null pipeline!");
		m_BoundPipeline = pipeline;
	}

	void VulkanCommandBuffer::BindTextures(uint32_t firstSlot, const Ref<Texture2D>* textures, uint32_t count)
	{
		for (uint32_t i = 0; i < count; i++)
		{
			if (textures[i])
				textures[i]->Bind(firstSlot + i);
		}
	}

	void VulkanCommandBuffer::BindFramebufferAttachment(uint32_t slot, const Ref<Framebuffer>& target, uint32_t attachmentIndex)
	{
		auto* vkfb = dynamic_cast<VulkanFramebuffer*>(target.get());
		WF_CORE_ASSERT(vkfb, "BindFramebufferAttachment - not a Vulkan framebuffer!");
		VulkanContext::Get()->RegisterTexture(slot,
			vkfb->GetColorAttachmentView(attachmentIndex),
			vkfb->GetColorAttachmentSampler(attachmentIndex));
	}

	void VulkanCommandBuffer::UpdateUniformBuffer(const Ref<UniformBuffer>& ubo, const void* data, uint64_t size, uint64_t offset)
	{
		// The Vulkan UBO advances an internal ring slice per update; its
		// descriptor set is bound with dynamic offsets, so this recorded
		// pass keeps reading the data written for it even when a later pass
		// overwrites the "current" camera.
		ubo->SetData(data, (uint32_t)size, (uint32_t)offset);
	}

	// =========================================================================
	// VulkanCommandBuffer - draws
	// =========================================================================
	void VulkanCommandBuffer::EnsureRenderingActive()
	{
		auto* ctx = VulkanContext::Get();
		if (ctx->IsRenderingActive())
			return;

		BeginSwapchainPass(ctx->GetSwapChainExtent().width, ctx->GetSwapChainExtent().height);
	}

	void VulkanCommandBuffer::DrawIndexed(const Ref<VertexArray>& vertexArray, uint32_t indexCount, uint32_t indexOffset)
	{
		WF_PROFILE_FUNCTION();

		auto* ctx = VulkanContext::Get();
		VkCommandBuffer cmd = ctx->GetCurrentCommandBuffer();
		EnsureRenderingActive();

		auto* pipeline = dynamic_cast<VulkanGraphicsPipeline*>(m_BoundPipeline.get());
		if (!pipeline)
		{
			WF_CORE_WARN("VulkanCommandBuffer::DrawIndexed called with no bound pipeline!");
			return;
		}

		const VulkanVertexArray* va = dynamic_cast<const VulkanVertexArray*>(vertexArray.get());
		if (!va)
		{
			WF_CORE_WARN("VulkanCommandBuffer::DrawIndexed - invalid vertex array!");
			return;
		}

		VkPipeline vkPipeline = pipeline->Resolve(va);
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vkPipeline);

		auto* shader = dynamic_cast<VulkanShader*>(pipeline->GetShader().get());
		if (!shader)
			return;
		shader->BindAndFlushDescriptors(cmd, ctx->GetCurrentFrameIndex());
		shader->FlushPushConstants(cmd);

		VkViewport vp = ctx->GetCurrentViewport();
		VkRect2D   sc = ctx->GetCurrentScissor();
		vkCmdSetViewport(cmd, 0, 1, &vp);
		vkCmdSetScissor(cmd, 0, 1, &sc);

		BindVertexBuffersAtCurrentSlice(cmd, va);

		VkBuffer idxBuf = va->GetVkIndexBuffer();
		WF_CORE_ASSERT(idxBuf != VK_NULL_HANDLE, "No index buffer bound!");
		vkCmdBindIndexBuffer(cmd, idxBuf, 0, VK_INDEX_TYPE_UINT32);

		uint32_t count = indexCount ? indexCount : vertexArray->GetIndexBuffer()->GetCount();
		vkCmdDrawIndexed(cmd, count, 1, indexOffset, 0, 0);
	}

	void VulkanCommandBuffer::DrawLines(const Ref<VertexArray>& vertexArray, uint32_t vertexCount, uint32_t vertexOffset)
	{
		WF_PROFILE_FUNCTION();

		auto* ctx = VulkanContext::Get();
		VkCommandBuffer cmd = ctx->GetCurrentCommandBuffer();
		EnsureRenderingActive();

		auto* pipeline = dynamic_cast<VulkanGraphicsPipeline*>(m_BoundPipeline.get());
		if (!pipeline)
		{
			WF_CORE_WARN("VulkanCommandBuffer::DrawLines called with no bound pipeline!");
			return;
		}

		const VulkanVertexArray* va = dynamic_cast<const VulkanVertexArray*>(vertexArray.get());
		if (!va)
		{
			WF_CORE_WARN("VulkanCommandBuffer::DrawLines - invalid vertex array!");
			return;
		}

		VkPipeline vkPipeline = pipeline->Resolve(va);
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vkPipeline);

		auto* shader = dynamic_cast<VulkanShader*>(pipeline->GetShader().get());
		if (!shader)
			return;
		shader->BindAndFlushDescriptors(cmd, ctx->GetCurrentFrameIndex());
		shader->FlushPushConstants(cmd);

		VkViewport vp = ctx->GetCurrentViewport();
		VkRect2D   sc = ctx->GetCurrentScissor();
		vkCmdSetViewport(cmd, 0, 1, &vp);
		vkCmdSetScissor(cmd, 0, 1, &sc);

		BindVertexBuffersAtCurrentSlice(cmd, va);

		vkCmdDraw(cmd, vertexCount, 1, vertexOffset, 0);
	}

	void VulkanCommandBuffer::SetLineWidth(float width)
	{
		vkCmdSetLineWidth(VulkanContext::Get()->GetCurrentCommandBuffer(), width);
	}

	void VulkanCommandBuffer::SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height, bool flipY)
	{
		auto* ctx = VulkanContext::Get();
		VkCommandBuffer cmd = ctx->GetCurrentCommandBuffer();

		VkViewport vp
		{
			.x = (float)x,
			.y = flipY ? (float)(y + height) : (float)y,
			.width = (float)width,
			.height = flipY ? -(float)height : (float)height,
			.minDepth = 0.0f,
			.maxDepth = 1.0f
		};
		VkRect2D sc{ .offset{.x = (int32_t)x, .y = (int32_t)y}, .extent{.width = width, .height = height} };

		ctx->SetViewport(vp, sc);
		vkCmdSetViewport(cmd, 0, 1, &vp);
		vkCmdSetScissor(cmd, 0, 1, &sc);
	}

}
