#pragma once

#include "Waffle/Renderer/Buffer.h"

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include <Volk/volk.h>
#include <vma/vk_mem_alloc.h>

namespace Waffle {

	// -------------------------------------------------------------------------
	// VulkanVertexBuffer
	// Dynamic (host-mapped) vertex buffers are a RING of slices: every
	// SetData writes the next slice and the draw binds the buffer at that
	// slice's offset. Without the ring, a second Flush (UI pass, selection
	// overlay) would overwrite the first batch's vertices before the
	// frame's deferred command buffer ever executed - GL is immune because
	// it draws immediately, but on Vulkan the earlier batches rendered
	// garbage from the last write.
	// -------------------------------------------------------------------------
	class VulkanVertexBuffer : public VertexBuffer
	{
	public:
		// Dynamic (no initial data - writable each frame)
		explicit VulkanVertexBuffer(uint32_t size);
		// Static (initial data uploaded once)
		VulkanVertexBuffer(float* vertices, uint32_t size);
		virtual ~VulkanVertexBuffer();

		// Owns raw Vulkan handles - copying would double-destroy them.
		VulkanVertexBuffer(const VulkanVertexBuffer&) = delete;
		VulkanVertexBuffer& operator=(const VulkanVertexBuffer&) = delete;

		virtual void Bind()   const override;
		virtual void Unbind() const override;

		virtual void SetData(const void* data, uint32_t size) override;

		virtual const BufferLayout& GetLayout() const override { return m_Layout; }
		virtual void SetLayout(const BufferLayout& layout) override { m_Layout = layout; }

		// Raw handle for the renderer API
		VkBuffer GetVulkanBuffer() const { return m_Buffer; }
		// Offset of the slice the LAST SetData wrote (0 for static buffers).
		VkDeviceSize GetCurrentOffset() const { return m_CurrentOffset; }

	private:
		VkBuffer      m_Buffer     = VK_NULL_HANDLE;
		VmaAllocation m_Allocation = VK_NULL_HANDLE;
		uint32_t      m_Size       = 0;
		bool          m_HostVisible = false;  // true → persistent map
		void*         m_MappedPtr  = nullptr;

		// Ring layout (dynamic buffers only)
		static constexpr uint32_t kSliceCount = 8;
		VkDeviceSize   m_SliceStride   = 0;
		uint32_t       m_NextSlice     = 0;
		VkDeviceSize   m_CurrentOffset = 0;

		BufferLayout m_Layout;
	};

	// -------------------------------------------------------------------------
	// VulkanIndexBuffer
	// -------------------------------------------------------------------------
	class VulkanIndexBuffer : public IndexBuffer
	{
	public:
		VulkanIndexBuffer(uint32_t* indices, uint32_t count);
		virtual ~VulkanIndexBuffer();

		// Owns raw Vulkan handles - copying would double-destroy them.
		VulkanIndexBuffer(const VulkanIndexBuffer&) = delete;
		VulkanIndexBuffer& operator=(const VulkanIndexBuffer&) = delete;

		virtual void Bind()   const override;
		virtual void Unbind() const override;

		virtual uint32_t GetCount() const override { return m_Count; }

		VkBuffer GetVulkanBuffer() const { return m_Buffer; }

	private:
		VkBuffer      m_Buffer     = VK_NULL_HANDLE;
		VmaAllocation m_Allocation = VK_NULL_HANDLE;
		uint32_t      m_Count      = 0;
	};

} // namespace Waffle
