#pragma once
#include <tuple>
#include <vector>
#include <array>

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

using Triangle = std::array<glm::i32vec2,3>;
}
