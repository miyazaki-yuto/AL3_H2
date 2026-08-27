#pragma once

#include "KamataEngine.h"

#include <array>

class BossGroundSpearAttack;
class Player;

// ボスを中心に4方向へレーザーを放ち、自身も回転する必殺技。
class BossRotatingLaserAttack final {
public:
	~BossRotatingLaserAttack();
	void Initialize(KamataEngine::Model* laserModel, KamataEngine::Camera* camera, Player* player);
	void Start(const KamataEngine::WorldTransform& bossWorldTransform);
	void Update(
	    KamataEngine::WorldTransform& bossWorldTransform,
	    BossGroundSpearAttack* groundSpearAttack);
	void Draw();
	void StopAllSounds();
	void SetAttackRangeMultiplier(float multiplier) { attackRangeMultiplier_ = multiplier; }
	void SetArenaBounds(const KamataEngine::Vector3& center, float radius) {
		arenaCenter_ = center;
		arenaRadius_ = radius;
	}

	bool IsActive() const { return isActive_; }
	bool IsWarning() const { return isActive_ && isWarning_; }
	bool IsFiring() const { return isActive_ && !isWarning_; }
	int ConsumeBossHealAmount() {
		const int amount = pendingBossHealAmount_;
		pendingBossHealAmount_ = 0;
		return amount;
	}

private:
	void UpdateLaserTransform(
	    size_t index,
	    const KamataEngine::Vector3& start,
	    const KamataEngine::Vector3& end,
	    float rotationY);
	void CheckPlayerCollision(
	    const KamataEngine::Vector3& start, const KamataEngine::Vector3& end);
	KamataEngine::Vector3 GetArenaEdge(const KamataEngine::Vector3& start, float angle) const;

	KamataEngine::Model* laserModel_ = nullptr;
	KamataEngine::Camera* camera_ = nullptr;
	Player* player_ = nullptr;
	std::array<KamataEngine::WorldTransform, 4> laserTransforms_;
	KamataEngine::ObjectColor laserColor_;
	float startRotationY_ = 0.0f;
	float rotationY_ = 0.0f;
	int warningTimer_ = 0;
	int remainingFrames_ = 0;
	int pendingBossHealAmount_ = 0;
	bool isActive_ = false;
	bool isWarning_ = false;
	KamataEngine::Vector3 arenaCenter_ = {};
	float arenaRadius_ = 0.0f;
	float attackRangeMultiplier_ = 1.0f;
	float beamEffectTime_ = 0.0f;
	uint32_t chargeStartSeHandle_ = 0;
	uint32_t chargeLoopSeHandle_ = 0;
	uint32_t chargeMaxSeHandle_ = 0;
	uint32_t beamShotSeHandle_ = 0;
	uint32_t beamLoopSeHandle_ = 0;
	uint32_t chargeStartVoiceHandle_ = 0;
	uint32_t chargeLoopVoiceHandle_ = 0;
	uint32_t beamShotVoiceHandle_ = 0;
	uint32_t beamLoopVoiceHandle_ = 0;
	bool isChargeStartPlaying_ = false;
	bool isChargeLoopPlaying_ = false;
	bool isBeamShotPlaying_ = false;
	bool isBeamLoopPlaying_ = false;
	float GetLaserRadius() const { return kLaserRadius * attackRangeMultiplier_; }

	inline static constexpr int kWarningFrames = 60;
	inline static constexpr int kLaserFrames = 300; // 3秒 (60 FPS)
	inline static constexpr float kTotalRotation = 12.5663706144f; // 2回転 (4π)
	inline static constexpr float kLaserRange = 5000.0f;
	inline static constexpr float kLaserRadius = 8.0f;
	inline static constexpr int kDamage = 15;
	inline static constexpr int kBossHealPerHit = 15;
};
