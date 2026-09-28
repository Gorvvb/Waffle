#pragma once

#include "Waffle/Renderer/UniformBuffer.h"

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include <Volk/volk.h>
#include <vma/vk_mem_alloc.h>

namespace Waffle {

	class VulkanUniformBuffer : public UniformBuffer
	{
	public:
		VulkanUniformBuffer(uint32_t size, uint32_t binding);
		virtual ~VulkanUniformBuffer();

		// Owns raw Vulkan handles - copying would double-destroy them.
		VulkanUniformBuffer(const VulkanUniformBuffer&) = delete;
		VulkanUniformBuffer& operator=(const VulkanUniformBuffer&) = delete;

		virtual void SetData(const void* data, uint32_t size, uint32_t offset = 0) override;

		VkBuffer   GetBuffer() const { return m_Buffer; }
		uint32_t   GetBinding() const { return m_Binding; }

	private:
		VkBuffer              m_Buffer         = VK_NULL_HANDLE;
		VmaAllocation         m_Allocation     = VK_NULL_HANDLE;
		void*                 m_MappedPtr      = nullptr;
		uint32_t              m_Size           = 0;
		uint32_t              m_Binding        = 0;

		// Ring layout
		static constexpr uint32_t kSliceCount = 16;
		VkDeviceSize         m_SliceStride    = 0;
		uint32_t             m_NextSlice      = 0;
	};
}