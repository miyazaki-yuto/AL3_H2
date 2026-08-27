#pragma once

#include "BossCloseExplosionAttack.h"
#include "BossChargeAttack.h"
#include "BossGapShockWaveAttack.h"
#include "BossGroundSpearAttack.h"
#include "BossRotatingLaserAttack.h"
#include "BossSummonMobs.h"

#include <cstdint>

class Player;

class BossAttackManager final {
public:
	enum class AttackState {
		Idle,
		Warning,
		Attacking,
		Down,
	};

	BossAttackManager() = default;
	~BossAttackManager() = default;

	void Initialize(
	    KamataEngine::Model* bulletModel,
	    KamataEngine::Model* laserModel,
	    KamataEngine::Model* predictionCircleModel,
	    KamataEngine::Model* chargePredictionLineModel,
	    KamataEngine::Model* groundSpearModel,
	    KamataEngine::Model* mobModel,
	    KamataEngine::Camera* camera,
	    Player* player);
	void Update(KamataEngine::WorldTransform& bossWorldTransform);
	void Draw();
	void SetArenaBounds(const KamataEngine::Vector3& center, float radius) {
		gapShockWaveAttack_.SetArenaBounds(center, radius);
		summonMobsAttack_.SetArenaBounds(center, radius);
		rotatingLaserAttack_.SetArenaBounds(center, radius);
	}
	void RegisterPlayerDamage(int damage);
	void SetBossPhase(int phase);
	int GetBossPhase() const { return bossPhase_; }
	float GetDDADifficulty() const { return ddaDifficulty_; }
	float GetDDATargetDifficulty() const { return ddaTargetDifficulty_; }
	int GetDDALevel() const { return ddaLevel_; }
	int GetDDATargetLevel() const { return ddaTargetLevel_; }
	float GetDDAAttackRangeMultiplier() const { return ddaAttackRangeMultiplier_; }
	float GetDDAAttackCooldownMultiplier() const;
	int GetDDANextAttackIntervalFrames() const;
	float GetDDASampleProgress() const {
		return static_cast<float>(ddaSampleTimer_) / kDDASampleFrames;
	}
	int GetDDADamageThisSample() const { return damageDuringDDASample_; }
	int GetDDADamageTakenThisSample() const { return damageTakenDuringDDASample_; }
	int ConsumeLaserHealAmount() {
		return rotatingLaserAttack_.ConsumeBossHealAmount();
	}
	void StopAllAttackSounds() { rotatingLaserAttack_.StopAllSounds(); }
	bool IsMobTimeLimitActive() const { return summonMobsAttack_.IsSummoning(); }
	bool IsChargeDashing() const { return chargeAttack_.IsCharging(); }
	bool ConsumeGroundSpearEruptionEvent() {
		return groundSpearAttack_.ConsumeSpearEruptionEvent();
	}
	float GetMobTimeLimitRatio() const { return summonMobsAttack_.GetTimeLimitRatio(); }
	const char* GetDDAWeaponBiasName() const;
	const char* GetNextAttackName() const;
	bool GetPredictionLightData(KamataEngine::Vector3& position, float& radius) const;
	bool DamageMobsInCircle(
	    const KamataEngine::Vector3& center, float radius, int damage,
	    KamataEngine::Vector3* nearestMobPosition = nullptr) {
		return summonMobsAttack_.DamageMobsInCircle(
		    center, radius, damage, nearestMobPosition);
	}
	bool DamageMobsAlongSegment(
	    const ColliderManager::Segment& segment, int damage,
	    KamataEngine::Vector3* nearestHitPosition = nullptr,
	    float collisionPadding = 0.0f) {
		return summonMobsAttack_.DamageMobsAlongSegment(
		    segment, damage, nearestHitPosition, collisionPadding);
	}
	int ReflectEnemyProjectilesInCircle(const KamataEngine::Vector3& center, float radius) {
		return gapShockWaveAttack_.ReflectBulletsInCircle(center, radius) +
		       summonMobsAttack_.ReflectBulletsInCircle(center, radius);
	}
	int ReflectEnemyProjectilesTowardTargetInCircle(
	    const KamataEngine::Vector3& center, float radius, const KamataEngine::Vector3& target) {
		return gapShockWaveAttack_.ReflectBulletsTowardTargetInCircle(center, radius, target) +
		       summonMobsAttack_.ReflectBulletsTowardTargetInCircle(center, radius, target);
	}
	int ConsumeReflectedProjectileDamageInCircle(
	    const KamataEngine::Vector3& center, float radius) {
		return gapShockWaveAttack_.ConsumeReflectedBulletDamageInCircle(center, radius) +
		       summonMobsAttack_.ConsumeReflectedBulletDamageInCircle(center, radius);
	}

