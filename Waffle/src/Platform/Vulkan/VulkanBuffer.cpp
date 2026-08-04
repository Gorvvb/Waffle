#include "wfpch.h"
#include "VulkanBuffer.h"
#include "VulkanContext.h"
#include "VulkanUtils.h"

namespace Waffle {

	// =========================================================================
	// VulkanVertexBuffer
	// =========================================================================

	VulkanVertexBuffer::VulkanVertexBuffer(uint32_t size)
		: m_Size(size), m_HostVisible(true)
	{
		WF_PROFILE_FUNCTION();
		auto* ctx = VulkanContext::Get();
		VmaAllocator allocator = ctx->GetVmaAllocator();

		// Host-visible (dynamic) buffer - mapped persistently via VMA
		VulkanUtils::CreateBuffer(allocator,
			size,
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			VMA_MEMORY_USAGE_AUTO,
			m_Buffer, m_Allocation,
			VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);

		VmaAllocationInfo allocInfo{};
		vmaGetAllocationInfo(allocator, m_Allocation, &allocInfo);
		m_MappedPtr = allocInfo.pMappedData;
	}

	VulkanVertexBuffer::VulkanVertexBuffer(float* vertices, uint32_t size)
		: m_Size(size), m_HostVisible(false)
	{
		WF_PROFILE_FUNCTION();
		auto* ctx = VulkanContext::Get();
		VmaAllocator allocator = ctx->GetVmaAllocator();

		// Create staging buffer via VMA
		VkBuffer      stagingBuffer;
		VmaAllocation stagingAllocation;
		VulkanUtils::CreateBuffer(allocator,
			size,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VMA_MEMORY_USAGE_AUTO,
			stagingBuffer, stagingAllocation,
			VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);

		VmaAllocationInfo stagingAllocInfo{};
		vmaGetAllocationInfo(allocator, stagingAllocation, &stagingAllocInfo);
		memcpy(stagingAllocInfo.pMappedData, vertices, size);

		// Device-local vertex buffer
		VulkanUtils::CreateBuffer(allocator,
			size,
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			VMA_MEMORY_USAGE_AUTO,
			m_Buffer, m_Allocation);

		ctx->CopyBuffer(stagingBuffer, m_Buffer, size);

		vmaDestroyBuffer(allocator, stagingBuffer, stagingAllocation);
	}

	VulkanVertexBuffer::~VulkanVertexBuffer()
	{
		auto* ctx = VulkanContext::Get();
		if (!ctx) return;

		vmaDestroyBuffer(ctx->GetVmaAllocator(), m_Buffer, m_Allocation);
		m_Buffer = VK_NULL_HANDLE;
		m_Allocation = VK_NULL_HANDLE;
	}

	void VulkanVertexBuffer::Bind() const {}

	void VulkanVertexBuffer::Unbind() const {}

	void VulkanVertexBuffer::SetData(const void* data, uint32_t size)
	{
		WF_CORE_ASSERT(m_HostVisible && m_MappedPtr, "SetData called on non-dynamic vertex buffer!");
		memcpy(m_MappedPtr, data, size);
	}

	// =========================================================================
	// VulkanIndexBuffer
	// =========================================================================

	VulkanIndexBuffer::VulkanIndexBuffer(uint32_t* indices, uint32_t count)
		: m_Count(count)
	{
		WF_PROFILE_FUNCTION();
		auto* ctx = VulkanContext::Get();
		VmaAllocator allocator = ctx->GetVmaAllocator();

		VkDeviceSize size = count * sizeof(uint32_t);

		// Staging buffer via VMA
		VkBuffer      stagingBuffer;
		VmaAllocation stagingAllocation;
		VulkanUtils::CreateBuffer(allocator,
			size,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VMA_MEMORY_USAGE_AUTO,
			stagingBuffer, stagingAllocation,
			VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);

		VmaAllocationInfo stagingAllocInfo{};
		vmaGetAllocationInfo(allocator, stagingAllocation, &stagingAllocInfo);
		memcpy(stagingAllocInfo.pMappedData, indices, static_cast<size_t>(size));

		// Device-local index buffer
		VulkanUtils::CreateBuffer(allocator,
			size,
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
			VMA_MEMORY_USAGE_AUTO,
			m_Buffer, m_Allocation);

		ctx->CopyBuffer(stagingBuffer, m_Buffer, size);

		vmaDestroyBuffer(allocator, stagingBuffer, stagingAllocation);
	}

	VulkanIndexBuffer::~VulkanIndexBuffer()
	{
		auto* ctx = VulkanContext::Get();
		if (!ctx) return;

		vmaDestroyBuffer(ctx->GetVmaAllocator(), m_Buffer, m_Allocation);
		m_Buffer = VK_NULL_HANDLE;
		m_Allocation = VK_NULL_HANDLE;
	}

	void VulkanIndexBuffer::Bind()   const {}
	void VulkanIndexBuffer::Unbind() const {}

} // namespace Waffle
