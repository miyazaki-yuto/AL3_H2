#include "PlayerAttackController.h"

#include "Boss.h"
#include "ColliderManager.h"
#include "LerpManager.h"
#include "TransformUtility.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <random>

using namespace KamataEngine;

PlayerAttackController::~PlayerAttackController() {
	// Audioのループ再生は再生元オブジェクトの破棄だけでは停止しないため、
	// クリア・ゲームオーバーなどのシーン遷移時に明示的に終了する。
	StopAllSounds();
}

void PlayerAttackController::StopAllSounds() {
	StopChargeSounds();
	StopBeamSounds();
}

void PlayerAttackController::Initialize(
    Model* bulletModel, Model* bombModel, Model* effectModel, Model* attackModel,
    Model* slashLeftModel, Model* slashRightModel,
    Model* beamModel, Model* predictionModel) {
	bulletModel_ = bulletModel;
	bombModel_ = bombModel;
	effectModel_ = effectModel;
	attackModel_ = attackModel;
	slashLeftModel_ = slashLeftModel;
	slashRightModel_ = slashRightModel;
	beamModel_ = beamModel;
	predictionModel_ = predictionModel;
	slashSeHandle_ = Audio::GetInstance()->LoadWave("Audio/Game_Slash_SE.wav");
	beamShotSeHandle_ = Audio::GetInstance()->LoadWave("Audio/Game_BeamShot_SE.wav");
	bulletShotSeHandle_ = Audio::GetInstance()->LoadWave("Audio/Game_PlayerBulletShot_SE.wav");
	bulletHitSeHandle_ = Audio::GetInstance()->LoadWave("Audio/Game_PlayerBulletHit_SE.wav");
	bombPlaceSeHandle_ = Audio::GetInstance()->LoadWave("Audio/Game_PlayerBombPlace_SE.wav");
	bombTickSeHandle_ = Audio::GetInstance()->LoadWave("Audio/Game_PlayerBombTick_SE.wav");
	bombExplosionSeHandle_ = Audio::GetInstance()->LoadWave("Audio/Game_PlayerBombExplosion_SE.wav");
	chargeStartSeHandle_ = Audio::GetInstance()->LoadWave("Audio/Game_PlayerCharge_SE.wav");
	chargeLoopSeHandle_ = Audio::GetInstance()->LoadWave("Audio/Game_PlayerChargeLoop_SE.wav");
	chargeMaxSeHandle_ = Audio::GetInstance()->LoadWave("Audio/Game_PlayerChargeMax_SE.wav");
	dashStartSeHandle_ = Audio::GetInstance()->LoadWave("Audio/Game_PlayerDashStart_SE.wav");
	dashReflectSeHandle_ = Audio::GetInstance()->LoadWave("Audio/Game_PlayerDashReflect_SE.wav");
	beamLoopSeHandle_ = Audio::GetInstance()->LoadWave("Audio/Game_PlayerBeamLoop_SE.wav");
	predictionColor_.Initialize();
	predictionColor_.SetColor({1.0f, 0.8f, 0.1f, 0.75f});
	beamColor_.Initialize();
	// alpha=3はObjPSでPlayerビーム専用HLSLを選択するマーカー。
	beamColor_.SetColor({0.0f, 0.82f, 1.0f, 3.0f});
	beamImpactColor_.Initialize();
	beamImpactColor_.SetColor({0.0f, 0.82f, 1.0f, 17.0f});
	mainSkill_ = MainSkillType::Bullet;
	subSkillController_.SetSubSkills({SubSkillType::None, SubSkillType::None});
	bullets_.clear();
	flyingAttacks_.clear();
	bombs_.clear();
	explosionParticles_.clear();
	feedbackParticles_.clear();
	beamRetractions_.clear();
	reflectedBeamSegments_.clear();
	slashTransform_.Initialize();
	slashVisualTransform_.Initialize();
	beamTransform_.Initialize();
	beamImpactTransform_.Initialize();
	bulletCooldownTimer_ = 0;
	bulletAttackLockTimer_ = 0;
	slashTimer_ = 0;
	slashCooldownTimer_ = 0;
	chargeAttackTimer_ = 0;
	chargeAttackContactGuard_ = false;
	chargeAttackCooldownTimer_ = 0;
	deployCooldownTimer_ = 0;
	deployAttackLockTimer_ = 0;
	nextBombPredictionLayer_ = 0;
	chargeAttackDirection_ = {0.0f, 0.0f, 1.0f};
	chargeVelocity_ = {0.0f, 0.0f, 0.0f};
	slashHasDealtDamage_ = false;
	slashSlideDirection_ = {1.0f, 0.0f, 0.0f};
	slashSlideProgress_ = 0.0f;
	nextSlashSlidesRight_ = true;
	activeSlashSlidesRight_ = true;
	chargeHasDealtDamage_ = false;
	chargeReflectionCount_ = 0;
	isBeamActive_ = false;
	beamEffectTime_ = 0.0f;
	beamOrigin_ = {0.0f, 0.0f, 0.0f};
	beamDirection_ = {0.0f, 0.0f, 1.0f};
	beamExtendTimer_ = 0;
	beamExtendDurationFrames_ = 1;
	beamDamageIntervalTimer_ = 0;
	beamActiveTimer_ = 0;
	beamCooldownTimer_ = 0;
	isBeamBlockedByEnemy_ = false;
	beamHasPersistentBossContact_ = false;
	beamIgnoresBossOnCurrentSegment_ = false;
	beamContinuesWithoutHold_ = false;
	beamStopRequested_ = false;
	requiresBeamRelease_ = false;
	requiresAttackRelease_ = false;
	beamReflectionCount_ = 0;
	beamDamage_ = kBeamDamage;
	beamLength_ = kBeamLength;
	beamMaximumLength_ = kBeamLength;
	beamStartOffset_ = kBeamStartOffset;
	isChargingSubSkill_ = false;
	chargeFrames_ = 0;
	maxChargeHoldFrames_ = 0;
	chargeAimDirection_ = {0.0f, 0.0f, 1.0f};
	pendingChargeDamageMultiplier_ = 1.0f;
	pendingChargeRangeMultiplier_ = 1.0f;
	pendingBerserkLifeSteal_ = 0.0f;
	pendingSteelGrowth_ = 0;
	pendingSteelBurstCount_ = 0;
	pendingSuccessfulHitCount_ = 0;
	pendingBossHitCount_ = 0;
	pendingChargeGrowthEffectCount_ = 0;
	steelHitCount_ = 0;
	steelBurstHitCount_ = 0;
	steelBurstBuffTimer_ = 0;
	increaseHitCount_ = 0;
	chargeHitCount_ = 0;
	berserkHitCount_ = 0;
	chargeLoopVoiceHandle_ = 0;
	chargeStartVoiceHandle_ = 0;
	beamLoopVoiceHandle_ = 0;
	beamShotVoiceHandle_ = 0;
	isChargeLoopPlaying_ = false;
	isChargeStartPlaying_ = false;
	isBeamLoopPlaying_ = false;
	isBeamShotPlaying_ = false;
	hasPlayedChargeMaxSe_ = false;
	elapsedGameFrames_ = 0;
	beamWidth_ = kBeamWidth;
	chargeAttackHitRadius_ = 3.0f;
	arenaCenter_ = {};
	arenaRadius_ = 0.0f;
}

Vector3 PlayerAttackController::GetBerserkMoveDirection(const Vector3& playerPosition) const {
	if (!subSkillController_.IsBerserkActive() || targetBoss_ == nullptr || targetBoss_->IsDead()) return {};
	Vector3 direction = {targetBoss_->GetWorldTransform().translation_.x - playerPosition.x, 0.0f,
	                     targetBoss_->GetWorldTransform().translation_.z - playerPosition.z};
	const float length = std::sqrt(direction.x * direction.x + direction.z * direction.z);
	if (length <= 0.0001f) return {};
	direction.x /= length;
	direction.z /= length;
	return direction;
}

int PlayerAttackController::ConsumeBerserkLifeSteal() {
	// 小数分は次の命中へ繰り越し、成長量を失わないようにする。
	const int value = static_cast<int>(pendingBerserkLifeSteal_);
	pendingBerserkLifeSteal_ -= static_cast<float>(value);
	return value;
}

void PlayerAttackController::StopChargeSounds() {
	Audio* const audio = Audio::GetInstance();
	if (isChargeStartPlaying_) {
		audio->StopWave(chargeStartVoiceHandle_);
		isChargeStartPlaying_ = false;
	}
	if (isChargeLoopPlaying_) {
		audio->StopWave(chargeLoopVoiceHandle_);
		isChargeLoopPlaying_ = false;
	}
}

void PlayerAttackController::StopBeamSounds() {
	Audio* const audio = Audio::GetInstance();
	if (isBeamShotPlaying_) {
		audio->StopWave(beamShotVoiceHandle_);
		isBeamShotPlaying_ = false;
	}
	if (isBeamLoopPlaying_) {
		audio->StopWave(beamLoopVoiceHandle_);
		isBeamLoopPlaying_ = false;
	}
}

int PlayerAttackController::ConsumeSteelGrowth() {
	const int value = pendingSteelGrowth_;
	pendingSteelGrowth_ = 0;
	return value;
}

int PlayerAttackController::ConsumeSteelBurstCount() {
	const int value = pendingSteelBurstCount_;
	pendingSteelBurstCount_ = 0;
	return value;
}

int PlayerAttackController::ConsumeSuccessfulHitCount() {
	const int value = pendingSuccessfulHitCount_;
	pendingSuccessfulHitCount_ = 0;
	return value;
}

int PlayerAttackController::ConsumeBossHitCount() {
	const int value = pendingBossHitCount_;
	pendingBossHitCount_ = 0;
	return value;
}

float PlayerAttackController::GetSteelMoveSpeedMultiplier() const {
	if (!subSkillController_.IsStealActive()) return 1.0f;
	const float growthMultiplier = (std::max)(
	    kSteelMinimumSpeedMultiplier,
	    1.0f - kSteelMoveSpeedPenaltyPerHit * steelHitCount_);
	return growthMultiplier *
	    (steelBurstBuffTimer_ > 0 ? kSteelBurstMoveSpeedMultiplier : 1.0f);
}

float PlayerAttackController::GetChargeProgress() const {
	if (!isChargingSubSkill_) {
		return 0.0f;
	}
	return std::clamp(
	    static_cast<float>(chargeFrames_) /
	        static_cast<float>(GetRequiredChargeFrames()),
	    0.0f, 1.0f);
}

int PlayerAttackController::GetRequiredChargeFrames() const {
	const int reducedFrames = chargeHitCount_ * kChargeFramesReducedPerHit;
	return (std::max)(kChargeMinimumFrames, kChargeMaxFrames - reducedFrames);
}

float PlayerAttackController::GetChargeDamageMultiplier() const {
	if (!isChargingSubSkill_) {
		return pendingChargeDamageMultiplier_;
	}
	const float progress = GetChargeProgress();
	return 1.0f + (GetChargeMaximumDamageMultiplier() - 1.0f) * progress * progress;
}

float PlayerAttackController::GetChargeRangeMultiplier() const {
	if (!isChargingSubSkill_) {
		return pendingChargeRangeMultiplier_;
	}
	const float progress = GetChargeProgress();
	return 1.0f + (GetChargeMaximumRangeMultiplier() - 1.0f) * progress * progress;
}

float PlayerAttackController::GetChargeMaximumDamageMultiplier() const {
	const float elapsedIntervals =
	    static_cast<float>(elapsedGameFrames_) / kChargeGrowthIntervalFrames;
	const float singleStackMultiplier =
	    kChargeMaxDamageMultiplier + kChargeDamageGrowthPerInterval * elapsedIntervals;
	return 1.0f + (singleStackMultiplier - 1.0f) *
	    static_cast<float>(subSkillController_.GetStackCount(SubSkillType::Charge));
}

float PlayerAttackController::GetChargeMaximumRangeMultiplier() const {
	const float elapsedIntervals =
	    static_cast<float>(elapsedGameFrames_) / kChargeGrowthIntervalFrames;
	const float singleStackMultiplier =
	    kChargeMaxRangeMultiplier + kChargeRangeGrowthPerInterval * elapsedIntervals;
	return 1.0f + (singleStackMultiplier - 1.0f) *
	    static_cast<float>(subSkillController_.GetStackCount(SubSkillType::Charge));
}

