#include "BossSummonMobs.h"

#include "ColliderManager.h"
#include "LerpManager.h"
#include "Player.h"
#include "TransformUtility.h"

#include <algorithm>
#include <cmath>
#include <numbers>

using namespace KamataEngine;

void BossSummonMobs::Initialize(
    Model* mobModel,
    Model* bulletModel,
    Model* predictionCircleModel,
    Camera* camera,
    Player* player) {
	mobModel_ = mobModel;
	bulletModel_ = bulletModel;
	predictionCircleModel_ = predictionCircleModel;
	camera_ = camera;
	player_ = player;
	bulletShotSeHandle_ =
	    Audio::GetInstance()->LoadWave("Audio/Game_PlayerBulletShot_SE.wav");
	bulletHitSeHandle_ =
	    Audio::GetInstance()->LoadWave("Audio/Game_PlayerBulletHit_SE.wav");
	summonStartSeHandle_ =
	    Audio::GetInstance()->LoadWave("Audio/Game_BossSummonStart.wav");
	impactSeCooldown_ = 0;
	predictionColor_.Initialize();
	predictionColor_.SetColor({1.0f, 0.8f, 0.1f, 0.75f});
	mobColor_.Initialize();
	// BossHeadモデルを使う召喚Mobだけを、発光感のあるビビッド紫にする。
	mobColor_.SetColor({0.92f, 0.04f, 1.35f, 1.0f});
	mobs_.clear();
	bullets_.clear();
	remainingFrames_ = 0;
	warningTimer_ = 0;
	globalShootCooldown_ = 0;
	survivorCountOnTimeout_ = 0;
	isActive_ = false;
	isWarning_ = false;
}

void BossSummonMobs::Start(const Vector3& bossPosition) {
	if (
	    mobModel_ == nullptr || bulletModel_ == nullptr ||
	    predictionCircleModel_ == nullptr || camera_ == nullptr ||
	    player_ == nullptr) {
		return;
	}

	mobs_.clear();
	bullets_.clear();
	mobs_.reserve(kMobCount);
	constexpr float kTwoPi = std::numbers::pi_v<float> * 2.0f;
	for (size_t i = 0; i < kMobCount; ++i) {
		const float angle =
		    kTwoPi * static_cast<float>(i) / static_cast<float>(kMobCount);
		const float spawnRadius = (i % 2 == 0) ? kInnerSpawnRadius : kOuterSpawnRadius;

		auto mob = std::make_unique<Mob>();
		mob->worldTransform.Initialize();
		mob->predictionTransform.Initialize();
		mob->worldTransform.scale_ = {
		    kMobVisualScale, kMobVisualScale, kMobVisualScale};
		mob->worldTransform.rotation_.y = kMobModelForwardYawOffset;
		mob->worldTransform.translation_ = {
		    bossPosition.x + std::sin(angle) * spawnRadius,
		    bossPosition.y,
		    bossPosition.z + std::cos(angle) * spawnRadius,
		};
		ConstrainMobInsideArena(*mob);
		mob->predictionTransform.translation_ = {
		    mob->worldTransform.translation_.x,
		    bossPosition.y + kPredictionDisplayHeight,
		    mob->worldTransform.translation_.z,
		};
		mob->hp = kMobHP;
		mob->shootCooldown =
		    static_cast<int>(i + 1) * kInitialShootDelayStepFrames;
		mob->strafeDirection = (i % 2 == 0) ? 1.0f : -1.0f;
		UpdateWorldTransform(mob->worldTransform);
		mobs_.push_back(std::move(mob));
	}

	remainingFrames_ = 0;
	warningTimer_ = kWarningFrames;
	globalShootCooldown_ = kGlobalShootIntervalFrames;
	survivorCountOnTimeout_ = 0;
	isActive_ = true;
	isWarning_ = true;
	UpdatePredictionTransforms(0.0f);
	Audio::GetInstance()->PlayWave(summonStartSeHandle_, false, 0.80f);
}

