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
	m_SquareVA = Waffle::VertexArray::Create();

	float squareVertecies[4 * 3] = {
		-0.5f, -0.5f, 0.0f,
		 0.5f, -0.5f, 0.0f,
		 0.5f,  0.5f, 0.0f,
		-0.5f,  0.5f, 0.0f,
	};

	Waffle::Ref<Waffle::VertexBuffer> squareVB;
	squareVB.reset(Waffle::VertexBuffer::Create(squareVertecies, sizeof(squareVertecies)));
	squareVB->SetLayout({
		{ Waffle::ShaderDataType::Float3, "a_Position" }
		});
	m_SquareVA->AddVertexBuffer(squareVB);

	uint32_t squareIndecies[6] = { 0, 1, 2, 2, 3, 0 };
	Waffle::Ref<Waffle::IndexBuffer> squareIB;
	squareIB.reset(Waffle::IndexBuffer::Create(squareIndecies, sizeof(squareIndecies) / sizeof(uint32_t)));
	m_SquareVA->SetIndexBuffer(squareIB);

	m_FlatColorShader = Waffle::Shader::Create("assets/shaders/FlatColor.glsl");
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

	Waffle::Renderer::BeginScene(m_CameraController.GetCamera());

	std::dynamic_pointer_cast<Waffle::OpenGLShader>(m_FlatColorShader)->Bind();
	std::dynamic_pointer_cast<Waffle::OpenGLShader>(m_FlatColorShader)->UploadUniformFloat4("u_Color", m_SquareColor);

	m_FlatColorShader->Bind();
	Waffle::Renderer::Submit(m_FlatColorShader, m_SquareVA, glm::scale(glm::mat4(1.0f), glm::vec3(1.5f)));

	Waffle::Renderer::EndScene();
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
