#include "BossRotatingLaserAttack.h"

#include "BossGroundSpearAttack.h"
#include "ColliderManager.h"
#include "Player.h"
#include "TransformUtility.h"

#include <algorithm>
#include <cmath>
#include <numbers>

using namespace KamataEngine;

BossRotatingLaserAttack::~BossRotatingLaserAttack() {
	StopAllSounds();
}

void BossRotatingLaserAttack::StopAllSounds() {
	Audio* const audio = Audio::GetInstance();
	if (isChargeStartPlaying_) {
		audio->StopWave(chargeStartVoiceHandle_);
		isChargeStartPlaying_ = false;
	}
	if (isChargeLoopPlaying_) {
		audio->StopWave(chargeLoopVoiceHandle_);
		isChargeLoopPlaying_ = false;
	}
	if (isBeamShotPlaying_) {
		audio->StopWave(beamShotVoiceHandle_);
		isBeamShotPlaying_ = false;
	}
	if (isBeamLoopPlaying_) {
		audio->StopWave(beamLoopVoiceHandle_);
		isBeamLoopPlaying_ = false;
	}
}

void BossRotatingLaserAttack::Initialize(
    Model* laserModel, Model* sonicBoomModel, Camera* camera, Player* player) {
	laserModel_ = laserModel;
	sonicBoomModel_ = sonicBoomModel;
	camera_ = camera;
	player_ = player;
	chargeStartSeHandle_ = Audio::GetInstance()->LoadWave("Audio/Game_PlayerCharge_SE.wav");
	chargeLoopSeHandle_ = Audio::GetInstance()->LoadWave("Audio/Game_PlayerChargeLoop_SE.wav");
	chargeMaxSeHandle_ = Audio::GetInstance()->LoadWave("Audio/Game_PlayerChargeMax_SE.wav");
	beamShotSeHandle_ = Audio::GetInstance()->LoadWave("Audio/Game_BeamShot_SE.wav");
	beamLoopSeHandle_ = Audio::GetInstance()->LoadWave("Audio/Game_PlayerBeamLoop_SE.wav");
	for (WorldTransform& transform : laserTransforms_) {
		transform.Initialize();
	}
	for (WorldTransform& transform : beamImpactTransforms_) {
		transform.Initialize();
	}
	laserColor_.Initialize();
	// alpha=2はObjPSでBossビーム専用HLSLを選択するためのマーカー。
	laserColor_.SetColor({1.0f, 0.75f, 0.05f, 2.0f});
	beamImpactColor_.Initialize();
	beamImpactColor_.SetColor({1.0f, 0.04f, 0.01f, 17.0f});
	for (SonicBoom& boom : sonicBooms_) {
		boom.transform.Initialize();
		boom.color.Initialize();
		boom.active = false;
	}
	sonicBoomSpawnTimer_ = 0;
	beamEffectTime_ = 0.0f;
	sonicBoomSpawnTimer_ = 0;
	for (SonicBoom& boom : sonicBooms_) {
		boom.active = false;
	}
	isActive_ = false;
	isWarning_ = false;
	pendingBossHealAmount_ = 0;
}

void BossRotatingLaserAttack::Start(const WorldTransform& bossWorldTransform) {
	if (laserModel_ == nullptr || camera_ == nullptr || player_ == nullptr) {
		return;
	}
	StopAllSounds();
	startRotationY_ = bossWorldTransform.rotation_.y;
	rotationY_ = startRotationY_;
	warningTimer_ = kWarningFrames;
	remainingFrames_ = 0;
	isActive_ = true;
	isWarning_ = true;
	pendingBossHealAmount_ = 0;
	laserColor_.SetColor({1.0f, 0.75f, 0.05f, 2.0f});
	beamEffectTime_ = 0.0f;
	chargeStartVoiceHandle_ =
	    Audio::GetInstance()->PlayWave(chargeStartSeHandle_, false, 0.72f);
	isChargeStartPlaying_ = true;
	chargeLoopVoiceHandle_ =
	    Audio::GetInstance()->PlayWave(chargeLoopSeHandle_, true, 0.48f);
	isChargeLoopPlaying_ = true;
}

