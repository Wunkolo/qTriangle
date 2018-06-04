#pragma once
#include <cstddef>
#include <cstdint>
#include <algorithm>
#include <tuple>

#include "Types.hpp"
#include "Util.hpp"

namespace qTri
{
extern const std::pair<
	void(*)(qTri::Image& Frame, const qTri::Triangle& Tri),
	const char*
> FillAlgorithms[3];

bool CrossTest(const qTri::Vec2& Point, const qTri::Triangle& Tri);
bool Barycentric(const qTri::Vec2& Point, const qTri::Triangle& Tri);
}
