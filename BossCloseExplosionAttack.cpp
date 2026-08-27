#include "BossCloseExplosionAttack.h"

#include "ColliderManager.h"
#include "LerpManager.h"
#include "Player.h"
#include "TransformUtility.h"

using namespace KamataEngine;

void BossCloseExplosionAttack::Initialize(
    Model* predictionCircleModel,
    Camera* camera,
    Player* player) {
	predictionCircleModel_ = predictionCircleModel;
	camera_ = camera;
	player_ = player;
	forecastStartSeHandle_ = Audio::GetInstance()->LoadWave(
	    "Audio/Game_BossExplosionForecastCircleStart_SE.wav");
	explosionSeHandle_ = Audio::GetInstance()->LoadWave(
	    "Audio/Game_BossExplosion_SE.wav");
	worldTransform_.Initialize();
	objectColor_.Initialize();
	objectColor_.SetColor({1.0f, 0.8f, 0.1f, 0.7f});
	state_ = State::Inactive;
	stateTimer_ = 0;
	warningProgress_ = 0.0f;
	explosionEffectProgress_ = 0.0f;
	hasDealtDamage_ = false;
}

void BossCloseExplosionAttack::Start(const Vector3& bossPosition) {
	if (predictionCircleModel_ == nullptr || camera_ == nullptr || player_ == nullptr) {
		return;
	}

	state_ = State::Warning;
	stateTimer_ = kWarningFrames;
	warningProgress_ = 0.0f;
	explosionEffectProgress_ = 0.0f;
	hasDealtDamage_ = false;
	objectColor_.SetColor({1.0f, 0.8f, 0.1f, 0.7f});
	UpdateTransform(bossPosition);
	Audio::GetInstance()->PlayWave(forecastStartSeHandle_, false, 0.76f);
}

void BossCloseExplosionAttack::Update(const Vector3& bossPosition) {
	if (!IsActive()) {
		return;
	}

	if (state_ == State::Warning) {
		if (stateTimer_ <= 0) {
			state_ = State::Explosion;
			stateTimer_ = kExplosionFrames;
			warningProgress_ = 1.0f;
			// alpha=4はObjPSでBoss爆発専用HLSLを選択するマーカー。
			objectColor_.SetColor({1.0f, 0.1f, 0.05f, 4.0f});
			Audio::GetInstance()->PlayWave(explosionSeHandle_, false, 0.88f);
		} else {
			const int elapsedFrames = kWarningFrames - stateTimer_;
			warningProgress_ = static_cast<float>(elapsedFrames) /
			                   static_cast<float>(kWarningFrames - 1);
			--stateTimer_;
		}
	}

	UpdateTransform(bossPosition);

	if (state_ == State::Explosion) {
		explosionEffectProgress_ = 1.0f -
		    static_cast<float>(stateTimer_) / static_cast<float>(kExplosionFrames);
		// 共有モデルの未使用UV-offset Zを爆発進行度として専用HLSLへ送る。
		for (const auto& mesh : predictionCircleModel_->GetMeshes()) {
			if (mesh && mesh->GetMaterial()) {
				mesh->GetMaterial()->uvOffset_.z = explosionEffectProgress_;
				mesh->GetMaterial()->Update();
			}
		}
		CheckPlayerCollision();
		--stateTimer_;
		if (stateTimer_ <= 0) {
			state_ = State::Inactive;
		}
	}
}

void BossCloseExplosionAttack::Draw() {
	if (!IsActive() || predictionCircleModel_ == nullptr || camera_ == nullptr) {
		return;
	}

	predictionCircleModel_->Draw(worldTransform_, *camera_, &objectColor_);
}

void BossCloseExplosionAttack::UpdateTransform(const Vector3& bossPosition) {
	const float explosionRadius = kExplosionRadius * attackRangeMultiplier_;
	float displayRadius = explosionRadius;
	if (state_ == State::Warning) {
		displayRadius = LerpManager::Lerp(
		    explosionRadius * kPredictionStartScaleRatio,
		    explosionRadius,
		    warningProgress_,
		    LerpManager::EaseType::SmootherStep);
	}
	worldTransform_.scale_ = {displayRadius, 1.0f, displayRadius};
	worldTransform_.translation_ = {
	    bossPosition.x,
	    bossPosition.y + kDisplayHeight,
	    bossPosition.z,
	};
	UpdateWorldTransform(worldTransform_);
}

void BossCloseExplosionAttack::CheckPlayerCollision() {
	if (hasDealtDamage_ || player_ == nullptr) {
		return;
	}

	const ColliderManager::CircleXZ explosionCollider = {
	    worldTransform_.translation_,
	    kExplosionRadius * attackRangeMultiplier_,
	};
	const ColliderManager::CircleXZ playerCollider = {
	    player_->GetWorldTransform().translation_,
	    player_->GetCollisionRadius(),
	};

	if (ColliderManager::CheckCircleCircleXZ(explosionCollider, playerCollider)) {
		player_->ApplyDamage(kDamage);
		hasDealtDamage_ = true;
	}
}
