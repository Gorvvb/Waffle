#include "wfpch.h"
#include "Application.h"

#include "Input.h"
#include "Renderer/Renderer.h"

#include "Waffle/Log.h"

namespace Waffle {

	Application* Application::s_Instance = nullptr;

	Application::Application()
		: m_Camera(-1.6f, 1.6f, -0.9f, 0.9f)
	{
		WF_CORE_ASSERT(!s_Instance, "Application already exists!");
		s_Instance = this;

		m_Window = std::unique_ptr<Window>(Window::Create());
		m_Window->SetEventCallback(WF_BIND_EVENT_FN(Application::OnEvent));

		m_ImGuiLayer = new ImGuiLayer;
		PushOverlay(m_ImGuiLayer);

		m_VertexArray.reset(VertexArray::Create());

		float vertecies[7 * 3] = {
			-0.5f, -0.5f, 0.0f, 0.8f, 0.2f, 0.8f, 1.0f,
			 0.5f, -0.5f, 0.0f, 0.2f, 0.3f, 0.8f, 1.0f,
			 0.0f,  0.5f, 0.0f, 0.8f, 0.8f, 0.2f, 1.0f,
		};

		std::shared_ptr<VertexBuffer> vertexBuffer;
		vertexBuffer.reset(VertexBuffer::Create(vertecies, sizeof(vertecies)));

		BufferLayout layout = {
		{ ShaderDataType::Float3, "a_Position" },
		{ ShaderDataType::Float4, "a_Color" }
		};

		vertexBuffer->SetLayout(layout);
		m_VertexArray->AddVertexBuffer(vertexBuffer);

		uint32_t indecies[3] = { 0, 1, 2 };
		std::shared_ptr<IndexBuffer> indexBuffer;
		indexBuffer.reset(IndexBuffer::Create(indecies, sizeof(indecies) / sizeof(uint32_t)));
		m_VertexArray->SetIndexBuffer(indexBuffer);

		m_SquareVA.reset(VertexArray::Create());

		float squareVertecies[4 * 3] = {
			-0.75f, -0.75f, 0.0f,
			 0.75f, -0.75f, 0.0f,
			 0.75f,  0.75f, 0.0f,
			-0.75f,  0.75f, 0.0f,
		};

		std::shared_ptr<VertexBuffer> squareVB;
		squareVB.reset(VertexBuffer::Create(squareVertecies, sizeof(squareVertecies)));
		squareVB->SetLayout({ {ShaderDataType::Float3, "a_Position"} });
		m_SquareVA->AddVertexBuffer(squareVB);

		uint32_t squareIndecies[6] = { 0, 1, 2, 2, 3, 0 };
		std::shared_ptr<IndexBuffer> squareIB;
		squareIB.reset(IndexBuffer::Create(squareIndecies, sizeof(squareIndecies) / sizeof(uint32_t)));
		m_SquareVA->SetIndexBuffer(squareIB);

		std::string vertexSrc = R"(
			#version 460 core
			
			layout(location = 0) in vec3 a_Position;
			layout(location = 1) in vec4 a_Color;

			uniform mat4 u_ViewProjection;
			
			out vec3 v_Position;
			out vec4 v_Color;

			void main()
			{
				v_Position = a_Position;
				v_Color = a_Color;
				gl_Position = u_ViewProjection * vec4(a_Position, 1.0);
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

		m_Shader.reset(new Shader(vertexSrc, fragmentSrc));

		std::string blueVertextSrc = R"(
			#version 460 core
			
			layout(location = 0) in vec3 a_Position;

			uniform mat4 u_ViewProjection;
			
			out vec3 v_Position;

			void main()
			{
				v_Position = a_Position;
				gl_Position = u_ViewProjection * vec4(a_Position, 1.0);
			}
			)";

		std::string blueFragmentSrc = R"(
			#version 460 core
			
			layout(location = 0) out vec4 color;
			
			in vec3 v_Position;

			void main()
			{
				color = vec4(0.2, 0.3, 0.8, 1.0);
			}
			)";

		m_BlueShader.reset(new Shader(blueVertextSrc, blueFragmentSrc));
	}

	Application::~Application()
	{
	}

	void Application::PushLayer(Layer* layer)
	{
		m_LayerStack.PushLayer(layer);
		layer->OnAttach();
	}

	void Application::PushOverlay(Layer* layer)
	{
		m_LayerStack.PushOverlay(layer);
		layer->OnAttach();
	}

	void Application::OnEvent(Event& e)
	{
		EventDispatcher dispacher(e);
		dispacher.Dispatch<WindowCloseEvent>(WF_BIND_EVENT_FN(Application::OnWindowClose));

		for (auto it = m_LayerStack.end(); it != m_LayerStack.begin();)
		{
			(*--it)->OnEvent(e);
			if (e.handled)
				break;
		}
	}

	void Application::Run()
	{
		while (m_Running)
		{

			RenderCommand::SetClearColor({ 0.2f, 0.2f, 0.2f, 1.0f });
			RenderCommand::Clear(); // Might set color as a variable instead of RenderCommand::SetClearColor();

			m_Camera.SetPosition({ 0.5, 0.5, 1 });
			m_Camera.SetRotation(45.0f);

			Renderer::BeginScene(m_Camera);
			
			m_BlueShader->UploadUniformMat4("u_ViewProjection", m_Camera.GetViewProjectionMatrix());
			Renderer::Submit(m_BlueShader, m_SquareVA); // Might change to Renderer::SubmitMesh();

			m_Shader->UploadUniformMat4("u_ViewProjection", m_Camera.GetViewProjectionMatrix());
			Renderer::Submit(m_Shader, m_VertexArray);

			Renderer::EndScene();

			for (Layer* layer : m_LayerStack)
				layer->OnUpdate();

			m_ImGuiLayer->Begin();
			for (Layer* layer : m_LayerStack)
				layer->OnImGuiRender();
			m_ImGuiLayer->End();

			m_Window->OnUpdate();
		}
	}

	bool Application::OnWindowClose(WindowCloseEvent& e)
	{
		m_Running = false;
		return true;
	}
}