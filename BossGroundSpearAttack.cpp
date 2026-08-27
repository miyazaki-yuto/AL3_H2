#include "BossGroundSpearAttack.h"

#include "ColliderManager.h"
#include "LerpManager.h"
#include "Player.h"
#include "TransformUtility.h"

using namespace KamataEngine;

void BossGroundSpearAttack::Initialize(
    Model* spearModel,
    Model* predictionCircleModel,
    Camera* camera,
    Player* player) {
	spearModel_ = spearModel;
	predictionCircleModel_ = predictionCircleModel;
	camera_ = camera;
	player_ = player;
	groundSpearSeHandle_ = Audio::GetInstance()->LoadWave(
	    "Audio/Game_BossGroundSpear_SE.wav");

	for (Spear& spear : spears_) {
		spear.spearTransform.Initialize();
		spear.predictionTransform.Initialize();
		spear.predictionColor.Initialize();
		// alpha=15..16は地面槍予兆シェーダーと進行度のマーカー。
		spear.predictionColor.SetColor({1.0f, 0.12f, 0.015f, 15.0f});
		spear.state = SpearState::Dormant;
	}
	nextSpearIndex_ = 0;
	nextSpearTimer_ = -1;
	isActive_ = false;
	spearEruptionPending_ = false;
}

void BossGroundSpearAttack::Start(
    const Vector3& bossPosition,
    const Vector3& playerPosition) {
	if (
	    spearModel_ == nullptr || predictionCircleModel_ == nullptr || camera_ == nullptr ||
	    player_ == nullptr) {
		return;
	}

	groundHeight_ = bossPosition.y;
	for (Spear& spear : spears_) {
		spear.state = SpearState::Dormant;
		spear.stateTimer = 0;
		spear.hasDealtDamage = false;
		spear.isLaserShield = false;
	}

	BeginWarning(spears_[0], playerPosition);
	nextSpearIndex_ = 1;
	nextSpearTimer_ = -1;
	isActive_ = true;
	spearEruptionPending_ = false;
}

void BossGroundSpearAttack::Update() {
	if (!isActive_ || player_ == nullptr) {
		return;
	}

	bool spearStartedRising = false;
	for (Spear& spear : spears_) {
		switch (spear.state) {
		case SpearState::Warning:
			if (spear.stateTimer <= 0) {
				spear.state = SpearState::Rising;
				spear.stateTimer = kRisingFrames;
				spearStartedRising = true;
				spearEruptionPending_ = true;
				Audio::GetInstance()->PlayWave(groundSpearSeHandle_, false, 0.82f);
			} else {
				const int elapsedFrames = kWarningFrames - spear.stateTimer;
				const float warningProgress =
				    static_cast<float>(elapsedFrames) /
				    static_cast<float>(kWarningFrames - 1);
				UpdatePredictionTransform(spear, warningProgress);
				--spear.stateTimer;
			}
			break;

		case SpearState::Rising: {
			const int elapsedFrames = kRisingFrames - spear.stateTimer + 1;
			const float t =
			    static_cast<float>(elapsedFrames) / static_cast<float>(kRisingFrames);
			const float riseProgress =
			    LerpManager::ApplyEasing(t, LerpManager::EaseType::EaseOutBack);
			UpdateSpearTransform(spear, riseProgress);
			CheckPlayerCollision(spear);

			--spear.stateTimer;
			if (spear.stateTimer <= 0) {
				spear.state = SpearState::Active;
				spear.stateTimer = kActiveFrames;
				UpdateSpearTransform(spear, 1.0f);
			}
			break;
		}

		case SpearState::Active:
			player_->ResolveObstacleCollisionXZ(spear.groundPosition, GetCollisionRadius());
			--spear.stateTimer;
			if (spear.stateTimer <= 0) {
				spear.state = SpearState::Retracting;
				spear.stateTimer = kRetractingFrames;
			}
			break;

		case SpearState::Retracting: {
			const int elapsedFrames = kRetractingFrames - spear.stateTimer + 1;
			const float t =
			    static_cast<float>(elapsedFrames) / static_cast<float>(kRetractingFrames);
			// Reversed EaseOutBack: slight anticipation, followed by a fast retreat.
			const float retractingRiseProgress = LerpManager::ApplyEasing(
			    1.0f - t, LerpManager::EaseType::EaseOutBack);
			UpdateSpearTransform(spear, retractingRiseProgress);
			player_->ResolveObstacleCollisionXZ(spear.groundPosition, GetCollisionRadius());

			--spear.stateTimer;
			if (spear.stateTimer <= 0) {
				spear.state = SpearState::Dormant;
				UpdateSpearTransform(spear, 0.0f);
			}
			break;
		}

		case SpearState::Dormant:
		default:
			break;
		}
	}

	if (spearStartedRising && nextSpearIndex_ < spears_.size() && nextSpearTimer_ < 0) {
		nextSpearTimer_ = kNextSpearDelayFrames;
	}
	if (nextSpearTimer_ >= 0) {
		--nextSpearTimer_;
		if (nextSpearTimer_ <= 0 && nextSpearIndex_ < spears_.size()) {
			BeginWarning(
			    spears_[nextSpearIndex_], player_->GetWorldTransform().translation_);
			++nextSpearIndex_;
			nextSpearTimer_ = -1;
		}
	}

	if (nextSpearIndex_ >= spears_.size() && AreAllSpearsDormant()) {
		isActive_ = false;
	}
}

