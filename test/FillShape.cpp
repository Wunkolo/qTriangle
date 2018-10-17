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

constexpr std::size_t Width = 200;
constexpr std::size_t Height = 250;

int main()
{
	// Simple Square
	const qTri::Vec2 Edges[] = {
		qTri::Vec2{120, 110},
		qTri::Vec2{130, 140},
		qTri::Vec2{160, 140},
		qTri::Vec2{140, 160},
		qTri::Vec2{150, 190},
		qTri::Vec2{120, 170},
		qTri::Vec2{ 90, 190},
		qTri::Vec2{100, 160},
		qTri::Vec2{ 80, 140},
		qTri::Vec2{110, 140}
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
			
			// Write an image of the current triangle
			// Post-process from [0x00,0x01] to [0x00,0xFF]
			// Compiler vectorization loves loops like this
			for( std::size_t i = 0; i < Width * Height; ++i )
			{
				CurInversion.Pixels[i] *= 0xFF;
			}
			stbi_write_png(
				(FrameFolder / ("Tri" + std::to_string(FrameIdx) + ".png")).c_str(),
				Width,
				Height,
				1,
				CurInversion.Pixels.data(),
				0
			);
			// ffmpeg -f image2 -framerate 2 -i %d.png -vf "scale=iw*2:ih*2" -sws_flags neighbor Anim.gif
			++FrameIdx;
		}
	}

	return EXIT_SUCCESS;
}
