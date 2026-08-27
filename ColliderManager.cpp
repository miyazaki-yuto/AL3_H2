#include "ColliderManager.h"

#include <algorithm>
#include <cmath>

using namespace KamataEngine;

namespace {

constexpr float kEpsilon = 0.000001f;

float Dot(const Vector3& first, const Vector3& second) {
	return first.x * second.x + first.y * second.y + first.z * second.z;
}

Vector3 Subtract(const Vector3& first, const Vector3& second) {
	return {
	    first.x - second.x,
	    first.y - second.y,
	    first.z - second.z,
	};
}

float Length(const Vector3& vector) {
	return std::sqrt(Dot(vector, vector));
}

Vector3 Normalize(const Vector3& vector) {
	const float length = Length(vector);
	if (length <= kEpsilon) {
		return {};
	}
	return {vector.x / length, vector.y / length, vector.z / length};
}

std::array<Vector3, 3> GetNormalizedAxes(const ColliderManager::OBB& obb) {
	return {
	    Normalize(obb.orientations[0]),
	    Normalize(obb.orientations[1]),
	    Normalize(obb.orientations[2]),
	};
}

Vector3 ClosestPointOnSegment(const ColliderManager::Segment& segment, const Vector3& point) {
	const Vector3 segmentVector = Subtract(segment.end, segment.start);
	const float lengthSquared = Dot(segmentVector, segmentVector);
	if (lengthSquared <= kEpsilon) {
		return segment.start;
	}

	const Vector3 startToPoint = Subtract(point, segment.start);
	const float t = std::clamp(Dot(startToPoint, segmentVector) / lengthSquared, 0.0f, 1.0f);
	return {
	    segment.start.x + segmentVector.x * t,
	    segment.start.y + segmentVector.y * t,
	    segment.start.z + segmentVector.z * t,
	};
}

bool CheckSegmentAxis(float start, float direction, float minimum, float maximum, float& tMin, float& tMax) {
	if (std::abs(direction) <= kEpsilon) {
		return start >= minimum && start <= maximum;
	}

	float firstT = (minimum - start) / direction;
	float secondT = (maximum - start) / direction;
	if (firstT > secondT) {
		std::swap(firstT, secondT);
	}

	tMin = (std::max)(tMin, firstT);
	tMax = (std::min)(tMax, secondT);
	return tMin <= tMax;
}

} // namespace

bool ColliderManager::IsValid(const Sphere& sphere) {
	return sphere.radius >= 0.0f;
}

bool ColliderManager::IsValid(const CircleXZ& circle) {
	return circle.radius >= 0.0f;
}

bool ColliderManager::IsValid(const AABB& aabb) {
	return aabb.min.x <= aabb.max.x &&
	       aabb.min.y <= aabb.max.y &&
	       aabb.min.z <= aabb.max.z;
}

bool ColliderManager::IsValid(const OBB& obb) {
	if (obb.halfSize.x < 0.0f || obb.halfSize.y < 0.0f || obb.halfSize.z < 0.0f) {
		return false;
	}

	return Length(obb.orientations[0]) > kEpsilon &&
	       Length(obb.orientations[1]) > kEpsilon &&
	       Length(obb.orientations[2]) > kEpsilon;
}

float ColliderManager::DistanceSquared(const Vector3& first, const Vector3& second) {
	const float x = first.x - second.x;
	const float y = first.y - second.y;
	const float z = first.z - second.z;
	return x * x + y * y + z * z;
}

float ColliderManager::DistanceSquaredXZ(const Vector3& first, const Vector3& second) {
	const float x = first.x - second.x;
	const float z = first.z - second.z;
	return x * x + z * z;
}

bool ColliderManager::CheckSphereSphere(const Sphere& first, const Sphere& second) {
	if (!IsValid(first) || !IsValid(second)) {
		return false;
	}

	const float totalRadius = first.radius + second.radius;
	return DistanceSquared(first.center, second.center) <= totalRadius * totalRadius;
}

