#include <Waffle.h>
#include <Waffle/Core/EntryPoint.h>

#include "Platform/OpenGL/OpenGLShader.h"

#include "imgui/imgui.h"

#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"

#include "Sandbox2D.h"

class ExampleLayer : public Waffle::Layer
{
public:
	ExampleLayer()
		: Layer("Example"), m_CameraController(1280.0f / 720.0f, true)
	{
		m_VertexArray = Waffle::VertexArray::Create();

		float vertecies[7 * 3] = {
			-0.5f, -0.5f, 0.0f, 0.8f, 0.2f, 0.8f, 1.0f,
			 0.5f, -0.5f, 0.0f, 0.2f, 0.3f, 0.8f, 1.0f,
			 0.0f,  0.5f, 0.0f, 0.8f, 0.8f, 0.2f, 1.0f,
		};

		Waffle::Ref<Waffle::VertexBuffer> vertexBuffer = Waffle::VertexBuffer::Create(vertecies, sizeof(vertecies));

		Waffle::BufferLayout layout = {
		{ Waffle::ShaderDataType::Float3, "a_Position" },
		{ Waffle::ShaderDataType::Float4, "a_Color" }
		};

		vertexBuffer->SetLayout(layout);
		m_VertexArray->AddVertexBuffer(vertexBuffer);

		uint32_t indecies[3] = { 0, 1, 2 };
		Waffle::Ref<Waffle::IndexBuffer> indexBuffer = Waffle::IndexBuffer::Create(indecies, sizeof(indecies) / sizeof(uint32_t));
		m_VertexArray->SetIndexBuffer(indexBuffer);

		m_SquareVA = Waffle::VertexArray::Create();

		float squareVertecies[4 * 5] = {
			-0.5f, -0.5f, 0.0f, 0.0f, 0.0f,
			 0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
			 0.5f,  0.5f, 0.0f, 1.0f, 1.0f,
			-0.5f,  0.5f, 0.0f, 0.0f, 1.0f
		};

		Waffle::Ref<Waffle::VertexBuffer> squareVB = Waffle::VertexBuffer::Create(squareVertecies, sizeof(squareVertecies));
		squareVB->SetLayout({ 
			{ Waffle::ShaderDataType::Float3, "a_Position" },
			{ Waffle::ShaderDataType::Float2, "a_TexCoord" }
			});
		m_SquareVA->AddVertexBuffer(squareVB);

		uint32_t squareIndecies[6] = { 0, 1, 2, 2, 3, 0 };
		Waffle::Ref<Waffle::IndexBuffer> squareIB = Waffle::IndexBuffer::Create(squareIndecies, sizeof(squareIndecies) / sizeof(uint32_t));
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

		m_Shader = Waffle::Shader::Create("VertexPosColor", vertexSrc, fragmentSrc);

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

		m_FlatColorShader = Waffle::Shader::Create("FlatColor", flatColorVertextSrc, flatColorFragmentSrc);


		auto textureShader = m_ShaderLibrary.Load("assets/shaders/Texture.glsl");
		m_Texture = Waffle::Texture2D::Create("assets/textures/Checkerboard.png");

		m_GorvLogo = Waffle::Texture2D::Create("assets/textures/logo.png");

		std::dynamic_pointer_cast<Waffle::OpenGLShader>(textureShader)->Bind();
		std::dynamic_pointer_cast<Waffle::OpenGLShader>(textureShader)->UploadUniformInt("u_Texture", 0);
	}

	void OnUpdate(Waffle::Timestep dt) override
	{
		// Update
		m_CameraController.OnUpdate(dt);

		// Render
		Waffle::RenderCommand::SetClearColor({ 0.2f, 0.2f, 0.2f, 1.0f });
		Waffle::RenderCommand::Clear();

		Waffle::Renderer::BeginScene(m_CameraController.GetCamera());

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

		auto textureShader = m_ShaderLibrary.Get("Texture");

		m_Texture->Bind();
		Waffle::Renderer::Submit(textureShader, m_SquareVA, glm::scale(glm::mat4(1.0f), glm::vec3(1.5f)));

		m_GorvLogo->Bind();
		Waffle::Renderer::Submit(textureShader, m_SquareVA, glm::scale(glm::mat4(1.0f), glm::vec3(1.5f)));
		
		// Triangle
		//Waffle::Renderer::Submit(m_Shader, m_VertexArray);

		Waffle::Renderer::EndScene();
	}

	void OnImGuiRender() override
	{
		ImGui::Begin("Settings");
		ImGui::ColorEdit3("Square Color", glm::value_ptr(m_SquareColor));
		ImGui::End();
	}

	void OnEvent(Waffle::Event& e) override
	{
		m_CameraController.OnEvent(e);
	}

private:
	Waffle::ShaderLibrary m_ShaderLibrary;
	Waffle::Ref<Waffle::Shader> m_Shader;
	Waffle::Ref<Waffle::VertexArray> m_VertexArray;

	Waffle::Ref<Waffle::Shader> m_FlatColorShader, m_TextureShader;
	Waffle::Ref<Waffle::VertexArray> m_SquareVA;

	Waffle::Ref<Waffle::Texture2D> m_Texture, m_GorvLogo;

	Waffle::OrthographicCameraController m_CameraController;

	glm::vec3 m_SquareColor = { 0.2f, 0.3f, 0.8f };
};

class Sandbox : public Waffle::Application
{
public:
	Sandbox()
	{
		//PushLayer(new ExampleLayer());
		PushLayer(new Sandbox2D());
	}
	
	~Sandbox()
	{
	}
};

Waffle::Application* Waffle::CreateApplication()
{
	return new Sandbox();
}
