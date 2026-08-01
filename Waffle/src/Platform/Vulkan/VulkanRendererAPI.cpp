#include "wfpch.h"
#include "VulkanRendererAPI.h"
#include "VulkanContext.h"
#include "VulkanShader.h"
#include "VulkanVertexArray.h"
#include "VulkanFramebuffer.h"

namespace Waffle {

	void VulkanRendererAPI::Init()
	{
		WF_PROFILE_FUNCTION();
		// The VulkanContext handles all initialisation.
		// Nothing extra needed here.
	}

	void VulkanRendererAPI::SetViewPort(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
	{
		auto* ctx = VulkanContext::Get();
		VkCommandBuffer cmd = ctx->GetCurrentCommandBuffer();

		VkViewport vp{};
		vp.x        = (float)x;
		vp.y        = (float)y;
		vp.width    = (float)width;
		vp.height   = (float)height;
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
		if (ctx->IsRenderingActive()) return; // already rendering — skip

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

	// -------------------------------------------------------------------------
	// DrawIndexed
	// -------------------------------------------------------------------------
	void VulkanRendererAPI::DrawIndexed(const Ref<VertexArray>& vertexArray, uint32_t indexCount)
	{
		uint32_t count = indexCount ? indexCount : vertexArray->GetIndexBuffer()->GetCount();
		BindPipelineAndDraw(vertexArray, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, count, true);
	}

	// -------------------------------------------------------------------------
	// DrawLines
	// -------------------------------------------------------------------------
	void VulkanRendererAPI::DrawLines(const Ref<VertexArray>& vertexArray, uint32_t vertexCount)
	{
		BindPipelineAndDraw(vertexArray, VK_PRIMITIVE_TOPOLOGY_LINE_LIST, vertexCount, false);
	}

	// -------------------------------------------------------------------------
	// SetLineWidth
	// -------------------------------------------------------------------------
	void VulkanRendererAPI::SetLineWidth(float width)
	{
		auto* ctx = VulkanContext::Get();
		vkCmdSetLineWidth(ctx->GetCurrentCommandBuffer(), width);
	}

	// =========================================================================
	// BindPipelineAndDraw
	// =========================================================================
	void VulkanRendererAPI::BindPipelineAndDraw(const Ref<VertexArray>& vertexArray,
		VkPrimitiveTopology topology,
		uint32_t count,
		bool indexed)
	{
		WF_PROFILE_FUNCTION();

		auto* ctx = VulkanContext::Get();
		VkCommandBuffer cmd = ctx->GetCurrentCommandBuffer();

		// ---- Ensure rendering is active ------------------------------------
		if (!ctx->IsRenderingActive())
		{
			VkClearDepthStencilValue depth{ 1.0f, 0 };
			ctx->BeginSwapChainRendering(ctx->GetClearColor(), depth);
			VkViewport vp = ctx->GetCurrentViewport();
			VkRect2D sc   = ctx->GetCurrentScissor();
			vkCmdSetViewport(cmd, 0, 1, &vp);
			vkCmdSetScissor(cmd, 0, 1, &sc);
		}

		// ---- Resolve shader -----------------------------------------------
		auto* shader = ctx->GetBoundShader();
		if (!shader)
		{
			WF_CORE_WARN("VulkanRendererAPI::DrawIndexed called with no bound shader!");
			return;
		}

		// ---- Resolve vertex array -----------------------------------------
		const VulkanVertexArray* va =
			dynamic_cast<const VulkanVertexArray*>(vertexArray.get());
		if (!va)
		{
			WF_CORE_WARN("VulkanRendererAPI::Draw: invalid vertex array!");
			return;
		}

		// ---- Get / create pipeline ----------------------------------------
		// ADD THESE LOGS:
        VkPipeline pipeline = shader->GetOrCreatePipeline(va, topology, ctx->GetActiveColorFormats(), ctx->GetActiveDepthFormat());
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

		// ---- Bind descriptor sets -----------------------------------------
		shader->BindAndFlushDescriptors(cmd, ctx->GetCurrentFrameIndex());

		// ---- Push constants -----------------------------------------------
		shader->FlushPushConstants(cmd);

		// ---- Viewport / scissor (dynamic) ---------------------------------
		VkViewport vp = ctx->GetCurrentViewport();
		VkRect2D   sc = ctx->GetCurrentScissor();
		vkCmdSetViewport(cmd, 0, 1, &vp);
		vkCmdSetScissor(cmd, 0, 1, &sc);

		// ---- Bind vertex buffers ------------------------------------------
		const auto& vkBuffers  = va->GetVkVertexBuffers();
		const auto& vkOffsets  = va->GetVkOffsets();

		if (!vkBuffers.empty())
		{
			vkCmdBindVertexBuffers(cmd, 0,
				(uint32_t)vkBuffers.size(),
				vkBuffers.data(),
				vkOffsets.data());
		}

		// ---- Draw ----------------------------------------------------------
		if (indexed)
		{
			VkBuffer idxBuf = va->GetVkIndexBuffer();
			WF_CORE_ASSERT(idxBuf != VK_NULL_HANDLE, "No index buffer bound!");
			vkCmdBindIndexBuffer(cmd, idxBuf, 0, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed(cmd, count, 1, 0, 0, 0);
		}
		else
		{
			vkCmdDraw(cmd, count, 1, 0, 0);
		}
	}

} // namespace Waffle
