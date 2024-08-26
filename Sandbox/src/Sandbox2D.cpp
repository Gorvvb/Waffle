#include "Sandbox2D.h"
#include <imgui/imgui.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Platform/OpenGL/OpenGLShader.h"

#include <chrono>

Sandbox2D::Sandbox2D()
	: Layer("Sandbox2D"), m_CameraController(1280.0f / 720.0f, true)
{
}

void Sandbox2D::OnAttach()
{
	WF_PROFILE_FUNCTION();

	m_CheckerboardTexture = Waffle::Texture2D::Create("assets/textures/Checkerboard.png");
}

void Sandbox2D::OnDetach()
{
	WF_PROFILE_FUNCTION();
}

void Sandbox2D::OnUpdate(Waffle::Timestep dt)
{
	WF_PROFILE_FUNCTION();
	// Update
	m_CameraController.OnUpdate(dt);

	// Render
	{
		WF_PROFILE_SCOPE("Renderer Prep");
		Waffle::RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1.0f });
		Waffle::RenderCommand::Clear();
	}

	{
		WF_PROFILE_SCOPE("Renderer Draw");
		Waffle::Renderer2D::BeginScene(m_CameraController.GetCamera());

		Waffle::Renderer2D::DrawRotatedQuad({ -1.0f, 0.0f }, { 0.8f, 0.8f }, glm::radians(-45.0f), {0.8f, 0.2f, 0.3f, 1.0f});
		Waffle::Renderer2D::DrawQuad({ 0.5f, -0.5f }, { 0.5f, 0.75f }, m_SquareColor);
		Waffle::Renderer2D::DrawQuad({ 0.0f, 0.0f, -0.1f }, { 10.0f, 10.0f }, m_CheckerboardTexture, 10.0f, glm::vec4(1.0f, 0.9f, 0.9f, 1.0f));

		Waffle::Renderer2D::EndScene();
	}
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
