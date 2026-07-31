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

		// Create persistently-mapped host-coherent buffer
		VulkanUtils::CreateBuffer(dev, ctx->GetPhysicalDevice(),
			size,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			m_Buffer, m_Memory);

		if (m_Buffer == VK_NULL_HANDLE || m_Memory == VK_NULL_HANDLE)
		{
			WF_CORE_ERROR("Failed to create Vulkan buffer for UniformBuffer!");
			return;
		}

		vkMapMemory(dev, m_Memory, 0, size, 0, &m_MappedPtr);

		// Create descriptor set layout (set=0, one UBO binding)
		VkDescriptorSetLayoutBinding layoutBinding{};
		layoutBinding.binding            = binding;
		layoutBinding.descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		layoutBinding.descriptorCount    = 1;
		layoutBinding.stageFlags         = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

		VkDescriptorSetLayoutCreateInfo layoutInfo{};
		layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = 1;
		layoutInfo.pBindings    = &layoutBinding;

		VkResult layoutRes = vkCreateDescriptorSetLayout(dev, &layoutInfo, nullptr, &m_DescriptorSetLayout);
		if (layoutRes != VK_SUCCESS || m_DescriptorSetLayout == VK_NULL_HANDLE)
		{
			WF_CORE_ERROR("vkCreateDescriptorSetLayout failed with error code: {0}", (int)layoutRes);
			WF_CORE_ASSERT(false, "Failed to create UBO descriptor set layout!");
			return;
		}

		// Allocate descriptor set from shared pool
		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool     = pool;
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts        = &m_DescriptorSetLayout;

		VkResult res = vkAllocateDescriptorSets(dev, &allocInfo, &m_DescriptorSet);
		if (res != VK_SUCCESS || m_DescriptorSet == VK_NULL_HANDLE)
		{
			WF_CORE_ERROR("vkAllocateDescriptorSets in VulkanUniformBuffer failed with error code: {0}", (int)res);
			WF_CORE_ASSERT(false, "Failed to allocate UBO descriptor set!");
			return;
		}

		// Write descriptor (point to our buffer)
		VkDescriptorBufferInfo bufferInfo{};
		bufferInfo.buffer = m_Buffer;
		bufferInfo.offset = 0;
		bufferInfo.range  = size;

		VkWriteDescriptorSet write{};
		write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.dstSet          = m_DescriptorSet;
		write.dstBinding      = binding;
		write.dstArrayElement = 0;
		write.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		write.descriptorCount = 1;
		write.pBufferInfo     = &bufferInfo;

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
		vkDeviceWaitIdle(dev);

		if (m_MappedPtr)
			vkUnmapMemory(dev, m_Memory);

		if (m_DescriptorSet != VK_NULL_HANDLE)
			ctx->SafeFreeDescriptorSet(m_DescriptorSet);
		vkDestroyDescriptorSetLayout(dev, m_DescriptorSetLayout, nullptr);
		vkDestroyBuffer(dev, m_Buffer, nullptr);
		vkFreeMemory(dev, m_Memory, nullptr);
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
