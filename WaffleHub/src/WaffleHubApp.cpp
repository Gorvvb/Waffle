#include <Waffle.h>
#include <Waffle/Core/EntryPoint.h>
#include "HubLayer.h"

namespace Waffle {

	class WaffleHubApp : public Application
	{
	public:
		WaffleHubApp(const ApplicationSpecification& specification)
			: Application(specification)
		{
			PushLayer(new HubLayer());
		}

		~WaffleHubApp()
		{
		}
	};

	Application* CreateApplication(ApplicationCommandLineArgs args)
	{
		ApplicationSpecification spec;
		spec.Name = "Waffle Engine Hub";
		spec.IconPath = "Resources/Icons/logo.png";
		spec.CommandLineArgs = args;
		return new WaffleHubApp(spec);
	}
}
