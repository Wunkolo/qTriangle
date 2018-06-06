#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <type_traits>
#include <algorithm>
#include <numeric>
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/trigonometric.hpp>

#include <qTriangle/qTriangle.hpp>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

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
constexpr std::size_t Height = 80;

int main()
{
	// Simple Square
	const qTri::Vec2 Edges[] = {
		qTri::Vec2{10, 10},
		qTri::Vec2{70, 10},
		qTri::Vec2{70, 70},
		qTri::Vec2{10, 70}
	};

	qTri::Triangle Triangles[std::extent<decltype(Edges)>::value];

	for( std::size_t i = 0; i < std::extent<decltype(Edges)>::value; ++i )
	{
		Triangles[i] = qTri::Triangle{
			{
				{0, 0},
				Edges[i],
				Edges[(i + 1) % std::extent<decltype(Edges)>::value]
			}
		};
		const qTri::Vec2 Center = std::accumulate(
			std::cbegin(Triangles[i].Vert),
			std::cend(Triangles[i].Vert),
			qTri::Vec2{}
		) / 3;
		std::sort(
			std::begin(Triangles[i].Vert),
			std::end(Triangles[i].Vert),
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

	for( const auto& FillAlgorithm : qTri::FillAlgorithms )
	{
		std::printf(
			"%s:\n",
			FillAlgorithm.second
		);
		const auto FrameFolder = fs::path("Frames") / FillAlgorithm.second;
		fs::create_directories(FrameFolder);
		qTri::Image Frame(Width, Height);
		std::size_t FrameIdx = 0;
		for( const qTri::Triangle& CurTriangle : Triangles )
		{
			qTri::Image CurInversion(Width, Height);
			// Render triangle to inversion mask
			FillAlgorithm.first(CurInversion, CurTriangle);

			// Append inversion mask
			for( std::size_t i = 0; i < Width * Height; ++i )
			{
				Frame.Pixels[i] = CurInversion.Pixels[i] ? ~Frame.Pixels[i] : Frame.Pixels[i];
			}
			stbi_write_png(
				((FrameFolder / std::to_string(FrameIdx)).string() + ".png").c_str(),
				Width,
				Height,
				1,
				Frame.Pixels.data(),
				0
			);
			//qTri::Util::Draw(CurFrame);
			++FrameIdx;
		}
	}

	return EXIT_SUCCESS;
}
