#include <Waffle.h>
#include <Waffle/Core/EntryPoint.h>

#include "Platform/OpenGL/OpenGLShader.h"

#include "imgui/imgui.h"

#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"

#include "Sandbox2D.h"
//#include "ExampleLayer.h"

class Sandbox : public Waffle::Application
{
public:
	Sandbox(const Waffle::ApplicationSpecification& specification)
		: Waffle::Application(specification)
	{
		//PushLayer(new ExampleLayer());
		PushLayer(new Sandbox2D());
	}
	
	~Sandbox()
	{
	}
};

Waffle::Application* Waffle::CreateApplication(ApplicationCommandLineArgs args)
{
	ApplicationSpecification spec;
	spec.Name = "Sandbox";
	spec.CommandLineArgs = args;
	spec.WorkingDirectory = "../Waffle-Editor";
	return new Sandbox(spec);
}