void BossSummonMobs::Update() {
	if (!isActive_ || player_ == nullptr) {
		return;
	}
	if (isWarning_) {
		if (warningTimer_ <= 0) {
			isWarning_ = false;
			remainingFrames_ = kTimeLimitFrames;
		} else {
			const int elapsedFrames = kWarningFrames - warningTimer_;
			const float warningProgress =
			    static_cast<float>(elapsedFrames) /
			    static_cast<float>(kWarningFrames - 1);
			UpdatePredictionTransforms(warningProgress);
			--warningTimer_;
			return;
		}
	}

	const Vector3& playerPosition = player_->GetWorldTransform().translation_;
	const ColliderManager::CircleXZ playerCollider = {
	    playerPosition,
	    player_->GetCollisionRadius(),
	};
	// Playerが巨大化した分だけMobの待機円を外側へ広げる。
	// 少し余裕を持たせ、見た目の外周とMobが触れ続けないようにする。
	const float playerGrowthDistance =
	    (std::max)(0.0f, player_->GetCollisionRadius() - kBasePlayerCollisionRadius) *
	    kPlayerGrowthDistanceScale;
	const float preferredDistance = kPreferredDistance + playerGrowthDistance;
	const float shootRange =
	    kShootRange * attackRangeMultiplier_ + playerGrowthDistance;
	if (globalShootCooldown_ > 0) {
		--globalShootCooldown_;
	}
	size_t activeHostileBulletCount = static_cast<size_t>(std::count_if(
	    bullets_.begin(), bullets_.end(),
	    [](const std::unique_ptr<MobBullet>& bullet) {
		    return !bullet->isDead && !bullet->isReflectedByPlayer;
	    }));

	for (auto& mob : mobs_) {
		if (mob->isDead) {
			continue;
		}

		const float differenceX = playerPosition.x - mob->worldTransform.translation_.x;
		const float differenceZ = playerPosition.z - mob->worldTransform.translation_.z;
		const float distance = std::sqrt(differenceX * differenceX + differenceZ * differenceZ);
		if (distance > 0.0001f) {
			const float directionX = differenceX / distance;
			const float directionZ = differenceZ / distance;
			if (distance > preferredDistance + kDistanceTolerance) {
				mob->worldTransform.translation_.x += directionX * kMoveSpeed;
				mob->worldTransform.translation_.z += directionZ * kMoveSpeed;
			} else if (distance < preferredDistance - kDistanceTolerance) {
				mob->worldTransform.translation_.x -= directionX * kMoveSpeed;
				mob->worldTransform.translation_.z -= directionZ * kMoveSpeed;
			} else {
				mob->worldTransform.translation_.x +=
				    directionZ * mob->strafeDirection * kStrafeSpeed;
				mob->worldTransform.translation_.z -=
				    directionX * mob->strafeDirection * kStrafeSpeed;
			}

			const float targetRotationY =
			    std::atan2(directionX, directionZ) + kMobModelForwardYawOffset;
			const float rotationDifference = std::atan2(
			    std::sin(targetRotationY - mob->worldTransform.rotation_.y),
			    std::cos(targetRotationY - mob->worldTransform.rotation_.y));
			mob->worldTransform.rotation_.y += rotationDifference * kRotationInterpolation;

			if (mob->shootCooldown > 0) {
				--mob->shootCooldown;
			}
			if (
			    mob->shootCooldown <= 0 &&
			    distance <= shootRange &&
			    globalShootCooldown_ <= 0 &&
			    activeHostileBulletCount < kMaxActiveHostileBullets) {
				SpawnBullet(*mob, {directionX, 0.0f, directionZ});
				mob->shootCooldown = kShootCooldownFrames;
				globalShootCooldown_ = kGlobalShootIntervalFrames;
				++activeHostileBulletCount;
			}
		}
	}

	ResolveMobCollisions();
	ResolveMobPlayerCollisions();

	for (auto& mob : mobs_) {
		if (mob->isDead) {
			continue;
		}

		ConstrainMobInsideArena(*mob);
		UpdateWorldTransform(mob->worldTransform);
		const ColliderManager::CircleXZ mobCollider = {
		    mob->worldTransform.translation_,
		    kCollisionRadius,
		};
		if (ColliderManager::CheckCircleCircleXZ(playerCollider, mobCollider)) {
			player_->ApplyDamage(kContactDamage, false);
		}
	}
	UpdateBullets();

	std::erase_if(
	    mobs_,
	    [](const std::unique_ptr<Mob>& mob) { return mob->isDead; });
	if (mobs_.empty()) {
		Finish(false);
		return;
	}

	--remainingFrames_;
	if (remainingFrames_ <= 0) {
		Finish(true);
	}
}

