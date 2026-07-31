#include "wfpch.h"
#include "RenderCommand.h"

#include "Platform/OpenGL/OpenGlRendererAPI.h"
#include "Platform/Vulkan/VulkanRendererAPI.h"

namespace Waffle {

	// Switch this to RendererAPI::API::Vulkan (and change s_RendererAPI below)
	// to enable the Vulkan backend.  The API enum in RendererAPI.cpp controls
	// which backend is reported via Renderer::GetAPI().
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