#pragma once

#include "KamataEngine.h"

#include <cmath>

class Player;

// ボスが予兆線の方向へ直進する突進攻撃。
class BossChargeAttack final {
public:
	void Initialize(
	    KamataEngine::Model* predictionLineModel,
	    KamataEngine::Camera* camera,
	    Player* player);
	void Start(
	    const KamataEngine::Vector3& bossPosition,
	    const KamataEngine::Vector3& playerPosition);
	void Update(KamataEngine::WorldTransform& bossWorldTransform);
	void Draw();
	void StopForDown();
	void SetAttackRangeMultiplier(float multiplier) { attackRangeMultiplier_ = multiplier; }

	bool IsActive() const { return state_ != State::Inactive; }
	bool IsWarning() const { return state_ == State::Warning; }
	bool IsCharging() const { return state_ == State::Charging; }
	bool GetPredictionLightData(KamataEngine::Vector3& position, float& radius) const {
		if (!IsWarning()) return false;
		position = predictionTransform_.translation_;
		radius = std::sqrt(
		    predictionTransform_.scale_.x * predictionTransform_.scale_.x +
		    predictionTransform_.scale_.z * predictionTransform_.scale_.z);
		return true;
	}

private:
	enum class State { Inactive, Warning, Charging, Recovery };

	void UpdatePredictionTransform(const KamataEngine::Vector3& bossPosition);
	void UpdateChargeTarget(
	    const KamataEngine::Vector3& bossPosition,
	    const KamataEngine::Vector3& playerPosition);
	void CheckPlayerCollision(const KamataEngine::Vector3& bossPosition);

	KamataEngine::Model* predictionLineModel_ = nullptr;
	KamataEngine::Camera* camera_ = nullptr;
	Player* player_ = nullptr;
	KamataEngine::WorldTransform predictionTransform_;
	KamataEngine::ObjectColor predictionColor_;
	KamataEngine::Vector3 chargeDirection_ = {};
	float chargeDistance_ = 0.0f;
	float travelledDistance_ = 0.0f;
	State state_ = State::Inactive;
	int stateTimer_ = 0;
	bool hasHitPlayer_ = false;
	float attackRangeMultiplier_ = 1.0f;
	uint32_t dashSeHandle_ = 0;

	inline static constexpr int kWarningFrames = 60;
	inline static constexpr int kRecoveryFrames = 36;
	inline static constexpr float kChargeSpeed = 2.0f;
	inline static constexpr float kMinimumChargeDistance = 25.0f;
	inline static constexpr float kMaximumChargeDistance = 120.0f;
	inline static constexpr float kOvershootDistance = 10.0f;
	// Boss の大きさ (20 x 20) に合わせ、突進予兆も幅 20 にする。
	inline static constexpr float kPredictionWidth = 20.0f;
	inline static constexpr float kPredictionDisplayHeight = 0.05f;
	// Boss本体は20 x 20なので、通常接触判定と同じ半径10を使う。
	inline static constexpr float kCollisionRadius = 10.0f;
	inline static constexpr float kCollisionSeparationMargin = 1.0f;
	inline static constexpr int kDamage = 35;
	// 通常の地面槍より大きく吹き飛ばす、突進専用の強いノックバック。
	inline static constexpr float kKnockbackForce = 15.0f;
	inline static constexpr int kStunFrames = 120;
};