void PlayerAttackController::Update() {
	if (steelBurstBuffTimer_ > 0) --steelBurstBuffTimer_;
	++elapsedGameFrames_;
	beamEffectTime_ += 1.0f / 60.0f;
	beamImpactColor_.SetColor(
	    {0.0f, 0.82f, 1.0f, 17.0f + std::fmod(beamEffectTime_, 0.999f)});
	// PlayerBeam専用モデルのUV-offset ZをHLSLの時間入力として使用する。
	if (beamModel_ != nullptr) {
		for (const auto& mesh : beamModel_->GetMeshes()) {
			if (mesh && mesh->GetMaterial()) {
				mesh->GetMaterial()->uvOffset_.z = beamEffectTime_;
				mesh->GetMaterial()->Update();
			}
		}
	}
	Audio* const audio = Audio::GetInstance();
	if (isChargingSubSkill_) {
		if (!isChargeLoopPlaying_) {
			chargeLoopVoiceHandle_ =
			    audio->PlayWave(chargeLoopSeHandle_, true, 0.48f);
			isChargeLoopPlaying_ = true;
		}
		if (chargeFrames_ >= GetRequiredChargeFrames() && !hasPlayedChargeMaxSe_) {
			audio->PlayWave(chargeMaxSeHandle_, false, 0.82f);
			hasPlayedChargeMaxSe_ = true;
		}
	} else {
		StopChargeSounds();
		hasPlayedChargeMaxSe_ = false;
	}
	// 発射本体と終了収束アニメーションの両方が消えた後に音を止める。
	if (!isBeamActive_ && beamRetractions_.empty() &&
	    reflectedBeamSegments_.empty()) {
		StopBeamSounds();
	}
	chargeAttackContactGuard_ = false;
	UpdateBullets();
	UpdateBombs();
	UpdateExplosionParticles();
	UpdateFeedbackParticles();
	UpdateFlyingAttacks();
	if (bulletCooldownTimer_ > 0) --bulletCooldownTimer_;
	if (bulletAttackLockTimer_ > 0) --bulletAttackLockTimer_;
	if (slashTimer_ > 0) --slashTimer_;
	if (slashCooldownTimer_ > 0) --slashCooldownTimer_;
	if (chargeAttackCooldownTimer_ > 0) --chargeAttackCooldownTimer_;
	if (deployCooldownTimer_ > 0) --deployCooldownTimer_;
	if (deployAttackLockTimer_ > 0) --deployAttackLockTimer_;
	if (beamExtendTimer_ > 0) --beamExtendTimer_;
	if (beamCooldownTimer_ > 0) --beamCooldownTimer_;
	if (isBeamActive_ && beamActiveTimer_ > 0) --beamActiveTimer_;
	for (const auto& beamRetraction : beamRetractions_) {
		--beamRetraction->remainingFrames;
		if (beamRetraction->remainingFrames > 0) {
			UpdateBeamRetractionTransform(*beamRetraction);
		} else if (!beamRetraction->followingSegments.empty()) {
			std::unique_ptr<WorldTransform> nextSegment =
			    std::move(beamRetraction->followingSegments.front());
			beamRetraction->followingSegments.erase(
			    beamRetraction->followingSegments.begin());
			SetBeamRetractionSegment(*beamRetraction, *nextSegment);
		}
		ApplyBeamRetractionDamage(*beamRetraction);
	}
	std::erase_if(
	    beamRetractions_,
	    [](const std::unique_ptr<BeamRetraction>& beamRetraction) {
			return beamRetraction->remainingFrames <= 0 &&
			       beamRetraction->followingSegments.empty();
	    });
}

void PlayerAttackController::CancelActiveSkill() {
	// 発射済みの bullets_ と設置済みの bombs_ は攻撃結果として残す。
	isChargingSubSkill_ = false;
	chargeFrames_ = 0;
	maxChargeHoldFrames_ = 0;
	pendingChargeDamageMultiplier_ = 1.0f;
	pendingChargeRangeMultiplier_ = 1.0f;

	slashTimer_ = 0;
	bulletAttackLockTimer_ = 0;
	deployAttackLockTimer_ = 0;
	chargeAttackTimer_ = 0;
	chargeAttackContactGuard_ = false;
	chargeVelocity_ = {0.0f, 0.0f, 0.0f};

	if (isBeamActive_) {
		// 中断地点の形を使って、通常の終了アニメーションへ移行する。
		BeginBeamRetraction(beamTransform_);
		reflectedBeamSegments_.clear();
	}
	isBeamActive_ = false;
	beamExtendTimer_ = 0;
	beamExtendDurationFrames_ = 1;
	beamActiveTimer_ = 0;
	isBeamBlockedByEnemy_ = false;
	beamHasPersistentBossContact_ = false;
	beamIgnoresBossOnCurrentSegment_ = false;
	beamContinuesWithoutHold_ = false;
	beamStopRequested_ = false;
	requiresBeamRelease_ = true;
	requiresAttackRelease_ = true;
	StopChargeSounds();
	hasPlayedChargeMaxSe_ = false;
}

bool PlayerAttackController::ApplyActiveAttack(
    WorldTransform& playerTransform, bool isAttackHeld, bool isAttackTriggered,
    const Vector3& aimDirection) {
	// 突進反射中のチャージは、下の突進更新と同時に進める。
	// ここで処理すると移動更新前にreturnして滑走が止まってしまう。
	if (isChargingSubSkill_ && chargeAttackTimer_ <= 0) {
		// チャージ中も現在の狙い方向へ追従し、開始時の古い方向を残さない。
		chargeAimDirection_ = aimDirection;
		const int requiredChargeFrames = GetRequiredChargeFrames();
		if (isAttackHeld) {
			if (chargeFrames_ < requiredChargeFrames) {
				++chargeFrames_;
				return true;
			} else {
				++maxChargeHoldFrames_;
				if (maxChargeHoldFrames_ < kChargeAutoFireDelayFrames) {
					return true;
				}
			}
		} else {
			maxChargeHoldFrames_ = 0;
		}
		const float ratio = std::clamp(
		    static_cast<float>(chargeFrames_) /
		        static_cast<float>(requiredChargeFrames),
		    0.0f, 1.0f);
		// チャージ後半ほど増幅量が大きくなるように、時間比率を二乗して使う。
		const float amplifiedRatio = ratio * ratio;
		pendingChargeDamageMultiplier_ = 1.0f +
		    (GetChargeMaximumDamageMultiplier() - 1.0f) * amplifiedRatio;
		pendingChargeRangeMultiplier_ = 1.0f +
		    (GetChargeMaximumRangeMultiplier() - 1.0f) * amplifiedRatio;
		isChargingSubSkill_ = false;
		chargeFrames_ = 0;
		maxChargeHoldFrames_ = 0;
		StopChargeSounds();
		// 発射時の見た目と攻撃方向を一致させ、現在位置から攻撃を生成する。
		playerTransform.rotation_.y =
		    std::atan2(chargeAimDirection_.x, chargeAimDirection_.z) +
		    kPlayerModelForwardYawOffset;
		UpdateWorldTransform(playerTransform);
		return ActivateCurrentSkill(playerTransform, chargeAimDirection_);
	}
	if (slashTimer_ > 0 || bulletAttackLockTimer_ > 0 || deployAttackLockTimer_ > 0) {
		if (slashTimer_ > 0) {
			const int slideFrameCount = (std::max)(1, kSlashActiveFrames - 1);
			const int elapsedSlideFrames =
			    std::clamp(kSlashActiveFrames - slashTimer_, 0, slideFrameCount);
			const float slideTime = static_cast<float>(elapsedSlideFrames) /
			                        static_cast<float>(slideFrameCount);
			const float newSlideProgress = LerpManager::ApplyEasing(
			    slideTime, LerpManager::EaseType::SmootherStep);
			const float slideDelta =
			    (newSlideProgress - slashSlideProgress_) * kSlashSlideDistance;
			const float previousPlayerX = playerTransform.translation_.x;
			const float previousPlayerZ = playerTransform.translation_.z;
			playerTransform.translation_.x += slashSlideDirection_.x * slideDelta;
			playerTransform.translation_.z += slashSlideDirection_.z * slideDelta;
			slashSlideProgress_ = newSlideProgress;
			const float playerCollisionRadius =
			    playerTransform.scale_.x * kPlayerCollisionRadiusPerVisualScale;
			ConstrainPositionInsideArena(
			    playerTransform.translation_, playerCollisionRadius);
			// 壁で移動量が制限された場合も、斬撃をPlayerの実移動量へ一致させる。
			slashTransform_.translation_.x +=
			    playerTransform.translation_.x - previousPlayerX;
			slashTransform_.translation_.z +=
			    playerTransform.translation_.z - previousPlayerZ;
			UpdateWorldTransform(playerTransform);
			UpdateWorldTransform(slashTransform_);
			const float visualSideOffset =
			    (-1.0f + newSlideProgress * 2.0f) *
			    kSlashVisualSweepHalfDistance;
			slashVisualTransform_.translation_ = {
			    slashTransform_.translation_.x +
			        slashSlideDirection_.x * visualSideOffset,
			    slashTransform_.translation_.y,
			    slashTransform_.translation_.z +
			        slashSlideDirection_.z * visualSideOffset,
			};
			UpdateWorldTransform(slashVisualTransform_);
		}
		if (slashTimer_ > 0 && subSkillController_.IsReflectActive() && targetBoss_ != nullptr) {
			const float reflectRadius = kSlashForwardOffset * GetAttackSizeMultiplier() *
			    static_cast<float>(subSkillController_.GetStackCount(SubSkillType::Hansho));
			const int reflectedCount = targetBoss_->ReflectEnemyProjectilesTowardTargetInCircle(
			    slashTransform_.translation_, reflectRadius,
			    targetBoss_->GetWorldTransform().translation_);
			if (reflectedCount > 0) {
				SpawnFeedbackBurst(
				    slashTransform_.translation_, {1.0f, 0.78f, 0.08f, 1.0f},
				    FeedbackParticle::Motion::Spark, 10 + reflectedCount * 2);
			}
		}
		if (slashTimer_ > 0 && !slashHasDealtDamage_ &&
		    IsBossInCircle(slashTransform_.translation_, kSlashForwardOffset * GetAttackSizeMultiplier())) {
			DamageBoss(slashDamage_);
			slashHasDealtDamage_ = true;
		}
		if (slashTimer_ > 0 && targetBoss_ != nullptr) {
			const bool hitMob = targetBoss_->DamageMobsInCircle(
			    slashTransform_.translation_, kSlashForwardOffset * GetAttackSizeMultiplier(), slashDamage_);
			if (hitMob) {
				RegisterEnemyHit(slashDamage_);
			}
		}
		return true;
	}
	if (chargeAttackTimer_ > 0) {
		// 反射突進で滑っている最中も、メインスキルのクールタイムが
		// 終了していればサブスキルのチャージを並行して溜められる。
		if (subSkillController_.IsChargeActive()) {
			if (!isChargingSubSkill_ && isAttackTriggered &&
			    chargeAttackCooldownTimer_ == 0) {
				isChargingSubSkill_ = true;
				chargeStartVoiceHandle_ =
				    Audio::GetInstance()->PlayWave(chargeStartSeHandle_, false, 0.72f);
				isChargeStartPlaying_ = true;
				chargeFrames_ = 0;
				maxChargeHoldFrames_ = 0;
				chargeAimDirection_ = aimDirection;
			}
			if (isChargingSubSkill_) {
				chargeAimDirection_ = aimDirection;
				const int requiredChargeFrames = GetRequiredChargeFrames();
				bool releaseCharge = false;
				if (isAttackHeld) {
					if (chargeFrames_ < requiredChargeFrames) {
						++chargeFrames_;
					} else {
						++maxChargeHoldFrames_;
						releaseCharge =
						    maxChargeHoldFrames_ >= kChargeAutoFireDelayFrames;
					}
				} else {
					releaseCharge = true;
				}

				if (releaseCharge && chargeAttackCooldownTimer_ == 0) {
					const float ratio = std::clamp(
					    static_cast<float>(chargeFrames_) /
					        static_cast<float>(requiredChargeFrames),
					    0.0f, 1.0f);
					const float amplifiedRatio = ratio * ratio;
					pendingChargeDamageMultiplier_ = 1.0f +
					    (GetChargeMaximumDamageMultiplier() - 1.0f) * amplifiedRatio;
					pendingChargeRangeMultiplier_ = 1.0f +
					    (GetChargeMaximumRangeMultiplier() - 1.0f) * amplifiedRatio;
					isChargingSubSkill_ = false;
					chargeFrames_ = 0;
					maxChargeHoldFrames_ = 0;
					StopChargeSounds();
					playerTransform.rotation_.y =
					    std::atan2(chargeAimDirection_.x, chargeAimDirection_.z) +
					    kPlayerModelForwardYawOffset;
					UpdateWorldTransform(playerTransform);
					return ActivateCharge(chargeAimDirection_);
				}
			}
		}
		// クールタイム完了後に再入力すると、体ごと狙い方向へ切り返して再突進する。
		if (!subSkillController_.IsChargeActive() &&
		    isAttackTriggered && chargeAttackCooldownTimer_ == 0) {
			playerTransform.rotation_.y =
			    std::atan2(aimDirection.x, aimDirection.z) +
			    kPlayerModelForwardYawOffset;
			UpdateWorldTransform(playerTransform);
			return ActivateCharge(aimDirection);
		}
		// このフレームのBoss接触は、通常ノックバックではなく突進反射で処理する。
		chargeAttackContactGuard_ = true;
		if (chargeAttackTimer_ <= kChargeAttackDecelerationFrames) {
			// 終了間際は抵抗で速度を落とす。
			chargeVelocity_.x *= kChargeAttackEndDrag;
			chargeVelocity_.z *= kChargeAttackEndDrag;
		} else {
			// 加速度を積み、最高速は超えないようにする。
			chargeVelocity_.x += chargeAttackDirection_.x * kChargeAttackAcceleration;
			chargeVelocity_.z += chargeAttackDirection_.z * kChargeAttackAcceleration;
			const float speed = std::sqrt(
			    chargeVelocity_.x * chargeVelocity_.x + chargeVelocity_.z * chargeVelocity_.z);
			if (speed > kChargeAttackSpeed) {
				const float scale = kChargeAttackSpeed / speed;
				chargeVelocity_.x *= scale;
				chargeVelocity_.z *= scale;
			}
		}
		playerTransform.translation_.x += chargeVelocity_.x;
		playerTransform.translation_.z += chargeVelocity_.z;
		--chargeAttackTimer_;
		UpdateWorldTransform(playerTransform);
		const bool hitArenaWall = ReflectChargeFromArena(playerTransform);
		if (hitArenaWall && subSkillController_.IsReflectActive()) {
			Audio::GetInstance()->PlayWave(dashReflectSeHandle_, false, 0.82f);
			SpawnFeedbackBurst(
			    playerTransform.translation_, {1.0f, 0.78f, 0.08f, 1.0f},
			    FeedbackParticle::Motion::Spark, 14);
		}
		if (hitArenaWall && !subSkillController_.IsReflectActive()) {
			// 反射を選んでいない突進は壁で止める。
			chargeAttackTimer_ = 0;
			return true;
		}
		const float playerCollisionRadius =
		    playerTransform.scale_.x * kPlayerCollisionRadiusPerVisualScale;
		const float chargeContactRadius =
		    (std::max)(chargeAttackHitRadius_, playerCollisionRadius);
		if (IsBossInCircle(playerTransform.translation_, chargeContactRadius)) {
			if (!chargeHasDealtDamage_) {
				DamageBoss(chargeAttackDamage_);
				chargeHasDealtDamage_ = true;
			}
			if (subSkillController_.IsReflectActive()) {
				ReflectChargeFromBoss(playerTransform);
				Audio::GetInstance()->PlayWave(dashReflectSeHandle_, false, 0.82f);
				SpawnFeedbackBurst(
				    playerTransform.translation_, {1.0f, 0.78f, 0.08f, 1.0f},
				    FeedbackParticle::Motion::Spark, 16);
				chargeHasDealtDamage_ = false;
			} else {
				// 通常突進はBossを通り抜けず、接触面で停止する。
				StopChargeAtBoss(playerTransform);
			}
		}
		if (targetBoss_ != nullptr) {
			const bool hitMob = targetBoss_->DamageMobsInCircle(
			    playerTransform.translation_, chargeAttackHitRadius_, chargeAttackDamage_);
			if (hitMob) {
				RegisterEnemyHit(chargeAttackDamage_);
			}
		}
		return true;
	}
	if (isBeamActive_) {
		const bool endedByTimeLimit = beamActiveTimer_ <= 0;
		if ((!isAttackHeld && !beamContinuesWithoutHold_) || endedByTimeLimit) {
			// ボタンを離した位置で空中停止させず、壁か敵に届くまで伸長を続ける。
			beamStopRequested_ = true;
			if (endedByTimeLimit) {
				requiresBeamRelease_ = true;
			}
		}
		if (!isBeamBlockedByEnemy_) {
			if (beamExtendTimer_ > 0) {
				UpdateBeamExtensionTransform();
			} else {
				UpdateBeamTransform(beamDirection_);
			}
		}
		const ColliderManager::Segment beamSegment = {
		    {beamOrigin_.x + beamDirection_.x * beamStartOffset_,
		     beamOrigin_.y,
		     beamOrigin_.z + beamDirection_.z * beamStartOffset_},
		    {beamTransform_.translation_.x + beamDirection_.x * beamTransform_.scale_.z,
		     beamTransform_.translation_.y,
		     beamTransform_.translation_.z + beamDirection_.z * beamTransform_.scale_.z},
		};
		const bool hitBoss = !beamIgnoresBossOnCurrentSegment_ &&
		    targetBoss_ != nullptr && ColliderManager::CheckSegmentCircleXZ(
		    beamSegment,
		    {targetBoss_->GetWorldTransform().translation_,
		     targetBoss_->GetCollisionRadius() + beamWidth_});
		bool reachedStopSurface = isBeamBlockedByEnemy_;
		if (hitBoss) {
				const Vector3 bossPosition = targetBoss_->GetWorldTransform().translation_;
				const float toBossX = bossPosition.x - beamOrigin_.x;
				const float toBossZ = bossPosition.z - beamOrigin_.z;
				const float centerDistance = toBossX * beamDirection_.x + toBossZ * beamDirection_.z;
				const float impactDistance = (std::max)(
				    beamStartOffset_,
				    centerDistance - targetBoss_->GetCollisionRadius());
				if (subSkillController_.IsReflectActive() &&
				    beamReflectionCount_ < kMaxBeamReflectionCount *
				        subSkillController_.GetStackCount(SubSkillType::Hansho)) {
					UpdateBeamTransformToTipDistance(beamOrigin_, impactDistance);
					SaveCurrentBeamSegment();
					beamHasPersistentBossContact_ = true;
					const Vector3 reflectionOrigin = {
					    beamOrigin_.x + beamDirection_.x * impactDistance,
					    beamOrigin_.y,
					    beamOrigin_.z + beamDirection_.z * impactDistance,
					};
					beamIgnoresBossOnCurrentSegment_ = true;
					ReflectBeamInRandomOppositeDirection(reflectionOrigin);
				} else if (!isBeamBlockedByEnemy_) {
					UpdateBeamTransformToTipDistance(beamOrigin_, impactDistance);
					isBeamBlockedByEnemy_ = true;
					beamHasPersistentBossContact_ = true;
					reachedStopSurface = true;
				}
		}

		Vector3 mobHitPosition = {};
		const bool hitMob = !hitBoss && targetBoss_ != nullptr &&
		    targetBoss_->DamageMobsAlongSegment(
		        beamSegment, beamDamage_, &mobHitPosition, beamWidth_);
		if (hitMob) {
			RegisterEnemyHit(beamDamage_);
			const float hitDistance = (std::max)(
			    beamStartOffset_,
			    (mobHitPosition.x - beamOrigin_.x) * beamDirection_.x +
			        (mobHitPosition.z - beamOrigin_.z) * beamDirection_.z);
			if (subSkillController_.IsReflectActive() &&
			    beamReflectionCount_ < kMaxBeamReflectionCount *
			        subSkillController_.GetStackCount(SubSkillType::Hansho)) {
				// Mobとの命中地点までを一本の線分として確定し、そこから反射線を伸ばす。
				UpdateBeamTransformToTipDistance(beamOrigin_, hitDistance);
				SaveCurrentBeamSegment();
				beamIgnoresBossOnCurrentSegment_ = false;
				ReflectBeamInRandomOppositeDirection(mobHitPosition);
			} else if (subSkillController_.IsReflectActive() || beamStopRequested_) {
				UpdateBeamTransformToTipDistance(beamOrigin_, hitDistance);
				isBeamBlockedByEnemy_ = true;
				reachedStopSurface = true;
			}
		}

		// 反射後に残っている線分も実体のあるビームとして扱う。
		// 後から線分内へ入ったMobにも攻撃を適用する。
		if (targetBoss_ != nullptr) {
			for (const auto& reflectedSegment : reflectedBeamSegments_) {
				const WorldTransform& transform = reflectedSegment->worldTransform;
				const Vector3 direction = {
				    std::sin(transform.rotation_.y), 0.0f,
				    std::cos(transform.rotation_.y)};
				const ColliderManager::Segment segment = {
				    {transform.translation_.x - direction.x * transform.scale_.z,
				     transform.translation_.y,
				     transform.translation_.z - direction.z * transform.scale_.z},
				    {transform.translation_.x + direction.x * transform.scale_.z,
				     transform.translation_.y,
				     transform.translation_.z + direction.z * transform.scale_.z},
				};
				if (targetBoss_->DamageMobsAlongSegment(
				        segment, beamDamage_, nullptr, beamWidth_)) {
					RegisterEnemyHit(beamDamage_);
				}
			}
		}

		if (!hitBoss && !hitMob && beamExtendTimer_ <= 0 && arenaRadius_ > 0.0f) {
			const float arenaTipDistance = GetDistanceToArenaEdge(
			    beamOrigin_, beamDirection_, beamWidth_);
			const float currentTipDistance =
			    beamStartOffset_ + beamTransform_.scale_.z * 2.0f;
			if (currentTipDistance >= arenaTipDistance - 0.01f) {
				if (subSkillController_.IsReflectActive() &&
				    beamReflectionCount_ < kMaxBeamReflectionCount *
				        subSkillController_.GetStackCount(SubSkillType::Hansho)) {
					SaveCurrentBeamSegment();
					ReflectBeamFromArena(beamSegment.end);
				} else {
					reachedStopSurface = true;
				}
			}
		}
		if (beamDamageIntervalTimer_ > 0) {
			--beamDamageIntervalTimer_;
		} else {
			if (targetBoss_ != nullptr && (hitBoss || beamHasPersistentBossContact_)) {
				DamageBoss(beamDamage_);
			}
			beamDamageIntervalTimer_ = kBeamDamageIntervalFrames;
		}
		if (beamStopRequested_ && reachedStopSurface) {
			isBeamActive_ = false;
			beamContinuesWithoutHold_ = false;
			beamStopRequested_ = false;
			// 壁または敵まで届いた完成形から、一本のビームとして収束させる。
			BeginBeamRetraction(beamTransform_);
			reflectedBeamSegments_.clear();
			return false;
		}
		return true;
	}
	if (!isAttackHeld) {
		requiresBeamRelease_ = false;
		requiresAttackRelease_ = false;
	}
	return false;
}

