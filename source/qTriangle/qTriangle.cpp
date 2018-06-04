#include <qTriangle/qTriangle.hpp>


#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/component_wise.hpp>

namespace qTri
{
bool Barycentric(const qTri::Vec2& Point, const qTri::Triangle& Tri)
{
	const qTri::Vec2 V0 = Tri.C - Tri.A;
	const qTri::Vec2 V1 = Tri.B - Tri.A;
	const qTri::Vec2 V2 = Point - Tri.A;

	const std::uint32_t Dot00 = glm::compAdd(V0 * V0);
	const std::uint32_t Dot01 = glm::compAdd(V0 * V1);
	const std::uint32_t Dot02 = glm::compAdd(V0 * V2);
	const std::uint32_t Dot11 = glm::compAdd(V1 * V1);
	const std::uint32_t Dot12 = glm::compAdd(V1 * V2);

	const glm::float32_t InvDenom = 1.0f / (Dot00 * Dot11 - Dot01 * Dot01);
	const glm::float32_t U = (Dot11 * Dot02 - Dot01 * Dot12) * InvDenom;
	const glm::float32_t V = (Dot00 * Dot12 - Dot01 * Dot02) * InvDenom;

	return (U >= 0.0f) && (V >= 0.0f) && (U + V < 1.0f);
}
}
