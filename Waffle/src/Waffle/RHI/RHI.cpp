#include "wfpch.h"
#include "GraphicsPipeline.h"
#include "CommandBuffer.h"

#include "Waffle/Renderer/RendererAPI.h"

#include "Platform/OpenGL/OpenGLCommandBuffer.h"
#include "Platform/Vulkan/VulkanCommandBuffer.h"

namespace Waffle {

	Ref<GraphicsPipeline> GraphicsPipeline::Create(const Desc& desc)
	{
		WF_CORE_ASSERT(desc.Shader, "GraphicsPipeline::Create - shader is null!");

		switch (RendererAPI::GetAPI())
		{
		case RendererAPI::API::OpenGL:
			return CreateRef<OpenGLGraphicsPipeline>(desc);
		case RendererAPI::API::Vulkan:
			return CreateRef<VulkanGraphicsPipeline>(desc);
		default:
			break;
		}

		WF_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

	Ref<CommandBuffer> CommandBuffer::Create()
	{
		switch (RendererAPI::GetAPI())
		{
		case RendererAPI::API::OpenGL:
			return CreateRef<OpenGLCommandBuffer>();
		case RendererAPI::API::Vulkan:
			return CreateRef<VulkanCommandBuffer>();
		default:
			break;
		}

		WF_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

}
