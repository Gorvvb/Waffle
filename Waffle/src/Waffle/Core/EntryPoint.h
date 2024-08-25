#pragma once

#ifdef WF_PLATFORM_WINDOWS

extern Waffle::Application* Waffle::CreateApplication();

int main(int argc, char** argv)
{
	Waffle::Log::Init();
	WF_CORE_WARN("Initialized Log!");
	int a = 5;
	WF_INFO("Hello! Var={0}", a);

	auto app = Waffle::CreateApplication();
	app->Run();
	delete app;
}
#endif