bool PlayerAttackController::TryActivate(
    WorldTransform& playerTransform, const Vector3& aimDirection) {
	if (requiresAttackRelease_) return false;
	if (mainSkill_ == MainSkillType::Beam) {
		// ビームだけが先に曲がって見えないよう、発射時に体の正面を狙い方向へ確定する。
		playerTransform.rotation_.y =
		    std::atan2(aimDirection.x, aimDirection.z) +
		    kPlayerModelForwardYawOffset;
		UpdateWorldTransform(playerTransform);
	}
	if (subSkillController_.IsChargeActive()) {
		// 攻撃直後のクールタイムをチャージ時間として先取りできないようにする。
		// 使用可能になってから改めて押した入力だけを受け付ける。
		if (GetMainSkillCooldownRatio() < 1.0f) {
			return false;
		}
		isChargingSubSkill_ = true;
		chargeStartVoiceHandle_ =
		    Audio::GetInstance()->PlayWave(chargeStartSeHandle_, false, 0.72f);
		isChargeStartPlaying_ = true;
		chargeFrames_ = 0;
		maxChargeHoldFrames_ = 0;
		chargeAimDirection_ = aimDirection;
		return true;
	}
	return ActivateCurrentSkill(playerTransform, aimDirection);
}

bool PlayerAttackController::ActivateCurrentSkill(
    WorldTransform& playerTransform, const Vector3& aimDirection) {
	switch (mainSkill_) {
	case MainSkillType::Bullet:
		return ActivateBullet(playerTransform, aimDirection);
	case MainSkillType::Slash:
		return ActivateSlash(playerTransform, aimDirection);
	case MainSkillType::Charge:
		return ActivateCharge(aimDirection);
	case MainSkillType::Deploy:
		return ActivateDeploy(playerTransform, aimDirection);
	case MainSkillType::Beam:
		return ActivateBeam(playerTransform, aimDirection);
	default:
		return false;
	}
}

void PlayerAttackController::Draw(const Camera& camera) const {
	if (attackModel_ == nullptr) return;
	for (const auto& bullet : bullets_) {
		bulletModel_->Draw(bullet->worldTransform, camera);
	}
	for (const auto& flyingAttack : flyingAttacks_) {
		// 反翔で追加生成される弾は、発生元にかかわらずPlayerBulletモデルへ統一する。
		bulletModel_->Draw(flyingAttack->worldTransform, camera);
	}
	for (const auto& bomb : bombs_) {
		if (bomb->fuseTimer > 0 && predictionModel_ != nullptr) {
			predictionModel_->Draw(bomb->predictionTransform, camera, &predictionColor_);
		}
		if (bomb->fuseTimer > 0) {
			bombModel_->Draw(bomb->worldTransform, camera);
		}
		// 設置地点全体へBoss爆破と同形の青い面状エフェクトを重ねる。
		if (bomb->fuseTimer <= 0 && bomb->explosionTimer > 0 &&
		    predictionModel_ != nullptr) {
			predictionModel_->Draw(
			    bomb->predictionTransform, camera, &bomb->explosionColor);
		}
	}
	for (const auto& particle : explosionParticles_) {
		effectModel_->Draw(particle->worldTransform, camera, &particle->color);
	}
	for (const auto& particle : feedbackParticles_) {
		effectModel_->Draw(particle->worldTransform, camera, &particle->color);
	}
	if (slashTimer_ > 0) {
		Model* slashModel = activeSlashSlidesRight_ ? slashRightModel_ : slashLeftModel_;
		if (slashModel != nullptr) {
			slashModel->Draw(slashVisualTransform_, camera);
		} else {
			attackModel_->Draw(slashVisualTransform_, camera);
		}
	}
	if (isBeamActive_) {
		beamModel_->Draw(beamTransform_, camera, &beamColor_);
		for (const auto& beamSegment : reflectedBeamSegments_) {
			beamModel_->Draw(beamSegment->worldTransform, camera, &beamColor_);
		}
		if (predictionModel_ != nullptr) {
			predictionModel_->Draw(beamImpactTransform_, camera, &beamImpactColor_);
		}
	}
	for (const auto& beamRetraction : beamRetractions_) {
		for (const auto& followingSegment : beamRetraction->followingSegments) {
			beamModel_->Draw(*followingSegment, camera, &beamColor_);
		}
		beamModel_->Draw(beamRetraction->worldTransform, camera, &beamColor_);
	}
}