void BossRotatingLaserAttack::Update(
    WorldTransform& bossWorldTransform, BossGroundSpearAttack* groundSpearAttack) {
	if (!isActive_) {
		return;
	}
	beamEffectTime_ += 1.0f / 60.0f;
	beamImpactColor_.SetColor(
	    {1.0f, 0.04f, 0.01f, 17.0f + std::fmod(beamEffectTime_, 0.999f)});
	UpdateSonicBooms(bossWorldTransform.translation_);
	// Materialの未使用UV-offset Z成分を専用HLSLの時間入力にする。
	// BossBeam専用モデルなので、他のOBJのUVには影響しない。
	if (laserModel_ != nullptr) {
		for (const auto& mesh : laserModel_->GetMeshes()) {
			if (mesh && mesh->GetMaterial()) {
				mesh->GetMaterial()->uvOffset_.z = beamEffectTime_;
				mesh->GetMaterial()->Update();
			}
		}
	}

	if (isWarning_) {
		// 発射前は4方向の予兆線を静止表示する。
		for (size_t i = 0; i < laserTransforms_.size(); ++i) {
			const float angle = rotationY_ +
			                    std::numbers::pi_v<float> * 0.5f * static_cast<float>(i);
			const Vector3 start = bossWorldTransform.translation_;
			const Vector3 end = GetArenaEdge(start, angle);
			UpdateLaserTransform(i, start, end, angle);
		}
		if (--warningTimer_ <= 0) {
			// 予測終了でチャージ音を止め、Playerと同じビーム音へ切り替える。
			if (isChargeStartPlaying_) {
				Audio::GetInstance()->StopWave(chargeStartVoiceHandle_);
				isChargeStartPlaying_ = false;
			}
			if (isChargeLoopPlaying_) {
				Audio::GetInstance()->StopWave(chargeLoopVoiceHandle_);
				isChargeLoopPlaying_ = false;
			}
			Audio::GetInstance()->PlayWave(chargeMaxSeHandle_, false, 0.82f);
			beamShotVoiceHandle_ =
			    Audio::GetInstance()->PlayWave(beamShotSeHandle_, false, 0.82f);
			isBeamShotPlaying_ = true;
			beamLoopVoiceHandle_ =
			    Audio::GetInstance()->PlayWave(beamLoopSeHandle_, true, 0.54f);
			isBeamLoopPlaying_ = true;
			isWarning_ = false;
			remainingFrames_ = kLaserFrames;
			sonicBoomSpawnTimer_ = 0;
			laserColor_.SetColor({1.0f, 0.15f, 0.05f, 2.0f});
		}
		return;
	}

	// 3秒で2回転。EaseInQuad により、開始時は遅く徐々に回転速度を上げる。
	const int elapsedFrames = kLaserFrames - remainingFrames_;
	const float t = static_cast<float>(elapsedFrames) /
	                static_cast<float>(kLaserFrames - 1);
	rotationY_ = startRotationY_ + kTotalRotation * t * t;
	bossWorldTransform.rotation_.y = rotationY_;
	UpdateWorldTransform(bossWorldTransform);

	for (size_t i = 0; i < laserTransforms_.size(); ++i) {
		const float laserRadius = GetLaserRadius();
		const float angle = rotationY_ + std::numbers::pi_v<float> * 0.5f * static_cast<float>(i);
		const Vector3 start = bossWorldTransform.translation_;
		Vector3 end = GetArenaEdge(start, angle);

		Vector3 blockedPosition = {};
		const bool wasBlocked = groundSpearAttack != nullptr &&
		                        groundSpearAttack->BlockLaserWithSpear(
		                            start, end, laserRadius, blockedPosition);
		if (wasBlocked) {
			// 槍が受け止めたフレームは、その位置でレーザーを止める。
			end = blockedPosition;
		}
		UpdateLaserTransform(i, start, end, angle);
		if (!wasBlocked) {
			CheckPlayerCollision(start, end);
		}
	}

	if (--remainingFrames_ <= 0) {
		isActive_ = false;
		StopAllSounds();
	}
}

void BossRotatingLaserAttack::Draw() {
	if (!isActive_ || laserModel_ == nullptr || camera_ == nullptr) {
		return;
	}
	for (const WorldTransform& transform : laserTransforms_) {
		laserModel_->Draw(transform, *camera_, &laserColor_);
	}
	if (!isWarning_ && sonicBoomModel_ != nullptr) {
		for (const WorldTransform& impactTransform : beamImpactTransforms_) {
			sonicBoomModel_->Draw(impactTransform, *camera_, &beamImpactColor_);
		}
		for (const SonicBoom& boom : sonicBooms_) {
			if (boom.active) {
				sonicBoomModel_->Draw(boom.transform, *camera_, &boom.color);
			}
		}
	}
}

