#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <type_traits>
#include <array>

#include <glm/glm.hpp>

constexpr std::size_t Width = 80;
constexpr std::size_t Height = 50;
using FrameBuffer = std::array< glm::float32_t, Width * Height >;

struct Triangle
{
	// Clock-wise order
	glm::u64vec2 A;
	glm::u64vec2 B;
	glm::u64vec2 C;
};

void Draw( const FrameBuffer& Frame );
void FillTriangle( FrameBuffer& Frame, const Triangle& Tri );

int main()
{
	FrameBuffer CurFrame = {};
	FillTriangle(
		CurFrame,
		{
			{       0,         0 },
			{   Width,  Height/2 },
			{ Width/2,    Height }
		}
	);
	Draw( CurFrame );

	return EXIT_SUCCESS;
}

void Draw( const FrameBuffer& Frame )
{
	static constexpr char Shades[] = " .:*oe&#%@";
	for( std::size_t y = 0; y < Height; ++y )
	{
		std::putchar('|');
		for( std::size_t x = 0; x < Width; ++x )
		{
			std::putchar(
				Shades[
					static_cast<std::size_t>(
						glm::clamp(
							Frame[ x + y * Width ],
							0.0f,
							1.0f
						) * ( std::extent<decltype(Shades)>::value - 2 )
					)
				]
			);
		}
		std::puts("|");
	}
}

void FillTriangle( FrameBuffer& Frame, const Triangle& Tri )
{
	const glm::u64vec2 V0 = Tri.C - Tri.A;
	const glm::u64vec2 V1 = Tri.B - Tri.A;

	for( std::size_t y = 0; y < Height; ++y )
	{
		for( std::size_t x = 0; x < Width; ++x )
		{
			const glm::u64vec2 CurPoint{ x, y };
			const glm::u64vec2 V2 = CurPoint - Tri.A;

			const std::uint64_t Dot00 = V0.x * V0.x + V0.y * V0.y;
			const std::uint64_t Dot01 = V0.x * V1.x + V0.y * V1.y;
			const std::uint64_t Dot02 = V0.x * V2.x + V0.y * V2.y;
			const std::uint64_t Dot11 = V1.x * V1.x + V1.y * V1.y;
			const std::uint64_t Dot12 = V1.x * V2.x + V1.y * V2.y;

			const glm::float64_t InvDenom = 1.0 / (Dot00 * Dot11 - Dot01 * Dot01);
			const glm::float64_t U = (Dot11 * Dot02 - Dot01 * Dot12) * InvDenom;
			const glm::float64_t V = (Dot00 * Dot12 - Dot01 * Dot02) * InvDenom;

			if(
				(U >= 0.0) &&
				(V >= 0.0) &&
				(U + V < 1.0)
			)
			{
				Frame[x + y * Width ] = 1.0f;
			}
		}
	}
}
