#include "Boss.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <numbers>


using namespace KamataEngine;

void Boss::Initialize(
    Model* model,
    Model* headModel,
    Model* bulletModel,
	Model* laserModel,
    Model* predictionCircleModel,
	Model* chargePredictionLineModel,
	Model* groundSpearModel,
	Model* mobModel,
    Camera* camera,
    Player* player) {
	model_ = model;
	headModel_ = headModel;
	camera_ = camera;
	objectColor_.Initialize();
	objectColor_.SetColor({1.0f, 1.0f, 1.0f, 1.0f});
	chargeEnergyColor_.Initialize();
	chargeEnergyColor_.SetColor({1.0f, 0.08f, 0.02f, 11.0f});
	chargeEnergyTime_ = 0.0f;
	bossPhase_ = 1;
	bossHP_ = kPhase1MaxHP;
	damageFlashTimer_ = 0;
	downAnimationFrame_ = 0;
	deathAnimationTimer_ = 0;
	worldTransform_.Initialize();
	visualWorldTransform_.Initialize();
	headVisualWorldTransform_.Initialize();
	worldTransform_.scale_ = {1.0f, 1.0f, 1.0f};
	worldTransform_.translation_ = {5.0f, 0.0f, 10.0f};
	randomState_ = static_cast<uint32_t>(
	    std::chrono::steady_clock::now().time_since_epoch().count()) ^ 0xB0551234u;
	randomMoveTimer_ = 0;
	UpdateWorldTransform(worldTransform_);
	visualWorldTransform_.scale_ = worldTransform_.scale_;
	visualWorldTransform_.rotation_ = worldTransform_.rotation_;
	visualWorldTransform_.rotation_.y += kModelForwardYawOffset;
	visualWorldTransform_.translation_ = worldTransform_.translation_;
	visualWorldTransform_.translation_.y +=
	    kModelGroundOffset * visualWorldTransform_.scale_.y;
	UpdateWorldTransform(visualWorldTransform_);
	headVisualWorldTransform_.scale_ = visualWorldTransform_.scale_;
	headVisualWorldTransform_.rotation_ = visualWorldTransform_.rotation_;
	headVisualWorldTransform_.translation_ = visualWorldTransform_.translation_;
	headVisualWorldTransform_.translation_.y +=
	    kHeadHeightOffset * headVisualWorldTransform_.scale_.y;
	UpdateWorldTransform(headVisualWorldTransform_);
	attackManager_.Initialize(
	    bulletModel, laserModel, predictionCircleModel, chargePredictionLineModel, groundSpearModel, mobModel, camera, player);
}