bool ColliderManager::CheckAABBAABB(const AABB& first, const AABB& second) {
	if (!IsValid(first) || !IsValid(second)) {
		return false;
	}

	return first.min.x <= second.max.x && first.max.x >= second.min.x &&
	       first.min.y <= second.max.y && first.max.y >= second.min.y &&
	       first.min.z <= second.max.z && first.max.z >= second.min.z;
}

bool ColliderManager::CheckSphereAABB(const Sphere& sphere, const AABB& aabb) {
	if (!IsValid(sphere) || !IsValid(aabb)) {
		return false;
	}

	const Vector3 closestPoint = {
	    std::clamp(sphere.center.x, aabb.min.x, aabb.max.x),
	    std::clamp(sphere.center.y, aabb.min.y, aabb.max.y),
	    std::clamp(sphere.center.z, aabb.min.z, aabb.max.z),
	};
	return DistanceSquared(sphere.center, closestPoint) <= sphere.radius * sphere.radius;
}

bool ColliderManager::CheckPointSphere(const Vector3& point, const Sphere& sphere) {
	if (!IsValid(sphere)) {
		return false;
	}
	return DistanceSquared(point, sphere.center) <= sphere.radius * sphere.radius;
}

bool ColliderManager::CheckPointAABB(const Vector3& point, const AABB& aabb) {
	if (!IsValid(aabb)) {
		return false;
	}

	return point.x >= aabb.min.x && point.x <= aabb.max.x &&
	       point.y >= aabb.min.y && point.y <= aabb.max.y &&
	       point.z >= aabb.min.z && point.z <= aabb.max.z;
}

bool ColliderManager::CheckSegmentSphere(const Segment& segment, const Sphere& sphere) {
	if (!IsValid(sphere)) {
		return false;
	}

	const Vector3 closestPoint = ClosestPointOnSegment(segment, sphere.center);
	return DistanceSquared(closestPoint, sphere.center) <= sphere.radius * sphere.radius;
}

bool ColliderManager::CheckSegmentAABB(const Segment& segment, const AABB& aabb) {
	if (!IsValid(aabb)) {
		return false;
	}

	const Vector3 direction = Subtract(segment.end, segment.start);
	float tMin = 0.0f;
	float tMax = 1.0f;

	return CheckSegmentAxis(segment.start.x, direction.x, aabb.min.x, aabb.max.x, tMin, tMax) &&
	       CheckSegmentAxis(segment.start.y, direction.y, aabb.min.y, aabb.max.y, tMin, tMax) &&
	       CheckSegmentAxis(segment.start.z, direction.z, aabb.min.z, aabb.max.z, tMin, tMax);
}

ColliderManager::OBB ColliderManager::CreateOBB(
    const WorldTransform& worldTransform,
    const Vector3& localHalfSize) {
	OBB result{};
	result.center = {
	    worldTransform.matWorld_.m[3][0],
	    worldTransform.matWorld_.m[3][1],
	    worldTransform.matWorld_.m[3][2],
	};

	const std::array<Vector3, 3> worldAxes = {
	    Vector3{worldTransform.matWorld_.m[0][0], worldTransform.matWorld_.m[0][1], worldTransform.matWorld_.m[0][2]},
	    Vector3{worldTransform.matWorld_.m[1][0], worldTransform.matWorld_.m[1][1], worldTransform.matWorld_.m[1][2]},
	    Vector3{worldTransform.matWorld_.m[2][0], worldTransform.matWorld_.m[2][1], worldTransform.matWorld_.m[2][2]},
	};

	const float axisXLength = Length(worldAxes[0]);
	const float axisYLength = Length(worldAxes[1]);
	const float axisZLength = Length(worldAxes[2]);
	result.orientations = {
	    Normalize(worldAxes[0]),
	    Normalize(worldAxes[1]),
	    Normalize(worldAxes[2]),
	};
	result.halfSize = {
	    std::abs(localHalfSize.x) * axisXLength,
	    std::abs(localHalfSize.y) * axisYLength,
	    std::abs(localHalfSize.z) * axisZLength,
	};
	return result;
}

