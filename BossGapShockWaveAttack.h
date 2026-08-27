#pragma once

#include "KamataEngine.h"
#include <algorithm>
#include <memory>
#include <vector>

class Player;

class BossGapShockWaveAttack final {
public:
	BossGapShockWaveAttack() = default;
	~BossGapShockWaveAttack() = default;

	void Initialize(KamataEngine::Model* bulletModel, KamataEngine::Camera* camera, Player* player);
	void Start(const KamataEngine::Vector3& bossPosition, const KamataEngine::Vector3& playerPosition);
	void Update();
	void Draw();
	void SetAttackRangeMultiplier(float multiplier) { attackRangeMultiplier_ = multiplier; }
	void SetDamageMultiplier(float multiplier) { damageMultiplier_ = multiplier; }
	void SetBossPhase(int phase) { bossPhase_ = std::clamp(phase, 1, 3); }
	void SetArenaBounds(const KamataEngine::Vector3& center, float radius) {
		arenaCenter_ = center;
		arenaRadius_ = radius;
	}
	int ReflectBulletsInCircle(const KamataEngine::Vector3& center, float radius);
	int ReflectBulletsTowardTargetInCircle(
	    const KamataEngine::Vector3& center, float radius,
	    const KamataEngine::Vector3& target);
	int ConsumeReflectedBulletDamageInCircle(
	    const KamataEngine::Vector3& center, float radius);

	bool IsActive() const { return isActive_; }
	size_t GetBulletCount() const { return bullets_.size(); }

private:
	struct Bullet {
		KamataEngine::WorldTransform worldTransform;
		KamataEngine::Vector3 velocity = {};
		int remainingLife = 0;
		bool isDead = false;
		bool isReflectedByPlayer = false;
	};

	void SpawnWave();

	KamataEngine::Model* bulletModel_ = nullptr;
	KamataEngine::Camera* camera_ = nullptr;
	Player* player_ = nullptr;
	std::vector<std::unique_ptr<Bullet>> bullets_;

	KamataEngine::Vector3 origin_ = {};
	float gapCenterAngle_ = 0.0f;
	float gapRotationDirection_ = 1.0f;
	int spawnedWaveCount_ = 0;
	int attackSequence_ = 0;
	int waveTimer_ = 0;
	bool isActive_ = false;
	KamataEngine::Vector3 arenaCenter_ = {};
	float arenaRadius_ = 0.0f;
	float attackRangeMultiplier_ = 1.0f;
	float damageMultiplier_ = 1.0f;
	uint32_t bulletShotSeHandle_ = 0;
	uint32_t bulletHitSeHandle_ = 0;
	int impactSeCooldown_ = 0;
	int bossPhase_ = 1;
	float GetBulletRadius() const { return kBulletRadius * attackRangeMultiplier_; }
	int GetDamage() const {
		return (std::max)(1, static_cast<int>(kDamage * damageMultiplier_));
	}

	inline static constexpr int kBulletCountPerWave = 40;
	int GetWaveCount() const { return 2 + bossPhase_; }
	// 各弾幕を見分けて隙間へ移動できるよう、0.5秒から約0.67秒へ延長する。
	// 各ウェーブを見切りやすくするため、間隔を約1秒まで広げる。
	inline static constexpr int kWaveIntervalFrames = 60;
	inline static constexpr int kBulletLifeFrames = 240;
	inline static constexpr float kGapAngleDegrees = 45.0f;
	inline static constexpr float kGapMoveDegreesPerWave = 55.0f;
	inline static constexpr float kBulletSpeed = 0.35f;
	inline static constexpr float kBulletRadius = 1.0f;
	// BossBulletモデルの正面（-Z）をゲーム側の進行方向へ合わせる補正。
	inline static constexpr float kBulletModelForwardYawOffset = 3.14159265f;
	inline static constexpr int kDamage = 10;
	inline static constexpr int kImpactSeIntervalFrames = 4;
};
