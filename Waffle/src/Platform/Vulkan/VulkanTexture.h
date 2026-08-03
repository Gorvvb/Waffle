#pragma once

#include "Waffle/Renderer/Texture.h"

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include <Volk/volk.h>
#include <vma/vk_mem_alloc.h>
#include <string>
#include <unordered_map>

namespace Waffle {

	class VulkanTexture2D : public Texture2D
	{
	public:
		VulkanTexture2D(uint32_t width, uint32_t height, TextureFilter filter);
		VulkanTexture2D(const std::string& path, TextureFilter filter);
		virtual ~VulkanTexture2D();

		virtual uint32_t GetWidth()  const override { return m_Width;  }
		virtual uint32_t GetHeight() const override { return m_Height; }

		virtual uint64_t GetRendererID() const override;

		virtual void SetData(void* data, uint32_t size) override;
		virtual void Bind(uint32_t slot = 0) const override;
		virtual void SetFilter(TextureFilter filter) override;
		virtual std::string GetPath() const override { return m_Path; }
		virtual bool operator==(const Texture& other) const override;

		// Raw Vulkan handles
		VkImage       GetImage()     const { return m_Image; }
		VkImageView   GetImageView() const { return m_ImageView; }
		VkSampler     GetSampler()   const { return m_Sampler; }

		// The descriptor set for direct Vulkan binding (e.g. ImGui)
		VkDescriptorSet GetDescriptorSet() const { return m_ImGuiDescriptorSet; }

	private:
		void CreateTextureFromData(void* data, uint32_t width, uint32_t height, uint32_t channels);
		void CreateSampler(TextureFilter filter);
		void CreateImGuiDescriptorSet();

		std::string m_Path;
		uint32_t    m_Width  = 0;
		uint32_t    m_Height = 0;
		uint32_t    m_Channels = 4;

		VkImage       m_Image       = VK_NULL_HANDLE;
		VmaAllocation m_Allocation  = VK_NULL_HANDLE;
		VkImageView   m_ImageView   = VK_NULL_HANDLE;
		VkSampler     m_Sampler     = VK_NULL_HANDLE;
		VkFormat      m_Format      = VK_FORMAT_R8G8B8A8_UNORM;

		// Pre-allocated descriptor set for use with ImGui and texture slots
		VkDescriptorSet m_ImGuiDescriptorSet = VK_NULL_HANDLE;

		// Slot-indexed descriptor sets (one per Bind() slot used)
		mutable std::unordered_map<uint32_t, VkDescriptorSet> m_SlotDescriptorSets;
	};

} // namespace Waffle