bool PlayerAttackController::GetAttackLightPosition(Vector3& position) const {
	// 反射でbeamOrigin_が移動しても、ライトはPlayer側の最初の発射地点へ固定する。
	if (isBeamActive_) {
		position = primaryBeamOrigin_;
		return true;
	}
	if (slashTimer_ > 0) {
		position = slashOrigin_;
		return true;
	}
	// 設置攻撃は爆発前に表示される予測円の中心を照らす。
	for (auto iterator = bombs_.rbegin(); iterator != bombs_.rend(); ++iterator) {
		if (*iterator != nullptr && (*iterator)->fuseTimer > 0) {
			position = (*iterator)->predictionTransform.translation_;
			return true;
		}
	}
	// 複数発射時は新しい弾を代表にする。消滅した弾はライト対象にしない。
	for (auto iterator = bullets_.rbegin(); iterator != bullets_.rend(); ++iterator) {
		if (*iterator != nullptr && (*iterator)->isAlive) {
			position = (*iterator)->worldTransform.translation_;
			return true;
		}
	}
	return false;
}

bool PlayerAttackController::ActivateBullet(
    const WorldTransform& playerTransform, const Vector3& aimDirection) {
	if (bulletModel_ == nullptr || bulletCooldownTimer_ > 0) return false;
	auto bullet = std::make_unique<Bullet>();
	bullet->worldTransform.Initialize();
	const float scale = kBulletScale * pendingChargeRangeMultiplier_ * GetAttackSizeMultiplier();
	bullet->worldTransform.scale_ = {scale, scale, scale};
	bullet->worldTransform.translation_ = {
	    playerTransform.translation_.x + aimDirection.x * kBulletSpawnOffset,
	    scale,
	    playerTransform.translation_.z + aimDirection.z * kBulletSpawnOffset,
	};
	const float bulletSpeedMultiplier = subSkillController_.IsIncreaseActive() ?
	    (std::max)(kIncreaseMinimumSpeedMultiplier,
	               1.0f - kIncreaseBulletSpeedPenaltyPerHit * increaseHitCount_) : 1.0f;
	bullet->velocity = {aimDirection.x * kBulletSpeed * bulletSpeedMultiplier, 0.0f,
	                    aimDirection.z * kBulletSpeed * bulletSpeedMultiplier};
	// 弾モデルの前方を+Zとして、発射時から進行方向へ向ける。
	bullet->worldTransform.rotation_.y =
	    std::atan2(bullet->velocity.x, bullet->velocity.z) +
	    kBulletModelForwardYawOffset;
	bullet->damage = static_cast<int>(kBulletDamage * pendingChargeDamageMultiplier_ * GetDamageMultiplier());
	bullet->hasReflected = subSkillController_.IsReflectActive();
	UpdateWorldTransform(bullet->worldTransform);
	bullets_.push_back(std::move(bullet));
	Audio::GetInstance()->PlayWave(bulletShotSeHandle_, false, 0.68f);
	pendingChargeDamageMultiplier_ = 1.0f;
	pendingChargeRangeMultiplier_ = 1.0f;
	bulletCooldownDuration_ = GetAttackInterval(kBulletIntervalFrames);
	bulletCooldownTimer_ = bulletCooldownDuration_;
	bulletAttackLockTimer_ = kBulletAttackLockFrames;
	return true;
}

bool PlayerAttackController::ActivateSlash(
    WorldTransform& playerTransform, const Vector3& aimDirection) {
	if (attackModel_ == nullptr || slashCooldownTimer_ > 0) return false;
	Audio::GetInstance()->PlayWave(slashSeHandle_, false, 0.78f);
	// 斬撃開始時に狙い方向へ短く踏み込む。
	playerTransform.translation_.x += aimDirection.x * kSlashLungeDistance;
	playerTransform.translation_.z += aimDirection.z * kSlashLungeDistance;
	playerTransform.rotation_.y =
	    std::atan2(aimDirection.x, aimDirection.z) +
	    kPlayerModelForwardYawOffset;
	activeSlashSlidesRight_ = nextSlashSlidesRight_;
	const float slideSide = activeSlashSlidesRight_ ? 1.0f : -1.0f;
	// 照準方向に対する右ベクトル。次の斬撃では符号を反転する。
	slashSlideDirection_ = {
	    aimDirection.z * slideSide, 0.0f,
	    -aimDirection.x * slideSide};
	slashSlideProgress_ = 0.0f;
	nextSlashSlidesRight_ = !nextSlashSlidesRight_;
	const float playerCollisionRadius =
	    playerTransform.scale_.x * kPlayerCollisionRadiusPerVisualScale;
	ConstrainPositionInsideArena(playerTransform.translation_, playerCollisionRadius);
	// 踏み込みでBossへめり込んだ場合は、接触面まで戻して通常接触ノックバックを防ぐ。
	if (targetBoss_ != nullptr && !targetBoss_->IsDead()) {
		const Vector3 bossPosition = targetBoss_->GetWorldTransform().translation_;
		float normalX = playerTransform.translation_.x - bossPosition.x;
		float normalZ = playerTransform.translation_.z - bossPosition.z;
		const float distance = std::sqrt(normalX * normalX + normalZ * normalZ);
		const float minimumDistance = targetBoss_->GetCollisionRadius() +
		    playerCollisionRadius + kSlashBossSeparationMargin;
		if (distance < minimumDistance) {
			if (distance > 0.0001f) {
				normalX /= distance;
				normalZ /= distance;
			} else {
				normalX = -aimDirection.x;
				normalZ = -aimDirection.z;
			}
			playerTransform.translation_.x = bossPosition.x + normalX * minimumDistance;
			playerTransform.translation_.z = bossPosition.z + normalZ * minimumDistance;
		}
	}
	UpdateWorldTransform(playerTransform);
	const float sizeMultiplier = pendingChargeRangeMultiplier_ * GetAttackSizeMultiplier();
	slashTransform_.scale_ = {4.5f * sizeMultiplier, 0.15f, 3.0f * sizeMultiplier};
	slashTransform_.rotation_.y = std::atan2(aimDirection.x, aimDirection.z);
	// Playerが巨大化するとBossとの接触距離も広がるため、増加した半径分だけ
	// 斬撃の中心を前へ出し、見た目と当たり判定がBossまで届くようにする。
	const float playerGrowthForwardOffset =
	    (std::max)(0.0f, playerCollisionRadius - kBasePlayerCollisionRadius);
	const float slashForwardOffset =
	    kSlashForwardOffset * sizeMultiplier + playerGrowthForwardOffset;
	slashTransform_.translation_ = {
	    playerTransform.translation_.x + aimDirection.x * slashForwardOffset,
	    kPlayerAttackBaseHeight,
	    playerTransform.translation_.z + aimDirection.z * slashForwardOffset,
	};
	slashOrigin_ = playerTransform.translation_;
	slashOrigin_.y = kPlayerAttackBaseHeight;
	UpdateWorldTransform(slashTransform_);
	slashVisualTransform_.scale_ = slashTransform_.scale_;
	slashVisualTransform_.rotation_ = slashTransform_.rotation_;
	slashVisualTransform_.translation_ = {
	    slashTransform_.translation_.x -
	        slashSlideDirection_.x * kSlashVisualSweepHalfDistance,
	    slashTransform_.translation_.y,
	    slashTransform_.translation_.z -
	        slashSlideDirection_.z * kSlashVisualSweepHalfDistance,
	};
	UpdateWorldTransform(slashVisualTransform_);
	slashTimer_ = kSlashActiveFrames;
	slashCooldownDuration_ = GetAttackInterval(kSlashIntervalFrames);
	slashCooldownTimer_ = slashCooldownDuration_;
	slashHasDealtDamage_ = false;
	slashDamage_ = static_cast<int>(kSlashDamage * pendingChargeDamageMultiplier_ * GetDamageMultiplier());
	if (subSkillController_.IsReflectActive()) {
		SpawnFlyingSlashes(playerTransform, aimDirection, sizeMultiplier);
	}
	pendingChargeDamageMultiplier_ = 1.0f;
	pendingChargeRangeMultiplier_ = 1.0f;
	return true;
}

bool PlayerAttackController::ActivateCharge(const Vector3& aimDirection) {
	if (chargeAttackCooldownTimer_ > 0) return false;
	Audio::GetInstance()->PlayWave(dashStartSeHandle_, false, 0.78f);
	chargeAttackDirection_ = aimDirection;
	chargeVelocity_ = {0.0f, 0.0f, 0.0f};
	chargeAttackTimer_ = static_cast<int>(kChargeAttackFrames * pendingChargeRangeMultiplier_ * GetAttackSizeMultiplier());
	if (subSkillController_.IsReflectActive()) {
		chargeAttackTimer_ *= subSkillController_.GetStackCount(SubSkillType::Hansho);
	}
	chargeAttackHitRadius_ = 3.0f * GetAttackSizeMultiplier();
	chargeAttackCooldownDuration_ = GetAttackInterval(kChargeAttackCooldownFrames);
	chargeAttackCooldownTimer_ = chargeAttackCooldownDuration_;
	chargeHasDealtDamage_ = false;
	chargeAttackDamage_ = static_cast<int>(kChargeAttackDamage * pendingChargeDamageMultiplier_ * GetDamageMultiplier());
	pendingChargeDamageMultiplier_ = 1.0f;
	pendingChargeRangeMultiplier_ = 1.0f;
	chargeReflectionCount_ = 0;
	return true;
}

bool PlayerAttackController::ActivateDeploy(
    const WorldTransform& playerTransform, const Vector3& aimDirection) {
	if (bombModel_ == nullptr || effectModel_ == nullptr || deployCooldownTimer_ > 0) return false;
	auto bomb = std::make_unique<Bomb>();
	bomb->worldTransform.Initialize();
	bomb->explosionColor.Initialize();
	bomb->explosionColor.SetColor({0.0f, 0.82f, 1.0f, 7.0f});
	// 増大やチャージによる攻撃サイズ倍率を、爆発範囲だけでなく爆弾本体にも反映する。
	const float sizeMultiplier =
	    pendingChargeRangeMultiplier_ * GetAttackSizeMultiplier();
	const float bombVisualScale = kBombScale * sizeMultiplier;
	bomb->worldTransform.scale_ = {
	    bombVisualScale, bombVisualScale, bombVisualScale};
	bomb->worldTransform.translation_ = {
	    playerTransform.translation_.x + aimDirection.x * kBombForwardOffset,
	    kBombModelGroundOffset * bombVisualScale,
	    playerTransform.translation_.z + aimDirection.z * kBombForwardOffset,
	};
	ConstrainPositionInsideArena(bomb->worldTransform.translation_, bombVisualScale);
	bomb->predictionTransform.Initialize();
	bomb->explosionScale = kBombExplosionScale * sizeMultiplier;
	bomb->damage = static_cast<int>(kBombDamage * pendingChargeDamageMultiplier_ * GetDamageMultiplier());
	bomb->predictionTransform.scale_ = {bomb->explosionScale, 1.0f, bomb->explosionScale};
	const float predictionHeight = kBombPredictionDisplayHeight +
	    static_cast<float>(nextBombPredictionLayer_) * kBombPredictionLayerStep;
	bomb->predictionTransform.translation_ = {
	    bomb->worldTransform.translation_.x,
	    predictionHeight,
	    bomb->worldTransform.translation_.z,
	};
	nextBombPredictionLayer_ =
	    (nextBombPredictionLayer_ + 1) % kBombPredictionLayerCount;
	bomb->fuseTimer = kBombFuseFrames;
	UpdateWorldTransform(bomb->worldTransform);
	UpdateWorldTransform(bomb->predictionTransform);
	bombs_.push_back(std::move(bomb));
	Audio::GetInstance()->PlayWave(bombPlaceSeHandle_, false, 0.72f);
	pendingChargeDamageMultiplier_ = 1.0f;
	pendingChargeRangeMultiplier_ = 1.0f;
	deployCooldownDuration_ = GetAttackInterval(kBombIntervalFrames);
	deployCooldownTimer_ = deployCooldownDuration_;
	deployAttackLockTimer_ = kBombAttackLockFrames;
	return true;
}

