#pragma once
#include <cstddef>
#include <cstdint>
#include <algorithm>
#include <vector>
#include <tuple>

#include "Types.hpp"
#include "Util.hpp"

namespace qTri
{
extern const std::vector<
	std::pair<
		void(* const)(qTri::Image&, const qTri::Triangle&), // Function Pointer
		const char*                                         // Algorithm Name
	>
> FillAlgorithms;
}
