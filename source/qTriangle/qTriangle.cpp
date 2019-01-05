#include <qTriangle/qTriangle.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/component_wise.hpp>

namespace qTri
{
//// Cross Product Method

// Get Cross-Product Z component from two directional vectors
inline std::int32_t Det(
	const glm::i32vec2& Top,
	const glm::i32vec2& Bottom
)
{
	return Top.x * Bottom.y - Top.y * Bottom.x;
}

// Cross-product test against each edge, ensuring the area
// of each parallelogram is positive
// Requires that the triangle is in clockwise order
bool CrossTest(const glm::i32vec2& Point, const Triangle& Tri)
{
	return glm::all(
		glm::greaterThanEqual(
			glm::i32vec3(
				Det(Tri[1] - Tri[0], Point - Tri[1]),
				Det(Tri[2] - Tri[1], Point - Tri[2]),
				Det(Tri[0] - Tri[2], Point - Tri[0])
			),
			glm::i32vec3(0)
		)
	);
}

void CrossFill(Image& Frame, const Triangle& Tri)
{
	// Directional vectors along all three triangle edges
	const glm::i32vec2 EdgeDir[3] = {
		Tri[1] - Tri[0],
		Tri[2] - Tri[1],
		Tri[0] - Tri[2]
	};

	// Calculate triangle bounds
	const auto XBounds = std::minmax({Tri[0].x, Tri[1].x, Tri[2].x});
	const auto YBounds = std::minmax({Tri[0].y, Tri[1].y, Tri[2].y});
	// Bounds width and height
	const std::size_t Width  = static_cast<std::size_t>(XBounds.second - XBounds.first);
	const std::size_t Height = static_cast<std::size_t>(YBounds.second - YBounds.first);

	// Pre-offset the output buffer by the bounds of the triangle
	std::uint8_t* Fragments = &Frame.Pixels[XBounds.first + YBounds.first * Frame.Width];

	// Point being tested against
	glm::i32vec2 CurPoint(XBounds.first,YBounds.first);

	// Iterate scanlines
	for( std::size_t y = 0; y < Height; ++y )
	{
		// Iterate columns within scanline
		std::size_t x = 0;

		///
		// SIMD stuff would go here
		///

		// Serial
		for( ; x < Width; x += 1 )
		{
			const glm::i32vec2 PointDir[3] = {
				CurPoint - Tri[0],
				CurPoint - Tri[1],
				CurPoint - Tri[2]
			};

			const glm::i32vec3 Crosses = glm::vec3(
				Det( EdgeDir[0], PointDir[0] ),
				Det( EdgeDir[1], PointDir[1] ),
				Det( EdgeDir[2], PointDir[2] )
			);

			Fragments[x] |= glm::all(
				glm::greaterThanEqual(
					Crosses,
					glm::i32vec3(0)
				)
			);
			// Increment to next pixel
			CurPoint.x += 1;
		}
		// Offset output buffer to next scanline
		Fragments += Frame.Width;
		// Increment to next row
		CurPoint.y += 1;
		// Move X coordinate back to left-most side
		CurPoint.x = XBounds.first;
	}
}

//// Barycentric Method

bool Barycentric(const glm::i32vec2& Point, const Triangle& Tri)
{
	const std::int32_t Det01 = Det( Tri[0], Tri[1] );
	const std::int32_t Det20 = Det( Tri[2], Tri[0] );

	const std::int32_t U = Det20
		+ Det(      Tri[0],  Point )
		+ Det(       Point, Tri[2] );
	const std::int32_t V = Det01
		+ Det(      Tri[1],  Point )
		+ Det(       Point, Tri[0] );

	if( U < 0 || V < 0 )
	{
		return false;
	}

	const std::int32_t Area = Det( Tri[1], Tri[2] ) + Det20 + Det01;

	return (U + V) < Area;
}

void BarycentricFill(Image& Frame, const Triangle& Tri)
{
	const std::int32_t Det01 = Det( Tri[0], Tri[1] );
	const std::int32_t Det20 = Det( Tri[2], Tri[0] );
	const std::int32_t Area  = Det( Tri[1], Tri[2] ) + Det20 + Det01;
	const auto XBounds = std::minmax({Tri[0].x, Tri[1].x, Tri[2].x});
	const auto YBounds = std::minmax({Tri[0].y, Tri[1].y, Tri[2].y});
	// Bounds width and height
	const std::size_t Width  = static_cast<std::size_t>(XBounds.second - XBounds.first);
	const std::size_t Height = static_cast<std::size_t>(YBounds.second - YBounds.first);

	// Pre-offset the output buffer by the bounds of the triangle
	std::uint8_t* Fragments = &Frame.Pixels[XBounds.first + YBounds.first * Frame.Width];

	// Point being tested against
	glm::i32vec2 CurPoint(XBounds.first,YBounds.first);

	// Iterate scanlines
	for( std::size_t y = 0; y < Height; ++y )
	{
		// Iterate columns within scanline
		std::size_t x = 0;

		///
		// SIMD stuff would go here
		///

		// Serial
		for( ; x < Width; x += 1 )
		{
			const std::int32_t U = Det20
				+ Det(      Tri[0],  CurPoint )
				+ Det(    CurPoint,    Tri[2] );
			const std::int32_t V = Det01
				+ Det(      Tri[1],  CurPoint )
				+ Det(    CurPoint,    Tri[0] );

			Fragments[x] |= (U + V) < Area && U >= 0 && V >= 0;
			// Increment to next pixel
			CurPoint.x += 1;
		}
		// Offset output buffer to next scanline
		Fragments += Frame.Width;
		// Increment to next row
		CurPoint.y += 1;
		// Move X coordinate back to left-most side
		CurPoint.x = XBounds.first;
	}
}
//// Utils

template< bool TestFunc(const glm::i32vec2& Point, const Triangle& Tri) >
void SerialBlit(Image& Frame, const Triangle& Tri)
{
	const auto XBounds = std::minmax({Tri[0].x, Tri[1].x, Tri[2].x});
	const auto YBounds = std::minmax({Tri[0].y, Tri[1].y, Tri[2].y});
	glm::i32vec2 CurPoint;
	for( CurPoint.y = YBounds.first; CurPoint.y < YBounds.second; ++CurPoint.y )
	{
		for( CurPoint.x = XBounds.first; CurPoint.x < XBounds.second; ++CurPoint.x )
		{
			Frame.Pixels[CurPoint.x + CurPoint.y * Frame.Width] |= TestFunc(CurPoint, Tri);
		}
	}
}

//// Exports

const std::vector<
std::pair<
void(* const)(Image& Frame, const Triangle& Tri),
const char*
>
> FillAlgorithms = {
	// Cross-Product methods
	{SerialBlit<CrossTest>,		"Serial-CrossProduct"},
	{CrossFill,					"CrossProductFill"},
	// Barycentric methods
	{SerialBlit<Barycentric>,	"Serial-Barycentric"},
	{BarycentricFill,			"BarycentricFill"},
};
}

