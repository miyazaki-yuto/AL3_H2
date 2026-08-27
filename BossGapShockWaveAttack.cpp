#include "BossGapShockWaveAttack.h"

#include "ColliderManager.h"
#include "Player.h"
#include "TransformUtility.h"

#include <algorithm>
#include <cmath>
#include <numbers>

using namespace KamataEngine;

void BossGapShockWaveAttack::Initialize(Model* bulletModel, Camera* camera, Player* player) {
	bulletModel_ = bulletModel;
	camera_ = camera;
	player_ = player;
	bulletShotSeHandle_ =
	    Audio::GetInstance()->LoadWave("Audio/Game_PlayerBulletShot_SE.wav");
	bulletHitSeHandle_ =
	    Audio::GetInstance()->LoadWave("Audio/Game_PlayerBulletHit_SE.wav");
	impactSeCooldown_ = 0;
	bullets_.clear();
	isActive_ = false;
}

void BossGapShockWaveAttack::Start(const Vector3& bossPosition, const Vector3& playerPosition) {
	if (bulletModel_ == nullptr || camera_ == nullptr || player_ == nullptr) {
		return;
	}

	bullets_.clear();
	origin_ = bossPosition;
	gapCenterAngle_ = std::atan2(
	    playerPosition.x - bossPosition.x,
	    playerPosition.z - bossPosition.z);
	gapRotationDirection_ = (attackSequence_ % 2 == 0) ? 1.0f : -1.0f;
	++attackSequence_;
	spawnedWaveCount_ = 0;
	waveTimer_ = 0;
	isActive_ = true;

	SpawnWave();
}

void BossGapShockWaveAttack::SpawnWave() {
	const float bulletRadius = GetBulletRadius();
	if (!isActive_ || spawnedWaveCount_ >= GetWaveCount()) {
		return;
	}

	constexpr float kTwoPi = std::numbers::pi_v<float> * 2.0f;
	constexpr float kGapHalfAngle =
	    (kGapAngleDegrees * std::numbers::pi_v<float> / 180.0f) * 0.5f;
	constexpr float kGapMoveAngle =
	    kGapMoveDegreesPerWave * std::numbers::pi_v<float> / 180.0f;

	// 各ウェーブ時点のPlayer方向を基準に隙間を移動させる。
	const Vector3& playerPosition = player_->GetWorldTransform().translation_;
	const float playerDirection = std::atan2(
	    playerPosition.x - origin_.x,
	    playerPosition.z - origin_.z);
	gapCenterAngle_ = playerDirection +
	                  kGapMoveAngle * static_cast<float>(spawnedWaveCount_) *
	                      gapRotationDirection_;

	for (int i = 0; i < kBulletCountPerWave; ++i) {
		const float angle = kTwoPi * static_cast<float>(i) /
		                    static_cast<float>(kBulletCountPerWave);
		const float differenceFromGap = std::atan2(
		    std::sin(angle - gapCenterAngle_),
		    std::cos(angle - gapCenterAngle_));
		if (std::abs(differenceFromGap) <= kGapHalfAngle) {
			continue;
		}

		auto bullet = std::make_unique<Bullet>();
		bullet->worldTransform.Initialize();
		bullet->worldTransform.scale_ = {bulletRadius, bulletRadius, bulletRadius};
		bullet->worldTransform.translation_ = {origin_.x, origin_.y + bulletRadius, origin_.z};
		bullet->velocity = {
		    std::sin(angle) * kBulletSpeed,
		    0.0f,
		    std::cos(angle) * kBulletSpeed,
		};
		bullet->worldTransform.rotation_.y =
		    std::atan2(bullet->velocity.x, bullet->velocity.z) +
		    kBulletModelForwardYawOffset;
		bullet->remainingLife = kBulletLifeFrames;
		UpdateWorldTransform(bullet->worldTransform);
		bullets_.push_back(std::move(bullet));
	}

	++spawnedWaveCount_;
	waveTimer_ = kWaveIntervalFrames;
	// 円形弾幕は弾ごとではなく、1ウェーブにつき発射音を1回だけ鳴らす。
	Audio::GetInstance()->PlayWave(bulletShotSeHandle_, false, 0.58f);
}

