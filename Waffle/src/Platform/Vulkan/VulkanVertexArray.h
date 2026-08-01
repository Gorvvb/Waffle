#pragma once

#include "Waffle/Renderer/VertexArray.h"
#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include <Volk/volk.h>
#include <vector>

namespace Waffle {

	class VulkanVertexArray : public VertexArray
	{
	public:
		VulkanVertexArray();
		virtual ~VulkanVertexArray();

		virtual void Bind()   const override;
		virtual void Unbind() const override;

		virtual void AddVertexBuffer(const Ref<VertexBuffer>& vertexBuffer) override;
		virtual void SetIndexBuffer(const Ref<IndexBuffer>& indexBuffer) override;

		virtual const std::vector<Ref<VertexBuffer>>& GetVertexBuffers() const override { return m_VertexBuffers; }
		virtual const Ref<IndexBuffer>& GetIndexBuffer() const override { return m_IndexBuffer; }

		// ---- Vulkan-specific pipeline input state ----------------------------
		const std::vector<VkVertexInputBindingDescription>&   GetBindingDescriptions()   const { return m_BindingDescriptions; }
		const std::vector<VkVertexInputAttributeDescription>& GetAttributeDescriptions() const { return m_AttributeDescriptions; }

		// Raw VkBuffer handles (used by VulkanRendererAPI for vkCmdBindVertexBuffers)
		const std::vector<VkBuffer>&     GetVkVertexBuffers() const { return m_VkBuffers; }
		const std::vector<VkDeviceSize>& GetVkOffsets()       const { return m_VkOffsets; }
		VkBuffer                         GetVkIndexBuffer()   const { return m_VkIndexBuffer; }

	private:
		static VkFormat ShaderDataTypeToVulkanFormat(ShaderDataType type);

		std::vector<Ref<VertexBuffer>> m_VertexBuffers;
		Ref<IndexBuffer>               m_IndexBuffer;

		// Vertex input descriptions for pipeline creation
		std::vector<VkVertexInputBindingDescription>   m_BindingDescriptions;
		std::vector<VkVertexInputAttributeDescription> m_AttributeDescriptions;

		// Raw handles cached for fast draw-time binding
		std::vector<VkBuffer>     m_VkBuffers;
		std::vector<VkDeviceSize> m_VkOffsets;
		VkBuffer                  m_VkIndexBuffer = VK_NULL_HANDLE;

		uint32_t m_BindingIndex = 0;
	};

} // namespace Waffle