bool PlayerAttackController::ActivateBeam(
    const WorldTransform& playerTransform, const Vector3& aimDirection) {
	if (beamModel_ == nullptr || requiresBeamRelease_ || beamCooldownTimer_ > 0) return false;
	StopBeamSounds();
	beamShotVoiceHandle_ =
	    Audio::GetInstance()->PlayWave(beamShotSeHandle_, false, 0.82f);
	isBeamShotPlaying_ = true;
	beamLoopVoiceHandle_ =
	    Audio::GetInstance()->PlayWave(beamLoopSeHandle_, true, 0.54f);
	isBeamLoopPlaying_ = true;
	isBeamActive_ = true;
	beamOrigin_ = playerTransform.translation_;
	// SteelなどでPlayerが成長しても、ビーム後端を必ず体の正面より外へ出す。
	beamStartOffset_ = (std::max)(
	    kBeamStartOffset,
	    playerTransform.scale_.z * kPlayerForwardExtentPerVisualScale +
	        kBeamMuzzleMargin);
	reflectedBeamSegments_.clear();
	beamDirection_ = aimDirection;
	beamDamageIntervalTimer_ = 0;
	beamActiveTimer_ = kBeamActiveFrames;
	beamCooldownDuration_ = GetAttackInterval(kBeamCooldownFrames);
	beamCooldownTimer_ = beamCooldownDuration_;
	isBeamBlockedByEnemy_ = false;
	beamHasPersistentBossContact_ = false;
	beamIgnoresBossOnCurrentSegment_ = false;
	beamReflectionCount_ = 0;
	beamStopRequested_ = false;
	beamContinuesWithoutHold_ = pendingChargeDamageMultiplier_ > 1.0f ||
	    pendingChargeRangeMultiplier_ > 1.0f;
	beamDamage_ = static_cast<int>(kBeamDamage * pendingChargeDamageMultiplier_ * GetDamageMultiplier());
	beamMaximumLength_ = kBeamLength * pendingChargeRangeMultiplier_;
	const float chargeBeamWidthMultiplier =
	    1.0f + (pendingChargeRangeMultiplier_ - 1.0f) * kChargeBeamWidthBonusScale;
	beamWidth_ = kBeamWidth * chargeBeamWidthMultiplier * GetAttackSizeMultiplier();
	// 増大で太くなっても下端が地面へ入らないよう、中心を半径分まで持ち上げる。
	beamOrigin_.y = (std::max)(kPlayerAttackBaseHeight, beamWidth_);
	primaryBeamOrigin_ = beamOrigin_;
	beamLength_ = (std::min)(beamMaximumLength_, (std::max)(
	    0.0f, (GetDistanceToArenaEdge(beamOrigin_, beamDirection_, beamWidth_) -
	               beamStartOffset_) * 0.5f));
	pendingChargeDamageMultiplier_ = 1.0f;
	pendingChargeRangeMultiplier_ = 1.0f;
	BeginBeamExtension();
	return true;
}

void PlayerAttackController::UpdateBullets() {
	for (const auto& bullet : bullets_) {
		bullet->worldTransform.translation_.x += bullet->velocity.x;
		bullet->worldTransform.translation_.y += bullet->velocity.y;
		bullet->worldTransform.translation_.z += bullet->velocity.z;
		bullet->worldTransform.translation_.y = bullet->worldTransform.scale_.y;
		if (ReflectBulletFromArena(*bullet)) {
			const int maximumReflectionCount =
			    kMaxBulletReflectionCountPerStack *
			    subSkillController_.GetStackCount(SubSkillType::Hansho);
			if (!subSkillController_.IsReflectActive() ||
			    bullet->reflectionCount >= maximumReflectionCount) {
				bullet->isAlive = false;
			} else {
				++bullet->reflectionCount;
				AccelerateBulletAfterReflection(*bullet);
				SpawnFeedbackBurst(
				    bullet->worldTransform.translation_, {1.0f, 0.78f, 0.08f, 1.0f},
				    FeedbackParticle::Motion::Spark, 10);
			}
			// 壁反射で変わった速度へ、モデルの向きも即座に合わせる。
			bullet->worldTransform.rotation_.y =
			    std::atan2(bullet->velocity.x, bullet->velocity.z) +
			    kBulletModelForwardYawOffset;
			UpdateWorldTransform(bullet->worldTransform);
			continue;
		}
		const float bulletRadius = bullet->worldTransform.scale_.x;
		const bool hitBoss = IsBossInCircle(bullet->worldTransform.translation_, bulletRadius);
		Vector3 hitMobPosition = {};
		const bool hitMob = targetBoss_ != nullptr && targetBoss_->DamageMobsInCircle(
		    bullet->worldTransform.translation_, bulletRadius, bullet->damage,
		    &hitMobPosition);
		if (hitBoss) {
			Audio::GetInstance()->PlayWave(bulletHitSeHandle_, false, 0.70f);
			DamageBoss(bullet->damage);
			if (bullet->hasReflected &&
			    bullet->reflectionCount < kMaxBulletReflectionCountPerStack *
			        subSkillController_.GetStackCount(SubSkillType::Hansho)) {
				ReflectBulletFromBoss(*bullet);
				SpawnFeedbackBurst(
				    bullet->worldTransform.translation_, {1.0f, 0.78f, 0.08f, 1.0f},
				    FeedbackParticle::Motion::Spark, 12);
			} else {
				bullet->isAlive = false;
			}
		} else if (hitMob) {
			Audio::GetInstance()->PlayWave(bulletHitSeHandle_, false, 0.70f);
			RegisterEnemyHit(bullet->damage);
			if (bullet->hasReflected &&
			    bullet->reflectionCount < kMaxBulletReflectionCountPerStack *
			        subSkillController_.GetStackCount(SubSkillType::Hansho)) {
				ReflectBulletFromEnemyPosition(*bullet, hitMobPosition);
				SpawnFeedbackBurst(
				    hitMobPosition, {1.0f, 0.78f, 0.08f, 1.0f},
				    FeedbackParticle::Motion::Spark, 12);
			} else {
				bullet->isAlive = false;
			}
		}
		// Boss・Mob反射後を含め、常に現在の移動方向を向かせる。
		bullet->worldTransform.rotation_.y =
		    std::atan2(bullet->velocity.x, bullet->velocity.z) +
		    kBulletModelForwardYawOffset;
		UpdateWorldTransform(bullet->worldTransform);
	}
	std::erase_if(
	    bullets_, [](const std::unique_ptr<Bullet>& bullet) { return !bullet->isAlive; });
}

void PlayerAttackController::SpawnFlyingSlashes(
    const WorldTransform& playerTransform, const Vector3& aimDirection,
    float sizeMultiplier) {
	const int stackCount =
	    subSkillController_.GetStackCount(SubSkillType::Hansho);
	const float centerAngle = std::atan2(aimDirection.x, aimDirection.z);
	for (int index = 0; index < stackCount; ++index) {
		const float angleOffset =
		    (static_cast<float>(index) - static_cast<float>(stackCount - 1) * 0.5f) *
		    kFlyingSlashAngleStep;
		const float angle = centerAngle + angleOffset;
		const Vector3 direction = {std::sin(angle), 0.0f, std::cos(angle)};
		auto flyingSlash = std::make_unique<FlyingAttack>();
		flyingSlash->worldTransform.Initialize();
		const float projectileScale = 1.35f * sizeMultiplier;
		flyingSlash->worldTransform.scale_ = {
		    projectileScale, projectileScale, projectileScale};
		flyingSlash->worldTransform.rotation_.y =
		    angle + kBulletModelForwardYawOffset;
		flyingSlash->worldTransform.translation_ = {
		    playerTransform.translation_.x + direction.x * kSlashForwardOffset,
		    kPlayerAttackBaseHeight,
		    playerTransform.translation_.z + direction.z * kSlashForwardOffset,
		};
		flyingSlash->velocity = {
		    direction.x * kFlyingSlashSpeed, 0.0f,
		    direction.z * kFlyingSlashSpeed};
		flyingSlash->remainingFrames = kFlyingSlashLifeFrames;
		flyingSlash->damage = (std::max)(1, slashDamage_ / 2);
		flyingSlash->collisionRadius = 1.5f * sizeMultiplier;
		UpdateWorldTransform(flyingSlash->worldTransform);
		flyingAttacks_.push_back(std::move(flyingSlash));
	}
}

void PlayerAttackController::SpawnDeployFlyingAttacks(
    const Vector3& origin, int sourceDamage) {
	const int stackCount =
	    subSkillController_.GetStackCount(SubSkillType::Hansho);
	const int projectileCount = kDeployFlyingAttackCountPerStack * stackCount;
	if (projectileCount <= 0) return;
	// 設置地点を中心に360度へ均等配置し、Bossへの追尾は行わない。
	const float angleStep = 2.0f * std::numbers::pi_v<float> /
	    static_cast<float>(projectileCount);
	for (int index = 0; index < projectileCount; ++index) {
		const float angle = angleStep * static_cast<float>(index);
		const Vector3 direction = {std::sin(angle), 0.0f, std::cos(angle)};
		auto fragment = std::make_unique<FlyingAttack>();
		fragment->worldTransform.Initialize();
		fragment->worldTransform.scale_ = {0.8f, 0.8f, 1.4f};
		fragment->worldTransform.rotation_.y =
		    angle + kBulletModelForwardYawOffset;
		fragment->worldTransform.translation_ = {
		    origin.x + direction.x * 2.0f, 0.8f,
		    origin.z + direction.z * 2.0f};
		fragment->velocity = {
		    direction.x * kDeployFlyingAttackSpeed, 0.0f,
		    direction.z * kDeployFlyingAttackSpeed};
		fragment->remainingFrames = kDeployFlyingAttackLifeFrames;
		fragment->damage = (std::max)(
		    1, sourceDamage / kDeployFlyingAttackCountPerStack);
		fragment->collisionRadius = 0.8f;
		fragment->homesTowardBoss = false;
		UpdateWorldTransform(fragment->worldTransform);
		flyingAttacks_.push_back(std::move(fragment));
	}
}

void PlayerAttackController::UpdateFlyingAttacks() {
	for (const auto& flyingAttack : flyingAttacks_) {
		if (flyingAttack->homesTowardBoss && targetBoss_ != nullptr &&
		    !targetBoss_->IsDead()) {
			Vector3 desiredDirection = {
			    targetBoss_->GetWorldTransform().translation_.x -
			        flyingAttack->worldTransform.translation_.x,
			    0.0f,
			    targetBoss_->GetWorldTransform().translation_.z -
			        flyingAttack->worldTransform.translation_.z,
			};
			const float desiredLength = std::sqrt(
			    desiredDirection.x * desiredDirection.x +
			    desiredDirection.z * desiredDirection.z);
			const float speed = std::sqrt(
			    flyingAttack->velocity.x * flyingAttack->velocity.x +
			    flyingAttack->velocity.z * flyingAttack->velocity.z);
			if (desiredLength > 0.0001f && speed > 0.0001f) {
				desiredDirection.x /= desiredLength;
				desiredDirection.z /= desiredLength;
				Vector3 blendedDirection = {
				    flyingAttack->velocity.x / speed *
				            (1.0f - kFlyingAttackHomingInterpolation) +
				        desiredDirection.x * kFlyingAttackHomingInterpolation,
				    0.0f,
				    flyingAttack->velocity.z / speed *
				            (1.0f - kFlyingAttackHomingInterpolation) +
				        desiredDirection.z * kFlyingAttackHomingInterpolation,
				};
				const float blendedLength = std::sqrt(
				    blendedDirection.x * blendedDirection.x +
				    blendedDirection.z * blendedDirection.z);
				if (blendedLength > 0.0001f) {
					flyingAttack->velocity.x =
					    blendedDirection.x / blendedLength * speed;
					flyingAttack->velocity.z =
					    blendedDirection.z / blendedLength * speed;
				}
			}
		}

		flyingAttack->worldTransform.translation_.x += flyingAttack->velocity.x;
		flyingAttack->worldTransform.translation_.z += flyingAttack->velocity.z;
		flyingAttack->worldTransform.rotation_.y =
		    std::atan2(flyingAttack->velocity.x, flyingAttack->velocity.z) +
		    kBulletModelForwardYawOffset;
		--flyingAttack->remainingFrames;

		const float allowedRadius = arenaRadius_ - flyingAttack->collisionRadius;
		const float arenaX =
		    flyingAttack->worldTransform.translation_.x - arenaCenter_.x;
		const float arenaZ =
		    flyingAttack->worldTransform.translation_.z - arenaCenter_.z;
		if (allowedRadius > 0.0f &&
		    arenaX * arenaX + arenaZ * arenaZ > allowedRadius * allowedRadius) {
			flyingAttack->remainingFrames = 0;
			continue;
		}

		if (IsBossInCircle(
		        flyingAttack->worldTransform.translation_,
		        flyingAttack->collisionRadius)) {
			DamageBoss(flyingAttack->damage);
			flyingAttack->remainingFrames = 0;
		} else if (targetBoss_ != nullptr && targetBoss_->DamageMobsInCircle(
		               flyingAttack->worldTransform.translation_,
		               flyingAttack->collisionRadius, flyingAttack->damage)) {
			RegisterEnemyHit(flyingAttack->damage);
			flyingAttack->remainingFrames = 0;
		}
		UpdateWorldTransform(flyingAttack->worldTransform);
	}
	std::erase_if(
	    flyingAttacks_, [](const std::unique_ptr<FlyingAttack>& flyingAttack) {
		    return flyingAttack->remainingFrames <= 0;
	    });
}

bool PlayerAttackController::ReflectBulletFromArena(Bullet& bullet) const {
	const float bulletRadius = bullet.worldTransform.scale_.x;
	const float allowedRadius = arenaRadius_ - bulletRadius;
	if (allowedRadius <= 0.0f) return false;
	float dx = bullet.worldTransform.translation_.x - arenaCenter_.x;
	float dz = bullet.worldTransform.translation_.z - arenaCenter_.z;
	const float distanceSquared = dx * dx + dz * dz;
	if (distanceSquared <= allowedRadius * allowedRadius) return false;
	const float distance = std::sqrt(distanceSquared);
	if (distance <= 0.0001f) return false;
	dx /= distance;
	dz /= distance;
	bullet.worldTransform.translation_.x = arenaCenter_.x + dx * allowedRadius;
	bullet.worldTransform.translation_.z = arenaCenter_.z + dz * allowedRadius;
	const float velocityAlongNormal = bullet.velocity.x * dx + bullet.velocity.z * dz;
	if (velocityAlongNormal > 0.0f) {
		bullet.velocity.x -= 2.0f * velocityAlongNormal * dx;
		bullet.velocity.z -= 2.0f * velocityAlongNormal * dz;
	}
	return true;
}

