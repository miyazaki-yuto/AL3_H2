#include "BossAttackManager.h"

#include "Player.h"

#include <algorithm>
#include <chrono>
#include <cmath>

using namespace KamataEngine;

void BossAttackManager::Initialize(
    Model* bulletModel,
	Model* laserModel,
    Model* predictionCircleModel,
	Model* chargePredictionLineModel,
	Model* groundSpearModel,
	Model* mobModel,
    Camera* camera,
    Player* player) {
	player_ = player;
	gapShockWaveAttack_.Initialize(bulletModel, camera, player);
	rotatingLaserAttack_.Initialize(laserModel, camera, player);
	closeExplosionAttack_.Initialize(predictionCircleModel, camera, player);
	groundSpearAttack_.Initialize(groundSpearModel, predictionCircleModel, camera, player);
	summonMobsAttack_.Initialize(
	    mobModel, bulletModel, predictionCircleModel, camera, player);
	chargeAttack_.Initialize(chargePredictionLineModel, camera, player);
	groundSpearSequenceInProgress_ = false;
	summonMobsFollowupStarted_ = false;
	approachingCloseExplosion_ = false;
	forcedLaserPending_ = false;
	randomState_ = static_cast<uint32_t>(
	    std::chrono::steady_clock::now().time_since_epoch().count());
	hasLastAttackType_ = false;
	nextAttackType_ = SelectWeaponAdaptedAttack();
	nextAttackAfterDown_ = AttackType::GapShockWave;
	attackCooldown_ = kFirstAttackDelayFrames;
	downTimer_ = 0;
	ddaSampleTimer_ = 0;
	damageDuringDDASample_ = 0;
	damageTakenDuringDDASample_ = 0;
	previousPlayerHp_ = player_->GetHP();
	bossPhase_ = 1;
	ddaLevel_ = 3;
	ddaTargetLevel_ = 3;
	ddaDifficulty_ = 0.50f;
	ddaTargetDifficulty_ = 0.50f;
	ddaAttackRangeMultiplier_ = 1.0f;
	ApplyDDAParameters();
}

void BossAttackManager::RegisterPlayerDamage(int damage) {
	if (damage > 0) {
		damageDuringDDASample_ += damage;
	}
}

void BossAttackManager::SetBossPhase(int phase) {
	phase = std::clamp(phase, 1, 3);
	if (bossPhase_ == phase) return;
	bossPhase_ = phase;
	gapShockWaveAttack_.SetBossPhase(bossPhase_);
	groundSpearAttack_.SetBossPhase(bossPhase_);
	// 後半フェーズでは最低難易度も段階的に上げ、移行直後から変化を出す。
	ddaLevel_ = (std::max)(ddaLevel_, bossPhase_);
	ddaTargetLevel_ = (std::max)(ddaTargetLevel_, bossPhase_);
	ddaDifficulty_ =
	    static_cast<float>(ddaLevel_ - kDDAMinLevel) /
	    static_cast<float>(kDDAMaxLevel - kDDAMinLevel);
	ddaTargetDifficulty_ =
	    static_cast<float>(ddaTargetLevel_ - kDDAMinLevel) /
	    static_cast<float>(kDDAMaxLevel - kDDAMinLevel);
	ApplyDDAParameters();
}

