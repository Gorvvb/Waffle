#include "wfpch.h"
#include "FatalSignal.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <csignal>
#include <io.h>
#include <exception>

namespace Waffle {
	FatalSignal FatalSignal::s_State;

	void FatalSignal::Die() {
		_Exit(-1);
	}

	void FatalSignal::Timeout() {
		// Called from the watchdog thread only - std::cerr may be fine here,
		// but keep to stdio to stay consistent with the handler.
		std::fputs("FATAL SIGNAL TIMEOUT\n", stderr);
		Die();
	}

	void FatalSignal::Handler(const char* what) {
		// Runs from a signal handler / terminate handler. Creating threads,
		// formatting into streams or touching the heap here can deadlock if
		// the crash happened while the heap lock was held (common for heap
		// corruption, which is what SIGSEGV usually is). Keep it to
		// async-signal-safe calls: write() + _Exit().
		static std::atomic<bool> active{ false };
		if (active.exchange(true))
			Die(); // nested fault while handling - bail out immediately

		auto writeStr = [](const char* s) { _write(2, s, (unsigned int)strlen(s)); };
		writeStr("FATAL SIGNAL RECEIVED: ");
		writeStr(what);
		writeStr("\n");

		Die();
	}

	void FatalSignal::Install(long timeout) {
		(void)timeout; // watchdog thread removed - see Handler note

		std::set_terminate([] {
			auto eptr = std::current_exception();
			const char* what = "<none>";
			try {
				if (eptr) std::rethrow_exception(eptr);
			}
			catch (const std::exception& e) {
				what = e.what();
			}
			Handler(what);
		});

		auto sig = [](int signalCode) {
			const char* name = "<none>";
			switch (signalCode) {
			case SIGABRT: name = "SIGABRT"; break;
			case SIGFPE:  name = "SIGFPE";  break;
			case SIGILL:  name = "SIGILL";  break;
			case SIGINT:  name = "SIGINT";  break;
			case SIGSEGV: name = "SIGSEGV"; break;
			case SIGTERM: name = "SIGTERM"; break;
			}
			Handler(name);
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
