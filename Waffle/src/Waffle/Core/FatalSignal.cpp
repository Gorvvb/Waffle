#include "wfpch.h"
#include "FatalSignal.h"

#include <iostream>
#include <thread>
#include <chrono>
#include <exception>

namespace Waffle {
	FatalSignal FatalSignal::s_State;

	void FatalSignal::Die() {
		_Exit(-1);
	}

	void FatalSignal::Timeout() {
		std::cerr << "FATAL SIGNAL TIMEOUT" << std::endl;
		Die();
	}

	void FatalSignal::Handler(const char* what) {
		if (m_Active) {
			std::cerr << "NESTED ERROR STATE: " << what << std::endl;
			Die();
		}

		std::cerr << "FATAL SIGNAL RECEIVED: " << what << std::endl;
		m_Active = true;

		std::thread t([&] {
			auto dur = std::chrono::duration<long, std::milli>(m_Timeout);
			std::this_thread::sleep_for(dur);
			Timeout();
		});
		t.detach();

		for (auto& fn : m_Callbacks)
		{
			try {
				fn();
			}
			catch (...) {}
		}

		Die();
	}

	void FatalSignal::Install(long timeout) {
		s_State.m_Timeout = timeout;

		std::set_terminate([] {
			auto eptr = std::current_exception();
			const char* what = "<none>";
			try {
				if (eptr) std::rethrow_exception(eptr);
			}
			catch (const std::exception& e) {
				what = e.what();
			}
			s_State.Handler(what);
		});

		auto sig = [](int signalCode) {
			const char* name = "<none>";
			switch (signalCode) {
			case SIGABRT: name = "SIGABRT"; break;
			case SIGFPE: name = "SIGFPE"; break;
			case SIGILL: name = "SIGILL"; break;
			case SIGINT: name = "SIGINT"; break;
			case SIGSEGV: name = "SIGSEGV"; break;
			case SIGTERM: name = "SIGTERM"; break;
			}
			s_State.Handler(name);
		};

		signal(SIGABRT, sig);
		signal(SIGFPE, sig);
		signal(SIGILL, sig);
		signal(SIGINT, sig);
		signal(SIGSEGV, sig);
		signal(SIGTERM, sig);
	}

	void FatalSignal::Add(ProcFn fn) {
		s_State.m_Callbacks.push_back(fn);
	}
}
