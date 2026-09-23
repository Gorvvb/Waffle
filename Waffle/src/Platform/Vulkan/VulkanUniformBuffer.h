#pragma once

#include "Waffle/Renderer/UniformBuffer.h"

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include <Volk/volk.h>
#include <vma/vk_mem_alloc.h>

namespace Waffle {

	// -------------------------------------------------------------------------
	// VulkanUniformBuffer
	// Host-visible UBO backed by a persistently-mapped VkBuffer.
	// The buffer is a RING of slices: every SetData writes the next slice and
	// the consuming shader descriptor sets are UNIFORM_BUFFER_DYNAMIC, bound
	// with that slice's offset. This keeps multiple passes per frame correct
	// under deferred execution - without the ring, the GPU would read the
	// LAST pass's data for every pass (a single host-mapped UBO is written
	// again before the frame's command buffer ever runs).
	// -------------------------------------------------------------------------
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

} // namespace Waffle
