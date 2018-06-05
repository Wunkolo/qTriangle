#include <qTriangle/qTriangle.hpp>

#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/component_wise.hpp>

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
	for(
		std::size_t y = std::min<std::size_t>(YBounds.first, 0);
		y < std::max<std::size_t>(YBounds.second, Frame.Height);
		++y
	)
	{
		for(
			std::size_t x = std::min<std::size_t>(XBounds.first, 0);
			x < std::max<std::size_t>(XBounds.second, Frame.Width);
			++x
		)
		{
			const Vec2 CurPoint{x, y};
			const bool Inside = CrossArea(DirectionBA, CurPoint - Tri.Vert[1]) >= 0 &&
				CrossArea(DirectionCB, CurPoint - Tri.Vert[2]) >= 0 &&
				CrossArea(DirectionAC, CurPoint - Tri.Vert[0]) >= 0;
			Frame.Pixels[x + y * Frame.Width] |= Inside;
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
	return (U >= 0.0f) && (V >= 0.0f) && (U + V < 1.0f);
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
	for(
		std::size_t y = std::min<std::size_t>(YBounds.first, 0);
		y < std::max<std::size_t>(YBounds.second, Frame.Height);
		++y
	)
	{
		for(
			std::size_t x = std::min<std::size_t>(XBounds.first, 0);
			x < std::max<std::size_t>(XBounds.second, Frame.Width);
			++x
		)
		{
			const Vec2 CurPoint{x, y};
			const Vec2 V2 = CurPoint - Tri.Vert[0];
			const std::uint32_t Dot02 = glm::compAdd(V0 * V2);
			const std::uint32_t Dot12 = glm::compAdd(V1 * V2);
			const glm::float32_t U = (Dot11 * Dot02 - Dot01 * Dot12) / Det;
			const glm::float32_t V = (Dot00 * Dot12 - Dot01 * Dot02) / Det;
			Frame.Pixels[x + y * Frame.Width] |= ((U >= 0.0f) && (V >= 0.0f) && (U + V < 1.0f));
		}
	}
}

template< bool TestFunc(const Vec2& Point, const Triangle& Tri) >
void SerialBlit(Image& Frame, const Triangle& Tri)
{
	const auto XBounds = std::minmax({Tri.Vert[0].x, Tri.Vert[1].x, Tri.Vert[2].x});
	const auto YBounds = std::minmax({Tri.Vert[0].y, Tri.Vert[1].y, Tri.Vert[2].y});
	for(
		std::size_t y = std::min<std::size_t>(YBounds.first, 0);
		y < std::max<std::size_t>(YBounds.second, Frame.Height);
		++y
	)
	{
		for(
			std::size_t x = std::min<std::size_t>(XBounds.first, 0);
			x < std::max<std::size_t>(XBounds.second, Frame.Width);
			++x
		)
		{
			Frame.Pixels[x + y * Frame.Width] |= TestFunc({x, y}, Tri);;
		}
	}
}

const std::pair<
	void(*)(Image& Frame, const Triangle& Tri),
	const char*
> FillAlgorithms[4] = {
	{SerialBlit<CrossTest>, "Serial-CrossProduct"},
	{SerialBlit<Barycentric>, "Serial-Barycentric"},
	{CrossFill, "Serial-CrossProductFill"},
	{BarycentricFill, "Serial-BarycentricFill"}
};
}
