#pragma once

#include "KamataEngine.h"

#include <algorithm>
#include <array>

class Player;

class BossGroundSpearAttack final {
public:
	BossGroundSpearAttack() = default;
	~BossGroundSpearAttack() = default;

	void Initialize(
	    KamataEngine::Model* spearModel,
	    KamataEngine::Model* predictionCircleModel,
	    KamataEngine::Camera* camera,
	    Player* player);
	void Start(
	    const KamataEngine::Vector3& bossPosition,
	    const KamataEngine::Vector3& playerPosition);
	void Update();
	void Draw();
	void SetAttackRangeMultiplier(float multiplier) { attackRangeMultiplier_ = multiplier; }
	void SetDamageMultiplier(float multiplier) { damageMultiplier_ = multiplier; }
	void SetBossPhase(int phase) { bossPhase_ = std::clamp(phase, 1, 3); }

	bool IsActive() const { return isActive_; }
	bool IsWarning() const;
	bool IsAttacking() const;
	bool IsSequenceFinished() const;
	bool GetPredictionLightData(KamataEngine::Vector3& position, float& radius) const;
	bool ConsumeSpearEruptionEvent() {
		const bool erupted = spearEruptionPending_;
		spearEruptionPending_ = false;
		return erupted;
	}
	// 突進中のボスが触れた残存槍を、引き抜きアニメーションで消去する。
	bool DestroySpearHitByBoss(
	    const KamataEngine::Vector3& bossPosition, float bossCollisionRadius);
	bool BlockLaserWithSpear(
	    const KamataEngine::Vector3& laserStart,
	    const KamataEngine::Vector3& laserEnd,
	    float laserRadius,
	    KamataEngine::Vector3& hitPosition);
	// レーザーを防いだ槍は、必殺技が終わるまで残してから一斉に消去する。
	void RetractLaserShieldSpears();

private:
	inline static constexpr size_t kSpearCount = 3;
	static_assert(kSpearCount > 0, "kSpearCount must be greater than zero.");

	enum class SpearState {
		Dormant,
		Warning,
		Rising,
		Active,
		Retracting,
	};

	struct Spear {
		KamataEngine::WorldTransform spearTransform;
		KamataEngine::WorldTransform predictionTransform;
		KamataEngine::ObjectColor predictionColor;
		KamataEngine::Vector3 groundPosition = {};
		SpearState state = SpearState::Dormant;
		int stateTimer = 0;
		bool hasDealtDamage = false;
		bool isLaserShield = false;
	};

	void BeginWarning(Spear& spear, const KamataEngine::Vector3& targetPosition);
	void UpdatePredictionTransform(Spear& spear, float warningProgress);
	void UpdateSpearTransform(Spear& spear, float riseProgress);
	void CheckPlayerCollision(Spear& spear);
	bool AreAllSpearsDormant() const;

	KamataEngine::Model* spearModel_ = nullptr;
	KamataEngine::Model* predictionCircleModel_ = nullptr;
	KamataEngine::Camera* camera_ = nullptr;
	Player* player_ = nullptr;
	std::array<Spear, kSpearCount> spears_;
	float groundHeight_ = 0.0f;
	size_t nextSpearIndex_ = 0;
	int nextSpearTimer_ = -1;
	bool isActive_ = false;
	bool spearEruptionPending_ = false;
	float attackRangeMultiplier_ = 1.0f;
	float damageMultiplier_ = 1.0f;
	int bossPhase_ = 1;
	uint32_t groundSpearSeHandle_ = 0;
	float GetCollisionRadius() const { return kCollisionRadius * attackRangeMultiplier_; }
	int GetDamage() const {
		return (std::max)(1, static_cast<int>(kDamage * damageMultiplier_));
	}

	// 新GroundSpears.obj（高さ12.658575）を従来と同じ40ユニット高へ合わせる。
	inline static constexpr float kSpearScale = 3.159910f;
	inline static constexpr float kSpearModelHeight = 12.658575f;
	inline static constexpr float kSpearHeight = kSpearModelHeight * kSpearScale;
	// OBJ最下点Y=-0.258575を地面へ合わせる接地補正。
	inline static constexpr float kSpearGroundOffset = 0.817077f;
	inline static constexpr float kCollisionRadius = 5.7f;
	inline static constexpr float kPredictionStartScaleRatio = 0.05f;
	inline static constexpr float kPredictionDisplayHeight = 0.04f;
	inline static constexpr int kWarningFrames = 75;
	inline static constexpr int kRisingFrames = 20;
	inline static constexpr int kNextSpearDelayFrames = 12;
	inline static constexpr int kActiveFrames = 1200;
	inline static constexpr int kRetractingFrames = 15;
	inline static constexpr int kDamage = 20;
	inline static constexpr float kKnockbackForce = 1.2f;
	inline static constexpr int kStunFrames = 30;
};