void PlayerAttackController::AccelerateBulletAfterReflection(Bullet& bullet) const {
	bullet.velocity.x *= kBulletReflectionSpeedMultiplier;
	bullet.velocity.z *= kBulletReflectionSpeedMultiplier;
}

void PlayerAttackController::ConstrainPositionInsideArena(Vector3& position, float radius) const {
	const float allowedRadius = arenaRadius_ - radius;
	if (allowedRadius <= 0.0f) return;
	float dx = position.x - arenaCenter_.x;
	float dz = position.z - arenaCenter_.z;
	const float distanceSquared = dx * dx + dz * dz;
	if (distanceSquared <= allowedRadius * allowedRadius) return;
	const float distance = std::sqrt(distanceSquared);
	if (distance <= 0.0001f) return;
	position.x = arenaCenter_.x + dx / distance * allowedRadius;
	position.z = arenaCenter_.z + dz / distance * allowedRadius;
}

float PlayerAttackController::GetDistanceToArenaEdge(
    const Vector3& start, const Vector3& direction, float objectRadius) const {
	const float radius = arenaRadius_ - objectRadius;
	if (radius <= 0.0f) return kBeamLength;
	const float dx = start.x - arenaCenter_.x;
	const float dz = start.z - arenaCenter_.z;
	const float projection = dx * direction.x + dz * direction.z;
	const float c = dx * dx + dz * dz - radius * radius;
	const float discriminant = projection * projection - c;
	if (discriminant <= 0.0f) return 0.0f;
	return (std::max)(0.0f, -projection + std::sqrt(discriminant));
}

void PlayerAttackController::ReflectBulletFromBoss(Bullet& bullet) const {
	if (targetBoss_ == nullptr) {
		return;
	}
	const Vector3 bossPosition = targetBoss_->GetWorldTransform().translation_;
	ReflectBulletFromEnemyPosition(
	    bullet, bossPosition, targetBoss_->GetCollisionRadius());
}

void PlayerAttackController::ReflectBulletFromEnemyPosition(
    Bullet& bullet, const Vector3& enemyPosition, float separationRadius) const {
	float normalX = bullet.worldTransform.translation_.x - enemyPosition.x;
	float normalZ = bullet.worldTransform.translation_.z - enemyPosition.z;
	const float normalLength = std::sqrt(normalX * normalX + normalZ * normalZ);
	if (normalLength > 0.0001f) {
		normalX /= normalLength;
		normalZ /= normalLength;
	} else {
		const float velocityLength = std::sqrt(
		    bullet.velocity.x * bullet.velocity.x + bullet.velocity.z * bullet.velocity.z);
		normalX = (velocityLength > 0.0001f) ? -bullet.velocity.x / velocityLength : 0.0f;
		normalZ = (velocityLength > 0.0001f) ? -bullet.velocity.z / velocityLength : 1.0f;
	}
	const float velocityAlongNormal = bullet.velocity.x * normalX + bullet.velocity.z * normalZ;
	bullet.velocity.x -= 2.0f * velocityAlongNormal * normalX;
	bullet.velocity.z -= 2.0f * velocityAlongNormal * normalZ;
	const float separation = separationRadius + bullet.worldTransform.scale_.x + 0.1f;
	bullet.worldTransform.translation_.x = enemyPosition.x + normalX * separation;
	bullet.worldTransform.translation_.z = enemyPosition.z + normalZ * separation;
	++bullet.reflectionCount;
	AccelerateBulletAfterReflection(bullet);
}

void PlayerAttackController::ReflectChargeFromBoss(WorldTransform& playerTransform) {
	if (targetBoss_ == nullptr) {
		return;
	}
	const Vector3 bossPosition = targetBoss_->GetWorldTransform().translation_;
	float normalX = playerTransform.translation_.x - bossPosition.x;
	float normalZ = playerTransform.translation_.z - bossPosition.z;
	const float normalLength = std::sqrt(normalX * normalX + normalZ * normalZ);
	if (normalLength > 0.0001f) {
		normalX /= normalLength;
		normalZ /= normalLength;
	} else {
		normalX = -chargeAttackDirection_.x;
		normalZ = -chargeAttackDirection_.z;
	}
	const float velocityAlongNormal =
	    chargeVelocity_.x * normalX + chargeVelocity_.z * normalZ;
	chargeVelocity_.x -= 2.0f * velocityAlongNormal * normalX;
	chargeVelocity_.z -= 2.0f * velocityAlongNormal * normalZ;
	const float speed = std::sqrt(
	    chargeVelocity_.x * chargeVelocity_.x + chargeVelocity_.z * chargeVelocity_.z);
	if (speed > 0.0001f) {
		chargeAttackDirection_ = {
		    chargeVelocity_.x / speed, 0.0f, chargeVelocity_.z / speed};
	} else {
		chargeAttackDirection_ = {normalX, 0.0f, normalZ};
	}
	const float separation = targetBoss_->GetCollisionRadius() +
	    playerTransform.scale_.x * kPlayerCollisionRadiusPerVisualScale +
	    kChargeReflectionSeparation;
	playerTransform.translation_.x = bossPosition.x + normalX * separation;
	playerTransform.translation_.z = bossPosition.z + normalZ * separation;
	UpdateWorldTransform(playerTransform);
	++chargeReflectionCount_;
}

void PlayerAttackController::StopChargeAtBoss(WorldTransform& playerTransform) {
	if (targetBoss_ == nullptr) {
		return;
	}
	const Vector3 bossPosition = targetBoss_->GetWorldTransform().translation_;
	float normalX = playerTransform.translation_.x - bossPosition.x;
	float normalZ = playerTransform.translation_.z - bossPosition.z;
	const float normalLength = std::sqrt(normalX * normalX + normalZ * normalZ);
	if (normalLength > 0.0001f) {
		normalX /= normalLength;
		normalZ /= normalLength;
	} else {
		normalX = -chargeAttackDirection_.x;
		normalZ = -chargeAttackDirection_.z;
	}
	const float separation = targetBoss_->GetCollisionRadius() +
	    playerTransform.scale_.x * kPlayerCollisionRadiusPerVisualScale +
	    kChargeReflectionSeparation;
	playerTransform.translation_.x = bossPosition.x + normalX * separation;
	playerTransform.translation_.z = bossPosition.z + normalZ * separation;
	chargeVelocity_ = {0.0f, 0.0f, 0.0f};
	chargeAttackTimer_ = 0;
	UpdateWorldTransform(playerTransform);
}

bool PlayerAttackController::ReflectChargeFromArena(WorldTransform& playerTransform) {
	const float allowedRadius = arenaRadius_ -
	    playerTransform.scale_.x * kPlayerCollisionRadiusPerVisualScale;
	if (allowedRadius <= 0.0f) return false;
	float dx = playerTransform.translation_.x - arenaCenter_.x;
	float dz = playerTransform.translation_.z - arenaCenter_.z;
	const float distanceSquared = dx * dx + dz * dz;
	if (distanceSquared <= allowedRadius * allowedRadius) return false;
	const float distance = std::sqrt(distanceSquared);
	if (distance <= 0.0001f) return false;
	dx /= distance;
	dz /= distance;
	playerTransform.translation_.x = arenaCenter_.x + dx * allowedRadius;
	playerTransform.translation_.z = arenaCenter_.z + dz * allowedRadius;
	const float velocityAlongNormal = chargeVelocity_.x * dx + chargeVelocity_.z * dz;
	if (velocityAlongNormal > 0.0f && subSkillController_.IsReflectActive()) {
		chargeVelocity_.x -= 2.0f * velocityAlongNormal * dx;
		chargeVelocity_.z -= 2.0f * velocityAlongNormal * dz;
		const float speed = std::sqrt(
		    chargeVelocity_.x * chargeVelocity_.x + chargeVelocity_.z * chargeVelocity_.z);
		if (speed > 0.0001f) {
			chargeAttackDirection_ = {
			    chargeVelocity_.x / speed, 0.0f, chargeVelocity_.z / speed};
		}
		++chargeReflectionCount_;
	}
	UpdateWorldTransform(playerTransform);
	return true;
}

void PlayerAttackController::ReflectBeamInRandomOppositeDirection(
    const Vector3& reflectionOrigin) {
	SpawnFeedbackBurst(
	    reflectionOrigin, {1.0f, 0.78f, 0.08f, 1.0f},
	    FeedbackParticle::Motion::Spark, 18);
	static std::mt19937 randomEngine{std::random_device{}()};
	std::uniform_real_distribution<float> angleOffset(
	    -kBeamReflectionRandomAngleRadians, kBeamReflectionRandomAngleRadians);
	const float oppositeAngle =
	    std::atan2(-beamDirection_.x, -beamDirection_.z) + angleOffset(randomEngine);
	beamDirection_ = {std::sin(oppositeAngle), 0.0f, std::cos(oppositeAngle)};
	beamOrigin_ = reflectionOrigin;
	beamStartOffset_ = 0.0f;
	beamLength_ = (std::min)(beamMaximumLength_, (std::max)(
	    0.0f, (GetDistanceToArenaEdge(beamOrigin_, beamDirection_, beamWidth_) -
	               beamStartOffset_) * 0.5f));
	++beamReflectionCount_;
	isBeamBlockedByEnemy_ = false;
	BeginBeamExtension();
}

void PlayerAttackController::ReflectBeamFromArena(const Vector3& reflectionOrigin) {
	SpawnFeedbackBurst(
	    reflectionOrigin, {1.0f, 0.78f, 0.08f, 1.0f},
	    FeedbackParticle::Motion::Spark, 18);
	float normalX = reflectionOrigin.x - arenaCenter_.x;
	float normalZ = reflectionOrigin.z - arenaCenter_.z;
	const float normalLength = std::sqrt(normalX * normalX + normalZ * normalZ);
	if (normalLength <= 0.0001f) return;
	normalX /= normalLength;
	normalZ /= normalLength;
	const float directionAlongNormal = beamDirection_.x * normalX + beamDirection_.z * normalZ;
	beamDirection_.x -= 2.0f * directionAlongNormal * normalX;
	beamDirection_.z -= 2.0f * directionAlongNormal * normalZ;
	// 壁で線分が切り替わった後は、再びBossへ命中・反射できる。
	beamIgnoresBossOnCurrentSegment_ = false;
	beamOrigin_ = reflectionOrigin;
	beamStartOffset_ = 0.0f;
	beamLength_ = (std::min)(beamMaximumLength_, (std::max)(
	    0.0f, (GetDistanceToArenaEdge(beamOrigin_, beamDirection_, beamWidth_) -
	               beamStartOffset_) * 0.5f));
	++beamReflectionCount_;
	isBeamBlockedByEnemy_ = false;
	BeginBeamExtension();
}

void PlayerAttackController::SaveCurrentBeamSegment() {
	auto reflectedSegment = std::make_unique<ReflectedBeamSegment>();
	reflectedSegment->worldTransform.Initialize();
	reflectedSegment->worldTransform.scale_ = beamTransform_.scale_;
	reflectedSegment->worldTransform.rotation_ = beamTransform_.rotation_;
	reflectedSegment->worldTransform.translation_ = beamTransform_.translation_;
	UpdateWorldTransform(reflectedSegment->worldTransform);
	reflectedBeamSegments_.push_back(std::move(reflectedSegment));
}

void PlayerAttackController::UpdateBombs() {
	for (const auto& bomb : bombs_) {
		if (bomb->fuseTimer > 0) {
			--bomb->fuseTimer;
			if (bomb->fuseTimer > 0 && bomb->fuseTimer % kBombTickIntervalFrames == 0) {
				Audio::GetInstance()->PlayWave(bombTickSeHandle_, false, 0.62f);
			}
			continue;
		}
		if (bomb->explosionTimer < 0) {
			bomb->explosionTimer = kBombExplosionFrames;
			Audio::GetInstance()->PlayWave(bombExplosionSeHandle_, false, 0.88f);
			SpawnExplosionParticles(
			    bomb->worldTransform.translation_, bomb->explosionScale);
			if (targetBoss_ != nullptr) {
				// The timed bomb resolves its damage at the moment it detonates.
				// The visual expansion that follows is presentation only.
				// BossもMobと同様に、予測円と同じ爆発範囲内にいる場合だけ
				// ダメージを与える。以前は距離に関係なく直接DamageBossを
				// 呼んでいたため、フィールドのどこに置いても命中していた。
				if (IsBossInCircle(
				        bomb->worldTransform.translation_, bomb->explosionScale)) {
					DamageBoss(bomb->damage);
				}
				const bool hitMob = targetBoss_->DamageMobsInCircle(
				    bomb->worldTransform.translation_, bomb->explosionScale, bomb->damage);
				if (hitMob) {
					RegisterEnemyHit(bomb->damage);
				}
				if (subSkillController_.IsReflectActive()) {
					const float reflectRadius = bomb->explosionScale *
					    static_cast<float>(subSkillController_.GetStackCount(SubSkillType::Hansho));
					const int reflectedCount = targetBoss_->ReflectEnemyProjectilesInCircle(
					    bomb->worldTransform.translation_, reflectRadius);
					if (reflectedCount > 0) {
						SpawnFeedbackBurst(
						    bomb->worldTransform.translation_, {1.0f, 0.78f, 0.08f, 1.0f},
						    FeedbackParticle::Motion::Spark, 14 + reflectedCount * 2);
					}
					SpawnDeployFlyingAttacks(
					    bomb->worldTransform.translation_, bomb->damage);
				}
			}
			bomb->hasDealtDamage = true;
		}
		const float explosionProgress = 1.0f -
		    static_cast<float>(bomb->explosionTimer) /
		        static_cast<float>(kBombExplosionFrames);
		// alpha=7～8で青いPlayer爆破HLSLと進行度を指定する。
		bomb->explosionColor.SetColor({
		    0.0f, 0.82f, 1.0f, 7.0f + explosionProgress});
		--bomb->explosionTimer;
	}
	std::erase_if(bombs_, [](const std::unique_ptr<Bomb>& bomb) {
		// explosionTimer == -1 means the fuse has just finished and the bomb
		// must remain alive until UpdateBombs starts the explosion next frame.
		return bomb->explosionTimer == 0;
	});
}

