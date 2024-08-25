#pragma once

#ifdef WF_PLATFORM_WINDOWS

extern Waffle::Application* Waffle::CreateApplication();

int main(int argc, char** argv)
{
	Waffle::Log::Init();

	WF_PROFILE_BEGIN_SESSION("Startup", "WaffleProfile-Startup.json");
	auto app = Waffle::CreateApplication();
	WF_PROFILE_END_SESSION();
	WF_PROFILE_BEGIN_SESSION("Startup", "WaffleProfile-Runtime.json");
	app->Run();
	WF_PROFILE_END_SESSION();
	WF_PROFILE_BEGIN_SESSION("Startup", "WaffleProfile-Shutdown.json");
	delete app;
	WF_PROFILE_END_SESSION();
}
#endif