void Boss::Update() {
	if (IsDead()) {
		UpdateDeathAnimation();
		return;
	}
	attackManager_.SetBossPhase(GetHpPhase());
	chargeEnergyTime_ = std::fmod(chargeEnergyTime_ + 0.035f, 1.0f);
	chargeEnergyColor_.SetColor({
	    1.0f, 0.08f, 0.02f, 11.0f + chargeEnergyTime_});
	if (attackManager_.GetAttackState() == BossAttackManager::AttackState::Idle) {
		UpdateRandomMovement();
	}
	UpdateWorldTransform(worldTransform_);
	attackManager_.Update(worldTransform_);
	// 反翔済みのBoss弾・Mob弾は、Bossへ戻った時点で元の弾威力を与える。
	const int reflectedProjectileDamage =
	    attackManager_.ConsumeReflectedProjectileDamageInCircle(
	        worldTransform_.translation_, GetCollisionRadius());
	if (reflectedProjectileDamage > 0) {
		ApplyDamage(reflectedProjectileDamage);
		if (IsDead()) {
			return;
		}
	}
	const int laserHealAmount = attackManager_.ConsumeLaserHealAmount();
	if (laserHealAmount > 0 && bossHP_ > 0) {
		bossHP_ = (std::min)(GetMaxHP(), bossHP_ + laserHealAmount);
	}
	// 攻撃管理内の接近・突進による移動と回転を同じフレームに描画へ反映する。
	UpdateWorldTransform(worldTransform_);
	const bool isDown =
	    attackManager_.GetAttackState() == BossAttackManager::AttackState::Down;
	if (isDown) {
		++downAnimationFrame_;
	} else {
		downAnimationFrame_ = 0;
	}
	visualWorldTransform_.scale_ = worldTransform_.scale_;
	visualWorldTransform_.rotation_ = worldTransform_.rotation_;
	visualWorldTransform_.rotation_.y += kModelForwardYawOffset;
	if (isDown) {
		visualWorldTransform_.rotation_.z +=
		    std::sin(static_cast<float>(downAnimationFrame_) * kDownRockSpeed) *
		    kDownRockAmplitude;
	}
	visualWorldTransform_.translation_ = worldTransform_.translation_;
	visualWorldTransform_.translation_.y +=
	    kModelGroundOffset * visualWorldTransform_.scale_.y;
	UpdateWorldTransform(visualWorldTransform_);
	headVisualWorldTransform_.scale_ = visualWorldTransform_.scale_;
	headVisualWorldTransform_.rotation_ = visualWorldTransform_.rotation_;
	headVisualWorldTransform_.translation_ = visualWorldTransform_.translation_;
	headVisualWorldTransform_.translation_.y +=
	    kHeadHeightOffset * headVisualWorldTransform_.scale_.y;
	UpdateWorldTransform(headVisualWorldTransform_);

	switch (attackManager_.GetAttackState()) {
	case BossAttackManager::AttackState::Warning:
		// 攻撃直前は黄色にして予兆を伝える。
		objectColor_.SetColor({1.0f, 0.75f, 0.15f, 1.0f});
		break;
	case BossAttackManager::AttackState::Attacking:
		// 攻撃中は赤色にする。DDAからも同じ状態を参照できる。
		objectColor_.SetColor({1.0f, 0.2f, 0.2f, 1.0f});
		break;
	case BossAttackManager::AttackState::Down:
		// Playerのダウン演出と同じ赤紫の点滅で、攻撃チャンスを強調する。
		objectColor_.SetColor(
		    ((downAnimationFrame_ / 4) % 2) == 0
		        ? Vector4{1.35f, 0.08f, 0.25f, 1.0f}
		        : Vector4{0.42f, 0.03f, 0.12f, 1.0f});
		break;
	case BossAttackManager::AttackState::Idle:
	default:
		objectColor_.SetColor({1.0f, 1.0f, 1.0f, 1.0f});
		break;
	}

	// 被ダメージ色は攻撃予兆・攻撃中・ダウン中の色より優先する。
	// 明るさの異なる赤を交互に表示し、短いフラッシュとして認識しやすくする。
	if (damageFlashTimer_ > 0) {
		const bool isBrightFlash = ((damageFlashTimer_ / 2) % 2) == 0;
		objectColor_.SetColor(
		    isBrightFlash ? Vector4{1.8f, 0.02f, 0.02f, 1.0f}
		                  : Vector4{1.0f, 0.08f, 0.08f, 1.0f});
		--damageFlashTimer_;
	}
}

void Boss::UpdateDeathAnimation() {
	if (deathAnimationTimer_ < kDeathAnimationFrames) {
		++deathAnimationTimer_;
	}
	const float progress = std::clamp(
	    static_cast<float>(deathAnimationTimer_) /
	        static_cast<float>(kDeathAnimationFrames),
	    0.0f, 1.0f);
	const float whiteProgress = std::clamp(
	    static_cast<float>(deathAnimationTimer_) /
	        static_cast<float>(kDeathWhiteFlashFrames),
	    0.0f, 1.0f);
	const float fadeProgress = std::clamp(
	    static_cast<float>(deathAnimationTimer_ - kDeathWhiteFlashFrames) /
	        static_cast<float>(kDeathAnimationFrames - kDeathWhiteFlashFrames),
	    0.0f, 1.0f);

	UpdateWorldTransform(worldTransform_);
	const float expansion = 1.0f + 0.08f * progress;
	visualWorldTransform_.scale_ = {
	    worldTransform_.scale_.x * expansion,
	    worldTransform_.scale_.y * expansion,
	    worldTransform_.scale_.z * expansion};
	visualWorldTransform_.rotation_ = worldTransform_.rotation_;
	visualWorldTransform_.rotation_.y += kModelForwardYawOffset;
	visualWorldTransform_.rotation_.z +=
	    std::sin(static_cast<float>(deathAnimationTimer_) * 0.42f) *
	    0.10f * (1.0f - progress);
	visualWorldTransform_.translation_ = worldTransform_.translation_;
	visualWorldTransform_.translation_.y +=
	    kModelGroundOffset * visualWorldTransform_.scale_.y;
	UpdateWorldTransform(visualWorldTransform_);

	headVisualWorldTransform_.scale_ = visualWorldTransform_.scale_;
	headVisualWorldTransform_.rotation_ = visualWorldTransform_.rotation_;
	headVisualWorldTransform_.translation_ = visualWorldTransform_.translation_;
	headVisualWorldTransform_.translation_.y +=
	    kHeadHeightOffset * headVisualWorldTransform_.scale_.y;
	UpdateWorldTransform(headVisualWorldTransform_);

	const float whiteBrightness = 1.0f + 2.5f * whiteProgress;
	objectColor_.SetColor({
	    whiteBrightness, whiteBrightness, whiteBrightness,
	    1.0f - fadeProgress});
}

