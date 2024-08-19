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
		if (Waffle::Input::IsKeyPressed(WF_KEY_TAB))
			WF_TRACE("Tab key is pressed (POLL)!");
	}

	void OnEvent(Waffle::Event& event) override
	{
		if (event.GetEventType() == Waffle::EventType::KeyPressed)
		{
			Waffle::KeyPressedEvent& e = (Waffle::KeyPressedEvent&)event;
			if (e.GetKeyCode() == WF_KEY_TAB)
				WF_TRACE("Tab key is pressed (EVENT)!");
			WF_TRACE("{0}", (char)e.GetKeyCode());
		}
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
