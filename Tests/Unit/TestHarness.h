#pragma once

// Minimal zero-dependency test harness: WTEST(name) registers a test at static-init
// time; the main in TestMain.cpp runs every registered test and returns the failure count.

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace wtest {

	struct TestCase
	{
		const char* Name;
		void (*Fn)();
	};

	inline std::vector<TestCase>& Registry()
	{
		static std::vector<TestCase> registry;
		return registry;
	}

	inline int& FailureCount()
	{
		static int failures = 0;
		return failures;
	}

	inline void ExpectTrue(bool condition, const char* file, int line, const std::string& what)
	{
		if (!condition)
		{
			FailureCount()++;
			std::printf("    FAIL [%s:%d] %s\n", file, line, what.c_str());
		}
	}

	inline void ExpectNear(double a, double b, double epsilon, const char* expression, const char* file, int line)
	{
		if (!(std::fabs(a - b) <= epsilon))
		{
			FailureCount()++;
			std::printf("    FAIL [%s:%d] %s (%.6f vs %.6f)\n", file, line, expression, a, b);
		}
	}

	struct Registrar
	{
		Registrar(const char* name, void (*fn)()) { Registry().push_back({ name, fn }); }
	};

}

#define WTEST(name)                                                              \
	static void wtest_fn_##name();                                               \
	static ::wtest::Registrar wtest_reg_##name(#name, wtest_fn_##name);          \
	static void wtest_fn_##name()

#define EXPECT_TRUE(cond, ...)                                                   \
	::wtest::ExpectTrue(static_cast<bool>(cond), __FILE__, __LINE__              \
		__VA_OPT__(,) __VA_ARGS__)

#define EXPECT_NEAR(a, b, eps)                                                   \
	::wtest::ExpectNear(static_cast<double>(a), static_cast<double>(b),          \
		static_cast<double>(eps), #a " ~= " #b, __FILE__, __LINE__)
