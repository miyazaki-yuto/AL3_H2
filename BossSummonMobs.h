#pragma once
#include "KamataEngine.h"
#include "ColliderManager.h"
#include <memory>
#include <vector>

class Player;

class BossSummonMobs final {
public:
	BossSummonMobs() = default;
	~BossSummonMobs() = default;

	void Initialize(
	    KamataEngine::Model* mobModel,
	    KamataEngine::Model* bulletModel,
	    KamataEngine::Model* predictionCircleModel,
	    KamataEngine::Camera* camera,
	    Player* player);
	void Start(const KamataEngine::Vector3& bossPosition);
	void Update();
	void Draw();
	void SetAttackRangeMultiplier(float multiplier) { attackRangeMultiplier_ = multiplier; }
	void SetArenaBounds(const KamataEngine::Vector3& center, float radius) {
		arenaCenter_ = center;
		arenaRadius_ = radius;
	}

	bool DamageMobsInCircle(
	    const KamataEngine::Vector3& center, float radius, int damage,
	    KamataEngine::Vector3* nearestMobPosition = nullptr);
	bool DamageMobsAlongSegment(
	    const ColliderManager::Segment& segment, int damage,
	    KamataEngine::Vector3* nearestHitPosition = nullptr,
	    float collisionPadding = 0.0f);
	int ReflectBulletsInCircle(const KamataEngine::Vector3& center, float radius);
	int ReflectBulletsTowardTargetInCircle(
	    const KamataEngine::Vector3& center, float radius,
	    const KamataEngine::Vector3& target);
	int ConsumeReflectedBulletDamageInCircle(
	    const KamataEngine::Vector3& center, float radius);
	bool IsActive() const { return isActive_; }
	bool IsWarning() const { return isActive_ && isWarning_; }
	bool IsSummoning() const { return isActive_ && !isWarning_; }
	bool GetPredictionLightData(KamataEngine::Vector3& position, float& radius) const;
	size_t GetMobCount() const { return mobs_.size(); }
	int GetRemainingFrames() const { return remainingFrames_; }
	int GetSurvivorCountOnTimeout() const { return survivorCountOnTimeout_; }
	float GetTimeLimitRatio() const {
		return IsSummoning() ?
		    static_cast<float>(remainingFrames_) / static_cast<float>(kTimeLimitFrames) :
		    0.0f;
	}

private:
	struct Mob {
		KamataEngine::WorldTransform worldTransform;
		KamataEngine::WorldTransform predictionTransform;
		int hp = 0;
		int shootCooldown = 0;
		float strafeDirection = 1.0f;
		bool isDead = false;
	};
	struct MobBullet {
		KamataEngine::WorldTransform worldTransform;
		KamataEngine::Vector3 velocity = {};
		int remainingFrames = 0;
		bool isDead = false;
		bool isReflectedByPlayer = false;
	};

	void Finish(bool timedOut);
	void ResolveMobCollisions();
	void ResolveMobPlayerCollisions();
	void ConstrainMobInsideArena(Mob& mob) const;
	void UpdatePredictionTransforms(float warningProgress);
	void SpawnBullet(Mob& mob, const KamataEngine::Vector3& direction);
	void UpdateBullets();

	KamataEngine::Model* mobModel_ = nullptr;
	KamataEngine::Model* bulletModel_ = nullptr;
	KamataEngine::Model* predictionCircleModel_ = nullptr;
	KamataEngine::Camera* camera_ = nullptr;
	Player* player_ = nullptr;
	KamataEngine::ObjectColor predictionColor_;
	KamataEngine::ObjectColor mobColor_;
	std::vector<std::unique_ptr<Mob>> mobs_;
	std::vector<std::unique_ptr<MobBullet>> bullets_;
	int remainingFrames_ = 0;
	int warningTimer_ = 0;
	int globalShootCooldown_ = 0;
	int survivorCountOnTimeout_ = 0;
	bool isActive_ = false;
	bool isWarning_ = false;
	KamataEngine::Vector3 arenaCenter_ = {};
	float arenaRadius_ = 0.0f;
	float attackRangeMultiplier_ = 1.0f;
	uint32_t bulletShotSeHandle_ = 0;
	uint32_t bulletHitSeHandle_ = 0;
	uint32_t summonStartSeHandle_ = 0;
	int impactSeCooldown_ = 0;

	inline static constexpr size_t kMobCount = 4;
	inline static constexpr int kMobHP = 1;
	// BossHeadを子分らしい大きさで表示する。判定半径は従来値を維持する。
	inline static constexpr float kMobVisualScale = 0.38f;
	// BossHeadモデルの正面はゲーム側の+Z想定と逆向き。
	inline static constexpr float kMobModelForwardYawOffset = 3.14159265f;
	inline static constexpr int kTimeLimitFrames = 600;
	inline static constexpr int kContactDamage = 5;
	inline static constexpr float kMoveSpeed = 0.45f;
	inline static constexpr float kStrafeSpeed = 0.25f;
	inline static constexpr float kPreferredDistance = 35.0f;
	inline static constexpr float kDistanceTolerance = 7.0f;
	// Player初期サイズ時の判定半径。これを超えた巨大化分だけ維持距離を広げる。
	inline static constexpr float kBasePlayerCollisionRadius = 3.1f;
	inline static constexpr float kPlayerGrowthDistanceScale = 1.25f;
	// 遠距離から撃たず、Playerへある程度近づいてから射撃を開始する。
	inline static constexpr float kShootRange = 50.0f;
	inline static constexpr int kShootCooldownFrames = 120;
	// Mob全体で射撃間隔を共有し、一斉射撃による永久的なスキル中断を防ぐ。
	inline static constexpr int kGlobalShootIntervalFrames = 45;
	inline static constexpr size_t kMaxActiveHostileBullets = 4;
	inline static constexpr int kInitialShootDelayStepFrames = 6;
	inline static constexpr float kBulletSpeed = 1.1f;
	inline static constexpr float kBulletRadius = 0.8f;
	// BossBulletモデルの正面（-Z）をゲーム側の進行方向へ合わせる補正。
	inline static constexpr float kBulletModelForwardYawOffset = 3.14159265f;
	inline static constexpr int kBulletDamage = 5;
	inline static constexpr int kBulletLifeFrames = 300;
	inline static constexpr int kImpactSeIntervalFrames = 4;
	inline static constexpr float kRotationInterpolation = 0.18f;
	inline static constexpr float kCollisionRadius = 1.9f;
	inline static constexpr float kPlayerCollisionSeparation = 0.1f;
	inline static constexpr int kCollisionResolveIterations = 3;
	inline static constexpr int kWarningFrames = 90;
	inline static constexpr float kPredictionStartScaleRatio = 0.05f;
	inline static constexpr float kPredictionDisplayHeight = 0.04f;
	inline static constexpr float kInnerSpawnRadius = 25.0f;
	inline static constexpr float kOuterSpawnRadius = 100.0f;
};