void PlayerAttackController::SpawnExplosionParticles(
    const Vector3& origin, float explosionRadius) {
	static std::mt19937 randomEngine{std::random_device{}()};
	std::uniform_real_distribution<float> angleDistribution(0.0f, 6.28318531f);
	std::uniform_real_distribution<float> speedDistribution(0.025f, 0.055f);
	std::uniform_real_distribution<float> upwardDistribution(0.35f, 1.20f);
	std::uniform_real_distribution<float> scaleDistribution(0.08f, 0.16f);
	std::uniform_real_distribution<float> rotationDistribution(-0.16f, 0.16f);
	std::uniform_real_distribution<float> colorVariationDistribution(0.0f, 1.0f);

	for (int index = 0; index < kExplosionParticleCount; ++index) {
		const float angle = angleDistribution(randomEngine);
		const float speed = explosionRadius * speedDistribution(randomEngine);
		const float particleScale =
		    explosionRadius * scaleDistribution(randomEngine);
		auto particle = std::make_unique<ExplosionParticle>();
		particle->worldTransform.Initialize();
		particle->color.Initialize();
		particle->colorVariation = colorVariationDistribution(randomEngine);
		// alpha=7～8はObjPSでPlayer設置爆発専用HLSLを選ぶマーカー。
		particle->color.SetColor({0.0f, 0.82f, 1.0f, 7.0f});
		particle->worldTransform.scale_ = {
		    particleScale, particleScale, particleScale};
		particle->worldTransform.translation_ = {
		    origin.x, particleScale, origin.z};
		particle->worldTransform.rotation_ = {
		    angleDistribution(randomEngine), angleDistribution(randomEngine),
		    angleDistribution(randomEngine)};
		particle->velocity = {
		    std::sin(angle) * speed,
		    upwardDistribution(randomEngine),
		    std::cos(angle) * speed};
		particle->angularVelocity = {
		    rotationDistribution(randomEngine),
		    rotationDistribution(randomEngine),
		    rotationDistribution(randomEngine)};
		particle->baseScale = particleScale;
		particle->remainingFrames = kExplosionParticleLifeFrames;
		particle->totalFrames = kExplosionParticleLifeFrames;
		UpdateWorldTransform(particle->worldTransform);
		explosionParticles_.push_back(std::move(particle));
	}
}

void PlayerAttackController::UpdateExplosionParticles() {
	for (const auto& particle : explosionParticles_) {
		particle->worldTransform.translation_.x += particle->velocity.x;
		particle->worldTransform.translation_.y += particle->velocity.y;
		particle->worldTransform.translation_.z += particle->velocity.z;
		particle->velocity.y -= kExplosionParticleGravity;
		particle->worldTransform.rotation_.x += particle->angularVelocity.x;
		particle->worldTransform.rotation_.y += particle->angularVelocity.y;
		particle->worldTransform.rotation_.z += particle->angularVelocity.z;
		--particle->remainingFrames;
		const float lifeRatio = static_cast<float>(particle->remainingFrames) /
		    static_cast<float>(particle->totalFrames);
		const float ageRatio = 1.0f - (std::max)(0.0f, lifeRatio);
		// 進行度をalphaの小数部として専用HLSLへ渡す。
		particle->color.SetColor({
		    0.0f, 0.82f + 0.18f * particle->colorVariation, 1.0f,
		    7.0f + ageRatio});
		const float scale = particle->baseScale *
		    (0.20f + 0.80f * (std::max)(0.0f, lifeRatio));
		particle->worldTransform.scale_ = {scale, scale, scale};
		particle->worldTransform.translation_.y = (std::max)(
		    particle->worldTransform.translation_.y, scale * 0.35f);
		UpdateWorldTransform(particle->worldTransform);
	}
	std::erase_if(
	    explosionParticles_, [](const std::unique_ptr<ExplosionParticle>& particle) {
		    return particle->remainingFrames <= 0;
	    });
}

void PlayerAttackController::SpawnFeedbackBurst(
    const Vector3& origin, const Vector4& color,
    FeedbackParticle::Motion motion, int count) {
	if (feedbackParticles_.size() >= 160) return;
	static std::mt19937 randomEngine{std::random_device{}()};
	std::uniform_real_distribution<float> angleDistribution(0.0f, 6.28318531f);
	std::uniform_real_distribution<float> speedDistribution(0.35f, 0.95f);
	for (int index = 0; index < count; ++index) {
		const float angle = angleDistribution(randomEngine);
		auto particle = std::make_unique<FeedbackParticle>();
		particle->worldTransform.Initialize();
		particle->color.Initialize();
		particle->baseColor = color;
		particle->color.SetColor(color);
		particle->motion = motion;
		// Effect.objは小さめなので、戦闘カメラでも認識できる大きさを基準にする。
		particle->baseScale =
		    motion == FeedbackParticle::Motion::Expand ? 2.8f : 2.2f;
		particle->worldTransform.scale_ = {
		    particle->baseScale, particle->baseScale, particle->baseScale};
		particle->worldTransform.translation_ = {origin.x, origin.y + 2.0f, origin.z};
		particle->velocity = {
		    std::sin(angle) * speedDistribution(randomEngine),
		    motion == FeedbackParticle::Motion::Spark ? 0.45f : 0.08f,
		    std::cos(angle) * speedDistribution(randomEngine)};
		particle->totalFrames = motion == FeedbackParticle::Motion::Expand ? 16 : 22;
		particle->remainingFrames = particle->totalFrames;
		UpdateWorldTransform(particle->worldTransform);
		feedbackParticles_.push_back(std::move(particle));
	}
}

void PlayerAttackController::SpawnAbsorbParticles(
    const Vector3& origin, const Vector4& color, int count) {
	if (feedbackParticles_.size() >= 160) return;
	for (int index = 0; index < count; ++index) {
		auto particle = std::make_unique<FeedbackParticle>();
		particle->worldTransform.Initialize();
		particle->color.Initialize();
		particle->baseColor = color;
		particle->color.SetColor(color);
		particle->motion = FeedbackParticle::Motion::Absorb;
		particle->start = {origin.x, origin.y + 2.0f, origin.z};
		particle->target = {ownerPosition_.x, ownerPosition_.y + 1.0f, ownerPosition_.z};
		particle->baseScale = 2.4f + static_cast<float>(index % 3) * 0.45f;
		particle->worldTransform.scale_ = {
		    particle->baseScale, particle->baseScale, particle->baseScale};
		particle->worldTransform.translation_ = particle->start;
		particle->totalFrames = 24 + index * 2;
		particle->remainingFrames = particle->totalFrames;
		UpdateWorldTransform(particle->worldTransform);
		feedbackParticles_.push_back(std::move(particle));
	}
}

void PlayerAttackController::UpdateFeedbackParticles() {
	for (const auto& particle : feedbackParticles_) {
		--particle->remainingFrames;
		const float lifeRatio = std::clamp(
		    static_cast<float>(particle->remainingFrames) /
		        static_cast<float>(particle->totalFrames),
		    0.0f, 1.0f);
		const float progress = 1.0f - lifeRatio;
		float scale = particle->baseScale;
		if (particle->motion == FeedbackParticle::Motion::Absorb) {
			const float eased = progress * progress * (3.0f - 2.0f * progress);
			particle->target = {ownerPosition_.x, ownerPosition_.y + 1.0f, ownerPosition_.z};
			particle->worldTransform.translation_ = {
			    particle->start.x + (particle->target.x - particle->start.x) * eased,
			    particle->start.y + (particle->target.y - particle->start.y) * eased +
			        std::sin(progress * 6.28318531f) * 3.0f,
			    particle->start.z + (particle->target.z - particle->start.z) * eased};
			scale *= 0.35f + lifeRatio * 0.65f;
		} else {
			particle->worldTransform.translation_.x += particle->velocity.x;
			particle->worldTransform.translation_.y += particle->velocity.y;
			particle->worldTransform.translation_.z += particle->velocity.z;
			particle->velocity.y -= 0.035f;
			if (particle->motion == FeedbackParticle::Motion::Expand) {
				scale *= 1.0f + progress * 4.5f;
			} else {
				scale *= 0.25f + lifeRatio * 0.75f;
			}
		}
		particle->worldTransform.scale_ = {scale, scale, scale};
		particle->color.SetColor({
		    particle->baseColor.x, particle->baseColor.y,
		    particle->baseColor.z, lifeRatio});
		UpdateWorldTransform(particle->worldTransform);
	}
	std::erase_if(
	    feedbackParticles_, [](const std::unique_ptr<FeedbackParticle>& particle) {
		    return particle->remainingFrames <= 0;
	    });
}

bool PlayerAttackController::IsBossInCircle(const Vector3& center, float radius) const {
	if (targetBoss_ == nullptr || targetBoss_->IsDead()) {
		return false;
	}
	return ColliderManager::CheckCircleCircleXZ(
	    {center, radius},
	    {targetBoss_->GetWorldTransform().translation_, targetBoss_->GetCollisionRadius()});
}

void PlayerAttackController::DamageBoss(int damage) {
	if (targetBoss_ != nullptr) {
		targetBoss_->ApplyDamage(damage);
		RegisterEnemyHit(damage);
		if (damage > 0) {
			pendingBossHitCount_ = (std::min)(pendingBossHitCount_ + 1, 100);
		}
	}
}

void PlayerAttackController::RegisterEnemyHit(int damage) {
	if (damage <= 0) {
		return;
	}
	pendingSuccessfulHitCount_ = (std::min)(pendingSuccessfulHitCount_ + 1, 100);
	const Vector3 effectOrigin = targetBoss_ != nullptr
	    ? targetBoss_->GetWorldTransform().translation_
	    : ownerPosition_;
	if (subSkillController_.IsChargeActive()) {
		const int maximumGrowthHits =
		    (kChargeMaxFrames - kChargeMinimumFrames) /
		    kChargeFramesReducedPerHit;
		const int previousChargeHitCount = chargeHitCount_;
		chargeHitCount_ = (std::min)(
		    maximumGrowthHits,
		    chargeHitCount_ +
		        subSkillController_.GetStackCount(SubSkillType::Charge));
		if (chargeHitCount_ > previousChargeHitCount) {
			++pendingChargeGrowthEffectCount_;
		}
	}
	if (subSkillController_.IsBerserkActive()) {
		const int berserkStacks =
		    subSkillController_.GetStackCount(SubSkillType::Berserk);
		berserkHitCount_ += berserkStacks;
		const float lifeStealPerStack =
		    static_cast<float>(kBerserkLifeStealPerHit) +
		    kBerserkLifeStealGrowthPerHit * static_cast<float>(berserkHitCount_);
		pendingBerserkLifeSteal_ +=
		    lifeStealPerStack * static_cast<float>(berserkStacks);
		SpawnAbsorbParticles(
		    effectOrigin, {1.0f, 0.04f, 0.08f, 1.0f},
		    3 * berserkStacks);
	}
	if (subSkillController_.IsIncreaseActive() && increaseHitCount_ < kIncreaseMaxHitCount) {
		increaseHitCount_ = (std::min)(
		    kIncreaseMaxHitCount,
		    increaseHitCount_ + subSkillController_.GetStackCount(SubSkillType::Increase));
		SpawnFeedbackBurst(
		    effectOrigin, {0.72f, 0.12f, 1.0f, 1.0f},
		    FeedbackParticle::Motion::Expand,
		    2 * subSkillController_.GetStackCount(SubSkillType::Increase));
	}
	if (subSkillController_.IsStealActive()) {
		const int stealStacks = subSkillController_.GetStackCount(SubSkillType::Steal);
		if (stealStacks >= 2) {
			++steelBurstHitCount_;
			SpawnAbsorbParticles(
			    effectOrigin, {1.0f, 0.72f, 0.08f, 1.0f}, 5);
			if (steelBurstHitCount_ >= kSteelBurstRequiredHits) {
				steelBurstHitCount_ -= kSteelBurstRequiredHits;
				++pendingSteelBurstCount_;
				steelHitCount_ += kSteelBurstGrowthUnits;
				steelBurstBuffTimer_ = kSteelBurstBuffFrames;
				SpawnAbsorbParticles(
				    effectOrigin, {1.0f, 0.84f, 0.12f, 1.0f}, 32);
				SpawnFeedbackBurst(
				    effectOrigin, {1.0f, 0.72f, 0.05f, 1.0f},
				    FeedbackParticle::Motion::Expand, 18);
			}
		} else {
			pendingSteelGrowth_ += stealStacks;
			steelHitCount_ += stealStacks;
			SpawnAbsorbParticles(
			    effectOrigin, {0.08f, 0.85f, 1.0f, 1.0f}, 4 * stealStacks);
		}
	}
}