void BossGroundSpearAttack::Draw() {
	if (!isActive_ || camera_ == nullptr) {
		return;
	}

	for (const Spear& spear : spears_) {
		if (spear.state == SpearState::Warning && predictionCircleModel_ != nullptr) {
			predictionCircleModel_->Draw(
			    spear.predictionTransform, *camera_, &spear.predictionColor);
		} else if (
		    (spear.state == SpearState::Rising || spear.state == SpearState::Active ||
		     spear.state == SpearState::Retracting) &&
		    spearModel_ != nullptr) {
			spearModel_->Draw(spear.spearTransform, *camera_);
		}
	}
}

bool BossGroundSpearAttack::IsWarning() const {
	for (const Spear& spear : spears_) {
		if (spear.state == SpearState::Warning) {
			return true;
		}
	}
	return false;
}

bool BossGroundSpearAttack::GetPredictionLightData(Vector3& position, float& radius) const {
	for (const Spear& spear : spears_) {
		if (spear.state == SpearState::Warning) {
			position = spear.predictionTransform.translation_;
			radius = spear.predictionTransform.scale_.x;
			return true;
		}
	}
	return false;
}

bool BossGroundSpearAttack::IsAttacking() const {
	for (const Spear& spear : spears_) {
		if (spear.state == SpearState::Rising || spear.state == SpearState::Active) {
			return true;
		}
	}
	return false;
}

bool BossGroundSpearAttack::IsSequenceFinished() const {
	if (!isActive_ || nextSpearIndex_ < spears_.size()) {
		return false;
	}

	for (const Spear& spear : spears_) {
		if (spear.state == SpearState::Warning || spear.state == SpearState::Rising) {
			return false;
		}
	}
	return true;
}

bool BossGroundSpearAttack::DestroySpearHitByBoss(
    const Vector3& bossPosition, float bossCollisionRadius) {
	if (bossCollisionRadius <= 0.0f) {
		return false;
	}

	const ColliderManager::CircleXZ bossCollider = {bossPosition, bossCollisionRadius};
	for (Spear& spear : spears_) {
		// 地上に残っている槍だけを、突進を止める障害物として扱う。
		if (spear.state != SpearState::Active) {
			continue;
		}
		const ColliderManager::CircleXZ spearCollider = {
		    spear.groundPosition, GetCollisionRadius()};
		if (!ColliderManager::CheckCircleCircleXZ(bossCollider, spearCollider)) {
			continue;
		}

		spear.state = SpearState::Retracting;
		spear.stateTimer = kRetractingFrames;
		spear.hasDealtDamage = true;
		UpdateSpearTransform(spear, 1.0f);
		return true;
	}
	return false;
}

