#include "TestHarness.h"

#include "Waffle/Core/Log.h"

#include <crtdbg.h>
#include <csignal>
#include <cstdlib>

int main()
{
	// Assert failures abort before stdout flushes - unbuffered keeps progress visible.
	setvbuf(stdout, nullptr, _IONBF, 0);

	// Engine asserts are logged (debugbreak is a no-op in this project) - route CRT asserts
	// and aborts to the handler too, so a failing test prints instead of popping a dialog.
	signal(SIGABRT, [](int) { std::printf("    ABORT: test aborted (CRT assert or terminate)\n"); _exit(101); });
	_CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_DEBUG);
	_CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_DEBUG);
	_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_DEBUG);

	Waffle::Log::Init();

	std::printf("=== Waffle unit tests: %d registered ===\n", (int)::wtest::Registry().size());

	int failedTests = 0;
	for (const auto& testCase : ::wtest::Registry())
	{
		int failuresBefore = ::wtest::FailureCount();
		std::printf("[ RUN  ] %s\n", testCase.Name);

		try
		{
			testCase.Fn();
		}
		catch (const std::exception& e)
		{
			::wtest::FailureCount()++;
			std::printf("    FAIL: uncaught exception: %s\n", e.what());
		}
		catch (...)
		{
			::wtest::FailureCount()++;
			std::printf("    FAIL: uncaught non-standard exception\n");
		}

		if (::wtest::FailureCount() == failuresBefore)
			std::printf("[  OK  ] %s\n", testCase.Name);
		else
		{
			failedTests++;
			std::printf("[ FAIL ] %s\n", testCase.Name);
		}
	}

	std::printf("=== %d tests, %d failed, %d assertion failures ===\n",
		(int)::wtest::Registry().size(), failedTests, ::wtest::FailureCount());
	return failedTests == 0 ? 0 : 1;
}
