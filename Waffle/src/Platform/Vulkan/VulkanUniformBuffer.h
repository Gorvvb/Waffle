#pragma once

#include "Waffle/Renderer/UniformBuffer.h"
#include <vulkan/vulkan.h>

namespace Waffle {

	// -------------------------------------------------------------------------
	// VulkanUniformBuffer
	// Host-visible UBO backed by a persistently-mapped VkBuffer.
	// Each instance owns one VkDescriptorSet (set=0, binding=<binding>).
	// The descriptor set is registered with VulkanContext when SetData is called.
	// -------------------------------------------------------------------------
	class VulkanUniformBuffer : public UniformBuffer
	{
	public:
		VulkanUniformBuffer(uint32_t size, uint32_t binding);
		virtual ~VulkanUniformBuffer();

		virtual void SetData(const void* data, uint32_t size, uint32_t offset = 0) override;

		VkDescriptorSet GetDescriptorSet() const { return m_DescriptorSet; }
		uint32_t        GetBinding()       const { return m_Binding; }

	private:
		VkBuffer              m_Buffer         = VK_NULL_HANDLE;
		VkDeviceMemory        m_Memory         = VK_NULL_HANDLE;
		void*                 m_MappedPtr      = nullptr;
		uint32_t              m_Size           = 0;
		uint32_t              m_Binding        = 0;

		VkDescriptorSetLayout m_DescriptorSetLayout = VK_NULL_HANDLE;
		VkDescriptorSet       m_DescriptorSet       = VK_NULL_HANDLE;
	};

} // namespace Waffle
