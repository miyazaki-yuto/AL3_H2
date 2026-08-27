#pragma once

#include "KamataEngine.h"
#include "PlayerAttackController.h"

#include <array>

class Boss;

class Player {
public:
	Player() = default;
	~Player() = default;

	void Initialize(
	    KamataEngine::Model* model,
	    KamataEngine::Model* bulletModel,
	    KamataEngine::Model* bombModel,
	    KamataEngine::Model* effectModel,
	    KamataEngine::Model* attackModel,
	    KamataEngine::Model* slashLeftModel,
	    KamataEngine::Model* slashRightModel,
	    KamataEngine::Model* beamModel,
	    KamataEngine::Model* predictionModel,
	    KamataEngine::Camera* camera);
	void Update();
	void Draw();
	void SetMainSkill(MainSkillType mainSkill) { attackController_.SetMainSkill(mainSkill); }
	MainSkillType GetMainSkill() const { return attackController_.GetMainSkill(); }
	void SetSubSkills(const std::array<SubSkillType, 2>& subSkills) {
		attackController_.SetSubSkills(subSkills);
	}
	void SetTargetBoss(Boss* boss) { attackController_.SetTargetBoss(boss); }
	float GetChargeProgress() const { return attackController_.GetChargeProgress(); }
	float GetChargeDamageMultiplier() const { return attackController_.GetChargeDamageMultiplier(); }
	float GetChargeRangeMultiplier() const { return attackController_.GetChargeRangeMultiplier(); }
	float GetChargeMaximumDamageMultiplier() const {
		return attackController_.GetChargeMaximumDamageMultiplier();
	}
	float GetChargeMaximumRangeMultiplier() const {
		return attackController_.GetChargeMaximumRangeMultiplier();
	}
	float GetMainSkillCooldownRatio() const {
		return attackController_.GetMainSkillCooldownRatio();
	}
	bool IsChargeAttackActive() const { return attackController_.IsChargeAttackActive(); }
	bool IsChargingSubSkill() const { return attackController_.IsChargingSubSkill(); }
	void StopAllAttackSounds() { attackController_.StopAllSounds(); }
	bool IsBeamActive() const { return attackController_.IsBeamActive(); }
	int ConsumeChargeGrowthEffectCount() {
		return attackController_.ConsumeChargeGrowthEffectCount();
	}
	bool GetAttackLightPosition(KamataEngine::Vector3& position) const {
		return attackController_.GetAttackLightPosition(position);
	}
	// cancelActiveSkill=falseなら、HP減少と無敵時間だけを適用する。
	void ApplyDamage(int damage, bool cancelActiveSkill = true);
	void ApplyKnockbackAndStun(
	    const KamataEngine::Vector3& sourcePosition, float knockbackForce, int stunFrames);
	void ResolveObstacleCollisionXZ(
	    const KamataEngine::Vector3& obstacleCenter, float obstacleRadius);
	void ConstrainInsideArena(const KamataEngine::Vector3& arenaCenter, float arenaRadius);
	void SetArenaBounds(const KamataEngine::Vector3& arenaCenter, float arenaRadius) {
		attackController_.SetArenaBounds(arenaCenter, arenaRadius);
	}
	const KamataEngine::WorldTransform& GetWorldTransform() const { return worldTransform_; }
	float GetCollisionRadius() const { return collisionRadius_; }
	int GetHP() const { return playerHP_; }
	int GetMaxHP() const { return maxPlayerHP_; }
	int ConsumeSuccessfulHitCount() {
		return attackController_.ConsumeSuccessfulHitCount();
	}
	int ConsumeBossHitCount() { return attackController_.ConsumeBossHitCount(); }
	int GetSteelBurstStackCount() const {
		return attackController_.GetSteelBurstStackCount();
	}
	bool IsDoubleSteelActive() const {
		return attackController_.IsDoubleSteelActive();
	}
	bool IsDead() const { return playerHP_ <= 0; }

private:
	void UpdateChargeSquashAnimation();
	void UpdateSteelBodyScale();
	KamataEngine::Model* model_ = nullptr;
	KamataEngine::Camera* camera_ = nullptr;
	KamataEngine::WorldTransform worldTransform_;
	KamataEngine::ObjectColor objectColor_;
	KamataEngine::ObjectColor chargeEnergyColor_;
	float chargeEnergyTime_ = 0.0f;
	uint32_t avoidanceSeHandle_ = 0;
	uint32_t damageSeHandle_ = 0;
	PlayerAttackController attackController_;

