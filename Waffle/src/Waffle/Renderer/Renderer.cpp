#include "wfpch.h"
#include "Renderer.h"

#include "Platform/OpenGL/OpenGLShader.h"
#include "Renderer2D.h"
#include "PostProcessing.h"

namespace Waffle {

	Renderer::SceneData* Renderer::m_SceneData = new Renderer::SceneData;

	void Renderer::Init()
	{
		RenderCommand::Init();
		Renderer2D::Init();
	}

	void Renderer::Shutdown()
	{
		Renderer2D::Shutdown();
		PostProcessing::Shutdown();
	}

	void Renderer::OnWindowResize(uint32_t width, uint32_t height)
	{
		RenderCommand::SetViewPort(0, 0, width, height);
	}

	void Renderer::BeginScene(OrthographicCamera& camera)
	{
		m_SceneData->ViewProjectionMatrix = camera.GetViewProjectionMatrix();
	}

	void Renderer::EndScene()
	{
	}

	void Renderer::Submit(const Ref<Shader>& shader, const Ref<VertexArray>& vertexArray, const glm::mat4& transform)
	{
		shader->Bind();
		shader->SetMat4("u_ViewProjection", m_SceneData->ViewProjectionMatrix);
		shader->SetMat4("u_Transform", transform);

		vertexArray->Bind();
		RenderCommand::DrawIndexed(vertexArray);
	}
}