#include "wfpch.h"
#include "UniformBuffer.h"

#include "Waffle/Renderer/Renderer.h"
#include "Platform/OpenGL/OpenGLUniformBuffer.h"
#include "Platform/Vulkan/VulkanUniformBuffer.h"

namespace Waffle {

	Ref<UniformBuffer> UniformBuffer::Create(uint32_t size, uint32_t binding)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None:   WF_CORE_ASSERT(false, "RendererAPI::None is currently not supported!"); return nullptr;
			case RendererAPI::API::OpenGL: return CreateRef<OpenGLUniformBuffer>(size, binding);
			case RendererAPI::API::Vulkan: return CreateRef<VulkanUniformBuffer>(size, binding);
		}

		WF_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}
}