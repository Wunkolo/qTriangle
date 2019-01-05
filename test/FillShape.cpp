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

constexpr std::size_t Width = 300;
constexpr std::size_t Height = 300;

int main()
{
	// "a"
	const glm::i32vec2 Edges[] = {
		glm::i32vec2{235,152},
		glm::i32vec2{176,165},
		glm::i32vec2{144,173},
		glm::i32vec2{129,184},
		glm::i32vec2{124,202},
		glm::i32vec2{135,226},
		glm::i32vec2{168,235},
		glm::i32vec2{206,226},
		glm::i32vec2{230,201},
		glm::i32vec2{236,165},
		glm::i32vec2{235,152},

		glm::i32vec2{238,233},
		glm::i32vec2{200,257},
		glm::i32vec2{159,264},
		glm::i32vec2{105,247},
		glm::i32vec2{86,203},
		glm::i32vec2{93,174},
		glm::i32vec2{112,153},
		glm::i32vec2{138,141},
		glm::i32vec2{171,136},
		glm::i32vec2{236,123},
		glm::i32vec2{236,114},
		glm::i32vec2{226,82},
		glm::i32vec2{184,70},
		glm::i32vec2{146,79},
		glm::i32vec2{128,111},
		glm::i32vec2{92,106},
		glm::i32vec2{108,69},
		glm::i32vec2{140,48},
		glm::i32vec2{189,40},
		glm::i32vec2{234,47},
		glm::i32vec2{259,63},
		glm::i32vec2{270,87},
		glm::i32vec2{272,121},
		glm::i32vec2{272,169},
		glm::i32vec2{274,233},
		glm::i32vec2{283,259},
		glm::i32vec2{246,259},
		glm::i32vec2{238,233},
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
		const glm::i32vec2 Center = std::accumulate(
			std::cbegin(Triangles[i].Vert),
			std::cend(Triangles[i].Vert),
			glm::i32vec2{0,0}
		) / 3;
		std::sort(
			std::begin(Triangles[i].Vert),
			std::end(Triangles[i].Vert),
			[&Center](const glm::i32vec2& A, const glm::i32vec2& B) -> bool
				{
					// Sort points by its angle from the center
					const glm::i32vec2 DirectionA = Center - A;
					const glm::i32vec2 DirectionB = Center - B;
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