void BossAttackManager::UpdateDDA() {
	const int currentPlayerHp = player_->GetHP();
	if (currentPlayerHp < previousPlayerHp_) {
		damageTakenDuringDDASample_ += previousPlayerHp_ - currentPlayerHp;
	}
	previousPlayerHp_ = currentPlayerHp;

	++ddaSampleTimer_;
	if (ddaSampleTimer_ < kDDASampleFrames) {
		return;
	}

	// 攻撃できている度合い65%＋被弾を抑えられている度合い35%で評価する。
	const float attackScore = std::clamp(
	    static_cast<float>(damageDuringDDASample_) / 150.0f, 0.0f, 1.0f);
	const float defenseScore = 1.0f - std::clamp(
	    static_cast<float>(damageTakenDuringDDASample_) / 60.0f, 0.0f, 1.0f);
	const float performanceScore = attackScore * 0.65f + defenseScore * 0.35f;
	// HPフェーズが1段進むごとに、成績評価へ難易度を1段階加算する。
	ddaTargetLevel_ = std::clamp(
	    static_cast<int>(performanceScore * 5.0f) + 1 + (bossPhase_ - 1),
	    kDDAMinLevel, kDDAMaxLevel);

	// 5秒ごとに最大1段階だけ変化させ、難易度の急変を防ぐ。
	if (ddaLevel_ < ddaTargetLevel_) {
		++ddaLevel_;
	} else if (ddaLevel_ > ddaTargetLevel_) {
		--ddaLevel_;
	}
	ddaDifficulty_ =
	    static_cast<float>(ddaLevel_ - kDDAMinLevel) /
	    static_cast<float>(kDDAMaxLevel - kDDAMinLevel);
	ddaTargetDifficulty_ =
	    static_cast<float>(ddaTargetLevel_ - kDDAMinLevel) /
	    static_cast<float>(kDDAMaxLevel - kDDAMinLevel);
	ApplyDDAParameters();
	ddaSampleTimer_ = 0;
	damageDuringDDASample_ = 0;
	damageTakenDuringDDASample_ = 0;
}

float BossAttackManager::GetDDAAttackCooldownMultiplier() const {
	switch (ddaLevel_) {
	case 1: return 1.50f;
	case 2: return 1.25f;
	case 3: return 1.00f;
	case 4: return 0.80f;
	case 5: return 0.625f;
	default: return 1.00f;
	}
}

int BossAttackManager::GetDDANextAttackIntervalFrames() const {
	float cooldownMultiplier = GetDDAAttackCooldownMultiplier();
	if (bossPhase_ >= 2) {
		cooldownMultiplier *= kPhaseTwoCooldownMultiplier;
	}
	return (std::max)(1, static_cast<int>(kAttackIntervalFrames * cooldownMultiplier));
}

void BossAttackManager::ApplyDDAParameters() {
	// 難度1～5で範囲80～120%、火力60～140%へ1段ずつ強化する。
	ddaAttackRangeMultiplier_ = 0.80f + 0.10f * static_cast<float>(ddaLevel_ - 1);
	const float focusedAttackDamageMultiplier =
	    0.60f + 0.20f * static_cast<float>(ddaLevel_ - 1);
	// HP層が進むたびに範囲を大きく変え、緑→橙→赤の強化を視認しやすくする。
	// DDA倍率とは乗算し、既存のプレイ成績による細かな変化も維持する。
	const float hpPhaseRangeMultiplier =
	    bossPhase_ == 1 ? 1.0f : bossPhase_ == 2 ? 1.4f : 1.8f;
	const float focusedAttackRangeMultiplier =
	    ddaAttackRangeMultiplier_ * hpPhaseRangeMultiplier;
	gapShockWaveAttack_.SetAttackRangeMultiplier(focusedAttackRangeMultiplier);
	gapShockWaveAttack_.SetDamageMultiplier(focusedAttackDamageMultiplier);
	closeExplosionAttack_.SetAttackRangeMultiplier(ddaAttackRangeMultiplier_);
	groundSpearAttack_.SetAttackRangeMultiplier(focusedAttackRangeMultiplier);
	groundSpearAttack_.SetDamageMultiplier(focusedAttackDamageMultiplier);
	summonMobsAttack_.SetAttackRangeMultiplier(ddaAttackRangeMultiplier_);
	chargeAttack_.SetAttackRangeMultiplier(ddaAttackRangeMultiplier_);
	rotatingLaserAttack_.SetAttackRangeMultiplier(ddaAttackRangeMultiplier_);
}

float BossAttackManager::NextRandom01() {
	randomState_ = randomState_ * 1664525u + 1013904223u;
	return static_cast<float>((randomState_ >> 8) & 0x00FFFFFFu) /
	       static_cast<float>(0x01000000u);
}

