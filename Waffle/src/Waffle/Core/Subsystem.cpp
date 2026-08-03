#include "wfpch.h"
#include "Subsystem.h"
#include "Waffle/Core/Log.h"

namespace Waffle {

	std::vector<Ref<Subsystem>> SubsystemManager::s_SubsystemsList;
	std::unordered_map<std::type_index, Ref<Subsystem>> SubsystemManager::s_SubsystemsMap;
	bool SubsystemManager::s_Initialized = false;

	void SubsystemManager::Init()
	{
		if (s_Initialized) return;

		WF_CORE_INFO("SubsystemManager initializing {0} registered subsystems", s_SubsystemsList.size());

		for (auto& sys : s_SubsystemsList)
		{
			WF_CORE_INFO("Initializing subsystem: {0}", sys->GetName());
			sys->OnInit();
		}

		s_Initialized = true;
	}

	void SubsystemManager::Shutdown()
	{
		if (!s_Initialized) return;

		WF_CORE_INFO("SubsystemManager shutting down subsystems...");

		// Shutdown in reverse order of registration
		for (auto it = s_SubsystemsList.rbegin(); it != s_SubsystemsList.rend(); ++it)
		{
			WF_CORE_INFO("Shutting down subsystem: {0}", (*it)->GetName());
			(*it)->OnShutdown();
		}

		s_SubsystemsList.clear();
		s_SubsystemsMap.clear();
		s_Initialized = false;
	}

	void SubsystemManager::OnUpdate(Timestep ts)
	{
		for (auto& sys : s_SubsystemsList)
		{
			sys->OnUpdate(ts);
		}
	}

	void SubsystemManager::OnFixedUpdate(Timestep ts)
	{
		for (auto& sys : s_SubsystemsList)
		{
			sys->OnFixedUpdate(ts);
		}
	}

}