	inline static constexpr float kMoveSpeed = 1.0f;
	inline static constexpr float kBerserkAutoMoveStrength = 0.65f;
	inline static constexpr float kRotationInterpolation = 0.18f;
	// Player.objの正面はゲーム側の+Z想定と逆なので、表示方向を180度補正する。
	inline static constexpr float kModelForwardYawOffset = 3.14159265f;
	inline static constexpr int kControllerIndex = 0;
	inline static constexpr float kLeftStickDeadZone = 0.24f;
	inline static constexpr float kDashSpeed = 4.0f;
	inline static constexpr int kDashDurationFrames = 4;
	inline static constexpr int kDashDecelerationFrames = 20;
	inline static constexpr int kPostDashSlowFrames = 30;
	inline static constexpr float kPostDashSlowMultiplier = 0.8f;
	inline static constexpr int kDashCooldownFrames = 90;
	// フィールド内で視認しやすいよう、Player.objを原寸の2倍で表示する。
	inline static constexpr float kBasePlayerScale = 2.0f;
	// Player.objのXZ外形（半径約3.1）に合わせた円形判定。
	inline static constexpr float kCollisionRadius = 3.1f;
	// OBJの最下点Y=-0.916336を地面Y=0へ合わせるための中心高。
	inline static constexpr float kModelGroundOffset = 0.916336f;
	inline static constexpr int kBasePlayerHP = 100;
	inline static constexpr int kSteelMaxHealthPerHit = 2;
	inline static constexpr int kSteelBurstMaxHealthGain = 30;
	inline static constexpr int kSteelBurstHealAmount = 30;
	// HP成長量は維持したまま、体格と当たり判定の拡大を従来の半分に抑える。
	inline static constexpr float kSteelScalePerHit = 0.01f;
	inline static constexpr int kInvincibilityFrames = 30;
	inline static constexpr float kKnockbackDamping = 0.82f;
	// ダウン中の左右への揺れ幅。モデルだけを傾け、円形の当たり判定には影響しない。
	inline static constexpr float kStunRockAmplitude = 0.32f;
	inline static constexpr float kStunRockSpeed = 0.55f;
	inline static constexpr float kChargeSquashMaximum = 0.22f;
	inline static constexpr float kChargeSquashPulseAmount = 0.035f;
	inline static constexpr float kChargeSquashPulseSpeed = 0.20f;
	inline static constexpr float kChargeSquashInterpolation = 0.18f;
	inline static constexpr uint8_t kRightTriggerThreshold = 30;
	int playerHP_ = kBasePlayerHP;
	int maxPlayerHP_ = kBasePlayerHP;
	float playerScaleMultiplier_ = 1.0f;
	float collisionRadius_ = kCollisionRadius;
	float chargeSquashAmount_ = 0.0f;
	int chargeSquashAnimationFrame_ = 0;
	int invincibilityTimer_ = 0;
	int dashTimer_ = 0;
	int dashDecelerationTimer_ = 0;
	int postDashSlowTimer_ = 0;
	int dashCooldownTimer_ = 0;
	int stunTimer_ = 0;
	KamataEngine::Vector3 dashDirection_ = {0.0f, 0.0f, 1.0f};
	KamataEngine::Vector3 aimDirection_ = {0.0f, 0.0f, 1.0f};
	KamataEngine::Vector3 knockbackVelocity_ = {0.0f, 0.0f, 0.0f};
};