void Boss::UpdateRandomMovement() {
	if (randomMoveTimer_ <= 0) {
		randomState_ = randomState_ * 1664525u + 1013904223u;
		const float angleRatio =
		    static_cast<float>((randomState_ >> 8) & 0x00FFFFFFu) /
		    static_cast<float>(0x01000000u);
		const float angle = angleRatio * std::numbers::pi_v<float> * 2.0f;
		randomMoveDirection_ = {std::sin(angle), 0.0f, std::cos(angle)};
		randomState_ = randomState_ * 1664525u + 1013904223u;
		randomMoveTimer_ = kRandomMoveMinFrames +
		    static_cast<int>(randomState_ % kRandomMoveAdditionalFrames);
	}
	worldTransform_.translation_.x += randomMoveDirection_.x * kMoveSpeed;
	worldTransform_.translation_.z += randomMoveDirection_.z * kMoveSpeed;
	worldTransform_.rotation_.y =
	    std::atan2(randomMoveDirection_.x, randomMoveDirection_.z);
	--randomMoveTimer_;
}

void Boss::Draw() {
	if (model_ == nullptr || headModel_ == nullptr || camera_ == nullptr) {
		return;
	}
	const ObjectColor* drawColor = attackManager_.IsChargeDashing()
	    ? &chargeEnergyColor_
	    : &objectColor_;
	model_->Draw(visualWorldTransform_, *camera_, drawColor);
	headModel_->Draw(headVisualWorldTransform_, *camera_, drawColor);
	if (!IsDead()) {
		attackManager_.Draw();
	}
}

void Boss::ApplyDamage(int damage) {
	if (damage <= 0 || IsDead()) {
		return;
	}
	bossHP_ -= damage;
	// ゲージを削り切るたびに、次フェーズのHPを満タンで開始する。
	// 一撃が残りHPを超えた場合は、超過ダメージを次の層へ引き継ぐ。
	while (bossHP_ <= 0 && bossPhase_ < 3) {
		const int overflowDamage = -bossHP_;
		++bossPhase_;
		bossHP_ = GetMaxHP() - overflowDamage;
	}
	if (bossHP_ < 0) bossHP_ = 0;
	if (IsDead()) {
		attackManager_.StopAllAttackSounds();
	}
	attackManager_.SetBossPhase(bossPhase_);
	damageFlashTimer_ = kDamageFlashFrames;
	attackManager_.RegisterPlayerDamage(damage);
}

void Boss::ConstrainInsideArena(const Vector3& arenaCenter, float arenaRadius) {
	const float allowedRadius = arenaRadius - kCollisionRadius;
	if (allowedRadius <= 0.0f) return;
	float dx = worldTransform_.translation_.x - arenaCenter.x;
	float dz = worldTransform_.translation_.z - arenaCenter.z;
	const float distanceSquared = dx * dx + dz * dz;
	if (distanceSquared <= allowedRadius * allowedRadius) return;
	const float distance = std::sqrt(distanceSquared);
	if (distance > 0.0001f) {
		dx /= distance;
		dz /= distance;
	} else {
		dx = 0.0f;
		dz = 1.0f;
	}
	worldTransform_.translation_.x = arenaCenter.x + dx * allowedRadius;
	worldTransform_.translation_.z = arenaCenter.z + dz * allowedRadius;
	UpdateWorldTransform(worldTransform_);
}
