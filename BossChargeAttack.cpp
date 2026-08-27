#include "BossChargeAttack.h"

#include "ColliderManager.h"
#include "Player.h"
#include "TransformUtility.h"

#include <algorithm>
#include <cmath>

using namespace KamataEngine;

void BossChargeAttack::Initialize(Model* predictionLineModel, Camera* camera, Player* player) {
	predictionLineModel_ = predictionLineModel;
	camera_ = camera;
	player_ = player;
	dashSeHandle_ = Audio::GetInstance()->LoadWave("Audio/Game_BossDash_SE.wav");
	predictionTransform_.Initialize();
	predictionColor_.Initialize();
	predictionColor_.SetColor({1.0f, 0.7f, 0.05f, 0.75f});
	state_ = State::Inactive;
}

void BossChargeAttack::Start(const Vector3& bossPosition, const Vector3& playerPosition) {
	if (predictionLineModel_ == nullptr || camera_ == nullptr || player_ == nullptr) {
		return;
	}

	UpdateChargeTarget(bossPosition, playerPosition);

	travelledDistance_ = 0.0f;
	hasHitPlayer_ = false;
	state_ = State::Warning;
	stateTimer_ = kWarningFrames;
	UpdatePredictionTransform(bossPosition);
}

void BossChargeAttack::Update(WorldTransform& bossWorldTransform) {
	if (!IsActive()) {
		return;
	}

	if (state_ == State::Warning) {
		// 予兆中はプレイヤーを追い、予測線と突進先を常に更新する。
		UpdateChargeTarget(
		    bossWorldTransform.translation_, player_->GetWorldTransform().translation_);
		UpdatePredictionTransform(bossWorldTransform.translation_);
		if (--stateTimer_ <= 0) {
			state_ = State::Charging;
			Audio::GetInstance()->PlayWave(dashSeHandle_, false, 0.82f);
		}
		return;
	}

	if (state_ == State::Charging) {
		const float remainingDistance = chargeDistance_ - travelledDistance_;
		const float movement = (kChargeSpeed < remainingDistance) ? kChargeSpeed : remainingDistance;
		bossWorldTransform.translation_.x += chargeDirection_.x * movement;
		bossWorldTransform.translation_.z += chargeDirection_.z * movement;
		bossWorldTransform.rotation_.y = std::atan2(chargeDirection_.x, chargeDirection_.z);
		travelledDistance_ += movement;
		UpdateWorldTransform(bossWorldTransform);
		CheckPlayerCollision(bossWorldTransform.translation_);
		if (travelledDistance_ >= chargeDistance_) {
			state_ = State::Recovery;
			stateTimer_ = kRecoveryFrames;
		}
		return;
	}

	if (--stateTimer_ <= 0) {
		state_ = State::Inactive;
	}
}

void BossChargeAttack::UpdateChargeTarget(
    const Vector3& bossPosition, const Vector3& playerPosition) {
	chargeDirection_ = {playerPosition.x - bossPosition.x, 0.0f, playerPosition.z - bossPosition.z};
	const float distance = std::sqrt(
	    chargeDirection_.x * chargeDirection_.x + chargeDirection_.z * chargeDirection_.z);
	if (distance < 0.001f) {
		chargeDirection_ = {0.0f, 0.0f, 1.0f};
		chargeDistance_ = kMinimumChargeDistance;
		return;
	}

	chargeDirection_.x /= distance;
	chargeDirection_.z /= distance;
	chargeDistance_ = std::clamp(
	    distance + kOvershootDistance,
	    kMinimumChargeDistance,
	    kMaximumChargeDistance * attackRangeMultiplier_);
}

void BossChargeAttack::Draw() {
	if (state_ == State::Warning && predictionLineModel_ != nullptr && camera_ != nullptr) {
		predictionLineModel_->Draw(predictionTransform_, *camera_, &predictionColor_);
	}
}

void BossChargeAttack::StopForDown() {
	state_ = State::Inactive;
	stateTimer_ = 0;
}

void BossChargeAttack::UpdatePredictionTransform(const Vector3& bossPosition) {
	// BoxPredictionCircle は XZ 平面の 2x2 板。Z 方向を予測線の長さへ拡大する。
	predictionTransform_.scale_ = {
	    kPredictionWidth * attackRangeMultiplier_ * 0.5f,
	    1.0f,
	    chargeDistance_ * 0.5f};
	predictionTransform_.rotation_.y = std::atan2(chargeDirection_.x, chargeDirection_.z);
	predictionTransform_.translation_ = {
	    bossPosition.x + chargeDirection_.x * chargeDistance_ * 0.5f,
	    bossPosition.y + kPredictionDisplayHeight,
	    bossPosition.z + chargeDirection_.z * chargeDistance_ * 0.5f,
	};
	UpdateWorldTransform(predictionTransform_);
}

void BossChargeAttack::CheckPlayerCollision(const Vector3& bossPosition) {
	if (hasHitPlayer_ || player_ == nullptr) {
		return;
	}

	const ColliderManager::CircleXZ bossCollider = {bossPosition, kCollisionRadius};
	const ColliderManager::CircleXZ playerCollider = {
	    player_->GetWorldTransform().translation_, player_->GetCollisionRadius()};
	if (ColliderManager::CheckCircleCircleXZ(bossCollider, playerCollider)) {
		// 突進中にも即座に押し出して、ボスがプレイヤーを通り抜けないようにする。
		player_->ResolveObstacleCollisionXZ(
		    bossPosition, kCollisionRadius + kCollisionSeparationMargin);
		player_->ApplyDamage(kDamage);
		player_->ApplyKnockbackAndStun(bossPosition, kKnockbackForce, kStunFrames);
		hasHitPlayer_ = true;
	}
}
