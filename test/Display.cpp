#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <type_traits>
#include <array>

#include <glm/glm.hpp>

#include <qTriangle/qTriangle.hpp>

#ifdef _WIN32
#include <Windows.h>
// Statically enables "ENABLE_VIRTUAL_TERMINAL_PROCESSING" for the terminal
// at runtime to allow for unix-style escape sequences. 
const static bool _WndV100Enabled = []() -> bool
	{
		const auto Handle = GetStdHandle(STD_OUTPUT_HANDLE);
		DWORD ConsoleMode;
		GetConsoleMode(
			Handle,
			&ConsoleMode
		);
		SetConsoleMode(
			Handle,
			ConsoleMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING
		);
		GetConsoleMode(
			Handle,
			&ConsoleMode
		);
		return ConsoleMode & ENABLE_VIRTUAL_TERMINAL_PROCESSING;
	}();
#endif

constexpr std::size_t Width = 80;
constexpr std::size_t Height = 50;
using FrameBuffer = std::array<glm::float32_t, Width * Height>;

void Draw(const FrameBuffer& Frame);
void FillTriangle(FrameBuffer& Frame, const qTri::Triangle& Tri);

int main()
{
	FrameBuffer CurFrame = {};
	FillTriangle(
		CurFrame,
		{
			{0, 0},
			{Width, Height / 2},
			{Width / 2, Height}
		}
	);
	Draw(CurFrame);

	return EXIT_SUCCESS;
}

void Draw(const FrameBuffer& Frame)
{
	static constexpr char Shades[] = " .:*oe&#%@";
	for( std::size_t y = 0; y < Height; ++y )
	{
		std::fputs("\033[0;35m|\033[1;36m", stdout);
		for( std::size_t x = 0; x < Width; ++x )
		{
			std::putchar(
				Shades[
					static_cast<std::size_t>(
						glm::clamp(
							Frame[x + y * Width],
							0.0f,
							1.0f
						) * (std::extent<decltype(Shades)>::value - 2)
					)
				]
			);
		}
		std::fputs("\033[0;35m|\n", stdout);
	}
	std::fputs("\033[0m", stdout);
}

void FillTriangle(FrameBuffer& Frame, const qTri::Triangle& Tri)
{
	for( std::size_t y = 0; y < Height; ++y )
	{
		for( std::size_t x = 0; x < Width; ++x )
		{
			Frame[x + y * Width] = qTri::Barycentric({x, y}, Tri) ? 1.0f : 0.0f;
		}
	}
}