BossAttackManager::AttackType BossAttackManager::SelectWeaponAdaptedAttack() {
	const MainSkillType skill = player_->GetMainSkill();

	// 選択武器が攻撃を当てやすい状況を増やす。回転レーザーは
	// Mobの時間切れ専用なので、通常抽選には含めない。同じ攻撃が
	// 連続した場合は再抽選し、単調な連打にならないようにする。
	AttackType selected = AttackType::GapShockWave;
	for (int attempt = 0; attempt < 4; ++attempt) {
		const float roll = NextRandom01();
		switch (skill) {
		case MainSkillType::Bullet:
			selected = roll < 0.35f ? AttackType::GapShockWave :
			           roll < 0.65f ? AttackType::GroundSpear :
			           roll < 0.85f ? AttackType::Charge : AttackType::SummonMobs;
			break;
		case MainSkillType::Slash:
			selected = roll < 0.40f ? AttackType::CloseExplosion :
			           roll < 0.65f ? AttackType::GapShockWave :
			           roll < 0.85f ? AttackType::Charge :
			           roll < 0.95f ? AttackType::GroundSpear : AttackType::SummonMobs;
			break;
		case MainSkillType::Deploy:
			selected = roll < 0.30f ? AttackType::Charge :
			           roll < 0.55f ? AttackType::GroundSpear :
			           roll < 0.75f ? AttackType::GapShockWave :
			           roll < 0.90f ? AttackType::CloseExplosion : AttackType::SummonMobs;
			break;
		case MainSkillType::Charge:
			selected = roll < 0.30f ? AttackType::CloseExplosion :
			           roll < 0.55f ? AttackType::Charge :
			           roll < 0.75f ? AttackType::GroundSpear :
			           roll < 0.90f ? AttackType::GapShockWave : AttackType::SummonMobs;
			break;
		case MainSkillType::Beam:
			selected = roll < 0.35f ? AttackType::SummonMobs :
			           roll < 0.65f ? AttackType::GroundSpear :
			           roll < 0.85f ? AttackType::GapShockWave : AttackType::Charge;
			break;
		}
		// 既にMobがいる間は再召喚せず、射線を作りやすい地面槍へ差し替える。
		if (selected == AttackType::SummonMobs && summonMobsAttack_.IsActive()) {
			selected = AttackType::GroundSpear;
		}
		if (!hasLastAttackType_ || selected != lastAttackType_) {
			break;
		}
	}
	return selected;
}

BossAttackManager::AttackType BossAttackManager::SelectPhaseThreeSecondaryAttack(
    AttackType primaryAttack) const {
	// Boss自身が移動する突進と地面槍を同時に使うと、自分で槍へ衝突して
	// 即ダウンするため避ける。各主攻撃に対して安全に並行更新できる攻撃を選ぶ。
	switch (primaryAttack) {
	case AttackType::GapShockWave:
		return AttackType::GroundSpear;
	case AttackType::GroundSpear:
		return AttackType::GapShockWave;
	case AttackType::SummonMobs:
		return AttackType::GapShockWave;
	case AttackType::Charge:
		return AttackType::SummonMobs;
	case AttackType::CloseExplosion:
		return AttackType::GroundSpear;
	case AttackType::RotatingLaser:
	default:
		return AttackType::RotatingLaser;
	}
}

void BossAttackManager::StartAttack(
    AttackType attackType, WorldTransform& bossWorldTransform,
    bool recordAsLastAttack) {
	if (recordAsLastAttack) {
		lastAttackType_ = attackType;
		hasLastAttackType_ = true;
	}

	switch (attackType) {
	case AttackType::GapShockWave:
		gapShockWaveAttack_.Start(
		    bossWorldTransform.translation_,
		    player_->GetWorldTransform().translation_);
		break;
	case AttackType::CloseExplosion:
		approachingCloseExplosion_ = true;
		UpdateCloseExplosionApproach(bossWorldTransform);
		break;
	case AttackType::GroundSpear:
		groundSpearAttack_.Start(
		    bossWorldTransform.translation_,
		    player_->GetWorldTransform().translation_);
		groundSpearSequenceInProgress_ = groundSpearAttack_.IsActive();
		break;
	case AttackType::SummonMobs:
		// 既にMobが残っている場合は、二重召喚せず衝撃波へ差し替える。
		if (summonMobsAttack_.IsActive()) {
			gapShockWaveAttack_.Start(
			    bossWorldTransform.translation_,
			    player_->GetWorldTransform().translation_);
		} else {
			summonMobsAttack_.Start(bossWorldTransform.translation_);
			summonMobsFollowupStarted_ = false;
		}
		break;
	case AttackType::Charge:
		chargeAttack_.Start(
		    bossWorldTransform.translation_,
		    player_->GetWorldTransform().translation_);
		break;
	case AttackType::RotatingLaser:
		forcedLaserPending_ = false;
		rotatingLaserAttack_.Start(bossWorldTransform);
		break;
	}
}

