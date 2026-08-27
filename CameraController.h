#pragma once

#include "KamataEngine.h"

class CameraController {
public:
	CameraController() = default;
	~CameraController() = default;

	void Initialize(KamataEngine::Camera* camera, const KamataEngine::WorldTransform* target);
	void Update();
	void Reset();
	void SetTarget(const KamataEngine::WorldTransform* target);
	void SetSecondaryTarget(const KamataEngine::WorldTransform* target);
	void StartShake(float strength, int durationFrames, float frequency);

private:
	KamataEngine::Camera* camera_ = nullptr;
	const KamataEngine::WorldTransform* target_ = nullptr;
	const KamataEngine::WorldTransform* secondaryTarget_ = nullptr;

	KamataEngine::Vector3 GetTargetPosition() const;
	float GetRequiredDistance() const;

	// Y方向とZ方向に同じ距離を取ることで、45度の俯瞰角度を維持する。
	float distance_ = 25.0f;
	float manualDistance_ = 25.0f;
	KamataEngine::Vector3 previousTargetPosition_ = {};
	KamataEngine::Vector3 focusPosition_ = {};
	KamataEngine::Vector3 previousVelocity_ = {};
	KamataEngine::Vector3 lookAhead_ = {};
	KamataEngine::Vector3 previousShakeOffset_ = {};
	float shakeStrength_ = 0.0f;
	float shakePhase_ = 0.0f;
	float shakeFrequency_ = 1.0f;
	int shakeTimer_ = 0;

	static constexpr float kDefaultDistance = 50.0f ;
	static constexpr float kMinDistance = 10.0f;
	static constexpr float kMaxDistance = 200.0f;
	static constexpr float kZoomSpeed = 3.0f;
	static constexpr float kCameraInterpolation = 0.12f;
	static constexpr float kZoomInterpolation = 0.12f;
	static constexpr float kDeadZoneRadius = 8.0f;
	static constexpr float kFramingDistanceMultiplier = 2.0f;
	static constexpr float kFramingPadding = 30.0f;
	static constexpr float kLookAheadInterpolation = 0.15f;
	static constexpr float kVelocityLookAhead = 30.0f;
	static constexpr float kAccelerationLookAhead = 8.0f;
	static constexpr float kMaxLookAhead = 8.0f;
	static constexpr float kShakeDecay = 0.88f;
};
