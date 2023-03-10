#include "wfpch.h"
#include <Waffle.h>

class Sandbox : public Waffle::Application
{
public:
	Sandbox()
	{

	}
	
	~Sandbox()
	{

	}
};

Waffle::Application* Waffle::CreateApplication()
{
	return new Sandbox();
}
