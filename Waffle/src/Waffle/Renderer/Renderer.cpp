#include "wfpch.h"
#include "Renderer.h"

#include "RendererAPI.h"
#include "Renderer2D.h"
#include "PostProcessing.h"

namespace Waffle {

	void Renderer::Init()
	{
		RenderCommand::Init();

		// The command buffer must exist before anything records (Renderer2D
		// pipelines, first BeginScene).
		s_CommandBuffer = CommandBuffer::Create();

		Renderer2D::Init();
	}

	void Renderer::Shutdown()
	{
		// Release GPU objects while the graphics context is still alive.
		Renderer2D::Shutdown();
		PostProcessing::Shutdown();
		s_CommandBuffer = nullptr;
	}

	void Renderer::OnWindowResize(uint32_t width, uint32_t height)
	{
		RenderCommand::SetViewPort(0, 0, width, height);
	}

}
