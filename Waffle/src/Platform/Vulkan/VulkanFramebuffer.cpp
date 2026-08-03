#include "wfpch.h"
#include "VulkanFramebuffer.h"
#include "VulkanContext.h"
#include "VulkanUtils.h"

#include "backends/imgui_impl_vulkan.h"

namespace Waffle {

	// -------------------------------------------------------------------------
	// Format helpers
	// -------------------------------------------------------------------------
	static bool IsDepthFormat(FramebufferTextureFormat fmt)
	{
		return fmt == FramebufferTextureFormat::DEPTH24STENCIL8;
	}

	static VkFormat WaffleFormatToVulkan(FramebufferTextureFormat fmt)
	{
		switch (fmt)
		{
		case FramebufferTextureFormat::RGBA8:       return VK_FORMAT_R8G8B8A8_UNORM;
		case FramebufferTextureFormat::RED_INTEGER:  return VK_FORMAT_R32_SINT;
		case FramebufferTextureFormat::DEPTH24STENCIL8: return VK_FORMAT_D24_UNORM_S8_UINT;
		default: WF_CORE_ASSERT(false); return VK_FORMAT_UNDEFINED;
		}
	}

	// =========================================================================
	// Constructor / Destructor
	// =========================================================================

	VulkanFramebuffer::VulkanFramebuffer(const FramebufferSpecification& spec)
		: m_Specification(spec)
	{
		for (auto& att : spec.Attachments.Attachments)
		{
			if (IsDepthFormat(att.TextureFormat))
				m_DepthAttachmentSpec = att;
			else
				m_ColorAttachmentSpecs.push_back(att);
		}
		Invalidate();
	}

	VulkanFramebuffer::~VulkanFramebuffer()
	{
		CleanupAttachments();
	}

	// =========================================================================
	// Invalidate — create/recreate all attachments
	// =========================================================================
	void VulkanFramebuffer::Invalidate()
	{
		if (!m_ColorImages.empty())
			CleanupAttachments();

		auto* ctx    = VulkanContext::Get();
		VkDevice dev = ctx->GetDevice();
		uint32_t w   = m_Specification.Width;
		uint32_t h   = m_Specification.Height;

		// --- Color attachments ---
		size_t numColor = m_ColorAttachmentSpecs.size();
		m_ColorImages.resize(numColor);
		m_ColorAllocations.resize(numColor);
		m_ColorImageViews.resize(numColor);
		m_ColorSamplers.resize(numColor, VK_NULL_HANDLE);
		m_ColorImGuiDescriptorSets.resize(numColor, VK_NULL_HANDLE);

		m_ColorFormats.resize(numColor);

		for (size_t i = 0; i < numColor; i++)
		{
			VkFormat fmt = WaffleFormatToVulkan(m_ColorAttachmentSpecs[i].TextureFormat);
			m_ColorFormats[i] = fmt;
			CreateAttachment(w, h, fmt,
				VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
				VK_IMAGE_USAGE_SAMPLED_BIT |
				VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
				VK_IMAGE_USAGE_TRANSFER_DST_BIT,
				m_ColorImages[i], m_ColorAllocations[i], m_ColorImageViews[i]);

			// Transition to shader-read layout (initial state for sampling)
			VkCommandBuffer cmd = ctx->BeginSingleTimeCommands();
			VulkanUtils::TransitionImageLayout(cmd, m_ColorImages[i],
				VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				0, VK_ACCESS_2_SHADER_READ_BIT,
				VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT);
			ctx->EndSingleTimeCommands(cmd);

			// Create sampler for this color attachment using C++20 designated initializers
			VkSamplerCreateInfo samplerInfo
			{
				.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
				.magFilter = VK_FILTER_LINEAR,
				.minFilter = VK_FILTER_LINEAR,
				.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
				.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
				.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
				.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
				.anisotropyEnable = VK_FALSE,
				.maxAnisotropy = 1.0f,
				.compareEnable = VK_FALSE,
				.compareOp = VK_COMPARE_OP_ALWAYS,
				.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
				.unnormalizedCoordinates = VK_FALSE
			};
			vkCreateSampler(dev, &samplerInfo, nullptr, &m_ColorSamplers[i]);
		}

		// --- Depth attachment ---
		if (m_DepthAttachmentSpec.TextureFormat != FramebufferTextureFormat::None)
		{
			m_DepthFormat = WaffleFormatToVulkan(m_DepthAttachmentSpec.TextureFormat);
			CreateAttachment(w, h, m_DepthFormat,
				VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
				m_DepthImage, m_DepthAllocation, m_DepthView);
		}

		// --- ImGui descriptor sets ---
		RegisterImGuiDescriptorSets();

		if (!m_ColorAttachmentSpecs.empty())
			m_ColorFormat = WaffleFormatToVulkan(m_ColorAttachmentSpecs[0].TextureFormat);
	}

