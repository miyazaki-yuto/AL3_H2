#pragma once
#include "KamataEngine.h"
#include "BossAttackManager.h"
#include "TransformUtility.h"

#include <cstdint>

class Player;

class Boss {
public:
	Boss() = default;
	~Boss() = default;
	void Initialize(
	    KamataEngine::Model* model,
	    KamataEngine::Model* headModel,
	    KamataEngine::Model* bulletModel,
	    KamataEngine::Model* laserModel,
	    KamataEngine::Model* predictionCircleModel,
	    KamataEngine::Model* chargePredictionLineModel,
	    KamataEngine::Model* groundSpearModel,
	    KamataEngine::Model* mobModel,
	    KamataEngine::Camera* camera,
	    Player* player);
	void Update();
	void Draw();
	void StopAllAttackSounds() { attackManager_.StopAllAttackSounds(); }
	bool ConsumeGroundSpearEruptionEvent() {
		return attackManager_.ConsumeGroundSpearEruptionEvent();
	}
	void ApplyDamage(int damage);
	void ConstrainInsideArena(const KamataEngine::Vector3& arenaCenter, float arenaRadius);
	void SetArenaBounds(const KamataEngine::Vector3& arenaCenter, float arenaRadius) {
		arenaCenter_ = arenaCenter;
		arenaRadius_ = arenaRadius;
		attackManager_.SetArenaBounds(arenaCenter, arenaRadius);
	}
	bool DamageMobsInCircle(
	    const KamataEngine::Vector3& center, float radius, int damage,
	    KamataEngine::Vector3* nearestMobPosition = nullptr) {
		return attackManager_.DamageMobsInCircle(
		    center, radius, damage, nearestMobPosition);
	}
	bool DamageMobsAlongSegment(
	    const ColliderManager::Segment& segment, int damage,
	    KamataEngine::Vector3* nearestHitPosition = nullptr,
	    float collisionPadding = 0.0f) {
		return attackManager_.DamageMobsAlongSegment(
		    segment, damage, nearestHitPosition, collisionPadding);
	}
	int ReflectEnemyProjectilesInCircle(const KamataEngine::Vector3& center, float radius) {
		return attackManager_.ReflectEnemyProjectilesInCircle(center, radius);
	}
	int ReflectEnemyProjectilesTowardTargetInCircle(
	    const KamataEngine::Vector3& center, float radius, const KamataEngine::Vector3& target) {
		return attackManager_.ReflectEnemyProjectilesTowardTargetInCircle(center, radius, target);
	}
	const KamataEngine::WorldTransform& GetWorldTransform() const { return worldTransform_; }
	bool GetPredictionLightData(KamataEngine::Vector3& position, float& radius) const {
		return attackManager_.GetPredictionLightData(position, radius);
	}
	float GetCollisionRadius() const { return kCollisionRadius; }
	int GetHP() const { return bossHP_; }
	int GetMaxHP() const {
		switch (bossPhase_) {
		case 1: return kPhase1MaxHP;
		case 2: return kPhase2MaxHP;
		default: return kPhase3MaxHP;
		}
	}
	// MMO形式の多層HP。緑1000→オレンジ1500→赤2500の順に進む。
	int GetHpPhase() const { return bossPhase_; }
	float GetCurrentHpLayerRatio() const {
		return static_cast<float>(bossHP_) / static_cast<float>(GetMaxHP());
	}
	float GetDDADifficulty() const { return attackManager_.GetDDADifficulty(); }
	float GetDDATargetDifficulty() const { return attackManager_.GetDDATargetDifficulty(); }
	int GetDDALevel() const { return attackManager_.GetDDALevel(); }
	int GetDDATargetLevel() const { return attackManager_.GetDDATargetLevel(); }
	float GetDDAAttackRangeMultiplier() const {
		return attackManager_.GetDDAAttackRangeMultiplier();
	}
	float GetDDAAttackCooldownMultiplier() const {
		return attackManager_.GetDDAAttackCooldownMultiplier();
	}
	int GetDDANextAttackIntervalFrames() const {
		return attackManager_.GetDDANextAttackIntervalFrames();
	}
	float GetDDASampleProgress() const { return attackManager_.GetDDASampleProgress(); }
	int GetDDADamageThisSample() const { return attackManager_.GetDDADamageThisSample(); }
	int GetDDADamageTakenThisSample() const {
		return attackManager_.GetDDADamageTakenThisSample();
	}
	bool IsMobTimeLimitActive() const { return attackManager_.IsMobTimeLimitActive(); }
	float GetMobTimeLimitRatio() const { return attackManager_.GetMobTimeLimitRatio(); }
	const char* GetDDAWeaponBiasName() const { return attackManager_.GetDDAWeaponBiasName(); }
	const char* GetDDANextAttackName() const { return attackManager_.GetNextAttackName(); }
	bool IsDead() const { return bossHP_ <= 0; }
	bool IsDeathAnimationFinished() const {
		return IsDead() && deathAnimationTimer_ >= kDeathAnimationFrames;
	}

private:
	KamataEngine::Model* model_ = nullptr;
	KamataEngine::Model* headModel_ = nullptr;
	KamataEngine::Camera* camera_ = nullptr;
	KamataEngine::WorldTransform worldTransform_;
	KamataEngine::WorldTransform visualWorldTransform_;
	KamataEngine::WorldTransform headVisualWorldTransform_;
	KamataEngine::ObjectColor objectColor_;
	KamataEngine::ObjectColor chargeEnergyColor_;
	float chargeEnergyTime_ = 0.0f;
	BossAttackManager attackManager_;
	void UpdateRandomMovement();
	void UpdateDeathAnimation();
	// ボスのパラメータ
	inline static constexpr float kMoveSpeed = 0.2f;
	inline static constexpr float kRotationInterpolation = 0.15f;
	inline static constexpr float kCollisionRadius = 10.0f;
	// BossBody.objの最下点Y=-4.6316を地面Y=0へ合わせる描画専用オフセット。
	inline static constexpr float kModelGroundOffset = 4.6316f;
	// 新Bossモデルの正面はゲーム側の+Z想定と逆なので、描画だけ180度補正する。
	inline static constexpr float kModelForwardYawOffset = 3.14159265f;
	// 胴体上端Y=5.8929と頭下端Y=0.017662を接続するための頭専用オフセット。
	inline static constexpr float kHeadHeightOffset = 5.875238f;
	inline static constexpr int kPhase1MaxHP = 1000;
	inline static constexpr int kPhase2MaxHP = 1500;
	inline static constexpr int kPhase3MaxHP = 2500;
	// 被弾時に通常・攻撃中の色より優先して赤く点滅させる時間。
	inline static constexpr int kDamageFlashFrames = 10;
	// ダウン中はPlayerと同じ周期でモデルを左右に揺らし、赤紫に点滅させる。
	inline static constexpr float kDownRockAmplitude = 0.32f;
	inline static constexpr float kDownRockSpeed = 0.55f;
	// 60fps想定で約1.25秒。完了後にGameSceneが既存のCRT遷移を開始する。
	inline static constexpr int kDeathAnimationFrames = 75;
	inline static constexpr int kDeathWhiteFlashFrames = 20;
	int bossPhase_ = 1;
	int bossHP_ = kPhase1MaxHP;
	int damageFlashTimer_ = 0;
	int downAnimationFrame_ = 0;
	int deathAnimationTimer_ = 0;
	KamataEngine::Vector3 arenaCenter_ = {};
	float arenaRadius_ = 0.0f;
	KamataEngine::Vector3 randomMoveDirection_ = {1.0f, 0.0f, 0.0f};
	int randomMoveTimer_ = 0;
	uint32_t randomState_ = 0xB0551234u;
	inline static constexpr int kRandomMoveMinFrames = 75;
	inline static constexpr int kRandomMoveAdditionalFrames = 105;
};
