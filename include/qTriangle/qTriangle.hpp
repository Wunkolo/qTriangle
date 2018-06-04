#pragma once
#include <cstddef>
#include <cstdint>

#include "Types.hpp"

namespace qTri
{
bool CrossTest(const qTri::Vec2& Point, const qTri::Triangle& Tri);
bool Barycentric(const qTri::Vec2& Point, const qTri::Triangle& Tri);
}