	// =========================================================================
	// Bind — begin dynamic rendering for this framebuffer
	// =========================================================================
	void VulkanFramebuffer::Bind()
	{
		auto* ctx = VulkanContext::Get();
		ctx->SetActiveRenderingFormats(m_ColorFormats, m_DepthFormat);

		VkCommandBuffer cmd = ctx->GetCurrentCommandBuffer();

		// End any previously-active rendering (swap-chain or another FBO)
		if (ctx->IsRenderingActive())
		{
			vkCmdEndRendering(cmd);
			ctx->SetRenderingActive(false);
		}

		// Transition color attachments → attachment optimal using Synchronization 2
		for (size_t i = 0; i < m_ColorImages.size(); i++)
		{
			VulkanUtils::TransitionImageLayout(cmd, m_ColorImages[i],
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				VK_ACCESS_2_SHADER_READ_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
				VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
				VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);
		}

		// Build attachment infos
		std::vector<VkRenderingAttachmentInfo> colorAttachments(m_ColorImages.size());
		for (size_t i = 0; i < m_ColorImages.size(); i++)
		{
			colorAttachments[i] =
			{
				.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
				.imageView = m_ColorImageViews[i],
				.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
				.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
				.clearValue{.color = ctx->GetClearColor()}
			};
		}

		VkRenderingAttachmentInfo depthAttachment{};
		bool hasDepth = (m_DepthImage != VK_NULL_HANDLE);
		if (hasDepth)
		{
			VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
			if (VulkanUtils::HasStencilComponent(m_DepthFormat))
				aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;

			VulkanUtils::TransitionImageLayout(cmd, m_DepthImage,
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
				0, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
				VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
				VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
				aspectMask);

			depthAttachment =
			{
				.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
				.imageView = m_DepthView,
				.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
				.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
				.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.clearValue{.depthStencil{ 1.0f, 0 }}
			};
		}

		VkRenderingInfo renderingInfo
		{
			.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
			.renderArea
			{
				.offset{.x = 0, .y = 0},
				.extent{.width = m_Specification.Width, .height = m_Specification.Height}
			},
			.layerCount = 1,
			.colorAttachmentCount = static_cast<uint32_t>(colorAttachments.size()),
			.pColorAttachments = colorAttachments.empty() ? nullptr : colorAttachments.data(),
			.pDepthAttachment = hasDepth ? &depthAttachment : nullptr
		};

		vkCmdBeginRendering(cmd, &renderingInfo);
		ctx->SetRenderingActive(true);

		// Update viewport / scissor for this FBO's size.
		// NOTE: Do NOT use the negative-height Y-flip here — the FBO texture is
		// displayed by ImGui which handles UV flipping itself. The Y-flip only
		// belongs on the swapchain path (runtime/export).
		VkViewport vp
		{
			.x = 0.0f, .y = 0.0f,
			.width = static_cast<float>(m_Specification.Width),
			.height = static_cast<float>(m_Specification.Height),
			.minDepth = 0.0f, .maxDepth = 1.0f
		};
		VkRect2D sc
		{
			.offset{.x = 0, .y = 0},
			.extent{.width = m_Specification.Width, .height = m_Specification.Height}
		};
		ctx->SetViewport(vp, sc);
		vkCmdSetViewport(cmd, 0, 1, &vp);
		vkCmdSetScissor(cmd, 0, 1, &sc);

		m_IsRendering = true;
	}

