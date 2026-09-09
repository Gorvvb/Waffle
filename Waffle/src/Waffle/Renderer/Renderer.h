#pragma once

#include "RenderCommand.h"

#include "OrthographicCamera.h"
#include "Shader.h"

namespace Waffle {


	class Renderer
	{
	private:
		struct SceneData
		{
			glm::mat4 ViewProjectionMatrix;
		};

		static SceneData* m_SceneData;
	public:
		static void Init();
		// Must be called while the graphics context is still alive (i.e.
		// before the Application's window is destroyed) - releases Renderer2D
		// and PostProcessing GPU resources that would otherwise only die in
		// static destructors, after the context is gone.
		static void Shutdown();
		static void OnWindowResize(uint32_t width, uint32_t height);

		static void BeginScene(OrthographicCamera& camera);
		static void EndScene();

		static void Submit(const Ref<Shader>& shader, const Ref<VertexArray>& vertexArray, const glm::mat4& transform = glm::mat4(1.0f));

		static RendererAPI::API GetAPI() { return RendererAPI::GetAPI(); }
	};
}