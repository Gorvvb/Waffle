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

		// Create persistently-mapped host-coherent UBO buffer via VMA
		VmaAllocator allocator = ctx->GetVmaAllocator();
		VulkanUtils::CreateBuffer(allocator,
			size,
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

		// Create descriptor set layout (set=0, one UBO binding)
		VkDescriptorSetLayoutBinding layoutBinding
		{
			.binding = binding,
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
		};

		VkDescriptorSetLayoutCreateInfo layoutInfo
		{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.bindingCount = 1,
			.pBindings = &layoutBinding
		};

		VkResult layoutRes = vkCreateDescriptorSetLayout(dev, &layoutInfo, nullptr, &m_DescriptorSetLayout);
		if (layoutRes != VK_SUCCESS || m_DescriptorSetLayout == VK_NULL_HANDLE)
		{
			WF_CORE_ERROR("vkCreateDescriptorSetLayout failed with error code: {0}", (int)layoutRes);
			WF_CORE_ASSERT(false, "Failed to create UBO descriptor set layout!");
			return;
		}

		// Allocate descriptor set from shared pool
		VkDescriptorSetAllocateInfo dsAllocInfo
		{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.descriptorPool = pool,
			.descriptorSetCount = 1,
			.pSetLayouts = &m_DescriptorSetLayout
		};

		VkResult res = vkAllocateDescriptorSets(dev, &dsAllocInfo, &m_DescriptorSet);
		if (res != VK_SUCCESS || m_DescriptorSet == VK_NULL_HANDLE)
		{
			WF_CORE_ERROR("vkAllocateDescriptorSets in VulkanUniformBuffer failed with error code: {0}", (int)res);
			WF_CORE_ASSERT(false, "Failed to allocate UBO descriptor set!");
			return;
		}

		// Write descriptor (point to our buffer)
		VkDescriptorBufferInfo bufferInfo
		{
			.buffer = m_Buffer,
			.offset = 0,
			.range = size
		};

		VkWriteDescriptorSet write
		{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = m_DescriptorSet,
			.dstBinding = binding,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.pBufferInfo = &bufferInfo
		};

		vkUpdateDescriptorSets(dev, 1, &write, 0, nullptr);

		// Register with context (set=0, binding slot)
		ctx->BindDescriptorSet(0, m_DescriptorSet);
		ctx->RegisterUniformBuffer(m_Binding, m_Buffer, m_Size);
	}

	VulkanUniformBuffer::~VulkanUniformBuffer()
	{
		auto* ctx = VulkanContext::Get();
		if (!ctx) return;
		VkDevice dev = ctx->GetDevice();
		if (dev != VK_NULL_HANDLE)
			vkDeviceWaitIdle(dev);

		if (m_DescriptorSet != VK_NULL_HANDLE)
			ctx->SafeFreeDescriptorSet(m_DescriptorSet);
		if (m_DescriptorSetLayout != VK_NULL_HANDLE)
			vkDestroyDescriptorSetLayout(dev, m_DescriptorSetLayout, nullptr);

		if (m_Buffer != VK_NULL_HANDLE)
			vmaDestroyBuffer(ctx->GetVmaAllocator(), m_Buffer, m_Allocation);

		m_Buffer = VK_NULL_HANDLE;
		m_Allocation = VK_NULL_HANDLE;
	}

	void VulkanUniformBuffer::SetData(const void* data, uint32_t size, uint32_t offset)
	{
		WF_CORE_ASSERT(m_MappedPtr, "Uniform buffer not mapped!");
		WF_CORE_ASSERT(offset + size <= m_Size, "Uniform buffer write out of bounds!");
		memcpy((uint8_t*)m_MappedPtr + offset, data, size);

		// Re-register in case the bound descriptor set was cleared
		VulkanContext::Get()->BindDescriptorSet(0, m_DescriptorSet);
		VulkanContext::Get()->RegisterUniformBuffer(m_Binding, m_Buffer, m_Size);
	}

} // namespace Waffle
