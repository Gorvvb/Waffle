#include <Waffle.h>

class ExampleLayer : public Waffle::Layer
{
public:
	ExampleLayer()
		: Layer("Example")
	{
	}

	void OnUpdate() override
	{
		//WF_INFO("ExampleLayer::Update");
	}

	void OnEvent(Waffle::Event& event) override
	{
		//WF_TRACE("{0}", event);
	}
};

class Sandbox : public Waffle::Application
{
public:
	Sandbox()
	{
		PushLayer(new ExampleLayer());
		PushOverlay(new Waffle::ImGuiLayer());
	}
	
	~Sandbox()
	{

	}
};

Waffle::Application* Waffle::CreateApplication()
{
	return new Sandbox();
}