	// =========================================================================
	// Unbind — end dynamic rendering, transition attachments back to shader read
	// =========================================================================
	void VulkanFramebuffer::Unbind()
	{
		auto* ctx = VulkanContext::Get();
		VkCommandBuffer cmd = ctx->GetCurrentCommandBuffer();

		if (m_IsRendering)
		{
			vkCmdEndRendering(cmd);
			ctx->SetRenderingActive(false);
			m_IsRendering = false;
		}

		// Transition color attachments → shader read
		for (size_t i = 0; i < m_ColorImages.size(); i++)
		{
			VulkanUtils::TransitionImageLayout(cmd, m_ColorImages[i],
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_2_SHADER_READ_BIT,
				VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
				VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT);
		}

		// Restore swap-chain viewport (negative height for Y-flip applies to
		// the swapchain/runtime path only — ImGui sets its own viewport so this
		// flip does not affect editor UI rendering).
		auto swapExtent = ctx->GetSwapChainExtent();
		VkViewport vp
		{
			.x = 0.0f,
			.y = static_cast<float>(swapExtent.height),
			.width = static_cast<float>(swapExtent.width),
			.height = -static_cast<float>(swapExtent.height),
			.minDepth = 0.0f, .maxDepth = 1.0f
		};
		VkRect2D sc{ .offset{.x = 0, .y = 0}, .extent = swapExtent };
		ctx->SetViewport(vp, sc);
		vkCmdSetViewport(cmd, 0, 1, &vp);
		vkCmdSetScissor(cmd, 0, 1, &sc);
		VulkanContext::Get()->SetBoundShader(nullptr);
	}

	// =========================================================================
	// Resize
	// =========================================================================
	void VulkanFramebuffer::Resize(uint32_t width, uint32_t height)
	{
		m_Specification.Width  = width;
		m_Specification.Height = height;
		Invalidate();
	}

