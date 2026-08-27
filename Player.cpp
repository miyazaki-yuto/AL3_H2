#include "Player.h"

#include "LerpManager.h"
#include "TransformUtility.h"

#include <algorithm>
#include <cmath>

using namespace KamataEngine;

void Player::Initialize(
    Model* model, Model* bulletModel, Model* bombModel, Model* effectModel,
    Model* attackModel, Model* slashLeftModel, Model* slashRightModel,
    Model* beamModel, Model* predictionModel, Camera* camera) {
	model_ = model;
	camera_ = camera;
	objectColor_.Initialize();
	objectColor_.SetColor({1.0f, 1.0f, 1.0f, 1.0f});
	chargeEnergyColor_.Initialize();
	chargeEnergyColor_.SetColor({0.0f, 0.72f, 1.0f, 9.0f});
	chargeEnergyTime_ = 0.0f;
	avoidanceSeHandle_ = Audio::GetInstance()->LoadWave("Audio/Game_Avoidance_SE.wav");
	damageSeHandle_ = Audio::GetInstance()->LoadWave("Audio/Game_PlayerDamage_SE.wav");
	worldTransform_.Initialize();
	worldTransform_.scale_ = {kBasePlayerScale, kBasePlayerScale, kBasePlayerScale};
	// ボスの初期位置 (5, 10) から十分離して、開幕直後の接触を防ぐ。
	worldTransform_.translation_ = {
	    -20.0f, kModelGroundOffset * kBasePlayerScale, -20.0f};
	worldTransform_.rotation_.y = kModelForwardYawOffset;
	playerHP_ = kBasePlayerHP;
	maxPlayerHP_ = kBasePlayerHP;
	playerScaleMultiplier_ = 1.0f;
	collisionRadius_ = kCollisionRadius;
	chargeSquashAmount_ = 0.0f;
	chargeSquashAnimationFrame_ = 0;
	invincibilityTimer_ = 0;
	dashTimer_ = 0;
	dashDecelerationTimer_ = 0;
	postDashSlowTimer_ = 0;
	dashCooldownTimer_ = 0;
	stunTimer_ = 0;
	dashDirection_ = {0.0f, 0.0f, 1.0f};
	aimDirection_ = {0.0f, 0.0f, 1.0f};
	knockbackVelocity_ = {0.0f, 0.0f, 0.0f};
	attackController_.Initialize(
	    bulletModel, bombModel, effectModel, attackModel, slashLeftModel,
	    slashRightModel, beamModel, predictionModel);
	UpdateWorldTransform(worldTransform_);
}

