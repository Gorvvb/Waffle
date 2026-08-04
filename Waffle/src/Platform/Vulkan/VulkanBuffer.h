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
	// -------------------------------------------------------------------------
	class VulkanVertexBuffer : public VertexBuffer
	{
	public:
		// Dynamic (no initial data - writable each frame)
		explicit VulkanVertexBuffer(uint32_t size);
		// Static (initial data uploaded once)
		VulkanVertexBuffer(float* vertices, uint32_t size);
		virtual ~VulkanVertexBuffer();

		virtual void Bind()   const override;
		virtual void Unbind() const override;

		virtual void SetData(const void* data, uint32_t size) override;

		virtual const BufferLayout& GetLayout() const override { return m_Layout; }
		virtual void SetLayout(const BufferLayout& layout) override { m_Layout = layout; }

		// Raw handle for the renderer API
		VkBuffer GetVulkanBuffer() const { return m_Buffer; }

	private:
		VkBuffer      m_Buffer     = VK_NULL_HANDLE;
		VmaAllocation m_Allocation = VK_NULL_HANDLE;
		uint32_t      m_Size       = 0;
		bool          m_HostVisible = false;  // true → persistent map
		void*         m_MappedPtr  = nullptr;

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
