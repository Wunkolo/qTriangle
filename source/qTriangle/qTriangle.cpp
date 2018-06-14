#include <qTriangle/qTriangle.hpp>

#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/component_wise.hpp>

#include <immintrin.h>

namespace qTri
{
// Get Cross-Product Z component from two directiona vectors
inline std::int32_t CrossArea(const Vec2& DirA, const Vec2& DirB)
{
	return DirA.x * DirB.y - DirA.y * DirB.x;
}

// Cross-product test against each edge, ensuring the area
// of each parallelogram is positive
// Requires that the triangle is in clockwise order
bool CrossTest(const Vec2& Point, const Triangle& Tri)
{
	return
		CrossArea(Tri.Vert[1] - Tri.Vert[0], Point - Tri.Vert[1]) >= 0 &&
		CrossArea(Tri.Vert[2] - Tri.Vert[1], Point - Tri.Vert[2]) >= 0 &&
		CrossArea(Tri.Vert[0] - Tri.Vert[2], Point - Tri.Vert[0]) >= 0;
}

void CrossFill(Image& Frame, const Triangle& Tri)
{
	const Vec2 DirectionBA = Tri.Vert[1] - Tri.Vert[0];
	const Vec2 DirectionCB = Tri.Vert[2] - Tri.Vert[1];
	const Vec2 DirectionAC = Tri.Vert[0] - Tri.Vert[2];

	const auto XBounds = std::minmax({Tri.Vert[0].x, Tri.Vert[1].x, Tri.Vert[2].x});
	const auto YBounds = std::minmax({Tri.Vert[0].y, Tri.Vert[1].y, Tri.Vert[2].y});
	for( std::int32_t y = YBounds.first; y < YBounds.second; ++y )
	{
		for( std::int32_t x = XBounds.first; x < XBounds.second; ++x )
		{
			const Vec2 CurPoint{x, y};
			const bool Inside =
				CrossArea(DirectionBA, CurPoint - Tri.Vert[1]) >= 0 &&
				CrossArea(DirectionCB, CurPoint - Tri.Vert[2]) >= 0 &&
				CrossArea(DirectionAC, CurPoint - Tri.Vert[0]) >= 0;
			Frame.Pixels[x + y * Frame.Width] |= Inside;
		}
	}
}

void CrossFillAVX2(Image& Frame, const Triangle& Tri)
{
	// Load triangle vertices (assumed clockwise order)
	const __m256i TriVerts012 = _mm256_maskload_epi64(
		reinterpret_cast<const long long int*>(&Tri.Vert),
		_mm256_set_epi64x(0, -1, -1, -1)
	);
	// Rotate vertex components
	const __m256i TriVerts120 = _mm256_permute4x64_epi64(
		TriVerts012,
		_MM_SHUFFLE(3, 0, 2, 1)
	);
	// Directional vectors such that each triangle vertex is pointing to
	// the vertex before it, in counter-clockwise order
	const __m256i TriDirections = _mm256_sub_epi64(
		TriVerts120,
		TriVerts012
	);

	// Iterate through triangle bounds
	const auto XBounds = std::minmax({Tri.Vert[0].x, Tri.Vert[1].x, Tri.Vert[2].x});
	const auto YBounds = std::minmax({Tri.Vert[0].y, Tri.Vert[1].y, Tri.Vert[2].y});
	const std::size_t Width = static_cast<std::size_t>(XBounds.second - XBounds.first);
	const std::size_t Height = static_cast<std::size_t>(YBounds.second - YBounds.first);

	std::uint8_t* Dest = &Frame.Pixels[XBounds.first + YBounds.first * Frame.Width];
	for( std::size_t y = 0; y < Height; ++y )
	{
		// Left-most point of current scanline
		__m256i CurPoint = _mm256_set1_epi64x(
			(static_cast<std::int64_t>(YBounds.first + y) << 32) | static_cast<std::int32_t>(XBounds.first)
		);
		// Rasterize Scanline
		for( std::size_t x = 0; x < Width; ++x )
		{
			const __m256i PointDir = _mm256_sub_epi32(
				CurPoint,
				TriVerts120
			);
			// Compute Z-components of cross products in parallel
			// Tests three `DirA.x * DirB.y - DirA.y * DirB.x`
			// values at once

			// Swap XY Values of Vector B's elements (int32_t)
			// A: [ ~ | ~ | X2 | Y2 | X1 | Y1 | X0 | Y0 ]
			// B: [ ~ | ~ | Y2 | X2 | Y1 | X1 | Y0 | X0 ]
			const __m256i Product = _mm256_mullo_epi32(
				TriDirections,
				// Shuffles the two 128-bit lanes
				_mm256_shuffle_epi32(
					PointDir,
					_MM_SHUFFLE(2, 3, 0, 1)
				)
			);
			// Shift Y values into X value columns
			const __m256i Lower = _mm256_srli_epi64(
				Product,
				32
			);
			// Get Z component of cross product
			const __m256i CrossAreas = _mm256_sub_epi32(
				Product,
				Lower
			);
			// Check if `X >= 0` for each of the 3 Z-components 
			// ( x >= 0 ) ⇒ ¬( X < 0 )
			Dest[x + y * Frame.Width] |= _mm256_testz_si256(
				// Check only the cross-area elements
				_mm256_set_epi32(0, 0, 0, -1, 0, -1, 0, -1),
				_mm256_cmpgt_epi32(
					_mm256_setzero_si256(),
					CrossAreas
				)
			);
			// Increment to next sample coordinate
			CurPoint = _mm256_add_epi32(
				CurPoint,
				_mm256_set1_epi64x(1)
			);
		}
	}
}

bool Barycentric(const Vec2& Point, const Triangle& Tri)
{
	const Vec2 V0 = Tri.Vert[2] - Tri.Vert[0];
	const Vec2 V1 = Tri.Vert[1] - Tri.Vert[0];
	const Vec2 V2 = Point - Tri.Vert[0];

	const std::uint32_t Dot00 = glm::compAdd(V0 * V0);
	const std::uint32_t Dot01 = glm::compAdd(V0 * V1);
	const std::uint32_t Dot02 = glm::compAdd(V0 * V2);
	const std::uint32_t Dot11 = glm::compAdd(V1 * V1);
	const std::uint32_t Dot12 = glm::compAdd(V1 * V2);

	const glm::float32_t Det = (Dot00 * Dot11 - Dot01 * Dot01);
	const glm::float32_t U = (Dot11 * Dot02 - Dot01 * Dot12) / Det;
	const glm::float32_t V = (Dot00 * Dot12 - Dot01 * Dot02) / Det;

	// Convert to local plane's Barycentric coordiante system
	return
		(U >= 0.0f) &&
		(V >= 0.0f) &&
		(U + V < 1.0f);
}

// With this method:
//  Only V2,Dot02,Dot12,U, and V have to be re-generated for each vertex
void BarycentricFill(Image& Frame, const Triangle& Tri)
{
	const Vec2 V0 = Tri.Vert[2] - Tri.Vert[0];
	const Vec2 V1 = Tri.Vert[1] - Tri.Vert[0];

	const std::uint32_t Dot00 = glm::compAdd(V0 * V0);
	const std::uint32_t Dot01 = glm::compAdd(V0 * V1);
	const std::uint32_t Dot11 = glm::compAdd(V1 * V1);

	const glm::float32_t Det = (Dot00 * Dot11 - Dot01 * Dot01);

	const auto XBounds = std::minmax({Tri.Vert[0].x, Tri.Vert[1].x, Tri.Vert[2].x});
	const auto YBounds = std::minmax({Tri.Vert[0].y, Tri.Vert[1].y, Tri.Vert[2].y});
	for( std::int32_t y = YBounds.first; y < YBounds.second; ++y )
	{
		for( std::int32_t x = XBounds.first; x < XBounds.second; ++x )
		{
			const Vec2 CurPoint{x, y};
			const Vec2 V2 = CurPoint - Tri.Vert[0];
			const std::uint32_t Dot02 = glm::compAdd(V0 * V2);
			const std::uint32_t Dot12 = glm::compAdd(V1 * V2);
			const glm::float32_t U = (Dot11 * Dot02 - Dot01 * Dot12) / Det;
			const glm::float32_t V = (Dot00 * Dot12 - Dot01 * Dot02) / Det;
			Frame.Pixels[x + y * Frame.Width] |= (
				(U >= 0.0f) &&
				(V >= 0.0f) &&
				(U + V < 1.0f)
			);
		}
	}
}

template< bool TestFunc(const Vec2& Point, const Triangle& Tri) >
void SerialBlit(Image& Frame, const Triangle& Tri)
{
	const auto XBounds = std::minmax({Tri.Vert[0].x, Tri.Vert[1].x, Tri.Vert[2].x});
	const auto YBounds = std::minmax({Tri.Vert[0].y, Tri.Vert[1].y, Tri.Vert[2].y});
	for( std::int32_t y = YBounds.first; y < YBounds.second; ++y )
	{
		for( std::int32_t x = XBounds.first; x < XBounds.second; ++x )
		{
			{
				Frame.Pixels[x + y * Frame.Width] |= TestFunc({x, y}, Tri);;
			}
		}
	}
}

const std::vector<
	std::pair<
		void(* const)(Image& Frame, const Triangle& Tri),
		const char*
	>
> FillAlgorithms = {
	{ SerialBlit<CrossTest>,   "Serial-CrossProduct"         },
	{ SerialBlit<Barycentric>, "Serial-Barycentric"          },
	{ CrossFill,               "Serial-CrossProductFill"     },
	{ CrossFillAVX2,           "Serial-CrossProductFillAVX2" },
	{ BarycentricFill,         "Serial-BarycentricFill"      }
};
}
