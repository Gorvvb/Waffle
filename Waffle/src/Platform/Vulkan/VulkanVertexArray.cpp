#include "wfpch.h"
#include "VulkanVertexArray.h"
#include "VulkanBuffer.h"
#include "VulkanContext.h"

namespace Waffle {

	VulkanVertexArray::VulkanVertexArray()  {}
	VulkanVertexArray::~VulkanVertexArray() {}

	void VulkanVertexArray::Bind() const
	{
		// Notify the context which vertex array is "active" so the
		// renderer API can resolve the correct pipeline.
		VulkanContext::Get()->SetBoundVertexArray(const_cast<VulkanVertexArray*>(this));
	}

	void VulkanVertexArray::Unbind() const
	{
		VulkanContext::Get()->SetBoundVertexArray(nullptr);
	}

	void VulkanVertexArray::AddVertexBuffer(const Ref<VertexBuffer>& vertexBuffer)
	{
		WF_CORE_ASSERT(vertexBuffer->GetLayout().GetElements().size(),
			"Vertex buffer has no layout!");

		const auto& layout  = vertexBuffer->GetLayout();
		uint32_t    binding = m_BindingIndex++;

		// Binding description (one per vertex buffer)
		VkVertexInputBindingDescription bindingDesc{};
		bindingDesc.binding   = binding;
		bindingDesc.stride    = layout.GetStride();
		bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		m_BindingDescriptions.push_back(bindingDesc);

		// Attribute descriptions (one per layout element)
		uint32_t location = 0;
		if (!m_AttributeDescriptions.empty())
			location = (uint32_t)m_AttributeDescriptions.size();

		for (const auto& element : layout)
		{
			// Matrix types span multiple locations (one per column)
			if (element.Type == ShaderDataType::Mat3 || element.Type == ShaderDataType::Mat4)
			{
				uint32_t colCount = (element.Type == ShaderDataType::Mat4) ? 4 : 3;
				uint32_t colSize  = element.Size / colCount;
				for (uint32_t col = 0; col < colCount; col++)
				{
					VkVertexInputAttributeDescription attr{};
					attr.binding  = binding;
					attr.location = location++;
					attr.format   = (element.Type == ShaderDataType::Mat4)
						? VK_FORMAT_R32G32B32A32_SFLOAT
						: VK_FORMAT_R32G32B32_SFLOAT;
					attr.offset   = element.Offset + col * colSize;
					m_AttributeDescriptions.push_back(attr);
				}
			}
			else
			{
				VkVertexInputAttributeDescription attr{};
				attr.binding  = binding;
				attr.location = location++;
				attr.format   = ShaderDataTypeToVulkanFormat(element.Type);
				attr.offset   = element.Offset;
				m_AttributeDescriptions.push_back(attr);
			}
		}

		// Cache raw VkBuffer handle
		auto* vkVB = dynamic_cast<VulkanVertexBuffer*>(vertexBuffer.get());
		WF_CORE_ASSERT(vkVB, "Vertex buffer is not a VulkanVertexBuffer!");
		m_VkBuffers.push_back(vkVB->GetVulkanBuffer());
		m_VkOffsets.push_back(0);

		m_VertexBuffers.push_back(vertexBuffer);
	}

	void VulkanVertexArray::SetIndexBuffer(const Ref<IndexBuffer>& indexBuffer)
	{
		auto* vkIB = dynamic_cast<VulkanIndexBuffer*>(indexBuffer.get());
		WF_CORE_ASSERT(vkIB, "Index buffer is not a VulkanIndexBuffer!");
		m_VkIndexBuffer = vkIB->GetVulkanBuffer();
		m_IndexBuffer   = indexBuffer;
	}

	// -------------------------------------------------------------------------
	// Format mapping
	// -------------------------------------------------------------------------
	VkFormat VulkanVertexArray::ShaderDataTypeToVulkanFormat(ShaderDataType type)
	{
		switch (type)
		{
		case ShaderDataType::Float:  return VK_FORMAT_R32_SFLOAT;
		case ShaderDataType::Float2: return VK_FORMAT_R32G32_SFLOAT;
		case ShaderDataType::Float3: return VK_FORMAT_R32G32B32_SFLOAT;
		case ShaderDataType::Float4: return VK_FORMAT_R32G32B32A32_SFLOAT;
		case ShaderDataType::Int:    return VK_FORMAT_R32_SINT;
		case ShaderDataType::Int2:   return VK_FORMAT_R32G32_SINT;
		case ShaderDataType::Int3:   return VK_FORMAT_R32G32B32_SINT;
		case ShaderDataType::Int4:   return VK_FORMAT_R32G32B32A32_SINT;
		case ShaderDataType::Bool:   return VK_FORMAT_R8_UINT;
		default:
			WF_CORE_ASSERT(false, "Unknown ShaderDataType!");
			return VK_FORMAT_UNDEFINED;
		}
	}

} // namespace Waffle
