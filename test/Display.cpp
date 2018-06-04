#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <type_traits>
#include <array>
#include <algorithm>

#include <glm/glm.hpp>

#include <qTriangle/qTriangle.hpp>

#ifdef _WIN32
#define NOMINMAX
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
using FrameBuffer = std::array<bool, Width * Height>;

void Draw(const FrameBuffer& Frame);
void FillTriangle(FrameBuffer& Frame, const qTri::Triangle& Tri);

int main()
{
	FrameBuffer CurFrame = {};
	const qTri::Triangle Triangles[] = {
		{
			{Width, 0},
			{Width, Height / 2},
			{0, Height}
		},
		{
			{0, 0},
			{Width, Height / 2},
			{Width / 2, Height}
		},
	};
	for( std::size_t i = 0; i < std::extent<decltype(Triangles)>::value; ++i )
	{
		FillTriangle(
			CurFrame,
			Triangles[i]
		);
	}
	Draw(CurFrame);

	return EXIT_SUCCESS;
}

void Draw(const FrameBuffer& Frame)
{
	for( std::size_t y = 0; y < Height; ++y )
	{
		std::fputs("\033[0;35m|\033[1;36m", stdout);
		for( std::size_t x = 0; x < Width; ++x )
		{
			std::putchar(
				" @"[Frame[x + y * Width]]
			);
		}
		std::fputs("\033[0;35m|\n", stdout);
	}
	std::fputs("\033[0m", stdout);
}

void FillTriangle(FrameBuffer& Frame, const qTri::Triangle& Tri)
{
	const auto XBounds = std::minmax({Tri.A.x,Tri.B.x,Tri.C.x});
	const auto YBounds = std::minmax({Tri.A.y,Tri.B.y,Tri.C.y});
	for( std::size_t y = std::min<std::size_t>( YBounds.first, 0);
		y < std::max<std::size_t>(YBounds.second, Height);
		++y
	)
	{
		for( std::size_t x = std::min<std::size_t>(XBounds.first, 0);
			x < std::max<std::size_t>(XBounds.second, Width);
			++x
		)
		{
			Frame[x + y * Width] |= qTri::CrossTest({x, y}, Tri);
		}
	}
}