void Player::Update() {
	attackController_.SetOwnerPosition(worldTransform_.translation_);
	attackController_.Update();
	chargeEnergyTime_ = std::fmod(chargeEnergyTime_ + 0.035f, 1.0f);
	chargeEnergyColor_.SetColor({
	    0.0f, 0.72f, 1.0f, 9.0f + chargeEnergyTime_});
	UpdateChargeSquashAnimation();
	playerHP_ = (std::min)(maxPlayerHP_, playerHP_ + attackController_.ConsumeBerserkLifeSteal());
	const int steelGrowth = attackController_.ConsumeSteelGrowth();
	if (steelGrowth > 0) {
		const int gainedMaxHealth = kSteelMaxHealthPerHit * steelGrowth;
		maxPlayerHP_ += gainedMaxHealth;
		playerHP_ = (std::min)(maxPlayerHP_, playerHP_ + gainedMaxHealth);
	}
	const int steelBurstCount = attackController_.ConsumeSteelBurstCount();
	if (steelBurstCount > 0) {
		const int gainedMaxHealth = kSteelBurstMaxHealthGain * steelBurstCount;
		maxPlayerHP_ += gainedMaxHealth;
		// 増加HPを満たしたうえで、既存HPも30回復する。
		playerHP_ = (std::min)(
		    maxPlayerHP_, playerHP_ + gainedMaxHealth +
		                      kSteelBurstHealAmount * steelBurstCount);
	}
	UpdateSteelBodyScale();
	if (invincibilityTimer_ > 0) --invincibilityTimer_;
	if (dashCooldownTimer_ > 0) --dashCooldownTimer_;

	if (stunTimer_ > 0) {
		// ダウン中は赤紫に点滅させ、左右へ大きく揺らして操作不能状態を明示する。
		const bool isBrightFlash = ((stunTimer_ / 4) % 2) == 0;
		objectColor_.SetColor(
		    isBrightFlash ? Vector4{1.35f, 0.08f, 0.25f, 1.0f}
		                  : Vector4{0.42f, 0.03f, 0.12f, 1.0f});
		worldTransform_.rotation_.z =
		    std::sin(static_cast<float>(stunTimer_) * kStunRockSpeed) *
		    kStunRockAmplitude;
		worldTransform_.translation_.x += knockbackVelocity_.x;
		worldTransform_.translation_.z += knockbackVelocity_.z;
		knockbackVelocity_.x *= kKnockbackDamping;
		knockbackVelocity_.z *= kKnockbackDamping;
		--stunTimer_;
		if (stunTimer_ == 0) {
			knockbackVelocity_ = {0.0f, 0.0f, 0.0f};
			worldTransform_.rotation_.z = 0.0f;
			objectColor_.SetColor({1.0f, 1.0f, 1.0f, 1.0f});
		}
		UpdateWorldTransform(worldTransform_);
		return;
	}
	// 外部処理でスタンが解除された場合にも表示を通常状態へ戻す。
	worldTransform_.rotation_.z = 0.0f;
	objectColor_.SetColor({1.0f, 1.0f, 1.0f, 1.0f});
	const Input* const input = Input::GetInstance();
	Vector3 move = {0.0f, 0.0f, 0.0f};
	Vector3 aimInput = {0.0f, 0.0f, 0.0f};
	bool dashRequested = false;
	bool attackRequested = false;
	bool attackTriggered = false;
	if (input != nullptr) {
		if (input->PushKey(DIK_W)) move.z += 1.0f;
		if (input->PushKey(DIK_S)) move.z -= 1.0f;
		if (input->PushKey(DIK_A)) move.x -= 1.0f;
		if (input->PushKey(DIK_D)) move.x += 1.0f;
		if (input->PushKey(DIK_UP)) aimInput.z += 1.0f;
		if (input->PushKey(DIK_DOWN)) aimInput.z -= 1.0f;
		if (input->PushKey(DIK_LEFT)) aimInput.x -= 1.0f;
		if (input->PushKey(DIK_RIGHT)) aimInput.x += 1.0f;
		dashRequested = input->TriggerKey(DIK_LSHIFT);
		attackRequested = input->PushKey(DIK_SPACE);
		attackTriggered = input->TriggerKey(DIK_SPACE);

		XINPUT_STATE joystickState{};
		if (input->GetJoystickState(kControllerIndex, joystickState)) {
			constexpr float kStickMax = 32767.0f;
			const float stickX = static_cast<float>(joystickState.Gamepad.sThumbLX) / kStickMax;
			const float stickZ = static_cast<float>(joystickState.Gamepad.sThumbLY) / kStickMax;
			const float stickLength = std::sqrt(stickX * stickX + stickZ * stickZ);
			if (stickLength > kLeftStickDeadZone) {
				const float clampedLength = (stickLength > 1.0f) ? 1.0f : stickLength;
				const float analogPower =
				    (clampedLength - kLeftStickDeadZone) / (1.0f - kLeftStickDeadZone);
				move.x += (stickX / stickLength) * analogPower;
				move.z += (stickZ / stickLength) * analogPower;
			}
			const float aimX = static_cast<float>(joystickState.Gamepad.sThumbRX) / kStickMax;
			const float aimZ = static_cast<float>(joystickState.Gamepad.sThumbRY) / kStickMax;
			const float aimLength = std::sqrt(aimX * aimX + aimZ * aimZ);
			if (aimLength > kLeftStickDeadZone) {
				aimInput.x = aimX / aimLength;
				aimInput.z = aimZ / aimLength;
			}
			attackRequested = attackRequested ||
			                  joystickState.Gamepad.bRightTrigger >= kRightTriggerThreshold;

			XINPUT_STATE previousJoystickState{};
			if (input->GetJoystickStatePrevious(kControllerIndex, previousJoystickState)) {
				const bool isDashButtonPressed =
				    (joystickState.Gamepad.wButtons & XINPUT_GAMEPAD_A) != 0;
				const bool wasDashButtonPressed =
				    (previousJoystickState.Gamepad.wButtons & XINPUT_GAMEPAD_A) != 0;
				dashRequested = dashRequested || (isDashButtonPressed && !wasDashButtonPressed);
				attackTriggered = attackTriggered ||
				    (joystickState.Gamepad.bRightTrigger >= kRightTriggerThreshold &&
				     previousJoystickState.Gamepad.bRightTrigger < kRightTriggerThreshold);
			}
		}
	}
	if (attackController_.IsBerserkActive()) {
		const Vector3 berserkMove = attackController_.GetBerserkMoveDirection(worldTransform_.translation_);
		const float berserkStrength = kBerserkAutoMoveStrength *
		    static_cast<float>(attackController_.GetBerserkStackCount());
		move.x += berserkMove.x * berserkStrength;
		move.z += berserkMove.z * berserkStrength;
	}

	const float aimInputLength = std::sqrt(aimInput.x * aimInput.x + aimInput.z * aimInput.z);
	if (!attackController_.IsBeamActive()) {
		if (aimInputLength > 0.0f) {
			aimDirection_ = {aimInput.x / aimInputLength, 0.0f, aimInput.z / aimInputLength};
		} else {
			const float moveInputLength = std::sqrt(move.x * move.x + move.z * move.z);
			if (moveInputLength > 0.0f) {
				aimDirection_ = {move.x / moveInputLength, 0.0f, move.z / moveInputLength};
			}
		}
		if (aimInputLength > 0.0f || move.x != 0.0f || move.z != 0.0f) {
			const float targetRotationY =
			    std::atan2(aimDirection_.x, aimDirection_.z) + kModelForwardYawOffset;
			const float rotationDifference = std::atan2(
			    std::sin(targetRotationY - worldTransform_.rotation_.y),
			    std::cos(targetRotationY - worldTransform_.rotation_.y));
			worldTransform_.rotation_.y += rotationDifference * kRotationInterpolation;
		}
	}

	if (attackController_.ApplyActiveAttack(
	        worldTransform_, attackRequested, attackTriggered, aimDirection_)) {
		UpdateWorldTransform(worldTransform_);
		return;
	}
	if (attackRequested && attackController_.TryActivate(worldTransform_, aimDirection_)) {
		UpdateWorldTransform(worldTransform_);
		return;
	}
	if (dashRequested && dashTimer_ == 0 && dashDecelerationTimer_ == 0 &&
	    dashCooldownTimer_ == 0) {
		dashDirection_ = aimDirection_;
		dashTimer_ = kDashDurationFrames;
		dashCooldownTimer_ = kDashCooldownFrames;
		Audio::GetInstance()->PlayWave(avoidanceSeHandle_, false, 0.72f);
	}
	if (dashTimer_ > 0) {
		worldTransform_.translation_.x += dashDirection_.x * kDashSpeed;
		worldTransform_.translation_.z += dashDirection_.z * kDashSpeed;
		if (--dashTimer_ == 0) dashDecelerationTimer_ = kDashDecelerationFrames;
		UpdateWorldTransform(worldTransform_);
		return;
	}
	if (dashDecelerationTimer_ > 0) {
		const int elapsedFrames = kDashDecelerationFrames - dashDecelerationTimer_ + 1;
		const float t = static_cast<float>(elapsedFrames) / kDashDecelerationFrames;
		const float speed = LerpManager::Lerp(
		    kDashSpeed, 0.0f, t, LerpManager::EaseType::SmoothStep);
		worldTransform_.translation_.x += dashDirection_.x * speed;
		worldTransform_.translation_.z += dashDirection_.z * speed;
		if (--dashDecelerationTimer_ == 0) postDashSlowTimer_ = kPostDashSlowFrames;
		UpdateWorldTransform(worldTransform_);
		return;
	}

	const float moveLength = std::sqrt(move.x * move.x + move.z * move.z);
	if (moveLength > 0.0f) {
		if (moveLength > 1.0f) {
			move.x /= moveLength;
			move.z /= moveLength;
		}
		const float speed = kMoveSpeed * attackController_.GetIncreaseMoveSpeedMultiplier() *
		                    attackController_.GetSteelMoveSpeedMultiplier() *
		                    ((postDashSlowTimer_ > 0) ? kPostDashSlowMultiplier : 1.0f);
		worldTransform_.translation_.x += move.x * speed;
		worldTransform_.translation_.z += move.z * speed;
	}
	if (postDashSlowTimer_ > 0) --postDashSlowTimer_;
	UpdateWorldTransform(worldTransform_);
}