bool BossGroundSpearAttack::BlockLaserWithSpear(
    const Vector3& laserStart,
    const Vector3& laserEnd,
    float laserRadius,
    Vector3& hitPosition) {
	if (laserRadius < 0.0f) {
		return false;
	}

	const ColliderManager::Segment laserSegment = {laserStart, laserEnd};
	const float segmentX = laserEnd.x - laserStart.x;
	const float segmentZ = laserEnd.z - laserStart.z;
	const float segmentLengthSquared = segmentX * segmentX + segmentZ * segmentZ;
	Spear* nearestSpear = nullptr;
	float nearestT = 2.0f;

	for (Spear& spear : spears_) {
		if (spear.state != SpearState::Active) {
			continue;
		}
		const ColliderManager::CircleXZ spearCollider = {
		    spear.groundPosition, GetCollisionRadius() + laserRadius};
		if (!ColliderManager::CheckSegmentCircleXZ(laserSegment, spearCollider)) {
			continue;
		}

		const float toSpearX = spear.groundPosition.x - laserStart.x;
		const float toSpearZ = spear.groundPosition.z - laserStart.z;
		const float t = (segmentLengthSquared > 0.000001f)
		                    ? (toSpearX * segmentX + toSpearZ * segmentZ) / segmentLengthSquared
		                    : 0.0f;
		if (t >= 0.0f && t <= 1.0f && t < nearestT) {
			nearestT = t;
			nearestSpear = &spear;
		}
	}

	if (nearestSpear == nullptr) {
		return false;
	}

	// 槍の中心ではなく、レーザー線上の最近接点を終点にする。
	// これにより、レーザーのモデル軸と終点がずれず、遮断時に揺れない。
	hitPosition = {
	    laserStart.x + segmentX * nearestT,
	    laserStart.y,
	    laserStart.z + segmentZ * nearestT,
	};
	// 必殺技中は槍を残し続け、回転して再び来るレーザーも確実に防ぐ。
	nearestSpear->isLaserShield = true;
	return true;
}

void BossGroundSpearAttack::RetractLaserShieldSpears() {
	for (Spear& spear : spears_) {
		if (!spear.isLaserShield || spear.state != SpearState::Active) {
			continue;
		}
		spear.isLaserShield = false;
		spear.state = SpearState::Retracting;
		spear.stateTimer = kRetractingFrames;
		spear.hasDealtDamage = true;
		UpdateSpearTransform(spear, 1.0f);
	}
}

void BossGroundSpearAttack::BeginWarning(Spear& spear, const Vector3& targetPosition) {
	spear.groundPosition = {targetPosition.x, groundHeight_, targetPosition.z};
	spear.hasDealtDamage = false;
	spear.state = SpearState::Warning;
	spear.stateTimer = kWarningFrames;
	spear.predictionTransform.translation_ = {
	    spear.groundPosition.x,
	    groundHeight_ + kPredictionDisplayHeight,
	    spear.groundPosition.z,
	};
	UpdatePredictionTransform(spear, 0.0f);
	UpdateSpearTransform(spear, 0.0f);
}

void BossGroundSpearAttack::UpdatePredictionTransform(
    Spear& spear, float warningProgress) {
	const float displayRadius = LerpManager::Lerp(
	    GetCollisionRadius() * kPredictionStartScaleRatio,
	    GetCollisionRadius(),
	    warningProgress,
	    LerpManager::EaseType::SmootherStep);
	spear.predictionTransform.scale_ = {displayRadius, 1.0f, displayRadius};
	UpdateWorldTransform(spear.predictionTransform);
	spear.predictionColor.SetColor(
	    {1.0f, 0.10f, 0.01f, 15.0f + std::clamp(warningProgress, 0.0f, 0.999f)});
}

void BossGroundSpearAttack::UpdateSpearTransform(Spear& spear, float riseProgress) {
	// DDAで拡大した判定範囲と見た目を一致させ、横方向だけ段階的に太くする。
	spear.spearTransform.scale_ = {
	    kSpearScale * attackRangeMultiplier_, kSpearScale,
	    kSpearScale * attackRangeMultiplier_};
	spear.spearTransform.translation_ = {
	    spear.groundPosition.x,
	    groundHeight_ + kSpearGroundOffset -
	        kSpearHeight * (1.0f - riseProgress),
	    spear.groundPosition.z,
	};
	UpdateWorldTransform(spear.spearTransform);
}

void BossGroundSpearAttack::CheckPlayerCollision(Spear& spear) {
	if (spear.hasDealtDamage || player_ == nullptr) {
		return;
	}

	const ColliderManager::CircleXZ playerCollider = {
	    player_->GetWorldTransform().translation_,
	    player_->GetCollisionRadius(),
	};
	const ColliderManager::CircleXZ spearCollider = {
	    spear.groundPosition,
	    GetCollisionRadius(),
	};
	if (ColliderManager::CheckCircleCircleXZ(playerCollider, spearCollider)) {
		player_->ApplyDamage(GetDamage());
		player_->ApplyKnockbackAndStun(spear.groundPosition, kKnockbackForce, kStunFrames);
		spear.hasDealtDamage = true;
	}
}

bool BossGroundSpearAttack::AreAllSpearsDormant() const {
	for (const Spear& spear : spears_) {
		if (spear.state != SpearState::Dormant) {
			return false;
		}
	}
	return true;
}