void BossAttackManager::ScheduleNextAttack() {
	attackCooldown_ = GetDDANextAttackIntervalFrames();
	nextAttackType_ =
	    forcedLaserPending_ ? AttackType::RotatingLaser : SelectWeaponAdaptedAttack();
}

void BossAttackManager::UpdateCloseExplosionApproach(
    WorldTransform& bossWorldTransform) {
	const Vector3& playerPosition = player_->GetWorldTransform().translation_;
	const float differenceX = playerPosition.x - bossWorldTransform.translation_.x;
	const float differenceZ = playerPosition.z - bossWorldTransform.translation_.z;
	const float distance =
	    std::sqrt(differenceX * differenceX + differenceZ * differenceZ);
	if (distance <= kCloseExplosionApproachDistance || distance <= 0.0001f) {
		approachingCloseExplosion_ = false;
		closeExplosionAttack_.Start(bossWorldTransform.translation_);
		return;
	}
	bossWorldTransform.translation_.x +=
	    differenceX / distance * kCloseExplosionApproachSpeed;
	bossWorldTransform.translation_.z +=
	    differenceZ / distance * kCloseExplosionApproachSpeed;
	bossWorldTransform.rotation_.y = std::atan2(differenceX, differenceZ);
}

const char* BossAttackManager::GetAttackName(AttackType attackType) const {
	switch (attackType) {
	case AttackType::GapShockWave: return "Gap Shock Wave";
	case AttackType::CloseExplosion: return "Close Explosion";
	case AttackType::GroundSpear: return "Ground Spear";
	case AttackType::SummonMobs: return "Summon Mobs";
	case AttackType::Charge: return "Charge";
	case AttackType::RotatingLaser: return "Rotating Laser";
	default: return "Unknown";
	}
}

const char* BossAttackManager::GetNextAttackName() const {
	return GetAttackName(forcedLaserPending_ ? AttackType::RotatingLaser : nextAttackType_);
}

const char* BossAttackManager::GetDDAWeaponBiasName() const {
	if (player_ == nullptr) return "None";
	switch (player_->GetMainSkill()) {
	case MainSkillType::Bullet: return "Bullet: open lanes / predictable movement";
	case MainSkillType::Slash: return "Slash: boss approaches player";
	case MainSkillType::Deploy: return "Deploy: charge and spear openings";
	case MainSkillType::Charge: return "Dash: close and head-on attacks";
	case MainSkillType::Beam: return "Beam: mobs and long firing lanes";
	default: return "None";
	}
}