void BossGapShockWaveAttack::Update() {
	if (!isActive_) {
		return;
	}
	if (impactSeCooldown_ > 0) --impactSeCooldown_;

	if (spawnedWaveCount_ < GetWaveCount()) {
		--waveTimer_;
		if (waveTimer_ <= 0) {
			SpawnWave();
		}
	}

	const ColliderManager::CircleXZ playerCollider = {
	    player_->GetWorldTransform().translation_,
	    player_->GetCollisionRadius(),
	};

	const float bulletRadius = GetBulletRadius();
	for (auto& bullet : bullets_) {
		bullet->worldTransform.translation_.x += bullet->velocity.x;
		bullet->worldTransform.translation_.y += bullet->velocity.y;
		bullet->worldTransform.translation_.z += bullet->velocity.z;
		bullet->worldTransform.rotation_.y =
		    std::atan2(bullet->velocity.x, bullet->velocity.z) +
		    kBulletModelForwardYawOffset;
		if (arenaRadius_ > bulletRadius) {
			const float dx = bullet->worldTransform.translation_.x - arenaCenter_.x;
			const float dz = bullet->worldTransform.translation_.z - arenaCenter_.z;
			const float allowedRadius = arenaRadius_ - bulletRadius;
			if (dx * dx + dz * dz > allowedRadius * allowedRadius) {
				// ボス弾は壁で消滅させ、壁の外へは出さない。
				bullet->isDead = true;
				if (impactSeCooldown_ <= 0) {
					Audio::GetInstance()->PlayWave(bulletHitSeHandle_, false, 0.58f);
					impactSeCooldown_ = kImpactSeIntervalFrames;
				}
				continue;
			}
		}
		UpdateWorldTransform(bullet->worldTransform);

		const ColliderManager::CircleXZ bulletCollider = {
		    bullet->worldTransform.translation_,
		    bulletRadius,
		};
		if (!bullet->isReflectedByPlayer &&
		    ColliderManager::CheckCircleCircleXZ(playerCollider, bulletCollider)) {
			player_->ApplyDamage(GetDamage());
			bullet->isDead = true;
			if (impactSeCooldown_ <= 0) {
				Audio::GetInstance()->PlayWave(bulletHitSeHandle_, false, 0.64f);
				impactSeCooldown_ = kImpactSeIntervalFrames;
			}
		}

		--bullet->remainingLife;
		if (bullet->remainingLife <= 0) {
			bullet->isDead = true;
		}
	}

	std::erase_if(
	    bullets_,
	    [](const std::unique_ptr<Bullet>& bullet) {
		    return bullet->isDead;
	    });

	if (spawnedWaveCount_ >= GetWaveCount() && bullets_.empty()) {
		isActive_ = false;
	}
}

int BossGapShockWaveAttack::ReflectBulletsInCircle(const Vector3& center, float radius) {
	int reflectedCount = 0;
	const float bulletRadius = GetBulletRadius();
	for (const auto& bullet : bullets_) {
		if (bullet->isDead || !ColliderManager::CheckCircleCircleXZ(
		                           {center, radius},
		                           {bullet->worldTransform.translation_, bulletRadius})) {
			continue;
		}

		// 設置反翔では接触法線を使わず、その場で進行方向を完全に反転する。
		// 位置を範囲外へ押し出さないため、弾が瞬間移動して見えることもない。
		bullet->velocity.x = -bullet->velocity.x;
		bullet->velocity.y = -bullet->velocity.y;
		bullet->velocity.z = -bullet->velocity.z;
		bullet->worldTransform.rotation_.y =
		    std::atan2(bullet->velocity.x, bullet->velocity.z) +
		    kBulletModelForwardYawOffset;
		bullet->isReflectedByPlayer = true;
		UpdateWorldTransform(bullet->worldTransform);
		++reflectedCount;
	}
	return reflectedCount;
}

int BossGapShockWaveAttack::ReflectBulletsTowardTargetInCircle(
    const Vector3& center, float radius, const Vector3& target) {
	int reflectedCount = 0;
	const float bulletRadius = GetBulletRadius();
	for (const auto& bullet : bullets_) {
		if (bullet->isDead || !ColliderManager::CheckCircleCircleXZ(
		                           {center, radius},
		                           {bullet->worldTransform.translation_, bulletRadius})) {
			continue;
		}

		float directionX = target.x - bullet->worldTransform.translation_.x;
		float directionZ = target.z - bullet->worldTransform.translation_.z;
		const float directionLength = std::sqrt(directionX * directionX + directionZ * directionZ);
		if (directionLength <= 0.0001f) {
			continue;
		}
		directionX /= directionLength;
		directionZ /= directionLength;
		const float currentSpeed = std::sqrt(
		    bullet->velocity.x * bullet->velocity.x + bullet->velocity.z * bullet->velocity.z);
		const float speed = currentSpeed > 0.0001f ? currentSpeed : kBulletSpeed;
		bullet->velocity = {directionX * speed, 0.0f, directionZ * speed};
		bullet->worldTransform.rotation_.y =
		    std::atan2(bullet->velocity.x, bullet->velocity.z) +
		    kBulletModelForwardYawOffset;
		UpdateWorldTransform(bullet->worldTransform);
		bullet->isReflectedByPlayer = true;
		++reflectedCount;
	}
	return reflectedCount;
}

int BossGapShockWaveAttack::ConsumeReflectedBulletDamageInCircle(
    const Vector3& center, float radius) {
	int totalDamage = 0;
	const float bulletRadius = GetBulletRadius();
	for (const auto& bullet : bullets_) {
		if (bullet->isDead || !bullet->isReflectedByPlayer) {
			continue;
		}
		if (!ColliderManager::CheckCircleCircleXZ(
		        {center, radius},
		        {bullet->worldTransform.translation_, bulletRadius})) {
			continue;
		}
		bullet->isDead = true;
		totalDamage += GetDamage();
		if (impactSeCooldown_ <= 0) {
			Audio::GetInstance()->PlayWave(bulletHitSeHandle_, false, 0.72f);
			impactSeCooldown_ = kImpactSeIntervalFrames;
		}
	}
	std::erase_if(
	    bullets_,
	    [](const std::unique_ptr<Bullet>& bullet) { return bullet->isDead; });
	return totalDamage;
}

void BossGapShockWaveAttack::Draw() {
	if (bulletModel_ == nullptr || camera_ == nullptr) {
		return;
	}

	for (const auto& bullet : bullets_) {
		bulletModel_->Draw(bullet->worldTransform, *camera_);
	}
}