bool BossSummonMobs::GetPredictionLightData(Vector3& position, float& radius) const {
	if (!IsWarning() || mobs_.empty()) {
		return false;
	}
	position = {};
	for (const auto& mob : mobs_) {
		position.x += mob->predictionTransform.translation_.x;
		position.y += mob->predictionTransform.translation_.y;
		position.z += mob->predictionTransform.translation_.z;
	}
	const float inverseCount = 1.0f / static_cast<float>(mobs_.size());
	position.x *= inverseCount;
	position.y *= inverseCount;
	position.z *= inverseCount;
	radius = 0.0f;
	for (const auto& mob : mobs_) {
		const float differenceX = mob->predictionTransform.translation_.x - position.x;
		const float differenceZ = mob->predictionTransform.translation_.z - position.z;
		const float distance = std::sqrt(differenceX * differenceX + differenceZ * differenceZ);
		radius = (std::max)(
		    radius, distance + mob->predictionTransform.scale_.x);
	}
	return true;
}

void BossSummonMobs::SpawnBullet(Mob& mob, const Vector3& direction) {
	auto bullet = std::make_unique<MobBullet>();
	bullet->worldTransform.Initialize();
	bullet->worldTransform.scale_ = {kBulletRadius, kBulletRadius, kBulletRadius};
	const float spawnOffset = kCollisionRadius + kBulletRadius + 0.1f;
	bullet->worldTransform.translation_ = {
	    mob.worldTransform.translation_.x + direction.x * spawnOffset,
	    mob.worldTransform.translation_.y + kBulletRadius,
	    mob.worldTransform.translation_.z + direction.z * spawnOffset,
	};
	bullet->velocity = {
	    direction.x * kBulletSpeed,
	    0.0f,
	    direction.z * kBulletSpeed,
	};
	bullet->worldTransform.rotation_.y =
	    std::atan2(bullet->velocity.x, bullet->velocity.z) +
	    kBulletModelForwardYawOffset;
	bullet->remainingFrames = kBulletLifeFrames;
	UpdateWorldTransform(bullet->worldTransform);
	bullets_.push_back(std::move(bullet));
	Audio::GetInstance()->PlayWave(bulletShotSeHandle_, false, 0.58f);
}