void BossAttackManager::Update(WorldTransform& bossWorldTransform) {
	if (player_ == nullptr) {
		return;
	}
	latestBossPosition_ = bossWorldTransform.translation_;
	UpdateDDA();
	// Remaining spears keep updating as background obstacles during later attacks.
	if (groundSpearAttack_.IsActive()) {
		groundSpearAttack_.Update();
	}
	// Mobは独立更新し、存在中でもボス本体の攻撃管理を止めない。
	if (summonMobsAttack_.IsActive()) {
		const bool wasSummoning = summonMobsAttack_.IsSummoning();
		summonMobsAttack_.Update();
		if (summonMobsAttack_.IsSummoning() && !summonMobsFollowupStarted_) {
			summonMobsFollowupStarted_ = true;
			attackCooldown_ = 0;
			nextAttackType_ = SelectWeaponAdaptedAttack();
		}
		if (wasSummoning && !summonMobsAttack_.IsActive()) {
			if (summonMobsAttack_.GetSurvivorCountOnTimeout() > 0) {
				// 制限時間内に全滅できなければ、現在の攻撃終了後に必殺技を予約する。
				forcedLaserPending_ = true;
				nextAttackType_ = AttackType::RotatingLaser;
				attackCooldown_ = 0;
			}
			summonMobsFollowupStarted_ = false;
		}
	}
	if (downTimer_ > 0) {
		--downTimer_;
		if (downTimer_ == 0) {
			ScheduleNextAttack();
		}
		return;
	}

	if (approachingCloseExplosion_) {
		UpdateCloseExplosionApproach(bossWorldTransform);
		return;
	}

	if (gapShockWaveAttack_.IsActive()) {
		gapShockWaveAttack_.Update();
		if (!gapShockWaveAttack_.IsActive()) {
			ScheduleNextAttack();
		}
		return;
	}
	if (closeExplosionAttack_.IsActive()) {
		closeExplosionAttack_.Update(bossWorldTransform.translation_);
		if (!closeExplosionAttack_.IsActive()) {
			ScheduleNextAttack();
		}
		return;
	}
	if (groundSpearSequenceInProgress_) {
		if (groundSpearAttack_.IsSequenceFinished()) {
			groundSpearSequenceInProgress_ = false;
			ScheduleNextAttack();
		}
		return;
	}
	if (summonMobsAttack_.IsWarning()) {
		// 召喚位置を示す予兆中だけは、後続攻撃を始めない。
		return;
	}
	if (chargeAttack_.IsActive()) {
		const bool wasCharging = chargeAttack_.IsCharging();
		chargeAttack_.Update(bossWorldTransform);
		if (wasCharging && groundSpearAttack_.DestroySpearHitByBoss(
		                       bossWorldTransform.translation_, kBossCollisionRadius)) {
			chargeAttack_.StopForDown();
			downTimer_ = kDownFrames;
			nextAttackAfterDown_ = SelectWeaponAdaptedAttack();
			return;
		}
		if (!chargeAttack_.IsActive()) {
			ScheduleNextAttack();
		}
		return;
	}
	if (rotatingLaserAttack_.IsActive()) {
		rotatingLaserAttack_.Update(bossWorldTransform, &groundSpearAttack_);
		if (!rotatingLaserAttack_.IsActive()) {
			groundSpearAttack_.RetractLaserShieldSpears();
			// 必殺技の終了後は、ボスをダウン状態にする。
			downTimer_ = kDownFrames;
			forcedLaserPending_ = false;
			nextAttackAfterDown_ = SelectWeaponAdaptedAttack();
		}
		return;
	}

	if (attackCooldown_ > 0) {
		--attackCooldown_;
		return;
	}

	const AttackType primaryAttack = nextAttackType_;
	StartAttack(primaryAttack, bossWorldTransform, true);
	// 3本目では通常攻撃を一度に2種類開始する。回転レーザーだけは
	// GroundSpearを絶対的な安置として扱う既存ルールを守るため単独にする。
	if (bossPhase_ >= 3 && primaryAttack != AttackType::RotatingLaser) {
		const AttackType secondaryAttack =
		    SelectPhaseThreeSecondaryAttack(primaryAttack);
		if (secondaryAttack != primaryAttack) {
			StartAttack(secondaryAttack, bossWorldTransform, false);
		}
	}
}

bool BossAttackManager::GetPredictionLightData(Vector3& position, float& radius) const {
	// Down中は攻撃予兆を出さない。各攻撃側にWarning状態が残っていてもライトを消す。
	if (IsDown()) return false;
	// 円形予兆など、攻撃側が持つ範囲ライトを優先する。
	if (closeExplosionAttack_.GetPredictionLightData(position, radius)) return true;
	if (groundSpearAttack_.GetPredictionLightData(position, radius)) return true;
	if (summonMobsAttack_.GetPredictionLightData(position, radius)) return true;
	if (chargeAttack_.GetPredictionLightData(position, radius)) return true;
	// 衝撃波や発射中のレーザーを含む全攻撃にも赤いPointLightを持たせる。
	// 専用の予兆範囲を持たない場合はBoss本体を発光中心にする。
	if (IsWarning() || IsAttacking() || IsAttackInProgress()) {
		position = latestBossPosition_;
		radius = kBossCollisionRadius;
		return true;
	}
	return false;
}

void BossAttackManager::Draw() {
	gapShockWaveAttack_.Draw();
	closeExplosionAttack_.Draw();
	groundSpearAttack_.Draw();
	summonMobsAttack_.Draw();
	chargeAttack_.Draw();
	rotatingLaserAttack_.Draw();
}
