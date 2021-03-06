#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <type_traits>
#include <algorithm>
#include <random>

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/trigonometric.hpp>

#include <qTriangle/qTriangle.hpp>

#include "Bench.hpp"

#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
// Statically enables "ENABLE_VIRTUAL_TERMINAL_PROCESSING" for the terminal
// at runtime to allow for unix-style escape sequences. 
static const bool _WndV100Enabled = []() -> bool
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

static qTri::Triangle Triangles[12];

int main()
{
	// Generate random triangles
	std::random_device RandomDevice;
	std::mt19937 RandomEngine(RandomDevice());
	std::uniform_int_distribution<std::int32_t> WidthDis(0, Width);
	std::uniform_int_distribution<std::int32_t> HeightDis(0, Height);
	for( qTri::Triangle& CurTriangle : Triangles )
	{
		glm::i32vec2 Center{};
		// Randomly place vertices
		for( glm::i32vec2& CurVert : CurTriangle )
		{
			CurVert.x = WidthDis(RandomEngine);
			CurVert.y = HeightDis(RandomEngine);
			Center += CurVert;
		}
		// Sort points in clockwise order
		Center /= 3;
		std::sort(
			std::begin(CurTriangle),
			std::end(CurTriangle),
			[&Center](const glm::i32vec2& A, const glm::i32vec2& B) -> bool
				{
					// Points that have a larger angle away from the center are "heavier"
					const glm::i32vec2 DirectionA = Center - A;
					const glm::i32vec2 DirectionB = Center - B;
					const auto AngleA = glm::atan<glm::float32_t>(DirectionA.y, DirectionA.x);
					const auto AngleB = glm::atan<glm::float32_t>(DirectionB.y, DirectionB.x);
					return AngleA < AngleB;
				}
		);
	}
	
	// Generate 2d grid of points to test against
	std::vector<glm::i32vec2> FragCoords;
	for( std::size_t y = 0; y < Height; ++y )
	{
		for( std::size_t x = 0; x < Width; ++x )
		{
			FragCoords.emplace_back(x,y);
		}
	}

	for( const auto& FillAlgorithm : qTri::FillAlgorithms )
	{
		std::printf(
			"%s - ",
			FillAlgorithm.second
		);
		qTri::Image CurFrame(Width, Height);
		std::size_t ExecTime = 0;
		for( const qTri::Triangle& CurTriangle : Triangles )
		{
			ExecTime += Bench<>::Duration(
				FillAlgorithm.first,
				FragCoords.data(),
				CurFrame.Pixels.data(),
				FragCoords.size(),
				CurTriangle
			).count();
		}
		ExecTime /= std::extent<decltype(Triangles)>::value;
		std::printf(
			"%zu ns\n",
			ExecTime
		);
		qTri::Util::Draw(CurFrame);
	}

	return EXIT_SUCCESS;
}
