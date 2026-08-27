#pragma once

#include "KamataEngine.h"

class Player;

class BossCloseExplosionAttack final {
public:
	BossCloseExplosionAttack() = default;
	~BossCloseExplosionAttack() = default;

	void Initialize(
	    KamataEngine::Model* predictionCircleModel,
	    KamataEngine::Camera* camera,
	    Player* player);
	void Start(const KamataEngine::Vector3& bossPosition);
	void Update(const KamataEngine::Vector3& bossPosition);
	void Draw();
	void SetAttackRangeMultiplier(float multiplier) { attackRangeMultiplier_ = multiplier; }

	bool IsActive() const { return state_ != State::Inactive; }
	bool IsWarning() const { return state_ == State::Warning; }
	bool IsExploding() const { return state_ == State::Explosion; }
	bool GetPredictionLightData(KamataEngine::Vector3& position, float& radius) const {
		if (!IsWarning()) return false;
		position = worldTransform_.translation_;
		radius = worldTransform_.scale_.x;
		return true;
	}

private:
	enum class State {
		Inactive,
		Warning,
		Explosion,
	};

	void UpdateTransform(const KamataEngine::Vector3& bossPosition);
	void CheckPlayerCollision();

	KamataEngine::Model* predictionCircleModel_ = nullptr;
	KamataEngine::Camera* camera_ = nullptr;
	Player* player_ = nullptr;
	KamataEngine::WorldTransform worldTransform_;
	KamataEngine::ObjectColor objectColor_;
	State state_ = State::Inactive;
	int stateTimer_ = 0;
	float warningProgress_ = 0.0f;
	float explosionEffectProgress_ = 0.0f;
	bool hasDealtDamage_ = false;
	float attackRangeMultiplier_ = 1.0f;
	uint32_t forecastStartSeHandle_ = 0;
	uint32_t explosionSeHandle_ = 0;

	// Boss本体（直径約20）の周囲を狙う近距離攻撃として、基礎半径を抑える。
	// DDA倍率0.8～1.2適用後は半径44～66になる。
	inline static constexpr float kExplosionRadius = 55.0f;
	inline static constexpr float kPredictionStartScaleRatio = 0.05f;
	inline static constexpr float kDisplayHeight = 0.03f;
	inline static constexpr int kWarningFrames = 90;
	inline static constexpr int kExplosionFrames = 20;
	inline static constexpr int kDamage = 25;
};
