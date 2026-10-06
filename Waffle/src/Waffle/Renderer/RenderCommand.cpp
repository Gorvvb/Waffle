#include "wfpch.h"
#include "RenderCommand.h"

#include "Platform/OpenGL/OpenGlRendererAPI.h"
#include "Platform/Vulkan/VulkanRendererAPI.h"

namespace Waffle {

	// The backend instance is created LAZILY in Init(), never at static-init time: reading
	// RendererAPI::GetAPI() from a static initializer is a static-init-order lottery (the
	// renderer-api static may not be initialized yet), which crashed relaunches that
	// switched backends.
	RendererAPI* RenderCommand::s_RendererAPI = nullptr;

	void RenderCommand::Init()
	{
		WF_PROFILE_FUNCTION();

		// Created here (runtime), never at static-init time - see note above.
		if (!s_RendererAPI)
		{
			switch (RendererAPI::GetAPI())
			{
			case RendererAPI::API::Vulkan:  s_RendererAPI = new VulkanRendererAPI; break;
			case RendererAPI::API::OpenGL:  s_RendererAPI = new OpenGLRendererAPI; break;
			default:                        s_RendererAPI = new OpenGLRendererAPI; break;
			}
		}
		s_RendererAPI->Init();
	}
}