void BossSummonMobs::UpdateBullets() {
	if (player_ == nullptr) {
		return;
	}
	if (impactSeCooldown_ > 0) --impactSeCooldown_;
	const Vector3& playerPosition = player_->GetWorldTransform().translation_;
	const ColliderManager::CircleXZ playerCollider = {
	    playerPosition,
	    player_->GetCollisionRadius(),
	};
	const float allowedRadius = arenaRadius_ - kBulletRadius;

	for (auto& bullet : bullets_) {
		if (bullet->isDead) {
			continue;
		}
		bullet->worldTransform.translation_.x += bullet->velocity.x;
		bullet->worldTransform.translation_.z += bullet->velocity.z;
		bullet->worldTransform.rotation_.y =
		    std::atan2(bullet->velocity.x, bullet->velocity.z) +
		    kBulletModelForwardYawOffset;

		const float arenaX = bullet->worldTransform.translation_.x - arenaCenter_.x;
		const float arenaZ = bullet->worldTransform.translation_.z - arenaCenter_.z;
		if (allowedRadius > 0.0f &&
		    arenaX * arenaX + arenaZ * arenaZ > allowedRadius * allowedRadius) {
			bullet->isDead = true;
			if (impactSeCooldown_ <= 0) {
				Audio::GetInstance()->PlayWave(bulletHitSeHandle_, false, 0.58f);
				impactSeCooldown_ = kImpactSeIntervalFrames;
			}
		}

		if (!bullet->isDead && !bullet->isReflectedByPlayer) {
			const ColliderManager::CircleXZ bulletCollider = {
			    bullet->worldTransform.translation_,
			    kBulletRadius,
			};
			if (ColliderManager::CheckCircleCircleXZ(playerCollider, bulletCollider)) {
				player_->ApplyDamage(kBulletDamage, false);
				bullet->isDead = true;
				if (impactSeCooldown_ <= 0) {
					Audio::GetInstance()->PlayWave(bulletHitSeHandle_, false, 0.64f);
					impactSeCooldown_ = kImpactSeIntervalFrames;
				}
			}
		}

		--bullet->remainingFrames;
		if (bullet->remainingFrames <= 0) {
			bullet->isDead = true;
		}
		UpdateWorldTransform(bullet->worldTransform);
	}
	std::erase_if(
	    bullets_,
	    [](const std::unique_ptr<MobBullet>& bullet) { return bullet->isDead; });
}

void BossSummonMobs::ConstrainMobInsideArena(Mob& mob) const {
	const float allowedRadius = arenaRadius_ - kCollisionRadius;
	if (allowedRadius <= 0.0f) return;
	float dx = mob.worldTransform.translation_.x - arenaCenter_.x;
	float dz = mob.worldTransform.translation_.z - arenaCenter_.z;
	const float distanceSquared = dx * dx + dz * dz;
	if (distanceSquared <= allowedRadius * allowedRadius) return;
	const float distance = std::sqrt(distanceSquared);
	if (distance <= 0.0001f) return;
	mob.worldTransform.translation_.x = arenaCenter_.x + dx / distance * allowedRadius;
	mob.worldTransform.translation_.z = arenaCenter_.z + dz / distance * allowedRadius;
}

void BossSummonMobs::UpdatePredictionTransforms(float warningProgress) {
	const float displayRadius = LerpManager::Lerp(
	    kCollisionRadius * kPredictionStartScaleRatio,
	    kCollisionRadius,
	    warningProgress,
	    LerpManager::EaseType::SmootherStep);
	for (auto& mob : mobs_) {
		mob->predictionTransform.scale_ = {displayRadius, 1.0f, displayRadius};
		UpdateWorldTransform(mob->predictionTransform);
	}
}

void BossSummonMobs::ResolveMobCollisions() {
	constexpr float kMinimumDistanceSquared = 0.000001f;
	const float minimumDistance = kCollisionRadius * 2.0f;
	const float minimumDistanceSquared = minimumDistance * minimumDistance;

	for (int iteration = 0; iteration < kCollisionResolveIterations; ++iteration) {
		for (size_t i = 0; i < mobs_.size(); ++i) {
			if (mobs_[i]->isDead) {
				continue;
			}

			for (size_t j = i + 1; j < mobs_.size(); ++j) {
				if (mobs_[j]->isDead) {
					continue;
				}

				Vector3& firstPosition = mobs_[i]->worldTransform.translation_;
				Vector3& secondPosition = mobs_[j]->worldTransform.translation_;
				float differenceX = secondPosition.x - firstPosition.x;
				float differenceZ = secondPosition.z - firstPosition.z;
				float distanceSquared =
				    differenceX * differenceX + differenceZ * differenceZ;
				if (distanceSquared >= minimumDistanceSquared) {
					continue;
				}

				if (distanceSquared <= kMinimumDistanceSquared) {
					// 完全に同じ座標でも、添字に基づく一定方向へ安全に分離する。
					const float angle = static_cast<float>(i + j) * 2.39996323f;
					differenceX = std::sin(angle);
					differenceZ = std::cos(angle);
					distanceSquared = 1.0f;
				}

				const float distance = std::sqrt(distanceSquared);
				const float pushDistance = (minimumDistance - distance) * 0.5f;
				const float normalX = differenceX / distance;
				const float normalZ = differenceZ / distance;
				firstPosition.x -= normalX * pushDistance;
				firstPosition.z -= normalZ * pushDistance;
				secondPosition.x += normalX * pushDistance;
				secondPosition.z += normalZ * pushDistance;
			}
		}
	}
}

