#include <qTriangle/qTriangle.hpp>
#include <x86intrin.h>

template<std::uint8_t WidthExp2>
inline void CrossProductMethod(
	const glm::i32vec2 Points[], std::uint8_t Results[], std::size_t Count,
	const Triangle& Tri
);
template<std::uint8_t WidthExp2>
inline void BarycentricMethod(
	const glm::i32vec2 Points[], std::uint8_t Results[], std::size_t Count,
	const qTri::Triangle& Tri
);

#if defined(__SSE4_1__)

inline __m128i DetSSE41(
	const __m128i Mat2x2
)
{
	//    Topx,    Topy, Bottomx, Bottomy
	// Bottomy, Bottomx,    Topy, Topx,
	const __m128i Product = _mm_mullo_epi32(
		Mat2x2,
		_mm_shuffle_epi32(
			Mat2x2,
			_MM_SHUFFLE(0,1,2,3)
		)
	);
	return _mm_hsub_epi32(Product,Product);
}

// Serial
template<>
inline void CrossProductMethod<0>(
	const glm::i32vec2 Points[], std::uint8_t Results[], std::size_t Count,
	const Triangle& Tri
)
{
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

template<>
inline void BarycentricMethod<0>(
	const glm::i32vec2 Points[], std::uint8_t Results[], std::size_t Count,
	const qTri::Triangle& Tri
)
{
	// [ Tri[1].y, Tri[1].x, Tri[0].y, Tri[0].x]
	const __m128i Tri10 = _mm_loadu_si128(
		reinterpret_cast<const __m128i*>(Tri.data())
	);
	// [ Tri[2].y, Tri[2].x, Tri[2].y, Tri[2].x]
	const __m128i Tri22 = _mm_set1_epi64x(
		*reinterpret_cast<const std::uint64_t*>(Tri.data() + 2)
	);
	// [ Tri[0].y, Tri[0].x, Tri[2].y, Tri[2].x]
	const __m128i Tri02 = _mm_alignr_epi8(
		Tri10, Tri22,
		8
	);
	// [ Tri[2].y, Tri[2].x, Tri[1].y, Tri[1].x]
	const __m128i Tri21 = _mm_alignr_epi8(
		Tri22, Tri10,
		8
	);

	// | Tri[1].x | Tri[0].y | Tri[2].y | Tri[0].x |
	const __m128i ConstVec_1x0y2y0x = _mm_blend_epi32(
		_mm_shuffle_epi32(Tri10,0b10'01'00'00),// 1x0y__0x
		Tri22,                                 // ____2y__
		0b0010
	);

	// | Tri[1].y | Tri[0].x | Tri[2].x | Tri[0].y |
	const __m128i ConstVec_1y0x2x0y = _mm_blend_epi32(
		_mm_shuffle_epi32(Tri10,0b11'00'00'01),// 1y0x__0y
		_mm_shuffle_epi32(Tri22,0b00'00'00'00),// ____2x__
		0b0010
	);

	// Det01: Tri[1].y * Tri[0].x - Tri[0].y * Tri[1].x
	// Det20: Tri[2].x * Tri[0].y - Tri[0].x * Tri[2].y

	// | Tri[1].y | Tri[2].x | Tri[1].y | Tri[2].x |
	// |    *     |    *     |    *     |    *     |
	// | Tri[0].x | Tri[0].y | Tri[0].x | Tri[0].y |
	// |    -     |    -     |    -     |    -     |
	// | Tri[0].y | Tri[0].x | Tri[0].y | Tri[0].x |
	// |    *     |    *     |    *     |    *     |
	// | Tri[1].x | Tri[2].y | Tri[1].x | Tri[2].y |
	// [  Det01  ,  Det20    ,  Det01   ,  Det20   ]
	const __m128i Det0120 = _mm_sub_epi32(
		_mm_mullo_epi32(
			_mm_shuffle_epi32(Tri21,0b01'10'01'10),
			_mm_shuffle_epi32(Tri10,0b00'01'00'01)
		),
		_mm_mullo_epi32(
			_mm_shuffle_epi32(Tri10,0b01'00'01'00),
			_mm_shuffle_epi32(Tri21,0b00'11'00'11)
		)
	);

	// Area: Tri[2].y * Tri[1].x - Tri[2].x * Tri[1].y
	//       + Det20 + Det01

	// | Tri[2].x | Tri[2].y | Tri[2].x | Tri[2].y |
	// |    *     |    *     |    *     |    *     |
	// | Tri[1].y | Tri[1].x | Tri[1].y | Tri[1].x |
	// |         hsub        |         hsub        |
	// |    +     |    +     |    +     |    +     |
	// [  Det01   | Det01    |  Det01   |  Det01   ]
	// |    +     |    +     |    +     |    +     |
	// [  Det20   | Det20    |  Det20   |  Det20   ]
	// [  Area    |  Area    |  Area    |  Area    ]
	const __m128i AreaProduct = _mm_mullo_epi32(
		_mm_shuffle_epi32(
			Tri22, 0b00'01'00'01
		),
		_mm_shuffle_epi32(
			Tri10, 0b11'10'11'10
		)
	);
	const __m128i Area = _mm_add_epi32(
		_mm_hsub_epi32(
			AreaProduct, AreaProduct
		),
		_mm_hadd_epi32(
			Det0120,Det0120
		)
	);

	// [Area-1, Area-1, 0,0]
	const __m128i CheckConst = _mm_blend_epi16(
		_mm_setzero_si128(),
		_mm_sub_epi32( // Area - 1
			Area,
			_mm_set1_epi32(1)
		),
		0b11'11'00'00
	);

	for( std::size_t i = 0; i < Count; ++i )
	{
		// YXYX
		const __m128i Point = _mm_set1_epi64x(
			*reinterpret_cast<const std::uint64_t*>(Points + i)
		);
		const __m128i PointYXXY= _mm_shuffle_epi32(
			Point,
			0b01'00'00'01
		);
		const __m128i PointXYYX = _mm_alignr_epi8(
			PointYXXY,PointYXXY,8
		);

		// U:
		//   Point.y * Tri[0].x - Point.x * Tri[0].y
		// +
		//   Point.x * Tri[2].y - Point.y * Tri[2].x
		// + Det20
		// V:
		//   Point.x * Tri[0].y - Point.y * Tri[0].x
		// +
		//   Point.y * Tri[1].x - Point.x * Tri[1].y
		// + Det01

		// If I wanted to do two at a time, I could fit
		// two UVs into one 128-bit lane. Putting this
		// here for reference
		// |  Point.x |  Point.y |  Point.x |  Point.y |
		// |    *     |    *     |    *     |    *     |
		// [ Tri[0].y | Tri[0].x | Tri[0].y | Tri[0].x ] < const
		// |    -     |    -     |    -     |    -     |
		// |  Point.y |  Point.x |  Point.y |  Point.x |
		// |    *     |    *     |    *     |    *     |
		// [ Tri[0].x | Tri[0].y | Tri[0].x | Tri[0].y ] < const
		// |    +     |    +     |    +     |    +     |
		// |  Point.y |  Point.x |  Point.y |  Point.x |
		// |    *     |    *     |    *     |    *     |
		// [ Tri[1].x | Tri[2].y | Tri[1].x | Tri[2].y ] < const
		// |    -     |    -     |    -     |    -     |
		// |  Point.x |  Point.y |  Point.x |  Point.y |
		// |    *     |    *     |    *     |    *     |
		// [ Tri[1].y | Tri[2].x | Tri[1].y | Tri[2].x ] < const
		// |    +     |    +     |    +     |    +     |
		// [  Det01   |  Det20   |  Det01   |  Det20   ] < const
		// |    V1    |    U1    |    V0    |    U0    |

		// If I wanted to do 1 at a time though,
		// I could utilize more lanes to do
		// independent calculations in parallel
		// Such as the adds and multplications

		// |  Point.x |  Point.y | xy
		// |    *     |    *     |
		// | Tri[0].y | Tri[0].x ] < const
		// |    -     |    -     |
		// |  Point.y |  Point.x | yx
		// |    *     |    *     |
		// | Tri[0].x | Tri[0].y ] < const
		// |    +     |    +     |
		// |  Point.y |  Point.x | yx
		// |    *     |    *     |
		// | Tri[1].x | Tri[2].y ] < const
		// |    -     |    -     |
		// |  Point.x |  Point.y | xy
		// |    *     |    *     |
		// | Tri[1].y | Tri[2].x ] < const
		// |    +     |    +     |
		// |  Det01   |  Det20   ] < const
		// |    V     |    U     |
		//
		// V Utilizing all four lanes V
		// !Four determinants at once!
		// |  Point.y |  Point.x |  Point.x |  Point.y | yxxy
		// |    *     |    *     |    *     |    *     | mul
		// | Tri[1].x | Tri[0].y | Tri[2].y | Tri[0].x | < const
		// |    -     |    -     |    -     |    -     | sub
		// |  Point.x |  Point.y |  Point.y |  Point.x | xyyx
		// |    *     |    *     |    *     |    *     | mul
		// | Tri[1].y | Tri[0].x | Tri[2].x | Tri[0].y | < const
		// |         hadd        |         hadd        | hadd
		// |    +     |    +     |    +     |    +     | add
		// [  Det01   |  Det20   |  Det01   |  Det20   ] < const
		// |    V     |    U     |    V     |    U     |

		__m128i VU = _mm_sub_epi32(
			_mm_mullo_epi32(
				PointYXXY,
				ConstVec_1x0y2y0x //1x'0y'2y'0x
			),
			_mm_mullo_epi32(
				PointXYYX,
				ConstVec_1y0x2x0y //1y'0x'2x'0y
			)
		);
		VU = _mm_add_epi32(
			_mm_hadd_epi32(
				VU, VU
			),
			Det0120
		);
		// Area = (blah) + Det20 + Det01
		// U + V < Area ; U + V <= Area - 1
		// U + V - Area < 0
		// const auto AreaCheck = _mm_cmplt_epi32(
		// 	_mm_hadd_epi32(
		// 		VU, VU
		// 	),
		// 	Area
		// );
		// -U = U + (- 2 * U )
		//                |_mm_unpacklo_epi32(0,sign(VU,(-1,-1,-1,-1)))| ( little waste)
		// hadd[  ( V,U,V,U)     |      (0,-V,0,-U)      ]
		// |    V+U   |    U+V   |   -V     |   -U     |
		const __m128i CheckValues = _mm_hadd_epi32(
			_mm_unpacklo_epi32(
				_mm_sign_epi32(
					VU,
					_mm_set_epi32(-1,-1,-1,-1)
				),
				_mm_setzero_si128()
			),
			VU
		);
		// X <= Y;!( X > Y ); !( Y < X )
		const __m128i CheckParallel = _mm_cmplt_epi32(
			CheckConst,
			CheckValues
		);
		const std::uint16_t Mask = ~_mm_movemask_epi8(CheckParallel);
		Results[i] |= Mask == 0xFFFF;
		// |    <=    |    <=    |   <=     |   <=     |
		// | Area-1   | Area-1   |    0     |    0     | < const

		// U = (blah) + Det20; U >= 0; U >= -Det20; -U <= Det20
		// V = (blah) + Det01; V >= 0; V >= -Det01; -V <= Det01
		// X >= 0 : !(X < 0)
		// const auto SignCheck = _mm_cmplt_epi32(
		// 	VU, _mm_setzero_si128()
		// );

		// const auto AreaSignCheck = _mm_andnot_si128(
		// 	SignCheck, AreaCheck
		// );
		// Results[i] |= _mm_movemask_epi8(
		// 	AreaSignCheck
		// ) == 0xFFFF;
	}
}
#endif