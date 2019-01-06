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
		void(* const)(
			const glm::i32vec2 Points[], std::uint8_t Results[], std::size_t Count,
			const Triangle& Tri
		),
		const char*
	>
> FillAlgorithms;
}
