#pragma once
#include <tuple>
#include <vector>

#include <glm/fwd.hpp>
#include <glm/vec2.hpp>

namespace qTri
{
class Image
{
public:
	Image(std::size_t Width, std::size_t Height)
		: Width(Width),
		Height(Height)
	{
		Pixels.resize(Width * Height);
	}

	std::size_t Width;
	std::size_t Height;
	std::vector<std::uint8_t> Pixels;
};

struct alignas(16) Triangle
{
	glm::i32vec2 Vert[3];
};
}