float PlayerAttackController::GetDamageMultiplier() const {
	const int berserkStacks =
	    subSkillController_.GetStackCount(SubSkillType::Berserk);
	const int hanshoStacks =
	    subSkillController_.GetStackCount(SubSkillType::Hansho);
	float multiplier = 1.0f +
	    ((kBerserkDamageMultiplier - 1.0f) +
	     kBerserkBaseDamageBonusPerStack) *
	        static_cast<float>(berserkStacks) +
	    kHanshoBaseDamageBonusPerStack * static_cast<float>(hanshoStacks);
	if (subSkillController_.IsIncreaseActive()) {
		multiplier += kIncreaseDamagePerHit * increaseHitCount_;
	}
	if (steelBurstBuffTimer_ > 0) {
		multiplier *= kSteelBurstDamageMultiplier;
	}
	return multiplier;
}

int PlayerAttackController::GetAttackInterval(int baseInterval) const {
	float multiplier = 1.0f + (kBerserkAttackIntervalMultiplier - 1.0f) *
	    static_cast<float>(subSkillController_.GetStackCount(SubSkillType::Berserk));
	if (subSkillController_.IsIncreaseActive()) {
		multiplier += kIncreaseCooldownPerHit * increaseHitCount_;
	}
	return (std::max)(1, static_cast<int>(baseInterval * multiplier));
}

float PlayerAttackController::GetMainSkillCooldownRatio() const {
	int remaining = 0;
	int duration = 0;
	switch (mainSkill_) {
	case MainSkillType::Bullet:
		remaining = bulletCooldownTimer_;
		duration = bulletCooldownDuration_;
		break;
	case MainSkillType::Slash:
		remaining = slashCooldownTimer_;
		duration = slashCooldownDuration_;
		break;
	case MainSkillType::Charge:
		remaining = chargeAttackCooldownTimer_;
		duration = chargeAttackCooldownDuration_;
		break;
	case MainSkillType::Deploy:
		remaining = deployCooldownTimer_;
		duration = deployCooldownDuration_;
		break;
	case MainSkillType::Beam:
		remaining = beamCooldownTimer_;
		duration = beamCooldownDuration_;
		break;
	default:
		return 1.0f;
	}
	if (remaining <= 0 || duration <= 0) return 1.0f;
	return std::clamp(
	    1.0f - static_cast<float>(remaining) / static_cast<float>(duration),
	    0.0f, 1.0f);
}

float PlayerAttackController::GetAttackSizeMultiplier() const {
	return subSkillController_.IsIncreaseActive() ?
	    1.0f + kIncreaseSizePerHit * increaseHitCount_ : 1.0f;
}

float PlayerAttackController::GetIncreaseMoveSpeedMultiplier() const {
	if (!subSkillController_.IsIncreaseActive()) return 1.0f;
	return (std::max)(kIncreaseMinimumSpeedMultiplier,
	                  1.0f - kIncreaseMoveSpeedPenaltyPerHit * increaseHitCount_);
}

void PlayerAttackController::UpdateBeamTransform(const Vector3& aimDirection) {
	beamDirection_ = aimDirection;
	UpdateBeamTransformToTipDistance(
	    beamOrigin_, beamStartOffset_ + beamLength_ * 2.0f);
}

void PlayerAttackController::UpdateBeamTransformToTipDistance(
    const Vector3& beamOrigin, float tipDistance) {
	const float clampedTipDistance = (std::max)(tipDistance, beamStartOffset_);
	const float halfLength = (clampedTipDistance - beamStartOffset_) * 0.5f;
	beamTransform_.scale_ = {beamWidth_, beamWidth_, halfLength};
	beamTransform_.rotation_.y = std::atan2(beamDirection_.x, beamDirection_.z);
	const float centerOffset = beamStartOffset_ + halfLength;
	beamTransform_.translation_ = {
	    beamOrigin.x + beamDirection_.x * centerOffset,
	    beamOrigin.y,
	    beamOrigin.z + beamDirection_.z * centerOffset,
	};
	UpdateWorldTransform(beamTransform_);
	UpdateBeamImpactTransform();
}

void PlayerAttackController::BeginBeamExtension() {
	const float fullLength = beamLength_ * 2.0f;
	beamExtendDurationFrames_ = std::clamp(
	    static_cast<int>(std::ceil(fullLength / kBeamExtendWorldUnitsPerFrame)),
	    kBeamExtendMinimumFrames, kBeamExtendMaximumFrames);
	beamExtendTimer_ = beamExtendDurationFrames_;
	UpdateBeamExtensionTransform();
}

void PlayerAttackController::UpdateBeamExtensionTransform() {
	const float ratio = 1.0f -
	    static_cast<float>(beamExtendTimer_) /
	        static_cast<float>(beamExtendDurationFrames_);
	const float currentLength = beamLength_ * ratio;
	beamTransform_.scale_ = {beamWidth_, beamWidth_, currentLength};
	beamTransform_.rotation_.y = std::atan2(beamDirection_.x, beamDirection_.z);
	beamTransform_.translation_ = {
	    beamOrigin_.x + beamDirection_.x * (beamStartOffset_ + currentLength),
	    beamOrigin_.y,
	    beamOrigin_.z + beamDirection_.z * (beamStartOffset_ + currentLength),
	};
	UpdateWorldTransform(beamTransform_);
	UpdateBeamImpactTransform();
}

void PlayerAttackController::UpdateBeamImpactTransform() {
	const float impactRadius = (std::max)(5.5f, beamWidth_ * 3.0f);
	beamImpactTransform_.scale_ = {impactRadius, 1.0f, impactRadius};
	// PredictionCircleの水平面を起こし、ビームと直交する断面として先端へ密着させる。
	beamImpactTransform_.rotation_ = {
	    std::numbers::pi_v<float> * 0.5f,
	    std::atan2(beamDirection_.x, beamDirection_.z),
	    0.0f};
	beamImpactTransform_.translation_ = {
	    beamTransform_.translation_.x + beamDirection_.x * beamTransform_.scale_.z,
	    beamTransform_.translation_.y,
	    beamTransform_.translation_.z + beamDirection_.z * beamTransform_.scale_.z,
	};
	UpdateWorldTransform(beamImpactTransform_);
}

void PlayerAttackController::UpdateBeamRetractionTransform(BeamRetraction& beamRetraction) const {
	const float ratio =
	    static_cast<float>(beamRetraction.remainingFrames) /
	    static_cast<float>(beamRetraction.segmentDurationFrames);
	const float currentLength = beamRetraction.initialLength * ratio;
	beamRetraction.worldTransform.scale_ = {
	    beamRetraction.width, beamRetraction.width, currentLength};
	beamRetraction.worldTransform.rotation_.y =
	    std::atan2(beamRetraction.direction.x, beamRetraction.direction.z);
	if (beamRetraction.foldTowardOrigin) {
		// 反射ビームは各反射開始点を軸に、先端を引き戻して折りたたむ。
		beamRetraction.worldTransform.translation_ = {
		    beamRetraction.originPosition.x + beamRetraction.direction.x * currentLength,
		    beamRetraction.originPosition.y,
		    beamRetraction.originPosition.z + beamRetraction.direction.z * currentLength,
		};
	} else {
		beamRetraction.worldTransform.translation_ = {
		    beamRetraction.tipPosition.x - beamRetraction.direction.x * currentLength,
		    beamRetraction.tipPosition.y,
		    beamRetraction.tipPosition.z - beamRetraction.direction.z * currentLength,
		};
	}
	UpdateWorldTransform(beamRetraction.worldTransform);
}

void PlayerAttackController::ApplyBeamRetractionDamage(BeamRetraction& beamRetraction) {
	if (beamRetraction.remainingFrames <= 0 || targetBoss_ == nullptr) {
		return;
	}
	if (beamRetraction.damageIntervalTimer > 0) {
		--beamRetraction.damageIntervalTimer;
		return;
	}

	bool hitBoss = false;
	auto applySegmentDamage = [this, &beamRetraction, &hitBoss](
	                              const WorldTransform& transform) {
		if (transform.scale_.z <= 0.0001f) return;
		const Vector3 direction = {
		    std::sin(transform.rotation_.y), 0.0f, std::cos(transform.rotation_.y)};
		const ColliderManager::Segment segment = {
		    {transform.translation_.x - direction.x * transform.scale_.z,
		     transform.translation_.y,
		     transform.translation_.z - direction.z * transform.scale_.z},
		    {transform.translation_.x + direction.x * transform.scale_.z,
		     transform.translation_.y,
		     transform.translation_.z + direction.z * transform.scale_.z},
		};
		if (ColliderManager::CheckSegmentCircleXZ(
		        segment,
		        {targetBoss_->GetWorldTransform().translation_,
		         targetBoss_->GetCollisionRadius() + beamRetraction.width})) {
			hitBoss = true;
		}
		if (targetBoss_->DamageMobsAlongSegment(
		        segment, beamRetraction.damage, nullptr, beamRetraction.width)) {
			RegisterEnemyHit(beamRetraction.damage);
		}
	};

	applySegmentDamage(beamRetraction.worldTransform);
	for (const auto& followingSegment : beamRetraction.followingSegments) {
		applySegmentDamage(*followingSegment);
	}
	if (hitBoss) {
		DamageBoss(beamRetraction.damage);
	}
	beamRetraction.damageIntervalTimer = kBeamDamageIntervalFrames;
}

void PlayerAttackController::BeginBeamRetraction(const WorldTransform& beamTransform) {
	auto beamRetraction = std::make_unique<BeamRetraction>();
	beamRetraction->worldTransform.Initialize();
	beamRetraction->damage = beamDamage_;
	beamRetraction->width = beamWidth_;
	auto addSegment = [&beamRetraction](const WorldTransform& source) {
		auto segment = std::make_unique<WorldTransform>();
		segment->Initialize();
		segment->scale_ = source.scale_;
		segment->rotation_ = source.rotation_;
		segment->translation_ = source.translation_;
		UpdateWorldTransform(*segment);
		beamRetraction->followingSegments.push_back(std::move(segment));
	};
	for (const auto& reflectedSegment : reflectedBeamSegments_) {
		addSegment(reflectedSegment->worldTransform);
	}
	addSegment(beamTransform);
	float totalBeamLength = 0.0f;
	for (const auto& segment : beamRetraction->followingSegments) {
		totalBeamLength += segment->scale_.z * 2.0f;
	}
	const int totalRetractFrames = std::clamp(
	    static_cast<int>(std::round(totalBeamLength * kBeamRetractFramesPerWorldUnit)),
	    kBeamRetractMinimumTotalFrames, kBeamRetractMaximumTotalFrames);
	beamRetraction->retractFramesPerWorldUnit = totalBeamLength > 0.0001f ?
	    static_cast<float>(totalRetractFrames) / totalBeamLength : 0.0f;
	// Player側から先端へ送り込み、最終的にビーム最先端へ集約する。
	beamRetraction->foldTowardOrigin = false;
	std::unique_ptr<WorldTransform> firstSegment =
	    std::move(beamRetraction->followingSegments.front());
	beamRetraction->followingSegments.erase(beamRetraction->followingSegments.begin());
	SetBeamRetractionSegment(*beamRetraction, *firstSegment);
	beamRetractions_.push_back(std::move(beamRetraction));
}

void PlayerAttackController::SetBeamRetractionSegment(
    BeamRetraction& beamRetraction, const WorldTransform& beamTransform) {
	beamRetraction.worldTransform.scale_ = beamTransform.scale_;
	beamRetraction.worldTransform.rotation_ = beamTransform.rotation_;
	beamRetraction.worldTransform.translation_ = beamTransform.translation_;
	beamRetraction.direction = {
		std::sin(beamTransform.rotation_.y), 0.0f, std::cos(beamTransform.rotation_.y)};
	beamRetraction.initialLength = beamTransform.scale_.z;
	beamRetraction.originPosition = {
		beamTransform.translation_.x - beamRetraction.direction.x * beamTransform.scale_.z,
		beamTransform.translation_.y,
		beamTransform.translation_.z - beamRetraction.direction.z * beamTransform.scale_.z,
	};
	beamRetraction.tipPosition = {
		beamTransform.translation_.x + beamRetraction.direction.x * beamTransform.scale_.z,
		beamTransform.translation_.y,
		beamTransform.translation_.z + beamRetraction.direction.z * beamTransform.scale_.z,
	};
	const float segmentLength = beamTransform.scale_.z * 2.0f;
	beamRetraction.segmentDurationFrames = (std::max)(
	    kBeamRetractMinimumSegmentFrames,
	    static_cast<int>(std::round(
	        segmentLength * beamRetraction.retractFramesPerWorldUnit)));
	beamRetraction.remainingFrames = beamRetraction.segmentDurationFrames;
	UpdateBeamRetractionTransform(beamRetraction);
}
