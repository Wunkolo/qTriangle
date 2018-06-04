#pragma once
#include <chrono>

// Measures the time it takes to execute a execute a function

template< typename TimeT = std::chrono::nanoseconds >
struct Bench
{
	template< typename FunctionT, typename ...ArgsT >
	static TimeT Duration(FunctionT&& Func, ArgsT&&... Arguments)
	{
		// Start time
		const auto Start = std::chrono::high_resolution_clock::now();
		// Run function, perfect-forward arguments
		std::forward<decltype(Func)>(Func)(std::forward<ArgsT>(Arguments)...);
		// Return executation time.
		return std::chrono::duration_cast<TimeT>(
			std::chrono::high_resolution_clock::now() - Start
		);
	}
};
