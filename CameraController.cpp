#include "CameraController.h"

#include <algorithm>
#include <cmath>
#include <numbers>

using namespace KamataEngine;

void CameraController::Initialize(Camera* camera, const WorldTransform* target) {
	camera_ = camera;
	target_ = target;
	secondaryTarget_ = nullptr;
	Reset();
}

void CameraController::SetTarget(const WorldTransform* target) {
	target_ = target;
	Reset();
}

void CameraController::SetSecondaryTarget(const WorldTransform* target) {
	secondaryTarget_ = target;
	Reset();
}

void CameraController::StartShake(
    float strength, int durationFrames, float frequency) {
	if (strength <= 0.0f || durationFrames <= 0) {
		return;
	}
	// 継続する弱い振動が、被ダメージなどの強い振動を上書きしないようにする。
	if (strength >= shakeStrength_) {
		shakeStrength_ = strength;
		shakeFrequency_ = frequency;
	}
	shakeTimer_ = (std::max)(shakeTimer_, durationFrames);
}

Vector3 CameraController::GetTargetPosition() const {
	if (target_ == nullptr) {
		return {};
	}
	if (secondaryTarget_ == nullptr) {
		return target_->translation_;
	}
	return {
	    (target_->translation_.x + secondaryTarget_->translation_.x) * 0.5f,
	    (target_->translation_.y + secondaryTarget_->translation_.y) * 0.5f,
	    (target_->translation_.z + secondaryTarget_->translation_.z) * 0.5f,
	};
}

float CameraController::GetRequiredDistance() const {
	if (target_ == nullptr || secondaryTarget_ == nullptr) {
		return 0.0f;
	}
	const float differenceX = target_->translation_.x - secondaryTarget_->translation_.x;
	const float differenceZ = target_->translation_.z - secondaryTarget_->translation_.z;
	const float separation = std::sqrt(differenceX * differenceX + differenceZ * differenceZ);
	return separation * kFramingDistanceMultiplier + kFramingPadding;
}

