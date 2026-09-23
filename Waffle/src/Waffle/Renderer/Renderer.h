#pragma once

#include "RenderCommand.h"

#include "Waffle/RHI/CommandBuffer.h"

namespace Waffle {

	// -------------------------------------------------------------------------
	// Renderer
	// Owns renderer-wide services: the backend API handle, the main per-frame
	// CommandBuffer (used by Renderer2D, PostProcessing and explicit render
	// passes), and the 2D batcher.
	// -------------------------------------------------------------------------
	class Renderer
	{
	public:
		static void Init();
		// Must be called while the graphics context is still alive (i.e.
		// before the Application's window is destroyed) - releases Renderer2D
		// and PostProcessing GPU resources that would otherwise only die in
		// static destructors, after the context is gone.
		static void Shutdown();
		static void OnWindowResize(uint32_t width, uint32_t height);

		// The main command buffer. Record passes and draws through it.
		static CommandBuffer* GetCommandBuffer() { return s_CommandBuffer.get(); }

		static RendererAPI::API GetAPI() { return RendererAPI::GetAPI(); }

	private:
		inline static Ref<CommandBuffer> s_CommandBuffer;
	};
}