void Player::ApplyDamage(int damage, bool cancelActiveSkill) {
	if (damage <= 0 || invincibilityTimer_ > 0 || IsDead()) return;
	Audio::GetInstance()->PlayWave(damageSeHandle_, false, 0.86f);
	playerHP_ -= damage;
	if (playerHP_ < 0) playerHP_ = 0;
	UpdateSteelBodyScale();
	if (cancelActiveSkill) {
		attackController_.CancelActiveSkill();
	}
	invincibilityTimer_ = kInvincibilityFrames;
}

void Player::UpdateSteelBodyScale() {
	// 最大HPそのものは成長として維持するが、体格は現在残っている
	// 基礎HP100を超えた分だけに連動させる。増加HPが削られるほど縮み、
	// 全て失うと初期サイズへ戻る。回復すれば同じ比率で再び大きくなる。
	const int remainingBonusHealth = (std::max)(0, playerHP_ - kBasePlayerHP);
	const float remainingGrowthUnits =
	    static_cast<float>(remainingBonusHealth) /
	    static_cast<float>(kSteelMaxHealthPerHit);
	playerScaleMultiplier_ = 1.0f + kSteelScalePerHit * remainingGrowthUnits;
	collisionRadius_ = kCollisionRadius * playerScaleMultiplier_;
	const float pulse = attackController_.IsChargingSubSkill()
	    ? std::sin(static_cast<float>(chargeSquashAnimationFrame_) *
	               kChargeSquashPulseSpeed) *
	          kChargeSquashPulseAmount
	    : 0.0f;
	const float heightMultiplier = (std::max)(
	    0.65f, 1.0f - chargeSquashAmount_ *
	                     (kChargeSquashMaximum + pulse));
	worldTransform_.scale_ = {
	    kBasePlayerScale * playerScaleMultiplier_,
	    kBasePlayerScale * playerScaleMultiplier_ * heightMultiplier,
	    kBasePlayerScale * playerScaleMultiplier_,
	};
	// サイズが変わってもPlayerの底面を地面へ接地させる。
	worldTransform_.translation_.y =
	    kModelGroundOffset * worldTransform_.scale_.y;
}

