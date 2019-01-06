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
inline bool CrossTest(const glm::i32vec2& Point, const Triangle& Tri)
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

void CrossFill(
	const glm::i32vec2 Points[], std::uint8_t Results[], std::size_t Count,
	const Triangle& Tri
)
{
	// Directional vectors along all three triangle edges
	const glm::i32vec2 EdgeDir[3] = {
		Tri[1] - Tri[0],
		Tri[2] - Tri[1],
		Tri[0] - Tri[2]
	};

	// Iterate columns within scanline
	std::size_t i = 0;

	///
	// SIMD stuff would go here
	///

	// Serial
	for( ; i < Count; ++i )
	{
		const glm::i32vec2 PointDir[3] = {
			Points[i] - Tri[0],
			Points[i] - Tri[1],
			Points[i] - Tri[2]
		};

		const glm::i32vec3 Crosses = glm::vec3(
			Det( EdgeDir[0], PointDir[0] ),
			Det( EdgeDir[1], PointDir[1] ),
			Det( EdgeDir[2], PointDir[2] )
		);

		Results[i] |= glm::all(
			glm::greaterThanEqual(
				Crosses,
				glm::i32vec3(0)
			)
		);
	}
}

//// Barycentric Method

inline bool Barycentric(const glm::i32vec2& Point, const Triangle& Tri)
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

void BarycentricFill(
	const glm::i32vec2 Points[], std::uint8_t Results[], std::size_t Count,
	const Triangle& Tri
)
{
	const std::int32_t Det01 = Det( Tri[0], Tri[1] );
	const std::int32_t Det20 = Det( Tri[2], Tri[0] );
	const std::int32_t Area  = Det( Tri[1], Tri[2] ) + Det20 + Det01;

	std::size_t i = 0;

	///
	// SIMD stuff would go here
	///

	// Serial
	for( ; i < Count; ++i )
	{
		const std::int32_t U = Det20
			+ Det(      Tri[0], Points[i] )
			+ Det(   Points[i],    Tri[2] );
		const std::int32_t V = Det01
			+ Det(      Tri[1], Points[i] )
			+ Det(   Points[i],    Tri[0] );

		Results[i] |= (U + V) < Area && U >= 0 && V >= 0;
	}
}

//// Utils

template< bool TestFunc(const glm::i32vec2& Point, const Triangle& Tri) >
void SerialBlit(
	const glm::i32vec2 Points[], std::uint8_t Results[], std::size_t Count,
	const Triangle& Tri
)
{
	std::size_t i = 0;
	// Serial
	for( ; i < Count; ++i )
	{
		Results[i] |= TestFunc(Points[i],Tri);
	}
}

//// Exports

const std::vector<
	std::pair<
		void(* const)(
			const glm::i32vec2 Points[], std::uint8_t Results[], std::size_t Count,
			const Triangle& Tri
		),
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

