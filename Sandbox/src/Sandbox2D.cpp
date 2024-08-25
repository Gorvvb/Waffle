#include "Sandbox2D.h"
#include <imgui/imgui.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Platform/OpenGL/OpenGLShader.h"

Sandbox2D::Sandbox2D()
	: Layer("Sandbox2D"), m_CameraController(1280.0f / 720.0f, true)
{
}

void Sandbox2D::OnAttach()
{
}

void Sandbox2D::OnDetach()
{
}

void Sandbox2D::OnUpdate(Waffle::Timestep dt)
{
	// Update
	m_CameraController.OnUpdate(dt);

	// Render
	Waffle::RenderCommand::SetClearColor({ 0.2f, 0.2f, 0.2f, 1.0f });
	Waffle::RenderCommand::Clear();

	Waffle::Renderer2D::BeginScene(m_CameraController.GetCamera());

	Waffle::Renderer2D::DrawQuad({ -1.0f, 0.0f }, { 0.8f, 0.8f }, { 0.8f, 0.2f, 0.3f, 1.0f });
	Waffle::Renderer2D::DrawQuad({ 0.5f, -0.5f }, { 0.5f, 0.75f }, { 0.2f, 0.3f, 0.8f, 1.0f });

	Waffle::Renderer2D::EndScene();

	//std::dynamic_pointer_cast<Waffle::OpenGLShader>(m_FlatColorShader)->Bind();
	//std::dynamic_pointer_cast<Waffle::OpenGLShader>(m_FlatColorShader)->UploadUniformFloat4("u_Color", m_SquareColor);
}

void Sandbox2D::OnImGuiRender()
{
	ImGui::Begin("Settings");
	ImGui::ColorEdit4("Square Color", glm::value_ptr(m_SquareColor));
	ImGui::End();
}

void Sandbox2D::OnEvent(Waffle::Event& e)
{
	m_CameraController.OnEvent(e);
}