bool ColliderManager::CheckPointOBB(const Vector3& point, const OBB& obb) {
	if (!IsValid(obb)) {
		return false;
	}

	const std::array<Vector3, 3> axes = GetNormalizedAxes(obb);
	const Vector3 centerToPoint = Subtract(point, obb.center);
	return std::abs(Dot(centerToPoint, axes[0])) <= obb.halfSize.x &&
	       std::abs(Dot(centerToPoint, axes[1])) <= obb.halfSize.y &&
	       std::abs(Dot(centerToPoint, axes[2])) <= obb.halfSize.z;
}

bool ColliderManager::CheckSphereOBB(const Sphere& sphere, const OBB& obb) {
	if (!IsValid(sphere) || !IsValid(obb)) {
		return false;
	}

	const std::array<Vector3, 3> axes = GetNormalizedAxes(obb);
	const Vector3 centerDifference = Subtract(sphere.center, obb.center);
	const std::array<float, 3> halfSizes = {obb.halfSize.x, obb.halfSize.y, obb.halfSize.z};
	Vector3 closestPoint = obb.center;

	for (size_t i = 0; i < axes.size(); ++i) {
		const float distanceOnAxis = std::clamp(Dot(centerDifference, axes[i]), -halfSizes[i], halfSizes[i]);
		closestPoint.x += axes[i].x * distanceOnAxis;
		closestPoint.y += axes[i].y * distanceOnAxis;
		closestPoint.z += axes[i].z * distanceOnAxis;
	}

	return DistanceSquared(sphere.center, closestPoint) <= sphere.radius * sphere.radius;
}

bool ColliderManager::CheckSegmentOBB(const Segment& segment, const OBB& obb) {
	if (!IsValid(obb)) {
		return false;
	}

	const std::array<Vector3, 3> axes = GetNormalizedAxes(obb);
	const Vector3 startDifference = Subtract(segment.start, obb.center);
	const Vector3 endDifference = Subtract(segment.end, obb.center);
	const Segment localSegment = {
	    {Dot(startDifference, axes[0]), Dot(startDifference, axes[1]), Dot(startDifference, axes[2])},
	    {Dot(endDifference, axes[0]), Dot(endDifference, axes[1]), Dot(endDifference, axes[2])},
	};
	const AABB localAABB = {
	    {-obb.halfSize.x, -obb.halfSize.y, -obb.halfSize.z},
	    {obb.halfSize.x, obb.halfSize.y, obb.halfSize.z},
	};
	return CheckSegmentAABB(localSegment, localAABB);
}

bool ColliderManager::CheckAABBOBB(const AABB& aabb, const OBB& obb) {
	if (!IsValid(aabb) || !IsValid(obb)) {
		return false;
	}

	const OBB aabbAsOBB = {
	    {(aabb.min.x + aabb.max.x) * 0.5f, (aabb.min.y + aabb.max.y) * 0.5f, (aabb.min.z + aabb.max.z) * 0.5f},
	    {Vector3{1.0f, 0.0f, 0.0f}, Vector3{0.0f, 1.0f, 0.0f}, Vector3{0.0f, 0.0f, 1.0f}},
	    {(aabb.max.x - aabb.min.x) * 0.5f, (aabb.max.y - aabb.min.y) * 0.5f, (aabb.max.z - aabb.min.z) * 0.5f},
	};
	return CheckOBBOBB(aabbAsOBB, obb);
}

