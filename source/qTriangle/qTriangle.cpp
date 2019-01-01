#include <qTriangle/qTriangle.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/component_wise.hpp>
#include <glm/gtx/scalar_relational.hpp>

#ifdef __AVX2__
#include <immintrin.h>
#endif

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif

namespace qTri
{
//// Cross Product Method

// Get Cross-Product Z component from two directiona vectors
// This is just a 2x2 determinant
inline std::int32_t CrossArea(const glm::i32vec2& DirA, const glm::i32vec2& DirB)
{
	return DirA.x * DirB.y - DirA.y * DirB.x;
}

// Cross-product test against each edge, ensuring the area
// of each parallelogram is positive
// Requires that the triangle is in clockwise order
bool CrossTest(const glm::i32vec2& Point, const Triangle& Tri)
{
	return
		glm::all(
			glm::greaterThanEqual(
				glm::i32vec3(
					CrossArea(Tri.Vert[1] - Tri.Vert[0], Point - Tri.Vert[1]),
					CrossArea(Tri.Vert[2] - Tri.Vert[1], Point - Tri.Vert[2]),
					CrossArea(Tri.Vert[0] - Tri.Vert[2], Point - Tri.Vert[0])
				),
				glm::i32vec3(0)
			)
		);
}

void CrossFill(Image& Frame, const Triangle& Tri)
{
	const glm::i32vec2 EdgeDir[3] = {
		Tri.Vert[1] - Tri.Vert[0],
		Tri.Vert[2] - Tri.Vert[1],
		Tri.Vert[0] - Tri.Vert[2]
	};

	const auto XBounds = std::minmax({Tri.Vert[0].x, Tri.Vert[1].x, Tri.Vert[2].x});
	const auto YBounds = std::minmax({Tri.Vert[0].y, Tri.Vert[1].y, Tri.Vert[2].y});
	glm::i32vec2 CurPoint;
	for( CurPoint.y = YBounds.first; CurPoint.y < YBounds.second; ++CurPoint.y )
	{
		for( CurPoint.x = XBounds.first; CurPoint.x < XBounds.second; ++CurPoint.x )
		{
			const glm::i32vec2 PointDir[3] = {
				CurPoint - Tri.Vert[0],
				CurPoint - Tri.Vert[1],
				CurPoint - Tri.Vert[2]
			};

			const glm::i32vec3 Crosses = glm::vec3(
				CrossArea( EdgeDir[0], PointDir[0] ),
				CrossArea( EdgeDir[1], PointDir[1] ),
				CrossArea( EdgeDir[2], PointDir[2] )
			);

			Frame.Pixels[CurPoint.x + CurPoint.y * Frame.Width] |= glm::all(
				glm::greaterThanEqual(
					Crosses,
					glm::i32vec3(0)
				)
			);
		}
	}
}

#ifdef __AVX2__
void CrossFillAVX2(Image& Frame, const Triangle& Tri)
{
	// Load triangle vertices (assumed clockwise order)
	// Each 2D point is a pair of 32-bit integers being loaded as
	// if they were three 64-bit integers
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
	const __m256i TriDirections = _mm256_sub_epi32(
		TriVerts120,
		TriVerts012
	);

	// Iterate through triangle bounds
	const auto XBounds = std::minmax({Tri.Vert[0].x, Tri.Vert[1].x, Tri.Vert[2].x});
	const auto YBounds = std::minmax({Tri.Vert[0].y, Tri.Vert[1].y, Tri.Vert[2].y});
	const std::size_t Width = static_cast<std::size_t>(XBounds.second - XBounds.first);
	const std::size_t Height = static_cast<std::size_t>(YBounds.second - YBounds.first);

	std::uint8_t* Dest = &Frame.Pixels[XBounds.first + YBounds.first * Frame.Width];
	__m256i CurPoint = _mm256_set_epi32(
		YBounds.first, XBounds.first,
		YBounds.first, XBounds.first,
		YBounds.first, XBounds.first,
		YBounds.first, XBounds.first
	);
	// Left-most point of current scanline
	for( std::size_t y = 0; y < Height; ++y, Dest += Frame.Width )
	{
		// Rasterize Scanline
		std::size_t x = 0;

		#if defined(__AVX512F__) || defined(_MSC_VER)
		// Two samples at a time
		for( std::size_t i = 0; i < (Width - x) / 2; ++i, x += 2 )
		{
			const __m512i PointDir = _mm512_sub_epi32(
				_mm512_add_epi32(
					_mm512_broadcast_i32x8(CurPoint),
					_mm512_set_epi32(
						0, 1, 0, 1, // 
						0, 1, 0, 1, // NextPoint
						0, 0, 0, 0, // 
						0, 0, 0, 0  // Current Point
					)
				),
				_mm512_broadcast_i32x8(TriVerts120)
			);

			// Compute Z-components of cross products in parallel
			// Tests three `DirA.x * DirB.y - DirA.y * DirB.x`
			// values at once

			// Swap XY Values of Vector B's elements (int32_t)
			// A: [ ~ | ~ | X2 | Y2 | X1 | Y1 | X0 | Y0 ]
			// B: [ ~ | ~ | Y2 | X2 | Y1 | X1 | Y0 | X0 ]
			const __m512i Product = _mm512_mullo_epi32(
				_mm512_broadcast_i32x8(TriDirections),
				// Shuffles the two 128-bit lanes
				_mm512_shuffle_epi32(
					PointDir,
					static_cast<_MM_PERM_ENUM>(_MM_SHUFFLE(2, 3, 0, 1))
				)
			);

			// Shift Y values into X value columns
			const __m512i Lower = _mm512_srli_epi64(
				Product,
				32
			);
			// Get Z component of cross product
			const __m512i CrossAreas = _mm512_sub_epi32(
				Product,
				Lower
			);
			// Check if `X >= 0` for each of the 3 Z-components 
			// ( x >= 0 ) ⇒ ¬( X < 0 )
			const __mmask16 Test = _mm512_knot(
				_mm512_cmpge_epi32_mask(
					CrossAreas,
					_mm512_setzero_si512()
				)
			);
			Dest[x+0] |= ( Test & (0b00010101 << 0) ) == (0b00010101 << 0);
			Dest[x+1] |= ( Test & (0b00010101 << 8) ) == (0b00010101 << 8);
			// Increment to next column
			CurPoint = _mm256_add_epi32(
				CurPoint,
				_mm256_set_epi32(
					0, 2,
					0, 2,
					0, 2,
					0, 2
				)
			);
		}
		#endif
		// Serial
		for( ; x < Width; ++x )
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
			Dest[x] |= (_mm256_movemask_epi8(
				_mm256_cmpgt_epi32(
					CrossAreas,
					_mm256_setzero_si256()
				)
			) & 0x000F0F0F) == 0x000F0F0F;
			// Increment to next column
			CurPoint = _mm256_add_epi32(
				CurPoint,
				_mm256_set_epi32(
					0, 1,
					0, 1,
					0, 1,
					0, 1
				)
			);
		}
		// Increment to next Scanline
		CurPoint = _mm256_add_epi32(
			CurPoint,
			_mm256_set_epi32(
				1, 0,
				1, 0,
				1, 0,
				1, 0
			)
		);
		// Move CurPoint back to the left of the scanline
		CurPoint = _mm256_blend_epi32(
			CurPoint,
			_mm256_set1_epi32(XBounds.first),
			0b01010101
		);
	}
}
#endif

