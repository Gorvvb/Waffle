#pragma once

#include <csignal>
#include <functional>
#include <vector>

namespace Waffle {

	class FatalSignal {
	private:
		using ProcFn = std::function<void()>;

		static FatalSignal s_State;

		std::vector<ProcFn> m_Callbacks;
		long m_Timeout = 2000; // Timeout in milliseconds
		bool m_Active = false;

		static void Handler(const char* what);

		static void Timeout();

	public:
		static void Die();
		static void Install(long timeout = 2000);
		static void Add(ProcFn fn);
	};
}
