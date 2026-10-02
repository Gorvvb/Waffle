#include "wfpch.h"
#include "RenderCommand.h"

#include "Platform/OpenGL/OpenGlRendererAPI.h"
#include "Platform/Vulkan/VulkanRendererAPI.h"

namespace Waffle {

	// Backend selection: the API enum in RendererAPI.cpp controls which backend Renderer::GetAPI() reports; this factory instantiates the matching RendererAPI at startup.
	RendererAPI* RenderCommand::s_RendererAPI = []() -> RendererAPI*
	{
		// Read the desired API from the static member at startup
		switch (RendererAPI::GetAPI())
		{
		case RendererAPI::API::Vulkan:  return new VulkanRendererAPI;
		case RendererAPI::API::OpenGL:  return new OpenGLRendererAPI;
		default:                        return new OpenGLRendererAPI;
		}
	}();
}