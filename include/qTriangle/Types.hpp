#pragma once

#include <glm/fwd.hpp>
#include <glm/vec2.hpp>

namespace qTri
{

using Vec2 = glm::i32vec2;

struct Triangle
{
	glm::i32vec2 A;
	glm::i32vec2 B;
	glm::i32vec2 C;
};
}
