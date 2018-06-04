#pragma once

#include <glm/fwd.hpp>
#include <glm/vec2.hpp>

namespace qTri
{

using Vec2 = glm::u32vec2;

struct Triangle
{
	glm::u32vec2 A;
	glm::u32vec2 B;
	glm::u32vec2 C;
};
}
