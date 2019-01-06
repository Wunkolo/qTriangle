#include <qTriangle/qTriangle.hpp>
#include <x86intrin.h>

template<std::uint8_t WidthExp2>
inline void BarycentricMethod(
	const glm::i32vec2 Points[], std::uint8_t Results[], std::size_t Count,
	const qTri::Triangle& Tri
);

#if defined(__SSE4_1__)

inline std::int32_t DetSSE41(
	const __m128i& Mat2x2
	// Topx, Topy, Bottomx, Bottomy
)
{
	const auto Product = _mm_mullo_epi32(
		Mat2x2,
		_mm_shuffle_epi32(
			Mat2x2,
			_MM_SHUFFLE(0,1,2,3)
		)
	);
	const auto Sub = _mm_hsub_epi32(Product,Product);
	return _mm_extract_epi32(Sub,1);
}

// Serial
template<>
inline void BarycentricMethod<0>(
	const glm::i32vec2 Points[], std::uint8_t Results[], std::size_t Count,
	const qTri::Triangle& Tri
)
{
	const __m128i TriLo = _mm_loadu_si128(
		reinterpret_cast<const __m128i*>(Tri.data())
	);
	const __m128i TriHi = _mm_set1_epi64x(
		*reinterpret_cast<const std::uint64_t*>(Tri.data() + 2)
	);
	const std::int32_t Det01 = DetSSE41(
		_mm_alignr_epi8(TriLo,TriLo,8)
	);
	const std::int32_t Det20 = DetSSE41(
		_mm_blend_epi16(
			TriLo, TriHi,
			0b1111'0000
		)
	);
	const std::int32_t Area = DetSSE41(
		_mm_blend_epi16(
			TriLo, TriHi,
			0b0000'1111
		)
	) + Det20 + Det01;

	for( std::size_t i = 0; i < Count; ++i )
	{
		const __m128i CurPoint = _mm_set1_epi64x(
			*reinterpret_cast<const std::uint64_t*>(Points + i)
		);
		const std::int32_t U = Det20
			+ DetSSE41(
				_mm_alignr_epi8(TriLo,CurPoint,8)
			)
			+ DetSSE41(
				_mm_alignr_epi8(CurPoint,TriHi,8)
			);
		const std::int32_t V = Det01
			+ DetSSE41(
				_mm_blend_epi16(
					TriLo, CurPoint,
					0b0000'1111
				)
			)
			+ DetSSE41(
				_mm_blend_epi16(
					TriLo, CurPoint,
					0b1111'0000
				)
			);
		Results[i] |= (U + V) < Area && U >= 0 && V >= 0;
	}
}
#endif