	bool IsAttacking() const {
		if (IsDown()) {
			return false;
		}
		return approachingCloseExplosion_ || gapShockWaveAttack_.IsActive() || closeExplosionAttack_.IsExploding() ||
		       (groundSpearSequenceInProgress_ && groundSpearAttack_.IsAttacking()) ||
		       summonMobsAttack_.IsSummoning() || chargeAttack_.IsCharging() ||
		       rotatingLaserAttack_.IsFiring();
	}
	bool IsWarning() const {
		if (IsDown()) {
			return false;
		}
		return closeExplosionAttack_.IsWarning() ||
		       (groundSpearSequenceInProgress_ && groundSpearAttack_.IsWarning()) ||
		       summonMobsAttack_.IsWarning() ||
		       chargeAttack_.IsWarning() ||
		       rotatingLaserAttack_.IsWarning() ||
		       (!IsAttackInProgress() && attackCooldown_ > 0 && attackCooldown_ <= kWarningFrames);
	}
	AttackState GetAttackState() const {
		if (IsDown()) {
			return AttackState::Down;
		}
		if (IsAttacking()) {
			return AttackState::Attacking;
		}
		if (IsWarning()) {
			return AttackState::Warning;
		}
		return AttackState::Idle;
	}
	size_t GetBulletCount() const { return gapShockWaveAttack_.GetBulletCount(); }
	bool IsDown() const { return downTimer_ > 0; }

private:
	enum class AttackType {
		GapShockWave,
		CloseExplosion,
		GroundSpear,
		SummonMobs,
		Charge,
		RotatingLaser,
	};

	bool IsAttackInProgress() const {
		return IsDown() || gapShockWaveAttack_.IsActive() || closeExplosionAttack_.IsActive() ||
		       approachingCloseExplosion_ || groundSpearSequenceInProgress_ ||
		       summonMobsAttack_.IsWarning() || chargeAttack_.IsActive() ||
		       rotatingLaserAttack_.IsActive();
	}
	void UpdateDDA();
	void ApplyDDAParameters();
	AttackType SelectWeaponAdaptedAttack();
	AttackType SelectPhaseThreeSecondaryAttack(AttackType primaryAttack) const;
	void StartAttack(
	    AttackType attackType,
	    KamataEngine::WorldTransform& bossWorldTransform,
	    bool recordAsLastAttack);
	void ScheduleNextAttack();
	void UpdateCloseExplosionApproach(KamataEngine::WorldTransform& bossWorldTransform);
	float NextRandom01();
	const char* GetAttackName(AttackType attackType) const;

	Player* player_ = nullptr;
	BossGapShockWaveAttack gapShockWaveAttack_;
	BossCloseExplosionAttack closeExplosionAttack_;
	BossGroundSpearAttack groundSpearAttack_;
	BossSummonMobs summonMobsAttack_;
	BossChargeAttack chargeAttack_;
	BossRotatingLaserAttack rotatingLaserAttack_;
	bool groundSpearSequenceInProgress_ = false;
	// Mob が追跡を開始したら、召喚の完了を待たず次の攻撃を開始する。
	bool summonMobsFollowupStarted_ = false;
	bool approachingCloseExplosion_ = false;
	bool forcedLaserPending_ = false;
	AttackType nextAttackType_ = AttackType::GapShockWave;
	AttackType lastAttackType_ = AttackType::GapShockWave;
	bool hasLastAttackType_ = false;
	AttackType nextAttackAfterDown_ = AttackType::GapShockWave;
	int attackCooldown_ = 0;
	int downTimer_ = 0;
	int ddaSampleTimer_ = 0;
	int damageDuringDDASample_ = 0;
	int damageTakenDuringDDASample_ = 0;
	int previousPlayerHp_ = 0;
	int bossPhase_ = 1;
	int ddaLevel_ = 3;
	int ddaTargetLevel_ = 3;
	float ddaDifficulty_ = 0.50f;
	float ddaTargetDifficulty_ = 0.50f;
	float ddaAttackRangeMultiplier_ = 1.0f;
	KamataEngine::Vector3 latestBossPosition_ = {};
	uint32_t randomState_ = 0x71A35F29u;

	inline static constexpr int kFirstAttackDelayFrames = 60;
	inline static constexpr int kAttackIntervalFrames = 120;
	// HPゲージ2本目以降は、DDA算出後の待ち時間をさらに35%短縮する。
	inline static constexpr float kPhaseTwoCooldownMultiplier = 0.65f;
	inline static constexpr int kWarningFrames = 45;
	inline static constexpr int kDownFrames = 180;
	inline static constexpr float kBossCollisionRadius = 10.0f;
	inline static constexpr float kCloseExplosionApproachDistance = 55.0f;
	inline static constexpr float kCloseExplosionApproachSpeed = 0.85f;
	inline static constexpr int kDDASampleFrames = 300;
	inline static constexpr int kDDAMinLevel = 1;
	inline static constexpr int kDDAMaxLevel = 5;
};
