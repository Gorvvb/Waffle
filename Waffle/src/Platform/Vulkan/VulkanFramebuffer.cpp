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
		m_ColorMemories.resize(numColor);
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
				m_ColorImages[i], m_ColorMemories[i], m_ColorImageViews[i]);

			// Transition to shader-read layout (initial state for sampling)
			VkCommandBuffer cmd = ctx->BeginSingleTimeCommands();
			VulkanUtils::TransitionImageLayout(cmd, m_ColorImages[i],
				VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				0, VK_ACCESS_SHADER_READ_BIT,
				VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
			ctx->EndSingleTimeCommands(cmd);

			// Create sampler for this color attachment
			VkSamplerCreateInfo samplerInfo{};
			samplerInfo.sType     = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
			samplerInfo.magFilter = VK_FILTER_LINEAR;
			samplerInfo.minFilter = VK_FILTER_LINEAR;
			samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			samplerInfo.anisotropyEnable = VK_FALSE;
			samplerInfo.maxAnisotropy    = 1.0f;
			samplerInfo.borderColor      = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
			samplerInfo.unnormalizedCoordinates = VK_FALSE;
			samplerInfo.compareEnable    = VK_FALSE;
			samplerInfo.compareOp        = VK_COMPARE_OP_ALWAYS;
			samplerInfo.mipmapMode       = VK_SAMPLER_MIPMAP_MODE_LINEAR;
			vkCreateSampler(dev, &samplerInfo, nullptr, &m_ColorSamplers[i]);
		}

		// --- Depth attachment ---
		if (m_DepthAttachmentSpec.TextureFormat != FramebufferTextureFormat::None)
		{
			m_DepthFormat = WaffleFormatToVulkan(m_DepthAttachmentSpec.TextureFormat);
			CreateAttachment(w, h, m_DepthFormat,
				VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
				m_DepthImage, m_DepthMemory, m_DepthView);
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

		// Transition color attachments → attachment optimal
		for (size_t i = 0; i < m_ColorImages.size(); i++)
		{
			VulkanUtils::TransitionImageLayout(cmd, m_ColorImages[i],
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
				VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
		}

		// Build attachment infos
		std::vector<VkRenderingAttachmentInfo> colorAttachments(m_ColorImages.size());
		for (size_t i = 0; i < m_ColorImages.size(); i++)
		{
			colorAttachments[i] = {};
			colorAttachments[i].sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
			colorAttachments[i].imageView   = m_ColorImageViews[i];
			colorAttachments[i].imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			colorAttachments[i].loadOp      = VK_ATTACHMENT_LOAD_OP_CLEAR;
			colorAttachments[i].storeOp     = VK_ATTACHMENT_STORE_OP_STORE;
			colorAttachments[i].clearValue.color = ctx->GetClearColor();
		}

		VkRenderingAttachmentInfo depthAttachment{};
		bool hasDepth = (m_DepthImage != VK_NULL_HANDLE);
		if (hasDepth)
		{
			depthAttachment.sType                       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
			depthAttachment.imageView                   = m_DepthView;
			depthAttachment.imageLayout                 = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			depthAttachment.loadOp                      = VK_ATTACHMENT_LOAD_OP_CLEAR;
			depthAttachment.storeOp                     = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			depthAttachment.clearValue.depthStencil     = { 1.0f, 0 };
		}

		VkRenderingInfo renderingInfo{};
		renderingInfo.sType                = VK_STRUCTURE_TYPE_RENDERING_INFO;
		renderingInfo.renderArea.offset    = { 0, 0 };
		renderingInfo.renderArea.extent    = { m_Specification.Width, m_Specification.Height };
		renderingInfo.layerCount           = 1;
		renderingInfo.colorAttachmentCount = (uint32_t)colorAttachments.size();
		renderingInfo.pColorAttachments    = colorAttachments.empty() ? nullptr : colorAttachments.data();
		renderingInfo.pDepthAttachment     = hasDepth ? &depthAttachment : nullptr;

		vkCmdBeginRendering(cmd, &renderingInfo);
		ctx->SetRenderingActive(true);

		// Update viewport / scissor for this FBO's size
		VkViewport vp{};
		vp.x        = 0.0f;
		vp.y        = 0.0f;
		vp.width    = (float)m_Specification.Width;
		vp.height   = (float)m_Specification.Height;
		vp.minDepth = 0.0f;
		vp.maxDepth = 1.0f;
		VkRect2D sc{ { 0, 0 }, { m_Specification.Width, m_Specification.Height } };
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
				VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
				VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
		}

		// Restore swap-chain viewport
		auto swapExtent = ctx->GetSwapChainExtent();
		VkViewport vp{};
		vp.x        = 0.0f;
		vp.y        = 0.0f;
		vp.width    = (float)swapExtent.width;
		vp.height   = (float)swapExtent.height;
		vp.minDepth = 0.0f;
		vp.maxDepth = 1.0f;
		VkRect2D sc{ { 0, 0 }, swapExtent };
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
	int VulkanFramebuffer::ReadPixel(uint32_t attachmentIndex, int x, int y)
	{
		WF_CORE_ASSERT(attachmentIndex < m_ColorImages.size());
		WF_CORE_ASSERT(m_ColorAttachmentSpecs[attachmentIndex].TextureFormat
			== FramebufferTextureFormat::RED_INTEGER,
			"ReadPixel currently only supports RED_INTEGER attachments in Vulkan");

		auto* ctx    = VulkanContext::Get();
		VkDevice dev = ctx->GetDevice();

		// Wait for all rendering to finish
		vkDeviceWaitIdle(dev);

		// Create a 4-byte staging buffer
		VkBuffer       staging;
		VkDeviceMemory stagingMemory;
		VulkanUtils::CreateBuffer(dev, ctx->GetPhysicalDevice(),
			sizeof(int32_t),
			VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			staging, stagingMemory);

		VkCommandBuffer cmd = ctx->BeginSingleTimeCommands();

		// Transition to transfer src
		VulkanUtils::TransitionImageLayout(cmd, m_ColorImages[attachmentIndex],
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_TRANSFER_READ_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

		VkBufferImageCopy region{};
		region.bufferOffset                    = 0;
		region.bufferRowLength                 = 0;
		region.bufferImageHeight               = 0;
		region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel       = 0;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount     = 1;
		region.imageOffset                     = { x, y, 0 };
		region.imageExtent                     = { 1, 1, 1 };

		vkCmdCopyImageToBuffer(cmd, m_ColorImages[attachmentIndex],
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, staging, 1, &region);

		// Transition back to shader read
		VulkanUtils::TransitionImageLayout(cmd, m_ColorImages[attachmentIndex],
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

		ctx->EndSingleTimeCommands(cmd);

		// Read result
		void* mapped;
		vkMapMemory(dev, stagingMemory, 0, sizeof(int32_t), 0, &mapped);
		int result = *(int32_t*)mapped;
		vkUnmapMemory(dev, stagingMemory);

		vkDestroyBuffer(dev, staging, nullptr);
		vkFreeMemory(dev, stagingMemory, nullptr);

		return result;
	}

	// =========================================================================
	// ClearAttachment
	// =========================================================================
	void VulkanFramebuffer::ClearAttachment(uint32_t attachmentIndex, int value)
	{
		WF_CORE_ASSERT(attachmentIndex < m_ColorImages.size());
		// Scheduled clear; actual clear is applied at the start of Bind()
		// via VK_ATTACHMENT_LOAD_OP_CLEAR.  For per-attachment clear
		// outside of Bind(), we record it as an image clear in the command buffer.
		auto* ctx = VulkanContext::Get();
		VkCommandBuffer cmd = ctx->BeginSingleTimeCommands();

		VulkanUtils::TransitionImageLayout(cmd, m_ColorImages[attachmentIndex],
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

		VkClearColorValue clearVal{};
		clearVal.int32[0] = value;

		VkImageSubresourceRange range{};
		range.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
		range.baseMipLevel   = 0;
		range.levelCount     = 1;
		range.baseArrayLayer = 0;
		range.layerCount     = 1;

		vkCmdClearColorImage(cmd, m_ColorImages[attachmentIndex],
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearVal, 1, &range);

		VulkanUtils::TransitionImageLayout(cmd, m_ColorImages[attachmentIndex],
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

		ctx->EndSingleTimeCommands(cmd);
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
		VkImage& outImage, VkDeviceMemory& outMemory, VkImageView& outView)
	{
		auto* ctx = VulkanContext::Get();
		VkDevice dev = ctx->GetDevice();

		VkImageAspectFlags aspect = VulkanUtils::HasDepthComponent(format)
			? VK_IMAGE_ASPECT_DEPTH_BIT
			: VK_IMAGE_ASPECT_COLOR_BIT;
		if (VulkanUtils::HasStencilComponent(format))
			aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;

		VulkanUtils::CreateImage(dev, ctx->GetPhysicalDevice(),
			width, height, format, VK_IMAGE_TILING_OPTIMAL,
			usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			outImage, outMemory);

		outView = VulkanUtils::CreateImageView(dev, outImage, format, aspect);
	}

	void VulkanFramebuffer::RegisterImGuiDescriptorSets()
	{
		if (!ImGui::GetCurrentContext() || !ImGui::GetIO().BackendRendererUserData) return;

		auto* ctx = VulkanContext::Get();
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
		auto* ctx    = VulkanContext::Get();
		if (!ctx) return;
		VkDevice dev = ctx->GetDevice();
		vkDeviceWaitIdle(dev);

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
			vkDestroyImageView(dev, m_ColorImageViews[i], nullptr);
			vkDestroyImage(dev, m_ColorImages[i], nullptr);
			vkFreeMemory(dev, m_ColorMemories[i], nullptr);
		}
		m_ColorImages.clear();
		m_ColorMemories.clear();
		m_ColorImageViews.clear();
		m_ColorSamplers.clear();
		m_ColorImGuiDescriptorSets.clear();

		if (m_DepthImage)
		{
			vkDestroyImageView(dev, m_DepthView, nullptr);
			vkDestroyImage(dev, m_DepthImage, nullptr);
			vkFreeMemory(dev, m_DepthMemory, nullptr);
			m_DepthImage = VK_NULL_HANDLE;
		}
	}

} // namespace Waffle