#ifdef __ARM_NEON
void CrossFillNEON(Image& Frame, const Triangle& Tri)
{
	const int32x4x2_t TriVerts012 = vld1q_s32_x2(
		reinterpret_cast<const std::int32_t*>(&Tri.Vert)
	);
	const int32x4x2_t TriVerts120 = {
		// Lower
		// 3 2 | 1 0
		// 0 3 | 2 1 ; Rotate right
		vextq_s32(
			TriVerts012.val[0],
			TriVerts012.val[1],
			2
		),
		// Upper
		// 1 0 | 3 2
		// 2 1 | 0 3 ; Rotate right
		// - - | 3 0 ; Swap
		vextq_s32(
			vextq_s32(
				TriVerts012.val[1],
				TriVerts012.val[0],
				2
			),
			vextq_s32(
				TriVerts012.val[1],
				TriVerts012.val[0],
				2
			),
			2
		)
	};

	// Directional vectors such that each triangle vertex is pointing to
	// the vertex before it, in counter-clockwise order
	const int32x4x2_t TriDirections = {
		vsubq_s32(
			TriVerts120.val[0],
			TriVerts012.val[0]
		),
		vsubq_s32(
			TriVerts120.val[1],
			TriVerts012.val[1]
		)
	};

	// Iterate through triangle bounds
	const auto XBounds = std::minmax({Tri.Vert[0].x, Tri.Vert[1].x, Tri.Vert[2].x});
	const auto YBounds = std::minmax({Tri.Vert[0].y, Tri.Vert[1].y, Tri.Vert[2].y});
	const std::size_t Width = static_cast<std::size_t>(XBounds.second - XBounds.first);
	const std::size_t Height = static_cast<std::size_t>(YBounds.second - YBounds.first);

	std::uint8_t* Dest = &Frame.Pixels[XBounds.first + YBounds.first * Frame.Width];
	int32x4x2_t CurPoint = 
	{
		vreinterpretq_s32_u64(
			vdupq_n_u64(
				(static_cast<std::int64_t>(YBounds.first) << 32)
				| static_cast<std::int32_t>(XBounds.first)
			)
		),
		vreinterpretq_s32_u64(
			vdupq_n_u64(
				(static_cast<std::int64_t>(YBounds.first) << 32)
				| static_cast<std::int32_t>(XBounds.first)
			)
		)
	};
	for( std::size_t y = 0; y < Height; ++y, Dest += Frame.Width )
	{
		// Rasterize Scanline
		for( std::size_t x = 0; x < Width; ++x )
		{
			const int32x4x2_t PointDir = {
				vsubq_s32(
					CurPoint.val[0],
					TriVerts120.val[0]
				),
				vsubq_s32(
					CurPoint.val[1],
					TriVerts120.val[1]
				)
			};
			// Compute Z-components of cross products in parallel
		// Tests three `DirA.x * DirB.y - DirA.y * DirB.x`
		// values at once
		
		// Swap XY Values of Vector B's elements (int32_t)
		// A: [ ~ | ~ | X2 | Y2 | X1 | Y1 | X0 | Y0 ]
			// B: [ ~ | ~ | Y2 | X2 | Y1 | X1 | Y0 | X0 ]
			const int32x4x2_t Product = {
				vmulq_s32(
					TriDirections.val[0],
					vrev64q_s32(
						PointDir.val[0]
					)
				),
				vmulq_s32(
					TriDirections.val[1],
					vrev64q_s32(
						PointDir.val[1]
					)
				)
			};
			// Shift Y values into X value columns
			// Subtract and calculate final dot products
			const int32x4x2_t CrossAreas = {
				vsubq_s32(
					Product.val[0],
					vrev64q_s32(Product.val[0])
				),
				vsubq_s32(
					Product.val[1],
					vrev64q_s32(Product.val[1])
				)
			};
			// Check if `X >= 0` for each of the 3 Z-components 
			const std::int32_t CheckLow = vaddvq_s32(
				vandq_s32(
					// Mask only the cross-area elements
					int32x4_t{ -1,0,-1,0 },
					// Compare >= 0
					vreinterpretq_s32_u32(
						vcgeq_s32(
							CrossAreas.val[0],
							int32x4_t{ 0,0,0,0 }
						)
					)
				)
			);
			const std::int32_t CheckHigh = vaddvq_s32(
				vandq_s32(
					// Mask only the cross-area elements
					int32x4_t{ -1,0,0,0 },
					// Compare >= 0
					vreinterpretq_s32_u32(
						vcgeq_s32(
							CrossAreas.val[1],
							int32x4_t{ 0,0,0,0 }
						)
					)
				)
			);
			// Horizontally add the comparison bits and check if sum is -3
			Dest[x] |= ( CheckLow + CheckHigh == -3 );
			// Increment to next column
			CurPoint = {
				vaddq_s32(
					CurPoint.val[0],
					int32x4_t{ 1,0,1,0 }
				),
				vaddq_s32(
					CurPoint.val[1],
					int32x4_t{ 1,0,1,0 }
				)
			};
		}
		// Increment to next Scanline
		CurPoint = {
			vaddq_s32(
				CurPoint.val[0],
				int32x4_t{ 0,1,0,1 }
			),
			vaddq_s32(
				CurPoint.val[1],
				int32x4_t{ 0,1,0,1 }
			)
		};
		// Move CurPoint back to the left of the scanline
		CurPoint = {
			vbslq_s32(
				vreinterpretq_u32_u64(
					vdupq_n_u64(0x00000000'FFFFFFFF)
				),
				vdupq_n_s32(XBounds.first),
				CurPoint.val[0]
			),
			vbslq_s32(
				vreinterpretq_u32_u64(
					vdupq_n_u64(0x00000000'FFFFFFFF)
				),
				vdupq_n_s32(XBounds.first),
				CurPoint.val[1]
			)
		};
	}
}
#endif

//// Barycentric Method

bool Barycentric(const glm::i32vec2& Point, const Triangle& Tri)
{
	const glm::i32vec2 V0 = Tri.Vert[2] - Tri.Vert[0];
	const glm::i32vec2 V1 = Tri.Vert[1] - Tri.Vert[0];
	const glm::i32vec2 V2 = Point - Tri.Vert[0];

	const std::int32_t Dot00 = glm::compAdd(V0 * V0);
	const std::int32_t Dot01 = glm::compAdd(V0 * V1);
	const std::int32_t Dot02 = glm::compAdd(V0 * V2);
	const std::int32_t Dot11 = glm::compAdd(V1 * V1);
	const std::int32_t Dot12 = glm::compAdd(V1 * V2);

	// These are just three determinants
	const std::int32_t Area = (Dot00 * Dot11 - Dot01 * Dot01);
	const std::int32_t U    = (Dot11 * Dot02 - Dot01 * Dot12);
	const std::int32_t V    = (Dot00 * Dot12 - Dot01 * Dot02);

	// Convert to local plane's Barycentric coordiante system
	return
		(U >= 0) &&
		(V >= 0) &&
		(U + V < Area);
}

void BarycentricFill(Image& Frame, const Triangle& Tri)
{
	const glm::i32vec2 V0 = Tri.Vert[2] - Tri.Vert[0];
	const glm::i32vec2 V1 = Tri.Vert[1] - Tri.Vert[0];

	const std::int32_t Dot00 = glm::compAdd(V0 * V0);
	const std::int32_t Dot11 = glm::compAdd(V1 * V1);
	const std::int32_t Dot01 = glm::compAdd(V0 * V1);

	const std::int32_t Area = (Dot00 * Dot11 - Dot01 * Dot01);

	const auto XBounds = std::minmax({Tri.Vert[0].x, Tri.Vert[1].x, Tri.Vert[2].x});
	const auto YBounds = std::minmax({Tri.Vert[0].y, Tri.Vert[1].y, Tri.Vert[2].y});
	const std::size_t Width = static_cast<std::size_t>(XBounds.second - XBounds.first);
	const std::size_t Height = static_cast<std::size_t>(YBounds.second - YBounds.first);

	// Pre-compute starting point, and derivatives for loop
	std::uint8_t* Dest = &Frame.Pixels[XBounds.first + YBounds.first * Frame.Width];
	const glm::i32vec2 StartPoint{XBounds.first,YBounds.first};
	const glm::i32vec2 V2 = StartPoint - Tri.Vert[0];
	const std::int32_t Dot02 = glm::compAdd(V0 * V2);
	const std::int32_t Dot12 = glm::compAdd(V1 * V2);
	glm::i32vec2 UVStart{
		Dot11 * Dot02 - Dot01 * Dot12,
		Dot00 * Dot12 - Dot01 * Dot02
	};
	// Partial derivatives of U and V in terms of CurPoint
	const glm::i32vec2 dU = V0 * Dot11 - V1 * Dot01;
	const glm::i32vec2 dV = V1 * Dot00 - V0 * Dot01;

	for( std::size_t y = 0; y < Height; ++y, Dest += Frame.Width )
	{
		// Rasterize Scanline
		glm::i32vec2 CurUV = UVStart;
		for( std::size_t x = 0 ; x < Width; ++x )
		{
			// Test
			Dest[x] |= (
				(CurUV.x >= 0) &&
				(CurUV.y >= 0) &&
				(CurUV.x + CurUV.y < Area)
			);

			// Integrate
			CurUV.x += dU.x;
			CurUV.y += dV.x;
		}

		// Integrate
		UVStart.x += dU.y;
		UVStart.y += dV.y;
	}
}

#ifdef __AVX2__


void BarycentricFillAVX2(Image& Frame, const Triangle& Tri)
{
	const glm::i32vec2 V0 = Tri.Vert[2] - Tri.Vert[0];
	const glm::i32vec2 V1 = Tri.Vert[1] - Tri.Vert[0];

	const std::int32_t Dot00 = glm::compAdd(V0 * V0);
	const std::int32_t Dot01 = glm::compAdd(V0 * V1);
	const std::int32_t Dot11 = glm::compAdd(V1 * V1);

	const std::int32_t Area = (Dot00 * Dot11 - Dot01 * Dot01);

	const auto XBounds = std::minmax({Tri.Vert[0].x, Tri.Vert[1].x, Tri.Vert[2].x});
	const auto YBounds = std::minmax({Tri.Vert[0].y, Tri.Vert[1].y, Tri.Vert[2].y});
	const std::size_t Width = static_cast<std::size_t>(XBounds.second - XBounds.first);
	const std::size_t Height = static_cast<std::size_t>(YBounds.second - YBounds.first);

	// Pre-compute starting point, and derivatives for loop
	std::uint8_t* Dest = &Frame.Pixels[XBounds.first + YBounds.first * Frame.Width];
	const glm::i32vec2 StartPoint{XBounds.first,YBounds.first};
	const glm::i32vec2 V2 = StartPoint - Tri.Vert[0];
	const std::int32_t Dot02 = glm::compAdd(V0 * V2);
	const std::int32_t Dot12 = glm::compAdd(V1 * V2);
	glm::i32vec2 UVStart{
		Dot11 * Dot02 - Dot01 * Dot12,
		Dot00 * Dot12 - Dot01 * Dot02
	};
	// Partial derivatives of U and V in terms of CurPoint
	const glm::i32vec2 dU = V0 * Dot11 - V1 * Dot01;
	const glm::i32vec2 dV = V1 * Dot00 - V0 * Dot01;

	for( std::size_t y = 0; y < Height; ++y, Dest += Frame.Width )
	{
		// Rasterize Scanline
		glm::i32vec2 CurUV = UVStart;
		std::size_t x = 0;
		// Eight samples at a time
		// AV512
		#ifdef __AVX512F__
		__m512i CurUV_8 = _mm512_set_epi32(
			// Seventh UV over
			CurUV.y + dV.x * 7, CurUV.x + dU.x * 7,
			// Sixth UV over
			CurUV.y + dV.x * 6, CurUV.x + dU.x * 6,
			// Fifth UV over
			CurUV.y + dV.x * 5, CurUV.x + dU.x * 5,
			// Forth UV over
			CurUV.y + dV.x * 4, CurUV.x + dU.x * 4,
			// Forth UV over
			CurUV.y + dV.x * 3, CurUV.x + dU.x * 3,
			// Third UV over
			CurUV.y + dV.x * 2, CurUV.x + dU.x * 2,
			// Second UV over
			CurUV.y + dV.x, CurUV.x + dU.x,
			// Current UV
			CurUV.y, CurUV.x
		);
		const __m512i dUV_8 = _mm512_set_epi32(
			dV.x * 8, dU.x * 8,
			dV.x * 8, dU.x * 8,
			dV.x * 8, dU.x * 8,
			dV.x * 8, dU.x * 8,
			dV.x * 8, dU.x * 8,
			dV.x * 8, dU.x * 8,
			dV.x * 8, dU.x * 8,
			dV.x * 8, dU.x * 8
		);
		for( ; (Width - x) / 8; x += 8 )
		{
			// Test
			// U >= 0
			// V >= 0
			// Checks the sign bit of each 32-bit U and V pair as 64-bit ints
			// ( 0 <= X ) ⇒ ¬( 0 > X )
			const __mmask8 NegativeTest = _mm512_test_epi64_mask(
				CurUV_8,
				_mm512_set1_epi32(0x80000000)
			);
			// Area > U + V
			// Aligns the U and V and adds them into a 8 bit comparison
			// compares to the "Area" term
			const __mmask8 AreaTest = _mm256_cmpgt_epi32_mask(
				_mm256_set1_epi32(Area),
				_mm256_add_epi32(
					_mm512_cvtepi64_epi32(CurUV_8),
					_mm512_cvtepi64_epi32(
						_mm512_srai_epi64(CurUV_8,32)
					)
				)
			);
			// This does (¬NegativeTest AND AreaTest)
			const __mmask8 Tests = _kandn_mask8(NegativeTest, AreaTest);
			*reinterpret_cast<std::uint64_t*>(&Dest[x]) |= _pdep_u64(
				_cvtmask8_u32(Tests),
				0x0101010101010101
			);
			// Integrate
			CurUV_8 = _mm512_add_epi32(CurUV_8, dUV_8);

			CurUV.x += dU.x * 8;
			CurUV.y += dV.x * 8;
		}
		#endif

		// Four samples at a time
		// AVX2
		__m256i CurUV_4 = _mm256_set_epi32(
			// Forth UV over
			CurUV.y + dV.x * 3, CurUV.x + dU.x * 3,
			// Third UV over
			CurUV.y + dV.x * 2, CurUV.x + dU.x * 2,
			// Second UV over
			CurUV.y + dV.x, CurUV.x + dU.x,
			// Current UV
			CurUV.y, CurUV.x
		);
		const __m256i dUV_4 = _mm256_set_epi32(
			dV.x * 4, dU.x * 4,
			dV.x * 4, dU.x * 4,
			dV.x * 4, dU.x * 4,
			dV.x * 4, dU.x * 4
		);
		for( ; (Width - x) / 4; x += 4 )
		{
			// Test
			// U >= 0
			// V >= 0
			// ( 0 <= X ) ⇒ ¬( 0 > X )
			const __m256i NegativeTest = _mm256_cmpgt_epi32(
				_mm256_setzero_si256(),
				CurUV_4
			);
			// Area > U + V
			const __m256i AreaTest = _mm256_cmpgt_epi32(
				_mm256_set1_epi32(Area),
				_mm256_add_epi32(
					CurUV_4,
					_mm256_shuffle_epi32(CurUV_4, 0b10'11'00'01)
				)
			);
			const std::uint32_t Tests = _mm256_movemask_epi8(
				_mm256_andnot_si256( // This does the (¬NegativeTest AND AreaTest)
					NegativeTest,
					AreaTest
				)
			);
			// Dest[x + 0] |= ((Tests & 0x00'00'00'FF) == 0x00'00'00'FF);
			// Dest[x + 1] |= ((Tests & 0x00'00'FF'00) == 0x00'00'FF'00);
			// Dest[x + 2] |= ((Tests & 0x00'FF'00'00) == 0x00'FF'00'00);
			// Dest[x + 3] |= ((Tests & 0xFF'00'00'00) == 0xFF'00'00'00);
			// SWAR method of checking each byte
			// returning 0x01 where it is 0xFF and 0x00 otherwise by checking
			// that adding 0x01 to the byte causes a carry into the last bit
			const std::uint32_t CarrySlide = (Tests & 0x7F7F7F7F) + 0x01010101;
			const std::uint32_t LastCarry  = (Tests ^ 0x01010101) & 0x80808080;
			*reinterpret_cast<std::uint32_t*>(&Dest[x]) |= (CarrySlide & LastCarry) >> 7;
			// Integrate
			CurUV_4 = _mm256_add_epi32(CurUV_4, dUV_4);

			CurUV.x += dU.x * 4;
			CurUV.y += dV.x * 4;
		}

		// Two samples at a time
		// SSE2
		__m128i CurUV_2 = _mm_set_epi32(
			// Next UV over
			CurUV.y + dV.x, CurUV.x + dU.x,
			// Current UV
			CurUV.y, CurUV.x
		);
		const __m128i dUV_2 = _mm_set_epi32(
			dV.x * 2, dU.x * 2,
			dV.x * 2, dU.x * 2
		);
		for( ; (Width - x) / 2; x += 2 )
		{
			// Test
			// U >= 0
			// V >= 0
			// ( X >= 0 ) ⇒ ¬( X < 0 )
			const __m128i NegativeTest = _mm_cmplt_epi32(
				CurUV_2,
				_mm_setzero_si128()

			);
			// U + V < Area
			const __m128i AreaTest = _mm_cmplt_epi32(
				_mm_add_epi32(
					CurUV_2,
					_mm_shuffle_epi32( CurUV_2, 0b10'11'00'01 )
				),
				_mm_set1_epi32(Area)
			);
			const std::uint16_t Tests = _mm_movemask_epi8(
				_mm_andnot_si128( // This does the (¬NegativeTest AND AreaTests)
					NegativeTest,
					AreaTest
				)
			);
			// Dest[x+0] |= ( (Tests & 0x00'FF) == 0x00'FF );
			// Dest[x+1] |= ( (Tests & 0xFF'00) == 0xFF'00 );
			// SWAR method of checking each byte
			// returning 0x01 where it is 0xFF and 0x00 otherwise by checking
			// that adding 0x01 to the byte causes a carry into the last bit
			const std::uint16_t CarrySlide = (Tests & 0x7F7F) + 0x0101;
			const std::uint16_t LastCarry  = (Tests ^ 0x0101) & 0x8080;
			*reinterpret_cast<std::uint16_t*>(&Dest[x]) |= (CarrySlide & LastCarry) >> 7;
			// Integrate
			CurUV_2 = _mm_add_epi32( CurUV_2, dUV_2 );

			CurUV.x += dU.x * 2;
			CurUV.y += dV.x * 2;
		}

		for( ; x < Width; ++x )
		{
			// Test
			Dest[x] |= (
				(CurUV.x >= 0) &&
				(CurUV.y >= 0) &&
				(CurUV.x + CurUV.y < Area)
			);

			// Integrate
			CurUV.x += dU.x;
			CurUV.y += dV.x;
		}

		// Integrate
		UVStart.x += dU.y;
		UVStart.y += dV.y;
	}
}
#endif

#ifdef __ARM_NEON

inline std::int32_t NEONDot(const int32x2_t A, const int32x2_t B)
{
	return vaddv_s32(
		vmul_s32(A,B)
	);
}

void BarycentricFillNEON(Image& Frame, const Triangle& Tri)
{
	const int32x2x3_t CurTri = {
		vld1_s32(reinterpret_cast<const std::int32_t*>(&Tri.Vert[0])),
		vld1_s32(reinterpret_cast<const std::int32_t*>(&Tri.Vert[1])),
		vld1_s32(reinterpret_cast<const std::int32_t*>(&Tri.Vert[2]))
	};
	const int32x2_t V0 = vsub_s32(CurTri.val[2], CurTri.val[0]);
	const int33x2_t V1 = vsub_s32(CurTri.val[1], CurTri.val[0]);

	const std::int32_t Dot00 = NEONDot(V0,V0);
	const std::int32_t Dot01 = NEONDot(V0,V1);
	const std::int32_t Dot11 = NEONDot(V1,V1);


	const auto XBounds = std::minmax({Tri.Vert[0].x, Tri.Vert[1].x, Tri.Vert[2].x});
	const auto YBounds = std::minmax({Tri.Vert[0].y, Tri.Vert[1].y, Tri.Vert[2].y});
	const std::size_t Width = static_cast<std::size_t>(XBounds.second - XBounds.first);
	const std::size_t Height = static_cast<std::size_t>(YBounds.second - YBounds.first);

	std::uint8_t* Dest = &Frame.Pixels[XBounds.first + YBounds.first * Frame.Width];
	int32x2_t CurPoint = 
	{
		static_cast<std::int32_t>(XBounds.first),
		static_cast<std::int32_t>(YBounds.first)
	};
	for( std::size_t y = 0; y < Height; ++y, Dest += Frame.Width )
	{
		// Rasterize Scanline
		for( std::size_t x = 0; x < Width; ++x )
		{
			const int32x2_t V2 = vsub_s32(CurPoint,CurTri.val[0]);
			const std::int32_t Dot02 = NEONDot(V0,V2);
			const std::int32_t Dot12 = NEONDot(V1,V2);
			//     CrossVec1       Crossglm::i32vec2
			//        |     DotVec    |     DotVec(Reversed)
			//        |       |       |       |
			//        V       V       V       V
			// U = (Dot11 * Dot02 - Dot01 * Dot12);
			// V = (Dot00 * Dot12 - Dot01 * Dot02);
			const int32x2_t DotVec = int32x2_t{Dot02, Dot12};
			const int32x2_t UV = vsub_s32(
				vmul_s32(CrossVec1,DotVec),
				vmul_s32(Crossglm::i32vec2,vrev64_s32(DotVec))
			);
			const int32_t UVArea = vaddv_s32(UV);
			const int32_t Compare = vaddv_u32(
				vcge_s32(UV, vdup_n_s32(0) )
			);
			Dest[x] |= (
				Compare == -2 // Both terms are ">= 0"
				&&
				(UVArea < Area)
			);
			// Increment to next column
			CurPoint = vadd_s32(
				CurPoint,
				int32x2_t{1,0}
			);
		}
		// Increment to next Scanline
		CurPoint = vadd_s32(
			CurPoint,
			int32x2_t{0,1}
		);
		// Move CurPoint back to the left of the scanline
		CurPoint = vset_lane_s32(
			XBounds.first,
			CurPoint,

			0
		);
	}
}
#endif

//// Utils

template< bool TestFunc(const glm::i32vec2& Point, const Triangle& Tri) >
void SerialBlit(Image& Frame, const Triangle& Tri)
{
	const auto XBounds = std::minmax({Tri.Vert[0].x, Tri.Vert[1].x, Tri.Vert[2].x});
	const auto YBounds = std::minmax({Tri.Vert[0].y, Tri.Vert[1].y, Tri.Vert[2].y});
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
	{CrossFill,					"Serial-CrossProductFill"},
#ifdef __AVX2__
	{ CrossFillAVX2,			"AVX2-CrossProductFill" },
#endif
#ifdef __ARM_NEON
	{ CrossFillNEON,			"NEON-CrossProductFill" },
#endif
	// Barycentric methods
	{SerialBlit<Barycentric>,	"Serial-Barycentric"},
	{BarycentricFill,			"Serial-BarycentricFill"},
#ifdef __AVX2__
	{ BarycentricFillAVX2,		"AVX2-BarycentricFill"  },
#endif
#ifdef __ARM_NEON
	{ BarycentricFillNEON,		"NEON-BarycentricFill"  },
#endif
};
}

