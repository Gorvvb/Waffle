#include "wfpch.h"
#include "VulkanUniformBuffer.h"
#include "VulkanContext.h"
#include "VulkanUtils.h"

namespace Waffle {

	VulkanUniformBuffer::VulkanUniformBuffer(uint32_t size, uint32_t binding)
		: m_Size(size), m_Binding(binding)
	{
		auto* ctx = VulkanContext::Get();
		if (!ctx)
		{
			WF_CORE_ERROR("VulkanContext is null in VulkanUniformBuffer constructor!");
			return;
		}

		VkDevice dev = ctx->GetDevice();
		if (dev == VK_NULL_HANDLE)
		{
			WF_CORE_ERROR("Vulkan device is null in VulkanUniformBuffer constructor!");
			return;
		}

		VkDescriptorPool pool = ctx->GetDescriptorPool();
		if (pool == VK_NULL_HANDLE)
		{
			WF_CORE_ERROR("Vulkan descriptor pool is null in VulkanUniformBuffer constructor!");
			return;
		}

		// Ring slice stride aligned to the device's dynamic-offset alignment.
		VkPhysicalDeviceProperties props;
		vkGetPhysicalDeviceProperties(ctx->GetPhysicalDevice(), &props);
		uint32_t alignment = props.limits.minUniformBufferOffsetAlignment;
		m_SliceStride = ((VkDeviceSize)size + alignment - 1) / alignment * alignment;

		// Create persistently-mapped host-coherent UBO ring via VMA
		VmaAllocator allocator = ctx->GetVmaAllocator();
		VulkanUtils::CreateBuffer(allocator,
			m_SliceStride * kSliceCount,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VMA_MEMORY_USAGE_AUTO,
			m_Buffer, m_Allocation,
			VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);

		if (m_Buffer == VK_NULL_HANDLE || m_Allocation == VK_NULL_HANDLE)
		{
			WF_CORE_ERROR("Failed to create Vulkan VMA buffer for UniformBuffer!");
			return;
		}

		VmaAllocationInfo allocInfo{};
		vmaGetAllocationInfo(allocator, m_Allocation, &allocInfo);
		m_MappedPtr = allocInfo.pMappedData;

		// The shader owns its per-frame descriptor sets (reflected, dynamic
		// UBO + samplers); this buffer only registers itself with the
		// context so BindAndFlushDescriptors can point descriptors at it.
		ctx->RegisterUniformBuffer(m_Binding, m_Buffer, m_Size, 0);
	}

	VulkanUniformBuffer::~VulkanUniformBuffer()
	{
		auto* ctx = VulkanContext::Get();
		if (!ctx) return;
		VkDevice dev = ctx->GetDevice();
		if (dev != VK_NULL_HANDLE)
			vkDeviceWaitIdle(dev);

		// Drop our registration BEFORE destroying the VkBuffer - otherwise
		// the context hands a dead handle to BindAndFlushDescriptors.
		ctx->UnregisterUniformBuffer(m_Binding);

		if (m_Buffer != VK_NULL_HANDLE)
			vmaDestroyBuffer(ctx->GetVmaAllocator(), m_Buffer, m_Allocation);

		m_Buffer = VK_NULL_HANDLE;
		m_Allocation = VK_NULL_HANDLE;
	}

	void VulkanUniformBuffer::SetData(const void* data, uint32_t size, uint32_t offset)
	{
		WF_CORE_ASSERT(m_MappedPtr, "Uniform buffer not mapped!");
		WF_CORE_ASSERT(offset + size <= m_Size, "Uniform buffer write out of bounds!");

		// Advance the ring: pick the next slice. A slice is reused only
		// after kSliceCount updates; WaitForFrameUploads additionally covers
		// the in-flight frames that may still read this slice.
		if (auto* ctx = VulkanContext::Get())
			ctx->WaitForFrameUploads(ctx->GetCurrentFrameIndex());

		uint64_t sliceBase = (uint64_t)m_NextSlice * m_SliceStride;
		m_NextSlice = (m_NextSlice + 1) % kSliceCount;

		memcpy((uint8_t*)m_MappedPtr + sliceBase + offset, data, size);

		// Re-register with the slice's dynamic offset - descriptor sets are
		// bound with it at the next draw.
		VulkanContext::Get()->RegisterUniformBuffer(m_Binding, m_Buffer, m_Size, sliceBase);
	}

}
