#include <qTriangle/qTriangle.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/component_wise.hpp>

namespace qTri
{

// Get Cross-Product Z component from two directional vectors
inline std::int32_t Det(
	const glm::i32vec2& Top,
	const glm::i32vec2& Bottom
)
{
	return Top.x * Bottom.y - Top.y * Bottom.x;
}

//// Cross Product Method

template<std::uint8_t WidthExp2>
inline void CrossProductMethod(
	const glm::i32vec2 Points[], std::uint8_t Results[], std::size_t Count,
	const Triangle& Tri
)
{
	CrossProductMethod<WidthExp2-1>(
		Points, Results, Count,
		Tri
	);
}

// Serial
template<>
inline void CrossProductMethod<0>(
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

	for( std::size_t i = 0; i < Count; ++i )
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

template<std::uint8_t WidthExp2>
inline void BarycentricMethod(
	const glm::i32vec2 Points[], std::uint8_t Results[], std::size_t Count,
	const Triangle& Tri
)
{
	BarycentricMethod<WidthExp2-1>(
		Points, Results, Count,
		Tri
	);
}

// Serial
template<>
inline void BarycentricMethod<0>(
	const glm::i32vec2 Points[], std::uint8_t Results[], std::size_t Count,
	const Triangle& Tri
)
{
	const std::int32_t Det01 = Det( Tri[0], Tri[1] );
	const std::int32_t Det20 = Det( Tri[2], Tri[0] );
	const std::int32_t Area  = Det( Tri[1], Tri[2] ) + Det20 + Det01;

	for( std::size_t i = 0; i < Count; ++i )
	{
		const std::int32_t U = Det20
			+ Det(    Tri[0], Points[i] )
			+ Det( Points[i],    Tri[2] );
		const std::int32_t V = Det01
			+ Det(    Tri[1], Points[i] )
			+ Det( Points[i],    Tri[0] );

		Results[i] |= (U + V) < Area && U >= 0 && V >= 0;
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
	{CrossProductMethod<  0>,	"Serial-CrossProduct"},
	{CrossProductMethod< -1>,	"CrossProductMethod"},
	// Barycentric methods
	{BarycentricMethod<  0>,	"Serial-Barycentric"},
	{BarycentricMethod< -1>,	"BarycentricMethod"},
};
}