void Player::UpdateChargeSquashAnimation() {
	const float targetAmount =
	    attackController_.IsChargingSubSkill() ? 1.0f : 0.0f;
	chargeSquashAmount_ +=
	    (targetAmount - chargeSquashAmount_) * kChargeSquashInterpolation;
	if (std::abs(targetAmount - chargeSquashAmount_) < 0.001f) {
		chargeSquashAmount_ = targetAmount;
	}
	if (attackController_.IsChargingSubSkill()) {
		++chargeSquashAnimationFrame_;
	} else if (chargeSquashAmount_ == 0.0f) {
		chargeSquashAnimationFrame_ = 0;
	}
}

void Player::ResolveObstacleCollisionXZ(const Vector3& obstacleCenter, float obstacleRadius) {
	if (obstacleRadius <= 0.0f) return;
	const float minimumDistance = obstacleRadius + collisionRadius_;
	const float differenceX = worldTransform_.translation_.x - obstacleCenter.x;
	const float differenceZ = worldTransform_.translation_.z - obstacleCenter.z;
	const float distanceSquared = differenceX * differenceX + differenceZ * differenceZ;
	if (distanceSquared >= minimumDistance * minimumDistance) return;
	float normalX = 0.0f;
	float normalZ = 0.0f;
	if (distanceSquared > 0.000001f) {
		const float inverseDistance = 1.0f / std::sqrt(distanceSquared);
		normalX = differenceX * inverseDistance;
		normalZ = differenceZ * inverseDistance;
	} else {
		normalX = aimDirection_.x;
		normalZ = aimDirection_.z;
	}
	worldTransform_.translation_.x = obstacleCenter.x + normalX * minimumDistance;
	worldTransform_.translation_.z = obstacleCenter.z + normalZ * minimumDistance;
	UpdateWorldTransform(worldTransform_);
}

void Player::ConstrainInsideArena(const Vector3& arenaCenter, float arenaRadius) {
	const float allowedRadius = arenaRadius - collisionRadius_;
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

void Player::ApplyKnockbackAndStun(
    const Vector3& sourcePosition, float knockbackForce, int stunFrames) {
	if (knockbackForce <= 0.0f || stunFrames <= 0 || IsDead()) return;
	float directionX = worldTransform_.translation_.x - sourcePosition.x;
	float directionZ = worldTransform_.translation_.z - sourcePosition.z;
	const float directionLengthSquared = directionX * directionX + directionZ * directionZ;
	if (directionLengthSquared > 0.000001f) {
		const float inverseLength = 1.0f / std::sqrt(directionLengthSquared);
		directionX *= inverseLength;
		directionZ *= inverseLength;
	} else {
		directionX = aimDirection_.x;
		directionZ = aimDirection_.z;
	}
	knockbackVelocity_ = {directionX * knockbackForce, 0.0f, directionZ * knockbackForce};
	if (stunFrames > stunTimer_) stunTimer_ = stunFrames;
	dashTimer_ = 0;
	dashDecelerationTimer_ = 0;
}

void Player::Draw() {
	if (model_ == nullptr || camera_ == nullptr) return;
	const ObjectColor* drawColor = attackController_.IsChargeAttackActive()
	    ? &chargeEnergyColor_
	    : &objectColor_;
	model_->Draw(worldTransform_, *camera_, drawColor);
	attackController_.Draw(*camera_);
}