bool ColliderManager::CheckOBBOBB(const OBB& first, const OBB& second) {
	if (!IsValid(first) || !IsValid(second)) {
		return false;
	}

	const std::array<Vector3, 3> firstAxes = GetNormalizedAxes(first);
	const std::array<Vector3, 3> secondAxes = GetNormalizedAxes(second);
	const std::array<float, 3> firstHalfSize = {first.halfSize.x, first.halfSize.y, first.halfSize.z};
	const std::array<float, 3> secondHalfSize = {second.halfSize.x, second.halfSize.y, second.halfSize.z};

	float rotation[3][3]{};
	float absoluteRotation[3][3]{};
	for (size_t i = 0; i < 3; ++i) {
		for (size_t j = 0; j < 3; ++j) {
			rotation[i][j] = Dot(firstAxes[i], secondAxes[j]);
			absoluteRotation[i][j] = std::abs(rotation[i][j]) + kEpsilon;
		}
	}

	const Vector3 centerDifference = Subtract(second.center, first.center);
	const std::array<float, 3> translation = {
	    Dot(centerDifference, firstAxes[0]),
	    Dot(centerDifference, firstAxes[1]),
	    Dot(centerDifference, firstAxes[2]),
	};

	// それぞれのOBBが持つ6軸を検査する。
	for (size_t i = 0; i < 3; ++i) {
		const float secondRadius =
		    secondHalfSize[0] * absoluteRotation[i][0] +
		    secondHalfSize[1] * absoluteRotation[i][1] +
		    secondHalfSize[2] * absoluteRotation[i][2];
		if (std::abs(translation[i]) > firstHalfSize[i] + secondRadius) {
			return false;
		}
	}

	for (size_t j = 0; j < 3; ++j) {
		const float projectedTranslation = std::abs(
		    translation[0] * rotation[0][j] +
		    translation[1] * rotation[1][j] +
		    translation[2] * rotation[2][j]);
		const float firstRadius =
		    firstHalfSize[0] * absoluteRotation[0][j] +
		    firstHalfSize[1] * absoluteRotation[1][j] +
		    firstHalfSize[2] * absoluteRotation[2][j];
		if (projectedTranslation > firstRadius + secondHalfSize[j]) {
			return false;
		}
	}

	// 両OBBの軸同士の外積から得られる9軸を検査する。
	for (size_t i = 0; i < 3; ++i) {
		const size_t nextI = (i + 1) % 3;
		const size_t lastI = (i + 2) % 3;
		for (size_t j = 0; j < 3; ++j) {
			const size_t nextJ = (j + 1) % 3;
			const size_t lastJ = (j + 2) % 3;
			const float firstRadius =
			    firstHalfSize[nextI] * absoluteRotation[lastI][j] +
			    firstHalfSize[lastI] * absoluteRotation[nextI][j];
			const float secondRadius =
			    secondHalfSize[nextJ] * absoluteRotation[i][lastJ] +
			    secondHalfSize[lastJ] * absoluteRotation[i][nextJ];
			const float projectedTranslation = std::abs(
			    translation[lastI] * rotation[nextI][j] -
			    translation[nextI] * rotation[lastI][j]);
			if (projectedTranslation > firstRadius + secondRadius) {
				return false;
			}
		}
	}

	return true;
}

bool ColliderManager::CheckCircleCircleXZ(const CircleXZ& first, const CircleXZ& second) {
	if (!IsValid(first) || !IsValid(second)) {
		return false;
	}

	const float totalRadius = first.radius + second.radius;
	return DistanceSquaredXZ(first.center, second.center) <= totalRadius * totalRadius;
}

bool ColliderManager::CheckPointCircleXZ(const Vector3& point, const CircleXZ& circle) {
	if (!IsValid(circle)) {
		return false;
	}
	return DistanceSquaredXZ(point, circle.center) <= circle.radius * circle.radius;
}

bool ColliderManager::CheckSegmentCircleXZ(const Segment& segment, const CircleXZ& circle) {
	if (!IsValid(circle)) {
		return false;
	}

	const float segmentX = segment.end.x - segment.start.x;
	const float segmentZ = segment.end.z - segment.start.z;
	const float lengthSquared = segmentX * segmentX + segmentZ * segmentZ;

	if (lengthSquared <= kEpsilon) {
		return CheckPointCircleXZ(segment.start, circle);
	}

	const float startToCenterX = circle.center.x - segment.start.x;
	const float startToCenterZ = circle.center.z - segment.start.z;
	const float t = std::clamp(
	    (startToCenterX * segmentX + startToCenterZ * segmentZ) / lengthSquared,
	    0.0f,
	    1.0f);

	const Vector3 closestPoint = {
	    segment.start.x + segmentX * t,
	    circle.center.y,
	    segment.start.z + segmentZ * t,
	};
	return DistanceSquaredXZ(closestPoint, circle.center) <= circle.radius * circle.radius;
}
