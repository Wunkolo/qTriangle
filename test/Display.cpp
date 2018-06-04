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

static qTri::Triangle Triangles[5];

int main()
{
	// Generate random triangles
	std::random_device RandomDevice;
	std::mt19937 RandomEngine(RandomDevice());
	std::uniform_int_distribution<std::int32_t> WidthDis(0, Width);
	std::uniform_int_distribution<std::int32_t> HeightDis(0, Height);
	for( qTri::Triangle& CurTriangle : Triangles )
	{
		CurTriangle.A.x = WidthDis(RandomEngine);
		CurTriangle.A.y = HeightDis(RandomEngine);
		CurTriangle.B.x = WidthDis(RandomEngine);
		CurTriangle.B.y = HeightDis(RandomEngine);
		CurTriangle.C.x = WidthDis(RandomEngine);
		CurTriangle.C.y = HeightDis(RandomEngine);
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
				CurFrame,
				CurTriangle
			).count();
		}
		ExecTime /= std::extent<decltype(Triangles)>::value;
		std::printf(
			"%zu ns | %zu ms\n",
			ExecTime,
			ExecTime / 1000000
		);
		qTri::Util::Draw(CurFrame);
	}

	return EXIT_SUCCESS;
}