void BossSummonMobs::ResolveMobPlayerCollisions() {
	if (player_ == nullptr) {
		return;
	}

	constexpr float kMinimumDistanceSquared = 0.000001f;
	const Vector3& playerPosition = player_->GetWorldTransform().translation_;
	const float minimumDistance =
	    player_->GetCollisionRadius() + kCollisionRadius + kPlayerCollisionSeparation;
	const float minimumDistanceSquared = minimumDistance * minimumDistance;

	for (size_t index = 0; index < mobs_.size(); ++index) {
		auto& mob = mobs_[index];
		if (mob->isDead) {
			continue;
		}

		Vector3& mobPosition = mob->worldTransform.translation_;
		float differenceX = mobPosition.x - playerPosition.x;
		float differenceZ = mobPosition.z - playerPosition.z;
		float distanceSquared = differenceX * differenceX + differenceZ * differenceZ;
		if (distanceSquared >= minimumDistanceSquared) {
			continue;
		}
		if (distanceSquared <= kMinimumDistanceSquared) {
			const float angle = static_cast<float>(index) * 2.39996323f;
			differenceX = std::sin(angle);
			differenceZ = std::cos(angle);
			distanceSquared = 1.0f;
		}

		const float distance = std::sqrt(distanceSquared);
		mobPosition.x = playerPosition.x + differenceX / distance * minimumDistance;
		mobPosition.z = playerPosition.z + differenceZ / distance * minimumDistance;
	}
}

void BossSummonMobs::Draw() {
	if (!isActive_ || camera_ == nullptr) {
		return;
	}

	for (const auto& mob : mobs_) {
		if (mob->isDead) {
			continue;
		}
		if (isWarning_ && predictionCircleModel_ != nullptr) {
			predictionCircleModel_->Draw(
			    mob->predictionTransform, *camera_, &predictionColor_);
		} else if (mobModel_ != nullptr) {
			mobModel_->Draw(mob->worldTransform, *camera_, &mobColor_);
		}
	}
	if (!isWarning_ && bulletModel_ != nullptr) {
		for (const auto& bullet : bullets_) {
			if (!bullet->isDead) {
				bulletModel_->Draw(bullet->worldTransform, *camera_);
			}
		}
	}
}

int BossSummonMobs::ReflectBulletsInCircle(const Vector3& center, float radius) {
	if (!IsSummoning() || radius <= 0.0f) {
		return 0;
	}
	int reflectedCount = 0;
	for (auto& bullet : bullets_) {
		if (bullet->isDead || bullet->isReflectedByPlayer) {
			continue;
		}
		float differenceX = bullet->worldTransform.translation_.x - center.x;
		float differenceZ = bullet->worldTransform.translation_.z - center.z;
		const float distanceSquared = differenceX * differenceX + differenceZ * differenceZ;
		const float hitDistance = radius + kBulletRadius;
		if (distanceSquared > hitDistance * hitDistance) {
			continue;
		}
		float distance = std::sqrt(distanceSquared);
		if (distance <= 0.0001f) {
			differenceX = -bullet->velocity.x;
			differenceZ = -bullet->velocity.z;
			distance = std::sqrt(differenceX * differenceX + differenceZ * differenceZ);
		}
		if (distance <= 0.0001f) {
			differenceX = 0.0f;
			differenceZ = 1.0f;
			distance = 1.0f;
		}
		const float normalX = differenceX / distance;
		const float normalZ = differenceZ / distance;
		const float dot = bullet->velocity.x * normalX + bullet->velocity.z * normalZ;
		bullet->velocity.x -= 2.0f * dot * normalX;
		bullet->velocity.z -= 2.0f * dot * normalZ;
		bullet->worldTransform.rotation_.y =
		    std::atan2(bullet->velocity.x, bullet->velocity.z) +
		    kBulletModelForwardYawOffset;
		bullet->worldTransform.translation_.x = center.x + normalX * (hitDistance + 0.1f);
		bullet->worldTransform.translation_.z = center.z + normalZ * (hitDistance + 0.1f);
		bullet->isReflectedByPlayer = true;
		UpdateWorldTransform(bullet->worldTransform);
		++reflectedCount;
	}
	return reflectedCount;
}

