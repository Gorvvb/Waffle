#pragma once

#include "Waffle/Core/Base.h"
#include "Waffle/Core/Timestep.h"

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <typeindex>

namespace Waffle {

	class Subsystem
	{
	public:
		virtual ~Subsystem() = default;

		virtual void OnInit() {}
		virtual void OnUpdate(Timestep ts) {}
		virtual void OnFixedUpdate(Timestep ts) {}
		virtual void OnShutdown() {}

		virtual const char* GetName() const = 0;
	};

	class SubsystemManager
	{
	public:
		static void Init();
		static void Shutdown();
		static void OnUpdate(Timestep ts);
		static void OnFixedUpdate(Timestep ts);

		template<typename T, typename... Args>
		static Ref<T> RegisterSubsystem(Args&&... args)
		{
			static_assert(std::is_base_of<Subsystem, T>::value, "T must derive from Subsystem");

			std::type_index typeIdx(typeid(T));
			if (s_SubsystemsMap.find(typeIdx) != s_SubsystemsMap.end())
			{
				return std::static_pointer_cast<T>(s_SubsystemsMap[typeIdx]);
			}

			Ref<T> subsystem = CreateRef<T>(std::forward<Args>(args)...);
			s_SubsystemsMap[typeIdx] = subsystem;
			s_SubsystemsList.push_back(subsystem);

			if (s_Initialized)
			{
				subsystem->OnInit();
			}

			return subsystem;
		}

		template<typename T>
		static Ref<T> GetSubsystem()
		{
			std::type_index typeIdx(typeid(T));
			auto it = s_SubsystemsMap.find(typeIdx);
			if (it != s_SubsystemsMap.end())
			{
				return std::static_pointer_cast<T>(it->second);
			}
			return nullptr;
		}

	private:
		static std::vector<Ref<Subsystem>> s_SubsystemsList;
		static std::unordered_map<std::type_index, Ref<Subsystem>> s_SubsystemsMap;
		static bool s_Initialized;
	};

}
