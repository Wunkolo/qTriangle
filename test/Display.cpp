#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <type_traits>
#include <array>

#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/component_wise.hpp>

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

bool Barycentric(const qTri::Vec2& Point, const qTri::Triangle& Tri)
{
	const qTri::Vec2 V0 = Tri.C - Tri.A;
	const qTri::Vec2 V1 = Tri.B - Tri.A;
	const qTri::Vec2 V2 = Point - Tri.A;

	const std::uint32_t Dot00 = glm::compAdd(V0 * V0);
	const std::uint32_t Dot01 = glm::compAdd(V0 * V1);
	const std::uint32_t Dot02 = glm::compAdd(V0 * V2);
	const std::uint32_t Dot11 = glm::compAdd(V1 * V1);
	const std::uint32_t Dot12 = glm::compAdd(V1 * V2);

	const glm::float32_t InvDenom = 1.0f / (Dot00 * Dot11 - Dot01 * Dot01);
	const glm::float32_t U = (Dot11 * Dot02 - Dot01 * Dot12) * InvDenom;
	const glm::float32_t V = (Dot00 * Dot12 - Dot01 * Dot02) * InvDenom;

	return (U >= 0.0f) && (V >= 0.0f) && (U + V < 1.0f);
}

void FillTriangle(FrameBuffer& Frame, const qTri::Triangle& Tri)
{
	for( std::size_t y = 0; y < Height; ++y )
	{
		for( std::size_t x = 0; x < Width; ++x )
		{
			Frame[x + y * Width] = Barycentric({x, y}, Tri) ? 1.0f : 0.0f;
		}
	}
}