void BossRotatingLaserAttack::UpdateSonicBooms(const Vector3& bossPosition) {
	for (SonicBoom& boom : sonicBooms_) {
		if (!boom.active) continue;
		--boom.remainingFrames;
		if (boom.remainingFrames <= 0) {
			boom.active = false;
			continue;
		}
		const float progress = 1.0f - static_cast<float>(boom.remainingFrames) /
		    static_cast<float>(kSonicBoomLifetimeFrames);
		const float eased = 1.0f - (1.0f - progress) * (1.0f - progress);
		const float radius = 6.0f + eased * 30.0f;
		boom.transform.scale_ = {radius, 1.0f, radius};
		boom.transform.translation_ = {bossPosition.x, bossPosition.y + 0.35f, bossPosition.z};
		UpdateWorldTransform(boom.transform);
		// alpha=13..14 はObjPSでソニックブームを選び、端数を寿命として渡す。
		boom.color.SetColor({1.0f, 0.06f, 0.015f, 13.0f + progress});
	}
	if (isWarning_) return;
	if (sonicBoomSpawnTimer_-- <= 0) {
		SpawnSonicBoom(bossPosition);
		sonicBoomSpawnTimer_ = kSonicBoomIntervalFrames;
	}
}

void BossRotatingLaserAttack::SpawnSonicBoom(const Vector3& bossPosition) {
	for (SonicBoom& boom : sonicBooms_) {
		if (boom.active) continue;
		boom.active = true;
		boom.remainingFrames = kSonicBoomLifetimeFrames;
		boom.transform.scale_ = {6.0f, 1.0f, 6.0f};
		boom.transform.rotation_ = {0.0f, rotationY_, 0.0f};
		boom.transform.translation_ = {bossPosition.x, bossPosition.y + 0.35f, bossPosition.z};
		UpdateWorldTransform(boom.transform);
		boom.color.SetColor({1.0f, 0.06f, 0.015f, 13.0f});
		return;
	}
}

void BossRotatingLaserAttack::UpdateLaserTransform(
    size_t index, const Vector3& start, const Vector3& end, float rotationY) {
	const float dx = end.x - start.x;
	const float dz = end.z - start.z;
	const float length = std::sqrt(dx * dx + dz * dz);
	const float laserRadius = GetLaserRadius();
	WorldTransform& transform = laserTransforms_[index];
	// BossBeam.obj is centered at the origin and is approximately 2 units wide
	// and 2 units long. Match the same local-Z beam convention used by PlayerBeam:
	// X/Y are the radius and Z is the half length of the segment.
	transform.scale_ = {
	    laserRadius,
	    laserRadius,
	    length * 0.5f,
	};
	transform.rotation_.y = rotationY;
	transform.translation_ = {
	    (start.x + end.x) * 0.5f,
	    start.y,
	    (start.z + end.z) * 0.5f,
	};
	UpdateWorldTransform(transform);
	WorldTransform& impactTransform = beamImpactTransforms_[index];
	const float impactRadius = (std::max)(8.0f, laserRadius * 1.65f);
	impactTransform.scale_ = {impactRadius, 1.0f, impactRadius};
	// 水平な予測円を縦へ起こし、4本それぞれのビーム先端断面へ向ける。
	impactTransform.rotation_ = {
	    std::numbers::pi_v<float> * 0.5f,
	    rotationY,
	    0.0f};
	impactTransform.translation_ = {end.x, end.y, end.z};
	UpdateWorldTransform(impactTransform);
}

void BossRotatingLaserAttack::CheckPlayerCollision(const Vector3& start, const Vector3& end) {
	if (player_ == nullptr) {
		return;
	}
	const ColliderManager::Segment laserSegment = {start, end};
	const ColliderManager::CircleXZ playerCollider = {
	    player_->GetWorldTransform().translation_,
	    player_->GetCollisionRadius() + GetLaserRadius()};
	if (ColliderManager::CheckSegmentCircleXZ(laserSegment, playerCollider)) {
		const int hpBeforeDamage = player_->GetHP();
		player_->ApplyDamage(kDamage);
		if (player_->GetHP() < hpBeforeDamage) {
			// 無敵時間中の接触では回復せず、実ダメージが入った時だけ吸収する。
			pendingBossHealAmount_ += kBossHealPerHit;
		}
	}
}

Vector3 BossRotatingLaserAttack::GetArenaEdge(const Vector3& start, float angle) const {
	if (arenaRadius_ <= 0.0f) {
		return {start.x + std::sin(angle) * kLaserRange, start.y,
		        start.z + std::cos(angle) * kLaserRange};
	}
	const float radius = (std::max)(0.0f, arenaRadius_ - GetLaserRadius());
	const float dx = start.x - arenaCenter_.x;
	const float dz = start.z - arenaCenter_.z;
	const float dirX = std::sin(angle);
	const float dirZ = std::cos(angle);
	const float projection = dx * dirX + dz * dirZ;
	const float c = dx * dx + dz * dz - radius * radius;
	const float discriminant = projection * projection - c;
	const float distance = discriminant > 0.0f ?
	    -projection + std::sqrt(discriminant) : kLaserRange;
	return {start.x + dirX * distance, start.y, start.z + dirZ * distance};
}
