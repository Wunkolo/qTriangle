#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <type_traits>
#include <algorithm>
#include <random>

#include <glm/glm.hpp>

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

static qTri::Triangle Triangles[100'000];

int main()
{
	// Generate random triangles
	std::random_device RandomDevice;
	std::mt19937 RandomEngine(RandomDevice());
	std::uniform_int_distribution<std::int32_t> WidthDis(0, Width);
	std::uniform_int_distribution<std::int32_t> HeightDis(0, Height);
	for( qTri::Triangle& CurTriangle : Triangles )
	{
		qTri::Vec2 Center{};
		// Randomly place vertices
		for( qTri::Vec2& CurVert : CurTriangle.Vert )
		{
			CurVert.x = WidthDis(RandomEngine);
			CurVert.y = HeightDis(RandomEngine);
			Center += CurVert;
		}
		// Sort points in clockwise order
		Center /= 3;
		std::sort(
			std::begin(CurTriangle.Vert),
			std::end(CurTriangle.Vert),
			[&Center](const qTri::Vec2& A, const qTri::Vec2& B) -> bool
				{
					// Sort points by its angle from the center
					const qTri::Vec2 DirectionA = Center - A;
					const qTri::Vec2 DirectionB = Center - B;
					const auto AngleA = glm::atan<glm::float32_t>(DirectionA.y, DirectionA.x);
					const auto AngleB = glm::atan<glm::float32_t>(DirectionB.y, DirectionB.x);
					return AngleA < AngleB;
				}
		);
	}
	std::printf(
		"%zu Triangles\n"
		"%zu x %zu Image map\n",
		std::extent<decltype(Triangles)>::value,
		Width,
		Height
	);
	std::printf(
		"Algorithm | Average per triangle(ns) | Average per triangle(ms)\n"
	);
	// Benchmark each algorithm against all triangles
	for( const auto& FillAlgorithm : qTri::FillAlgorithms )
	{
		std::printf(
			"%s\t",
			FillAlgorithm.second
		);
		qTri::Image CurFrame(Width, Height);
		std::size_t ExecTime = 0;
		for( const qTri::Triangle& CurTriangle : Triangles )
		{
			ExecTime += Bench<>::Duration(
				FillAlgorithm.first,
				CurFrame,
				CurTriangle
			).count();
		}
		ExecTime /= std::extent<decltype(Triangles)>::value;
		std::printf(
			"| %zu ns\t| %zu ms\t\n",
			ExecTime,
			ExecTime / 1000000
		);
	}
	return EXIT_SUCCESS;
}
