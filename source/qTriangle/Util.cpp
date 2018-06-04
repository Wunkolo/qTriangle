#include <qTriangle/Util.hpp>
#include <qTriangle/Types.hpp>

namespace qTri
{
namespace Util
{
void Draw(const qTri::Image& Frame)
{
	for( std::size_t y = 0; y < Frame.Height; ++y )
	{
		std::fputs("\033[0;35m|\033[1;36m", stdout);
		for( std::size_t x = 0; x < Frame.Width; ++x )
		{
			std::putchar(
				" @"[Frame.Pixels[x + y * Frame.Width]]
			);
		}
		std::fputs("\033[0;35m|\n", stdout);
	}
	std::fputs("\033[0m", stdout);
}
}
}