	// =========================================================================
	// ReadPixel
	// =========================================================================
	// =========================================================================
	// ReadPixel
	// =========================================================================
	int VulkanFramebuffer::ReadPixel(uint32_t attachmentIndex, int x, int y)
	{
		WF_CORE_ASSERT(attachmentIndex < m_ColorImages.size());
		WF_CORE_ASSERT(m_ColorAttachmentSpecs[attachmentIndex].TextureFormat
			== FramebufferTextureFormat::RED_INTEGER,
			"ReadPixel currently only supports RED_INTEGER attachments in Vulkan");

		auto* ctx    = VulkanContext::Get();
		VkDevice dev = ctx->GetDevice();
		VmaAllocator allocator = ctx->GetVmaAllocator();

		bool wasRenderingActive = ctx->IsRenderingActive();
		VkCommandBuffer cmd = ctx->GetCurrentCommandBuffer();

		if (wasRenderingActive)
		{
			vkCmdEndRendering(cmd);
			ctx->SetRenderingActive(false);

			// Transition color attachment from COLOR_ATTACHMENT_OPTIMAL -> TRANSFER_SRC_OPTIMAL
			VulkanUtils::TransitionImageLayout(cmd, m_ColorImages[attachmentIndex],
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_2_TRANSFER_READ_BIT,
				VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT);
		}
		else
		{
			if (dev != VK_NULL_HANDLE)
				vkDeviceWaitIdle(dev);

			cmd = ctx->BeginSingleTimeCommands();

			VulkanUtils::TransitionImageLayout(cmd, m_ColorImages[attachmentIndex],
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				VK_ACCESS_2_SHADER_READ_BIT, VK_ACCESS_2_TRANSFER_READ_BIT,
				VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT);
		}

		// Create 4-byte staging buffer via VMA
		VkBuffer      staging;
		VmaAllocation stagingAllocation;
		VulkanUtils::CreateBuffer(allocator,
			sizeof(int32_t),
			VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VMA_MEMORY_USAGE_AUTO,
			staging, stagingAllocation,
			VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);

		int actualY = (int)m_Specification.Height - 1 - y;

		VkBufferImageCopy region
		{
			.bufferOffset = 0,
			.bufferRowLength = 0,
			.bufferImageHeight = 0,
			.imageSubresource
			{
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.mipLevel = 0,
				.baseArrayLayer = 0,
				.layerCount = 1
			},
			.imageOffset{.x = x, .y = actualY, .z = 0},
			.imageExtent{.width = 1, .height = 1, .depth = 1}
		};

		vkCmdCopyImageToBuffer(cmd, m_ColorImages[attachmentIndex],
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, staging, 1, &region);

		if (wasRenderingActive)
		{
			// Transition back TRANSFER_SRC_OPTIMAL -> COLOR_ATTACHMENT_OPTIMAL
			VulkanUtils::TransitionImageLayout(cmd, m_ColorImages[attachmentIndex],
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				VK_ACCESS_2_TRANSFER_READ_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
				VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);

			// End main command buffer recording and submit to GPU
			vkEndCommandBuffer(cmd);

			VkSubmitInfo submitInfo
			{
				.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
				.commandBufferCount = 1,
				.pCommandBuffers = &cmd
			};

			vkQueueSubmit(ctx->GetGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
			vkQueueWaitIdle(ctx->GetGraphicsQueue());

			// Re-begin command buffer for remaining calls in frame
			VkCommandBufferBeginInfo beginInfo
			{
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
				.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
			};
			vkBeginCommandBuffer(cmd, &beginInfo);

			// Resume dynamic rendering with LOAD_OP_LOAD to preserve rendered contents
			std::vector<VkRenderingAttachmentInfo> colorAttachments(m_ColorImages.size());
			for (size_t i = 0; i < m_ColorImages.size(); i++)
			{
				colorAttachments[i] =
				{
					.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
					.imageView = m_ColorImageViews[i],
					.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
					.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
					.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
					.clearValue{.color = ctx->GetClearColor()}
				};
			}

			VkRenderingAttachmentInfo depthAttachment{};
			bool hasDepth = (m_DepthImage != VK_NULL_HANDLE);
			if (hasDepth)
			{
				depthAttachment =
				{
					.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
					.imageView = m_DepthView,
					.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
					.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
					.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
					.clearValue{.depthStencil{ 1.0f, 0 }}
				};
			}

			VkRenderingInfo renderingInfo
			{
				.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
				.renderArea
				{
					.offset{.x = 0, .y = 0},
					.extent{.width = m_Specification.Width, .height = m_Specification.Height}
				},
				.layerCount = 1,
				.colorAttachmentCount = static_cast<uint32_t>(colorAttachments.size()),
				.pColorAttachments = colorAttachments.data(),
				.pDepthAttachment = hasDepth ? &depthAttachment : nullptr
			};

			vkCmdBeginRendering(cmd, &renderingInfo);
			ctx->SetRenderingActive(true);
		}
		else
		{
			// Transition back TRANSFER_SRC_OPTIMAL -> SHADER_READ_ONLY_OPTIMAL
			VulkanUtils::TransitionImageLayout(cmd, m_ColorImages[attachmentIndex],
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				VK_ACCESS_2_TRANSFER_READ_BIT, VK_ACCESS_2_SHADER_READ_BIT,
				VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT);

			ctx->EndSingleTimeCommands(cmd);
		}

		// Read result from host-mapped staging buffer
		VmaAllocationInfo allocInfo{};
		vmaGetAllocationInfo(allocator, stagingAllocation, &allocInfo);
		int result = *reinterpret_cast<int32_t*>(allocInfo.pMappedData);

		vmaDestroyBuffer(allocator, staging, stagingAllocation);

		return result;
	}

	// =========================================================================
	// ClearAttachment
	// =========================================================================
	void VulkanFramebuffer::ClearAttachment(uint32_t attachmentIndex, int value)
	{
		WF_CORE_ASSERT(attachmentIndex < m_ColorImages.size());

		auto* ctx = VulkanContext::Get();

		if (ctx->IsRenderingActive())
		{
			VkCommandBuffer cmd = ctx->GetCurrentCommandBuffer();

			VkClearAttachment clearAttachment{};
			clearAttachment.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			clearAttachment.colorAttachment = attachmentIndex;
			clearAttachment.clearValue.color.int32[0] = value;

			VkClearRect clearRect{};
			clearRect.rect.offset = { 0, 0 };
			clearRect.rect.extent = { m_Specification.Width, m_Specification.Height };
			clearRect.baseArrayLayer = 0;
			clearRect.layerCount = 1;

			vkCmdClearAttachments(cmd, 1, &clearAttachment, 1, &clearRect);
		}
		else
		{
			VkCommandBuffer cmd = ctx->BeginSingleTimeCommands();

			VulkanUtils::TransitionImageLayout(cmd, m_ColorImages[attachmentIndex],
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				VK_ACCESS_2_SHADER_READ_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
				VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT);

			VkClearColorValue clearVal{};
			clearVal.int32[0] = value;

			VkImageSubresourceRange range
			{
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1
			};

			vkCmdClearColorImage(cmd, m_ColorImages[attachmentIndex],
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearVal, 1, &range);

			VulkanUtils::TransitionImageLayout(cmd, m_ColorImages[attachmentIndex],
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_ACCESS_2_SHADER_READ_BIT,
				VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT);

			ctx->EndSingleTimeCommands(cmd);
		}
	}

	// =========================================================================
	// GetColorAttachmentRendererID
	// =========================================================================
	uint64_t VulkanFramebuffer::GetColorAttachmentRendererID(uint32_t index) const
	{
		WF_CORE_ASSERT(index < m_ColorImageViews.size());
		if (index >= m_ColorImGuiDescriptorSets.size() || !m_ColorImGuiDescriptorSets[index])
		{
			if (ImGui::GetCurrentContext() && ImGui::GetIO().BackendRendererUserData &&
				index < m_ColorSamplers.size() && index < m_ColorImageViews.size())
			{
				if (m_ColorImGuiDescriptorSets.size() <= index)
					const_cast<VulkanFramebuffer*>(this)->m_ColorImGuiDescriptorSets.resize(m_ColorImages.size(), VK_NULL_HANDLE);

				const_cast<VulkanFramebuffer*>(this)->m_ColorImGuiDescriptorSets[index] =
					ImGui_ImplVulkan_AddTexture(
						m_ColorSamplers[index],
						m_ColorImageViews[index],
						VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
			}
		}
		return (index < m_ColorImGuiDescriptorSets.size()) ? (uint64_t)m_ColorImGuiDescriptorSets[index] : 0;
	}

	// =========================================================================
	// Private helpers
	// =========================================================================

	void VulkanFramebuffer::CreateAttachment(uint32_t width, uint32_t height,
		VkFormat format, VkImageUsageFlags usage,
		VkImage& outImage, VmaAllocation& outAllocation, VkImageView& outView)
	{
		auto* ctx = VulkanContext::Get();
		VkDevice dev = ctx->GetDevice();

		VkImageAspectFlags aspect = VulkanUtils::HasDepthComponent(format)
			? VK_IMAGE_ASPECT_DEPTH_BIT
			: VK_IMAGE_ASPECT_COLOR_BIT;
		if (VulkanUtils::HasStencilComponent(format))
			aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;

		VulkanUtils::CreateImage(ctx->GetVmaAllocator(),
			width, height, format, VK_IMAGE_TILING_OPTIMAL,
			usage, VMA_MEMORY_USAGE_AUTO,
			outImage, outAllocation);

		outView = VulkanUtils::CreateImageView(dev, outImage, format, aspect);
	}

	void VulkanFramebuffer::RegisterImGuiDescriptorSets()
	{
		if (!ImGui::GetCurrentContext() || !ImGui::GetIO().BackendRendererUserData) return;

		for (size_t i = 0; i < m_ColorImages.size(); i++)
		{
			// Free previous if exists
			if (m_ColorImGuiDescriptorSets[i])
				ImGui_ImplVulkan_RemoveTexture(m_ColorImGuiDescriptorSets[i]);

			m_ColorImGuiDescriptorSets[i] = ImGui_ImplVulkan_AddTexture(
				m_ColorSamplers[i],
				m_ColorImageViews[i],
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		}
	}

	void VulkanFramebuffer::CleanupAttachments()
	{
		auto* ctx = VulkanContext::Get();
		if (!ctx) return;
		VkDevice dev = ctx->GetDevice();
		if (dev != VK_NULL_HANDLE)
			vkDeviceWaitIdle(dev);

		VmaAllocator allocator = ctx->GetVmaAllocator();
		bool hasBackendData = ImGui::GetCurrentContext() && ImGui::GetIO().BackendRendererUserData;
		for (size_t i = 0; i < m_ColorImages.size(); i++)
		{
			if (i < m_ColorImGuiDescriptorSets.size() && m_ColorImGuiDescriptorSets[i])
			{
				if (hasBackendData)
					ImGui_ImplVulkan_RemoveTexture(m_ColorImGuiDescriptorSets[i]);
			}
			if (i < m_ColorSamplers.size() && m_ColorSamplers[i])
				vkDestroySampler(dev, m_ColorSamplers[i], nullptr);
			if (i < m_ColorImageViews.size() && m_ColorImageViews[i])
				vkDestroyImageView(dev, m_ColorImageViews[i], nullptr);
			if (i < m_ColorImages.size() && m_ColorImages[i])
				vmaDestroyImage(allocator, m_ColorImages[i], m_ColorAllocations[i]);
		}
		m_ColorImages.clear();
		m_ColorAllocations.clear();
		m_ColorImageViews.clear();
		m_ColorSamplers.clear();
		m_ColorImGuiDescriptorSets.clear();

		if (m_DepthImage)
		{
			if (m_DepthView)
				vkDestroyImageView(dev, m_DepthView, nullptr);
			vmaDestroyImage(allocator, m_DepthImage, m_DepthAllocation);
			m_DepthImage = VK_NULL_HANDLE;
			m_DepthAllocation = VK_NULL_HANDLE;
			m_DepthView = VK_NULL_HANDLE;
		}
	}

} // namespace Waffle
