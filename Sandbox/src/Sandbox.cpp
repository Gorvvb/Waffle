#include <Waffle.h>

#include "Platform/OpenGL/OpenGLShader.h"

#include "imgui/imgui.h"

#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"


class ExampleLayer : public Waffle::Layer
{
public:
	ExampleLayer()
		: Layer("Example"), m_Camera(-1.6f, 1.6f, -0.9f, 0.9f), m_CameraPosition(0.0f)
	{
		m_VertexArray.reset(Waffle::VertexArray::Create());

		float vertecies[7 * 3] = {
			-0.5f, -0.5f, 0.0f, 0.8f, 0.2f, 0.8f, 1.0f,
			 0.5f, -0.5f, 0.0f, 0.2f, 0.3f, 0.8f, 1.0f,
			 0.0f,  0.5f, 0.0f, 0.8f, 0.8f, 0.2f, 1.0f,
		};

		std::shared_ptr<Waffle::VertexBuffer> vertexBuffer;
		vertexBuffer.reset(Waffle::VertexBuffer::Create(vertecies, sizeof(vertecies)));

		Waffle::BufferLayout layout = {
		{ Waffle::ShaderDataType::Float3, "a_Position" },
		{ Waffle::ShaderDataType::Float4, "a_Color" }
		};

		vertexBuffer->SetLayout(layout);
		m_VertexArray->AddVertexBuffer(vertexBuffer);

		uint32_t indecies[3] = { 0, 1, 2 };
		std::shared_ptr<Waffle::IndexBuffer> indexBuffer;
		indexBuffer.reset(Waffle::IndexBuffer::Create(indecies, sizeof(indecies) / sizeof(uint32_t)));
		m_VertexArray->SetIndexBuffer(indexBuffer);

		m_SquareVA.reset(Waffle::VertexArray::Create());

		float squareVertecies[4 * 3] = {
			-0.5f, -0.5f, 0.0f,
			 0.5f, -0.5f, 0.0f,
			 0.5f,  0.5f, 0.0f,
			-0.5f,  0.5f, 0.0f,
		};

		std::shared_ptr<Waffle::VertexBuffer> squareVB;
		squareVB.reset(Waffle::VertexBuffer::Create(squareVertecies, sizeof(squareVertecies)));
		squareVB->SetLayout({ {Waffle::ShaderDataType::Float3, "a_Position"} });
		m_SquareVA->AddVertexBuffer(squareVB);

		uint32_t squareIndecies[6] = { 0, 1, 2, 2, 3, 0 };
		std::shared_ptr<Waffle::IndexBuffer> squareIB;
		squareIB.reset(Waffle::IndexBuffer::Create(squareIndecies, sizeof(squareIndecies) / sizeof(uint32_t)));
		m_SquareVA->SetIndexBuffer(squareIB);

		std::string vertexSrc = R"(
			#version 460 core
			
			layout(location = 0) in vec3 a_Position;
			layout(location = 1) in vec4 a_Color;

			uniform mat4 u_ViewProjection;
			uniform mat4 u_Transform;
			
			out vec3 v_Position;
			out vec4 v_Color;

			void main()
			{
				v_Position = a_Position;
				v_Color = a_Color;
				gl_Position = u_ViewProjection * u_Transform * vec4(a_Position, 1.0);
			}
			)";

		std::string fragmentSrc = R"(
			#version 460 core
			
			layout(location = 0) out vec4 color;
			
			in vec3 v_Position;
			in vec4 v_Color;