int BossSummonMobs::ReflectBulletsTowardTargetInCircle(
    const Vector3& center, float radius, const Vector3& target) {
	if (!IsSummoning() || radius <= 0.0f) {
		return 0;
	}
	int reflectedCount = 0;
	for (auto& bullet : bullets_) {
		if (bullet->isDead || bullet->isReflectedByPlayer) {
			continue;
		}
		const float centerX = bullet->worldTransform.translation_.x - center.x;
		const float centerZ = bullet->worldTransform.translation_.z - center.z;
		const float hitDistance = radius + kBulletRadius;
		if (centerX * centerX + centerZ * centerZ > hitDistance * hitDistance) {
			continue;
		}
		const float targetX = target.x - bullet->worldTransform.translation_.x;
		const float targetZ = target.z - bullet->worldTransform.translation_.z;
		const float targetDistance = std::sqrt(targetX * targetX + targetZ * targetZ);
		if (targetDistance <= 0.0001f) {
			continue;
		}
		const float speed = std::sqrt(
		    bullet->velocity.x * bullet->velocity.x +
		    bullet->velocity.z * bullet->velocity.z);
		bullet->velocity.x = targetX / targetDistance * speed;
		bullet->velocity.z = targetZ / targetDistance * speed;
		bullet->worldTransform.rotation_.y =
		    std::atan2(bullet->velocity.x, bullet->velocity.z) +
		    kBulletModelForwardYawOffset;
		UpdateWorldTransform(bullet->worldTransform);
		bullet->isReflectedByPlayer = true;
		++reflectedCount;
	}
	return reflectedCount;
}

int BossSummonMobs::ConsumeReflectedBulletDamageInCircle(
    const Vector3& center, float radius) {
	int totalDamage = 0;
	for (const auto& bullet : bullets_) {
		if (bullet->isDead || !bullet->isReflectedByPlayer) {
			continue;
		}
		if (!ColliderManager::CheckCircleCircleXZ(
		        {center, radius},
		        {bullet->worldTransform.translation_, kBulletRadius})) {
			continue;
		}
		bullet->isDead = true;
		totalDamage += kBulletDamage;
		if (impactSeCooldown_ <= 0) {
			Audio::GetInstance()->PlayWave(bulletHitSeHandle_, false, 0.72f);
			impactSeCooldown_ = kImpactSeIntervalFrames;
		}
	}
	std::erase_if(
	    bullets_,
	    [](const std::unique_ptr<MobBullet>& bullet) { return bullet->isDead; });
	return totalDamage;
}

