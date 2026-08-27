#pragma once

#include "KamataEngine.h"
#include <array>

class ColliderManager final {
public:
	struct Sphere {
		KamataEngine::Vector3 center = {};
		float radius = 0.0f;
	};

	struct CircleXZ {
		KamataEngine::Vector3 center = {};
		float radius = 0.0f;
	};

	struct AABB {
		KamataEngine::Vector3 min = {};
		KamataEngine::Vector3 max = {};
	};

	struct Segment {
		KamataEngine::Vector3 start = {};
		KamataEngine::Vector3 end = {};
	};

	struct OBB {
		KamataEngine::Vector3 center = {};
		std::array<KamataEngine::Vector3, 3> orientations = {
		    KamataEngine::Vector3{1.0f, 0.0f, 0.0f},
		    KamataEngine::Vector3{0.0f, 1.0f, 0.0f},
		    KamataEngine::Vector3{0.0f, 0.0f, 1.0f},
		};
		KamataEngine::Vector3 halfSize = {};
	};

	// 3D判定
	static bool CheckSphereSphere(const Sphere& first, const Sphere& second);
	static bool CheckAABBAABB(const AABB& first, const AABB& second);
	static bool CheckSphereAABB(const Sphere& sphere, const AABB& aabb);
	static bool CheckPointSphere(const KamataEngine::Vector3& point, const Sphere& sphere);
	static bool CheckPointAABB(const KamataEngine::Vector3& point, const AABB& aabb);
	static bool CheckSegmentSphere(const Segment& segment, const Sphere& sphere);
	static bool CheckSegmentAABB(const Segment& segment, const AABB& aabb);
	static bool CheckOBBOBB(const OBB& first, const OBB& second);
	static bool CheckAABBOBB(const AABB& aabb, const OBB& obb);
	static bool CheckSphereOBB(const Sphere& sphere, const OBB& obb);
	static bool CheckPointOBB(const KamataEngine::Vector3& point, const OBB& obb);
	static bool CheckSegmentOBB(const Segment& segment, const OBB& obb);

	// 更新済みWorldTransformとモデルのローカル半サイズからOBBを生成する。
	static OBB CreateOBB(
	    const KamataEngine::WorldTransform& worldTransform,
	    const KamataEngine::Vector3& localHalfSize);

	// Y軸を無視するトップダウンゲーム向けXZ平面判定
	static bool CheckCircleCircleXZ(const CircleXZ& first, const CircleXZ& second);
	static bool CheckPointCircleXZ(const KamataEngine::Vector3& point, const CircleXZ& circle);
	static bool CheckSegmentCircleXZ(const Segment& segment, const CircleXZ& circle);

	static float DistanceSquared(
	    const KamataEngine::Vector3& first,
	    const KamataEngine::Vector3& second);
	static float DistanceSquaredXZ(
	    const KamataEngine::Vector3& first,
	    const KamataEngine::Vector3& second);

private:
	static bool IsValid(const Sphere& sphere);
	static bool IsValid(const CircleXZ& circle);
	static bool IsValid(const AABB& aabb);
	static bool IsValid(const OBB& obb);
};
