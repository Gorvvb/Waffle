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

		// Host-visible (dynamic) buffer — mapped persistently
		AllocateBuffer(size,
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		vkMapMemory(ctx->GetDevice(), m_Memory, 0, size, 0, &m_MappedPtr);
	}

	VulkanVertexBuffer::VulkanVertexBuffer(float* vertices, uint32_t size)
		: m_Size(size), m_HostVisible(false)
	{
		WF_PROFILE_FUNCTION();
		auto* ctx = VulkanContext::Get();
		VkDevice device = ctx->GetDevice();

		// Create staging buffer
		VkBuffer       stagingBuffer;
		VkDeviceMemory stagingMemory;
		VulkanUtils::CreateBuffer(device, ctx->GetPhysicalDevice(),
			size,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			stagingBuffer, stagingMemory);

		void* data;
		vkMapMemory(device, stagingMemory, 0, size, 0, &data);
		memcpy(data, vertices, size);
		vkUnmapMemory(device, stagingMemory);

		// Device-local vertex buffer
		AllocateBuffer(size,
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		ctx->CopyBuffer(stagingBuffer, m_Buffer, size);

		vkDestroyBuffer(device, stagingBuffer, nullptr);
		vkFreeMemory(device, stagingMemory, nullptr);
	}

	VulkanVertexBuffer::~VulkanVertexBuffer()
	{
		auto* ctx    = VulkanContext::Get();
		VkDevice dev = ctx->GetDevice();

		if (m_MappedPtr)
			vkUnmapMemory(dev, m_Memory);

		vkDestroyBuffer(dev, m_Buffer, nullptr);
		vkFreeMemory(dev, m_Memory, nullptr);
	}

	void VulkanVertexBuffer::Bind() const
	{
		// Recording happens in VulkanRendererAPI::DrawIndexed.
		// We just mark ourselves as the current vertex array's buffer (done via VulkanVertexArray).
	}

	void VulkanVertexBuffer::Unbind() const {}

	void VulkanVertexBuffer::SetData(const void* data, uint32_t size)
	{
		WF_CORE_ASSERT(m_HostVisible && m_MappedPtr, "SetData called on non-dynamic vertex buffer!");
		memcpy(m_MappedPtr, data, size);
	}

	void VulkanVertexBuffer::AllocateBuffer(VkDeviceSize size,
		VkBufferUsageFlags usage, VkMemoryPropertyFlags memProps)
	{
		auto* ctx = VulkanContext::Get();
		VulkanUtils::CreateBuffer(ctx->GetDevice(), ctx->GetPhysicalDevice(),
			size, usage, memProps, m_Buffer, m_Memory);
	}

	// =========================================================================
	// VulkanIndexBuffer
	// =========================================================================

	VulkanIndexBuffer::VulkanIndexBuffer(uint32_t* indices, uint32_t count)
		: m_Count(count)
	{
		WF_PROFILE_FUNCTION();
		auto* ctx    = VulkanContext::Get();
		VkDevice dev = ctx->GetDevice();

		VkDeviceSize size = count * sizeof(uint32_t);

		// Staging
		VkBuffer       stagingBuffer;
		VkDeviceMemory stagingMemory;
		VulkanUtils::CreateBuffer(dev, ctx->GetPhysicalDevice(),
			size,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			stagingBuffer, stagingMemory);

		void* data;
		vkMapMemory(dev, stagingMemory, 0, size, 0, &data);
		memcpy(data, indices, (size_t)size);
		vkUnmapMemory(dev, stagingMemory);

		// Device-local index buffer
		VulkanUtils::CreateBuffer(dev, ctx->GetPhysicalDevice(),
			size,
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			m_Buffer, m_Memory);

		ctx->CopyBuffer(stagingBuffer, m_Buffer, size);

		vkDestroyBuffer(dev, stagingBuffer, nullptr);
		vkFreeMemory(dev, stagingMemory, nullptr);
	}

	VulkanIndexBuffer::~VulkanIndexBuffer()
	{
		auto* ctx = VulkanContext::Get();
		vkDestroyBuffer(ctx->GetDevice(), m_Buffer, nullptr);
		vkFreeMemory(ctx->GetDevice(), m_Memory, nullptr);
	}

	void VulkanIndexBuffer::Bind()   const {}
	void VulkanIndexBuffer::Unbind() const {}

} // namespace Waffle