void CameraController::Update() {
	if (camera_ == nullptr || target_ == nullptr) {
		return;
	}

	// Windowsの標準ホイール量は1目盛り120。
	// 奥へ回す（正の値）と接近し、手前へ回すと離れる。
	constexpr float kWheelDelta = 120.0f;
	const int32_t wheel = Input::GetInstance()->GetWheel();
	if (wheel != 0) {
		manualDistance_ -= (static_cast<float>(wheel) / kWheelDelta) * kZoomSpeed;
		manualDistance_ = std::clamp(manualDistance_, kMinDistance, kMaxDistance);
	}

	// プレイヤーとボスが離れたときは、両者が画面に入る距離まで自動で引く。
	float requiredDistance = GetRequiredDistance();
	if (requiredDistance < kMinDistance) {
		requiredDistance = kMinDistance;
	} else if (requiredDistance > kMaxDistance) {
		requiredDistance = kMaxDistance;
	}
	const float desiredDistance =
	    (manualDistance_ > requiredDistance) ? manualDistance_ : requiredDistance;
	distance_ += (desiredDistance - distance_) * kZoomInterpolation;

	const Vector3 targetPosition = GetTargetPosition();
	// 中心対象がデッドゾーンを抜けた分だけカメラ注視点を動かす。
	// 小さな移動ではカメラが静止するため、画面酔いを抑えられる。
	const float focusDifferenceX = targetPosition.x - focusPosition_.x;
	const float focusDifferenceZ = targetPosition.z - focusPosition_.z;
	const float focusDifferenceLength = std::sqrt(
	    focusDifferenceX * focusDifferenceX + focusDifferenceZ * focusDifferenceZ);
	if (focusDifferenceLength > kDeadZoneRadius) {
		const float moveRatio =
		    (focusDifferenceLength - kDeadZoneRadius) / focusDifferenceLength;
		focusPosition_.x += focusDifferenceX * moveRatio;
		focusPosition_.z += focusDifferenceZ * moveRatio;
	}
	focusPosition_.y = targetPosition.y;

	const Vector3 velocity = {
	    focusPosition_.x - previousTargetPosition_.x,
	    focusPosition_.y - previousTargetPosition_.y,
	    focusPosition_.z - previousTargetPosition_.z,
	};
	const Vector3 acceleration = {
	    velocity.x - previousVelocity_.x,
	    velocity.y - previousVelocity_.y,
	    velocity.z - previousVelocity_.z,
	};

	// 速度と加速度から、進行方向へ先行する注視位置を作る。
	Vector3 desiredLookAhead = {
	    velocity.x * kVelocityLookAhead + acceleration.x * kAccelerationLookAhead,
	    0.0f,
	    velocity.z * kVelocityLookAhead + acceleration.z * kAccelerationLookAhead,
	};

	const float lookAheadLength = std::sqrt(
	    desiredLookAhead.x * desiredLookAhead.x + desiredLookAhead.z * desiredLookAhead.z);
	if (lookAheadLength > kMaxLookAhead) {
		const float scale = kMaxLookAhead / lookAheadLength;
		desiredLookAhead.x *= scale;
		desiredLookAhead.z *= scale;
	}

	lookAhead_.x += (desiredLookAhead.x - lookAhead_.x) * kLookAheadInterpolation;
	lookAhead_.z += (desiredLookAhead.z - lookAhead_.z) * kLookAheadInterpolation;

	const Vector3 desiredCameraPosition = {
	    focusPosition_.x + lookAhead_.x,
	    focusPosition_.y + distance_,
	    focusPosition_.z + lookAhead_.z - distance_,
	};

	// 前フレームのシェイク分を戻して、追従座標へ揺れが蓄積しないようにする。
	camera_->translation_.x -= previousShakeOffset_.x;
	camera_->translation_.y -= previousShakeOffset_.y;
	camera_->translation_.z -= previousShakeOffset_.z;
	previousShakeOffset_ = {};

	// カメラ座標を補間し、プレイヤーの急な方向転換を滑らかに追従する。
	camera_->translation_.x +=
	    (desiredCameraPosition.x - camera_->translation_.x) * kCameraInterpolation;
	camera_->translation_.y +=
	    (desiredCameraPosition.y - camera_->translation_.y) * kCameraInterpolation;
	camera_->translation_.z +=
	    (desiredCameraPosition.z - camera_->translation_.z) * kCameraInterpolation;

	if (shakeTimer_ > 0) {
		shakePhase_ += shakeFrequency_;
		previousShakeOffset_ = {
		    std::sin(shakePhase_ * 1.73f) * shakeStrength_,
		    std::sin(shakePhase_ * 2.31f + 1.1f) * shakeStrength_ * 0.42f,
		    std::cos(shakePhase_ * 1.37f + 0.6f) * shakeStrength_ * 0.72f,
		};
		camera_->translation_.x += previousShakeOffset_.x;
		camera_->translation_.y += previousShakeOffset_.y;
		camera_->translation_.z += previousShakeOffset_.z;
		shakeStrength_ *= kShakeDecay;
		--shakeTimer_;
		if (shakeTimer_ == 0) {
			shakeStrength_ = 0.0f;
		}
	}

	// X軸を45度傾け、プレイヤーを上方・後方から見下ろす。
	camera_->rotation_ = {std::numbers::pi_v<float> / 4.0f, 0.0f, 0.0f};
	camera_->UpdateMatrix();

	previousTargetPosition_ = focusPosition_;
	previousVelocity_ = velocity;
}

void CameraController::Reset() {
	distance_ = kDefaultDistance;
	manualDistance_ = kDefaultDistance;
	previousVelocity_ = {};
	lookAhead_ = {};
	previousShakeOffset_ = {};
	shakeStrength_ = 0.0f;
	shakePhase_ = 0.0f;
	shakeFrequency_ = 1.0f;
	shakeTimer_ = 0;

	if (camera_ == nullptr || target_ == nullptr) {
		return;
	}

	previousTargetPosition_ = GetTargetPosition();
	focusPosition_ = previousTargetPosition_;
	camera_->translation_ = {
	    previousTargetPosition_.x,
	    previousTargetPosition_.y + distance_,
	    previousTargetPosition_.z - distance_,
	};
	camera_->rotation_ = {std::numbers::pi_v<float> / 4.0f, 0.0f, 0.0f};
	camera_->UpdateMatrix();
}