bool BossSummonMobs::DamageMobsInCircle(
    const Vector3& center, float radius, int damage, Vector3* nearestMobPosition) {
	if (!IsSummoning() || radius <= 0.0f || damage <= 0) {
		return false;
	}

	bool hitAnyMob = false;
	bool hasNearestMobPosition = false;
	float nearestDistanceSquared = 0.0f;
	const ColliderManager::CircleXZ attackCollider = {center, radius};
	for (auto& mob : mobs_) {
		if (mob->isDead) {
			continue;
		}
		const ColliderManager::CircleXZ mobCollider = {
		    mob->worldTransform.translation_,
		    kCollisionRadius,
		};
		if (ColliderManager::CheckCircleCircleXZ(attackCollider, mobCollider)) {
			hitAnyMob = true;
			const float differenceX = mob->worldTransform.translation_.x - center.x;
			const float differenceZ = mob->worldTransform.translation_.z - center.z;
			const float distanceSquared =
			    differenceX * differenceX + differenceZ * differenceZ;
			if (nearestMobPosition != nullptr &&
			    (!hasNearestMobPosition || distanceSquared < nearestDistanceSquared)) {
				hasNearestMobPosition = true;
				nearestDistanceSquared = distanceSquared;
				*nearestMobPosition = mob->worldTransform.translation_;
			}
			mob->hp -= damage;
			if (mob->hp <= 0) {
				mob->isDead = true;
			}
		}
	}
	return hitAnyMob;
}

bool BossSummonMobs::DamageMobsAlongSegment(
    const ColliderManager::Segment& segment, int damage, Vector3* nearestHitPosition,
    float collisionPadding) {
	if (!IsSummoning() || damage <= 0) {
		return false;
	}

	const float segmentX = segment.end.x - segment.start.x;
	const float segmentY = segment.end.y - segment.start.y;
	const float segmentZ = segment.end.z - segment.start.z;
	const float segmentLengthSquared = segmentX * segmentX + segmentZ * segmentZ;
	if (segmentLengthSquared <= 0.0001f) {
		return false;
	}
	const float segmentLength = std::sqrt(segmentLengthSquared);
	Mob* nearestMob = nullptr;
	float nearestEntryT = 1.0f;
	const float hitRadius = kCollisionRadius + (std::max)(0.0f, collisionPadding);
	for (auto& mob : mobs_) {
		if (mob->isDead || !ColliderManager::CheckSegmentCircleXZ(
		                       segment, {mob->worldTransform.translation_, hitRadius})) {
			continue;
		}

		const float toMobX = mob->worldTransform.translation_.x - segment.start.x;
		const float toMobZ = mob->worldTransform.translation_.z - segment.start.z;
		const float centerT = std::clamp(
		    (toMobX * segmentX + toMobZ * segmentZ) / segmentLengthSquared,
		    0.0f, 1.0f);
		const float closestX = segment.start.x + segmentX * centerT;
		const float closestZ = segment.start.z + segmentZ * centerT;
		const float offsetX = mob->worldTransform.translation_.x - closestX;
		const float offsetZ = mob->worldTransform.translation_.z - closestZ;
		const float perpendicularSquared = offsetX * offsetX + offsetZ * offsetZ;
		const float entryOffset = std::sqrt((std::max)(
		    0.0f, hitRadius * hitRadius - perpendicularSquared)) /
		    segmentLength;
		const float entryT = (std::max)(0.0f, centerT - entryOffset);
		if (nearestMob == nullptr || entryT < nearestEntryT) {
			nearestMob = mob.get();
			nearestEntryT = entryT;
		}
	}
	if (nearestMob == nullptr) {
		return false;
	}

	nearestMob->hp -= damage;
	if (nearestMob->hp <= 0) {
		nearestMob->isDead = true;
	}
	if (nearestHitPosition != nullptr) {
		*nearestHitPosition = {
		    segment.start.x + segmentX * nearestEntryT,
		    segment.start.y + segmentY * nearestEntryT,
		    segment.start.z + segmentZ * nearestEntryT,
		};
	}
	return true;
}

void BossSummonMobs::Finish(bool timedOut) {
	survivorCountOnTimeout_ = timedOut ? static_cast<int>(mobs_.size()) : 0;
	mobs_.clear();
	bullets_.clear();
	remainingFrames_ = 0;
	warningTimer_ = 0;
	globalShootCooldown_ = 0;
	isActive_ = false;
	isWarning_ = false;
}
