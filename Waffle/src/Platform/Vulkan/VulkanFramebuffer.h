#pragma once

#include "Waffle/Renderer/Framebuffer.h"

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include <Volk/volk.h>
#include <vma/vk_mem_alloc.h>
#include <vector>

namespace Waffle {

	// VulkanFramebuffer
	class VulkanFramebuffer : public Framebuffer
	{
	public:
		explicit VulkanFramebuffer(const FramebufferSpecification& spec);
		virtual ~VulkanFramebuffer();

		// Owns raw Vulkan handles - copying would double-destroy them.
		VulkanFramebuffer(const VulkanFramebuffer&) = delete;
		VulkanFramebuffer& operator=(const VulkanFramebuffer&) = delete;

		void Invalidate();

		void SetClearOnBegin(bool clear) { m_ClearOnBegin = clear; }

		virtual void Bind()   override;
		virtual void Unbind() override;

		virtual void Resize(uint32_t width, uint32_t height) override;
		virtual int  ReadPixel(uint32_t attachmentIndex, int x, int y) override;
		virtual void ClearAttachment(uint32_t attachmentIndex, int value) override;

		virtual void* GetImGuiAttachmentId(uint32_t index = 0) const override;

		virtual const FramebufferSpecification& GetSpecification() const override { return m_Specification; }

		// Raw Vulkan handles
		VkImage     GetColorAttachmentImage(uint32_t index = 0) const { return m_ColorImages[index]; }
		VkImageView GetColorAttachmentView(uint32_t index = 0) const { return m_ColorImageViews[index]; }
		VkSampler   GetColorAttachmentSampler(uint32_t index = 0) const
		{
			return (index < m_ColorSamplers.size()) ? m_ColorSamplers[index] : VK_NULL_HANDLE;
		}
		VkFormat    GetColorFormat() const { return m_ColorFormat; }
		VkFormat    GetDepthFormat() const { return m_DepthFormat; }

	private:
		void CreateAttachment(uint32_t width, uint32_t height,
			VkFormat format, VkImageUsageFlags usage,
			VkImage& outImage, VmaAllocation& outAllocation, VkImageView& outView);
		void RegisterImGuiDescriptorSets();
		void CleanupAttachments();

		FramebufferSpecification m_Specification;

		std::vector<FramebufferTextureSpecification> m_ColorAttachmentSpecs;
		FramebufferTextureSpecification              m_DepthAttachmentSpec;

		// Color attachments
		std::vector<VkImage>       m_ColorImages;
		std::vector<VmaAllocation> m_ColorAllocations;
		std::vector<VkImageView>   m_ColorImageViews;
		std::vector<VkSampler>     m_ColorSamplers;

		// ImGui descriptor sets (returned by GetImGuiAttachmentId)
		std::vector<VkDescriptorSet> m_ColorImGuiDescriptorSets;
		std::vector<VkImageLayout> m_ColorCurrentLayouts;

		// Depth attachment
		VkImage       m_DepthImage      = VK_NULL_HANDLE;
		VmaAllocation m_DepthAllocation = VK_NULL_HANDLE;

		// Persistent 4-byte staging buffer for ReadPixel (created on first use, reused every call).
		VkBuffer m_PixelStaging = VK_NULL_HANDLE;
		VmaAllocation m_PixelStagingAllocation = VK_NULL_HANDLE;
		VkImageView   m_DepthView       = VK_NULL_HANDLE;

			VkFormat m_ColorFormat = VK_FORMAT_R8G8B8A8_UNORM;
		std::vector<VkFormat> m_ColorFormats;
		VkFormat m_DepthFormat = VK_FORMAT_D24_UNORM_S8_UINT;

		bool m_ClearOnBegin = true;
		bool m_IsRendering = false;
	};
}