			void main()
			{
				color = vec4(v_Position * 0.5 + 0.5, 1.0);
				color = v_Color;
			}
			)";

		m_Shader.reset(Waffle::Shader::Create(vertexSrc, fragmentSrc));

		std::string flatColorVertextSrc = R"(
			#version 460 core
			
			layout(location = 0) in vec3 a_Position;

			uniform mat4 u_ViewProjection;
			uniform mat4 u_Transform;
			
			out vec3 v_Position;

			void main()
			{
				v_Position = a_Position;
				gl_Position = u_ViewProjection * u_Transform * vec4(a_Position, 1.0);
			}
			)";

		std::string flatColorFragmentSrc = R"(
			#version 460 core
			
			layout(location = 0) out vec4 color;

			uniform vec3 u_Color;
			
			in vec3 v_Position;

			void main()
			{
				color = vec4(u_Color, 1.0f);
			}
			)";

		m_FlatColorShader.reset(Waffle::Shader::Create(flatColorVertextSrc, flatColorFragmentSrc));
	}

	void OnUpdate(Waffle::Timestep dt) override
	{
		if(Waffle::Input::IsKeyPressed(WF_KEY_LEFT))
			m_CameraPosition.x -= m_CameraMoveSpeed * dt;
		else if (Waffle::Input::IsKeyPressed(WF_KEY_RIGHT))
			m_CameraPosition.x += m_CameraMoveSpeed * dt;
		if (Waffle::Input::IsKeyPressed(WF_KEY_UP))
			m_CameraPosition.y += m_CameraMoveSpeed * dt;
		else if (Waffle::Input::IsKeyPressed(WF_KEY_DOWN))
			m_CameraPosition.y -= m_CameraMoveSpeed * dt;

		if (Waffle::Input::IsKeyPressed(WF_KEY_A))
			m_CameraRotation += m_CameraRotationSpeed * dt;
		else if (Waffle::Input::IsKeyPressed(WF_KEY_D))
			m_CameraRotation -= m_CameraRotationSpeed * dt;

		Waffle::RenderCommand::SetClearColor({ 0.2f, 0.2f, 0.2f, 1.0f });
		Waffle::RenderCommand::Clear();

		m_Camera.SetPosition(m_CameraPosition);
		m_Camera.SetRotation(m_CameraRotation);

		Waffle::Renderer::BeginScene(m_Camera);

		static glm::mat4 scale = glm::scale(glm::mat4(1.0f), glm::vec3(0.1f));

		std::dynamic_pointer_cast<Waffle::OpenGLShader>(m_FlatColorShader)->Bind();
		std::dynamic_pointer_cast<Waffle::OpenGLShader>(m_FlatColorShader)->UploadUniformFloat3("u_Color", m_SquareColor);
		for (int y = 0; y < 20; y++) {
			for (int x = 0; x < 20; x++)
			{
				glm::vec3 pos(x * 0.11f, y * 0.11f, 0.0f);
				glm::mat4 transform = glm::translate(glm::mat4(1.0f), pos) * scale;
				Waffle::Renderer::Submit(m_FlatColorShader, m_SquareVA, transform);
			}
		}
		Waffle::Renderer::Submit(m_Shader, m_VertexArray);

		Waffle::Renderer::EndScene();
	}

	void OnImGuiRender() override
	{
		ImGui::Begin("Settings");
		ImGui::ColorEdit3("Square Color", glm::value_ptr(m_SquareColor));
		ImGui::End();
	}

	void OnEvent(Waffle::Event& event) override { }

private:
	std::shared_ptr<Waffle::Shader> m_Shader;
	std::shared_ptr<Waffle::VertexArray> m_VertexArray;

	std::shared_ptr<Waffle::Shader> m_FlatColorShader;
	std::shared_ptr<Waffle::VertexArray> m_SquareVA;

	Waffle::OrthographicCamera m_Camera;
	glm::vec3 m_CameraPosition;
	float m_CameraMoveSpeed = 5.0f;

	float m_CameraRotation = 0.0f;
	float m_CameraRotationSpeed = 180.0f;

	glm::vec3 m_SquareColor = { 0.2f, 0.3f, 0.8f };
};

class Sandbox : public Waffle::Application
{
public:
	Sandbox()
	{
		PushLayer(new ExampleLayer());
	}
	
	~Sandbox()
	{
	}
};

Waffle::Application* Waffle::CreateApplication()
{
	return new Sandbox();
}
