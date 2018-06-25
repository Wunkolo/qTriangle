#include <qTriangle/qTriangle.hpp>

#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/component_wise.hpp>

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
	Vec2 CurPoint;
	for( CurPoint.y = YBounds.first; CurPoint.y < YBounds.second; ++CurPoint.y )
	{
		for( CurPoint.x = XBounds.first; CurPoint.x < XBounds.second; ++CurPoint.x )
		{
			const bool Inside =
				CrossArea(DirectionBA, CurPoint - Tri.Vert[1]) >= 0 &&
				CrossArea(DirectionCB, CurPoint - Tri.Vert[2]) >= 0 &&
				CrossArea(DirectionAC, CurPoint - Tri.Vert[0]) >= 0;
			Frame.Pixels[CurPoint.x + CurPoint.y * Frame.Width] |= Inside;
		}
	}
}

#ifdef __AVX2__
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
	__m256i CurPoint = _mm256_set1_epi64x(
		(static_cast<std::int64_t>(YBounds.first) << 32)
		| static_cast<std::int32_t>(XBounds.first)
	);
	// Left-most point of current scanline
	for( std::size_t y = 0; y < Height; ++y, Dest += Frame.Width )
	{
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
			Dest[x] |= _mm256_testz_si256(
				// Check only the cross-area elements
				_mm256_set_epi32(0, 0, 0, -1, 0, -1, 0, -1),
				_mm256_cmpgt_epi32(
					_mm256_setzero_si256(),
					CrossAreas
				)
			);
			// Increment to next column
			CurPoint = _mm256_add_epi32(
				CurPoint,
				_mm256_set1_epi64x(1)
			);
		}
		// Increment to next Scanline
		CurPoint = _mm256_add_epi32(
			CurPoint,
			_mm256_set1_epi64x(1L << 32)
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

bool Barycentric(const Vec2& Point, const Triangle& Tri)
{
	const Vec2 V0 = Tri.Vert[2] - Tri.Vert[0];
	const Vec2 V1 = Tri.Vert[1] - Tri.Vert[0];
	const Vec2 V2 = Point - Tri.Vert[0];

	const std::int32_t Dot00 = glm::compAdd(V0 * V0);
	const std::int32_t Dot01 = glm::compAdd(V0 * V1);
	const std::int32_t Dot02 = glm::compAdd(V0 * V2);
	const std::int32_t Dot11 = glm::compAdd(V1 * V1);
	const std::int32_t Dot12 = glm::compAdd(V1 * V2);

	const std::int32_t Area = (Dot00 * Dot11 - Dot01 * Dot01);
	const std::int32_t U = (Dot11 * Dot02 - Dot01 * Dot12);
	const std::int32_t V = (Dot00 * Dot12 - Dot01 * Dot02);

	// Convert to local plane's Barycentric coordiante system
	return
		(U >= 0) &&
		(V >= 0) &&
		(U + V < Area);
}

//  Only V2,Dot02,Dot12,U, and V have to be re-generated for each vertex
void BarycentricFill(Image& Frame, const Triangle& Tri)
{
	const Vec2 V0 = Tri.Vert[2] - Tri.Vert[0];
	const Vec2 V1 = Tri.Vert[1] - Tri.Vert[0];

	const std::int32_t Dot00 = glm::compAdd(V0 * V0);
	const std::int32_t Dot01 = glm::compAdd(V0 * V1);
	const std::int32_t Dot11 = glm::compAdd(V1 * V1);

	const std::int32_t Area = (Dot00 * Dot11 - Dot01 * Dot01);

	const auto XBounds = std::minmax({Tri.Vert[0].x, Tri.Vert[1].x, Tri.Vert[2].x});
	const auto YBounds = std::minmax({Tri.Vert[0].y, Tri.Vert[1].y, Tri.Vert[2].y});
	Vec2 CurPoint;
	for( CurPoint.y = YBounds.first; CurPoint.y < YBounds.second; ++CurPoint.y )
	{
		for( CurPoint.x = XBounds.first; CurPoint.x < XBounds.second; ++CurPoint.x )
		{
			const Vec2 V2 = CurPoint - Tri.Vert[0];
			const std::int32_t Dot02 = glm::compAdd(V0 * V2);
			const std::int32_t Dot12 = glm::compAdd(V1 * V2);
			const std::int32_t U = (Dot11 * Dot02 - Dot01 * Dot12);
			const std::int32_t V = (Dot00 * Dot12 - Dot01 * Dot02);
			Frame.Pixels[CurPoint.x + CurPoint.y * Frame.Width] |= (
				(U >= 0) &&
				(V >= 0) &&
				(U + V < Area)
			);
		}
	}
}

#ifdef __AVX2__

// SSE4.1
inline std::int64_t _mm_hadd_epi64(const __m128i A)
{
	// return _mm_extract_epi64(A,0) + _mm_extract_epi64(A,1);
	return _mm_cvtsi128_si64(
		_mm_add_epi64(
			_mm_unpackhi_epi64(A, A),
			_mm_unpacklo_epi64(A, A)
		)
	);
}

// SSE4.1
inline std::int32_t _mm_dot_epi64(const __m128i A, const __m128i B)
{
	return _mm_hadd_epi64(
		_mm_mul_epi32(
			A,
			B
		)
	);
}

// AVX2
// Sums 2 64-bit integers in each 128-bit lane
// Returns the two 64-bit results in a 128-bit register
inline __m128i _mm256_hadd_epi64(const __m256i A)
{
	const __m256i Sum = _mm256_add_epi64(
		_mm256_unpackhi_epi64(A, A),
		_mm256_unpacklo_epi64(A, A)
	);
	return _mm_unpacklo_epi64(
		_mm256_castsi256_si128(Sum),
		_mm256_extracti128_si256(Sum,1)
	);
}

// AVX2
// Two concurrent dot-products at once
// Store the two 64-bit results into a 128 register
inline __m128i _mm256_dot_epi64(const __m256i A, const __m256i B)
{
	const __m256i Product = _mm256_mul_epi32(A, B);
	return _mm256_hadd_epi64(Product);
}

void BarycentricFillAVX2(Image& Frame, const Triangle& Tri)
{
	// 128-bit vectors composing of two 64-bit values
	const __m128i CurTri[3] = {
		_mm_set_epi64x(Tri.Vert[0].y, Tri.Vert[0].x),
		_mm_set_epi64x(Tri.Vert[1].y, Tri.Vert[1].x),
		_mm_set_epi64x(Tri.Vert[2].y, Tri.Vert[2].x)
	};
	const __m128i V0 = _mm_sub_epi64(CurTri[2], CurTri[0]);
	const __m128i V1 = _mm_sub_epi64(CurTri[1], CurTri[0]);

	const std::int32_t Dot00 = _mm_dot_epi64(V0, V0);
	const std::int32_t Dot01 = _mm_dot_epi64(V0, V1);
	const std::int32_t Dot11 = _mm_dot_epi64(V1, V1);
	const __m128i CrossVec1 = _mm_set_epi64x(
		Dot00,
		Dot11
	);
	const __m128i CrossVec2 = _mm_set_epi64x(
		Dot01,
		Dot01
	);

	const std::int32_t Area = (Dot00 * Dot11 - Dot01 * Dot01);

	const auto XBounds = std::minmax({Tri.Vert[0].x, Tri.Vert[1].x, Tri.Vert[2].x});
	const auto YBounds = std::minmax({Tri.Vert[0].y, Tri.Vert[1].y, Tri.Vert[2].y});
	const std::size_t Width = static_cast<std::size_t>(XBounds.second - XBounds.first);
	const std::size_t Height = static_cast<std::size_t>(YBounds.second - YBounds.first);

	std::uint8_t* Dest = &Frame.Pixels[XBounds.first + YBounds.first * Frame.Width];
	__m128i CurPoint = _mm_set_epi64x(YBounds.first,XBounds.first);
	for( std::size_t y = 0; y < Height; ++y, Dest += Frame.Width )
	{
		// Rasterize Scanline
		std::size_t x = 0;
		// Two samples at a time
		for( ; x + 2 < Width; x += 2 )
		{
			const __m256i V2 = _mm256_sub_epi64(
				_mm256_set_m128i(
					// Second point ( next point over )
					_mm_add_epi64(
						CurPoint,
						_mm_set_epi64x(0,1)
					),
					// First point
					CurPoint
				),
				_mm256_broadcastsi128_si256(CurTri[0])
			);

			// contains 2 64-bit dot-products for each of the two samples
			const __m128i Dot02 = _mm256_dot_epi64(_mm256_broadcastsi128_si256(V0), V2);
			const __m128i Dot12 = _mm256_dot_epi64(_mm256_broadcastsi128_si256(V1), V2);
			const __m256i DotVec = _mm256_set_m128i(
				_mm_unpackhi_epi64(
					Dot02,
					Dot12
				),
				_mm_unpacklo_epi64(
					Dot02,
					Dot12
				)
			);

			//    CrossVec1       CrossVec2
			//       |     DotVec    |     DotVec(Reversed)
			//       |       |       |       |
			//       V       V       V       V
			//U = (Dot11 * Dot02 - Dot01 * Dot12);
			//V = (Dot00 * Dot12 - Dot01 * Dot02);
			const __m256i UV = _mm256_sub_epi64(
				_mm256_mul_epi32(
					_mm256_broadcastsi128_si256(CrossVec1),
					DotVec
				),
				_mm256_mul_epi32(
					_mm256_broadcastsi128_si256(CrossVec2),
					_mm256_alignr_epi8(DotVec, DotVec, 8)
				)
			);
			// ( x >= 0 ) ⇒ ¬( X < 0 ) ⇒ ¬( 0 > X )
			const __m256i UVTests = _mm256_cmpgt_epi32(
				_mm256_setzero_si256(),
				UV
			);
			const __m128i UVAreas = _mm256_hadd_epi64(UV);
			const __m128i UVAreaTests = _mm_cmpgt_epi64(
				_mm_set1_epi64x(Area),
				UVAreas
			);
			Dest[x] |= (
				_mm_testz_si128(
					_mm_set1_epi64x(0xFFFFFFFF),
					_mm256_extracti128_si256(UVTests,0)
				)
				&& _mm_extract_epi64(UVAreaTests,0) == -1
			);
			Dest[x + 1] |= (
				_mm_testz_si128(
					_mm_set1_epi64x(0xFFFFFFFF),
					_mm256_extracti128_si256(UVTests, 1)
				)
				&& _mm_extract_epi64(UVAreaTests, 1) == -1
			);
			CurPoint = _mm_add_epi64(
				CurPoint,
				_mm_set_epi64x(0, 2)
			);
		}
		// Serial-ish
		for( ; x < Width; ++x )
		{
			const __m128i V2 = _mm_sub_epi64(CurPoint,CurTri[0]);

			// TODO: Find a way to have the dot-product already result in
			// a 64x2 vector
			const std::int32_t Dot02 = _mm_dot_epi64(V0, V2);
			const std::int32_t Dot12 = _mm_dot_epi64(V1, V2);
			const __m128i DotVec = _mm_set_epi64x( Dot12, Dot02 );

			//    CrossVec1       CrossVec2
			//       |     DotVec    |     DotVec(Reversed)
			//       |       |       |       |
			//       V       V       V       V
			//U = (Dot11 * Dot02 - Dot01 * Dot12);
			//V = (Dot00 * Dot12 - Dot01 * Dot02);
			const __m128i UV = _mm_sub_epi64(
				_mm_mul_epi32(
					CrossVec1,
					DotVec
				),
				_mm_mul_epi32(
					CrossVec2,
					_mm_alignr_epi8(DotVec,DotVec,8)
				)
			);
			// ( x >= 0 ) ⇒ ¬( X < 0 )
			const __m128i UVTest = _mm_cmplt_epi32(
				UV,
				_mm_setzero_si128()
			);
			Dest[x] |= (
				_mm_testz_si128(
					_mm_set_epi32(0,-1,0,-1),
					UVTest
				) &&
				_mm_hadd_epi64(UV) < Area
			);
			CurPoint = _mm_add_epi64(
				CurPoint,
				_mm_set_epi64x(0,1)
			);
		}
		// CurPoint.x = XBounds.first;
		CurPoint = _mm_blend_epi32(
			CurPoint,
			_mm_set1_epi64x(XBounds.first),
			0b0011
		);
		CurPoint = _mm_add_epi64(
			CurPoint,
			_mm_set_epi64x(1,0)
		);
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
	const int32x2_t V1 = vsub_s32(CurTri.val[1], CurTri.val[0]);

	const std::int32_t Dot00 = NEONDot(V0,V0);
	const std::int32_t Dot01 = NEONDot(V0,V1);
	const std::int32_t Dot11 = NEONDot(V1,V1);

	const std::int32_t Area = (Dot00 * Dot11 - Dot01 * Dot01);

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
			const std::int32_t U = (Dot11 * Dot02 - Dot01 * Dot12);
			const std::int32_t V = (Dot00 * Dot12 - Dot01 * Dot02);
			Dest[x] |= (
				(U >= 0) &&
				(V >= 0) &&
				(U + V < Area)
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

template< bool TestFunc(const Vec2& Point, const Triangle& Tri) >
void SerialBlit(Image& Frame, const Triangle& Tri)
{
	const auto XBounds = std::minmax({Tri.Vert[0].x, Tri.Vert[1].x, Tri.Vert[2].x});
	const auto YBounds = std::minmax({Tri.Vert[0].y, Tri.Vert[1].y, Tri.Vert[2].y});
	Vec2 CurPoint;
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
	{ SerialBlit<CrossTest>,   "Serial-CrossProduct"         },
	{ CrossFill,               "Serial-CrossProductFill"     },
#ifdef __AVX2__
	{ CrossFillAVX2,           "Serial-CrossProductFillAVX2" },
#endif
#ifdef __ARM_NEON
	{ CrossFillNEON,           "Serial-CrossProductFillNEON" },
#endif
// Barycentric methods
	{ SerialBlit<Barycentric>, "Serial-Barycentric"          },
	{ BarycentricFill,         "Serial-BarycentricFill"      },
#ifdef __AVX2__
	{ BarycentricFillAVX2,     "Serial-BarycentricFillAVX2"  },
#endif
#ifdef __ARM_NEON
	{ BarycentricFillNEON,     "Serial-BarycentricFillNEON"  },
#endif
};
}
