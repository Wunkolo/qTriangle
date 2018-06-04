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

bool CrossTest(const Vec2& Point, const Triangle& Tri)
{
	// Cross-product Z component, modified for a 0,0 top-left coordiante system
	const std::int32_t W0 = CrossArea(Tri.B - Tri.A,Point - Tri.B);
	const std::int32_t W1 = CrossArea(Tri.C - Tri.B,Point - Tri.C);
	const std::int32_t W2 = CrossArea(Tri.A - Tri.C,Point - Tri.A);
	return W0 >= 0 && W1 >= 0 && W2 >= 0;
}

bool Barycentric(const Vec2& Point, const Triangle& Tri)
{
	const Vec2 V0 = Tri.C - Tri.A;
	const Vec2 V1 = Tri.B - Tri.A;
	const Vec2 V2 = Point - Tri.A;

	const std::uint32_t Dot00 = glm::compAdd(V0 * V0);
	const std::uint32_t Dot01 = glm::compAdd(V0 * V1);
	const std::uint32_t Dot02 = glm::compAdd(V0 * V2);
	const std::uint32_t Dot11 = glm::compAdd(V1 * V1);
	const std::uint32_t Dot12 = glm::compAdd(V1 * V2);

	const glm::float32_t InvDenom = 1.0f / (Dot00 * Dot11 - Dot01 * Dot01);
	const glm::float32_t U = (Dot11 * Dot02 - Dot01 * Dot12) * InvDenom;
	const glm::float32_t V = (Dot00 * Dot12 - Dot01 * Dot02) * InvDenom;

	// Convert to local plane's Barycentric coordiante system
	return (U >= 0.0f) && (V >= 0.0f) && (U + V < 1.0f);
}
}
