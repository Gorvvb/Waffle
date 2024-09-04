#pragma once

#include "Waffle/Core/Base.h"
#include "Waffle/Core/Application.h"

#ifdef WF_PLATFORM_WINDOWS

extern Waffle::Application* Waffle::CreateApplication(ApplicationCommandLineArgs args);

int main(int argc, char** argv)
{
	Waffle::Log::Init();

	WF_PROFILE_BEGIN_SESSION("Startup", "WaffleProfile-Startup.json");
	auto app = Waffle::CreateApplication({ argc, argv });
	WF_PROFILE_END_SESSION();
	WF_PROFILE_BEGIN_SESSION("Startup", "WaffleProfile-Runtime.json");
	app->Run();
	WF_PROFILE_END_SESSION();
	WF_PROFILE_BEGIN_SESSION("Shutdown", "WaffleProfile-Shutdown.json");
	delete app;
	WF_PROFILE_END_SESSION();
}
#endif