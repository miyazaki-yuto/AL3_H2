#pragma once

#include "KamataEngine.h"
#include "SkillTypes.h"
#include "SubSkillController.h"

#include <memory>
#include <array>
#include <vector>

class Boss;

// プレイヤーのメインスキルに関する生成・更新・描画・硬直を集約する。
class PlayerAttackController final {
public:
	~PlayerAttackController();
	void Initialize(
	    KamataEngine::Model* bulletModel, KamataEngine::Model* bombModel,
	    KamataEngine::Model* effectModel, KamataEngine::Model* attackModel,
	    KamataEngine::Model* slashLeftModel, KamataEngine::Model* slashRightModel,
	    KamataEngine::Model* beamModel,
	    KamataEngine::Model* predictionModel);
	void SetMainSkill(MainSkillType mainSkill) { mainSkill_ = mainSkill; }
	MainSkillType GetMainSkill() const { return mainSkill_; }
	void SetSubSkills(const std::array<SubSkillType, 2>& subSkills) {
		subSkillController_.SetSubSkills(subSkills);
	}
	void SetTargetBoss(Boss* boss) { targetBoss_ = boss; }
	void SetOwnerPosition(const KamataEngine::Vector3& position) { ownerPosition_ = position; }
	int ConsumeChargeGrowthEffectCount() {
		const int count = pendingChargeGrowthEffectCount_;
		pendingChargeGrowthEffectCount_ = 0;
		return count;
	}
	void SetArenaBounds(const KamataEngine::Vector3& center, float radius) {
		arenaCenter_ = center;
		arenaRadius_ = radius;
	}
	void Update();
	// 被ダメージ時に、現在入力・実行中の攻撃だけを中断する。
	// 既に生成済みの弾と爆弾は残す。
	void CancelActiveSkill();
	// シーン遷移を待たず、Player攻撃のループ音を即時停止する。
	void StopAllSounds();
	bool IsBeamActive() const { return isBeamActive_; }
	bool IsChargeAttackActive() const {
		return chargeAttackTimer_ > 0 || chargeAttackContactGuard_;
	}
	bool IsChargingSubSkill() const { return isChargingSubSkill_; }
	bool IsBerserkActive() const { return subSkillController_.IsBerserkActive(); }
	int GetBerserkStackCount() const {
		return subSkillController_.GetStackCount(SubSkillType::Berserk);
	}
	KamataEngine::Vector3 GetBerserkMoveDirection(const KamataEngine::Vector3& playerPosition) const;
	int ConsumeBerserkLifeSteal();
	int ConsumeSuccessfulHitCount();
	int ConsumeBossHitCount();
	float GetIncreaseMoveSpeedMultiplier() const;
	int ConsumeSteelGrowth();
	int ConsumeSteelBurstCount();
	int GetSteelBurstStackCount() const { return steelBurstHitCount_; }
	bool IsDoubleSteelActive() const {
		return subSkillController_.GetStackCount(SubSkillType::Steal) >= 2;
	}
	float GetSteelMoveSpeedMultiplier() const;
	float GetChargeProgress() const;
	float GetChargeDamageMultiplier() const;
	float GetChargeRangeMultiplier() const;
	float GetChargeMaximumDamageMultiplier() const;
	float GetChargeMaximumRangeMultiplier() const;
	// 選択中メインスキルのクールタイム回復率。使用可能なら1.0。
	float GetMainSkillCooldownRatio() const;
	// 現在表示中のPlayer攻撃に青ライトを追従させる。
	bool GetAttackLightPosition(KamataEngine::Vector3& position) const;

	// 攻撃中の移動を処理し、操作をロックすべき場合は true を返す。
	bool ApplyActiveAttack(
	    KamataEngine::WorldTransform& playerTransform,
	    bool isAttackHeld,
	    bool isAttackTriggered,
	    const KamataEngine::Vector3& aimDirection);
	// 攻撃を開始できた場合は true を返す。
	bool TryActivate(
	    KamataEngine::WorldTransform& playerTransform,
	    const KamataEngine::Vector3& aimDirection);
	void Draw(const KamataEngine::Camera& camera) const;

private:
	struct Bullet {
		KamataEngine::WorldTransform worldTransform;
		KamataEngine::Vector3 velocity = {};
		bool isAlive = true;
		bool hasReflected = false;
		int reflectionCount = 0;
		int damage = 0;
	};
	struct BeamRetraction {
		KamataEngine::WorldTransform worldTransform;
		std::vector<std::unique_ptr<KamataEngine::WorldTransform>> followingSegments;
		KamataEngine::Vector3 direction = {0.0f, 0.0f, 1.0f};
		KamataEngine::Vector3 originPosition = {0.0f, 0.0f, 0.0f};
		KamataEngine::Vector3 tipPosition = {0.0f, 0.0f, 0.0f};
		float initialLength = 0.0f;
		int remainingFrames = 0;
		int segmentDurationFrames = 1;
		int damageIntervalTimer = 0;
		int damage = 1;
		float width = 1.0f;
		float retractFramesPerWorldUnit = 0.0f;
		bool foldTowardOrigin = false;
	};
	struct ReflectedBeamSegment {
		KamataEngine::WorldTransform worldTransform;
	};
	struct FlyingAttack {
		KamataEngine::WorldTransform worldTransform;
		KamataEngine::Vector3 velocity = {};
		int remainingFrames = 0;
		int damage = 1;
		float collisionRadius = 1.0f;
		bool homesTowardBoss = false;
	};
	struct Bomb {
		KamataEngine::WorldTransform worldTransform;
		KamataEngine::WorldTransform predictionTransform;
		KamataEngine::ObjectColor explosionColor;
		int fuseTimer = 0;
		int explosionTimer = -1;
		bool hasDealtDamage = false;
		int damage = 0;
		float explosionScale = 0.0f;
	};
	struct ExplosionParticle {
		KamataEngine::WorldTransform worldTransform;
		KamataEngine::ObjectColor color;
		KamataEngine::Vector3 velocity = {};
		KamataEngine::Vector3 angularVelocity = {};
		float baseScale = 1.0f;
		float colorVariation = 0.0f;
		int remainingFrames = 0;
		int totalFrames = 1;
	};
	struct FeedbackParticle {
		enum class Motion { Spark, Expand, Absorb };
		KamataEngine::WorldTransform worldTransform;
		KamataEngine::ObjectColor color;
		KamataEngine::Vector3 start = {};
		KamataEngine::Vector3 target = {};
		KamataEngine::Vector3 velocity = {};
		KamataEngine::Vector4 baseColor = {1.0f, 1.0f, 1.0f, 1.0f};
		float baseScale = 1.0f;
		int remainingFrames = 0;
		int totalFrames = 1;
		Motion motion = Motion::Spark;
	};

	bool ActivateBullet(
	    const KamataEngine::WorldTransform& playerTransform,
	    const KamataEngine::Vector3& aimDirection);
	bool ActivateSlash(
	    KamataEngine::WorldTransform& playerTransform,
	    const KamataEngine::Vector3& aimDirection);
	bool ActivateCharge(const KamataEngine::Vector3& aimDirection);
	bool ActivateDeploy(
	    const KamataEngine::WorldTransform& playerTransform,
	    const KamataEngine::Vector3& aimDirection);
	bool ActivateBeam(
	    const KamataEngine::WorldTransform& playerTransform,
	    const KamataEngine::Vector3& aimDirection);
	bool ActivateCurrentSkill(
	    KamataEngine::WorldTransform& playerTransform,
	    const KamataEngine::Vector3& aimDirection);
	void UpdateBullets();
	void UpdateFlyingAttacks();
	void SpawnFlyingSlashes(
	    const KamataEngine::WorldTransform& playerTransform,
	    const KamataEngine::Vector3& aimDirection, float sizeMultiplier);
	void SpawnDeployFlyingAttacks(
	    const KamataEngine::Vector3& origin, int sourceDamage);
	bool ReflectBulletFromArena(Bullet& bullet) const;
	void AccelerateBulletAfterReflection(Bullet& bullet) const;
	bool ReflectChargeFromArena(KamataEngine::WorldTransform& playerTransform);
	void ConstrainPositionInsideArena(KamataEngine::Vector3& position, float radius) const;
	float GetDistanceToArenaEdge(
	    const KamataEngine::Vector3& start, const KamataEngine::Vector3& direction,
	    float objectRadius = 0.0f) const;
	void ReflectBulletFromBoss(Bullet& bullet) const;
	void ReflectBulletFromEnemyPosition(
	    Bullet& bullet, const KamataEngine::Vector3& enemyPosition,
	    float separationRadius = 0.0f) const;
	void ReflectChargeFromBoss(KamataEngine::WorldTransform& playerTransform);
	void StopChargeAtBoss(KamataEngine::WorldTransform& playerTransform);
	void ReflectBeamInRandomOppositeDirection(const KamataEngine::Vector3& reflectionOrigin);
	void ReflectBeamFromArena(const KamataEngine::Vector3& reflectionOrigin);
	void SaveCurrentBeamSegment();
	void UpdateBombs();
	void SpawnExplosionParticles(
	    const KamataEngine::Vector3& origin, float explosionRadius);
	void UpdateExplosionParticles();
	void SpawnFeedbackBurst(
	    const KamataEngine::Vector3& origin, const KamataEngine::Vector4& color,
	    FeedbackParticle::Motion motion, int count = 8);
	void SpawnAbsorbParticles(
	    const KamataEngine::Vector3& origin, const KamataEngine::Vector4& color,
	    int count);
	void UpdateFeedbackParticles();
	bool IsBossInCircle(const KamataEngine::Vector3& center, float radius) const;
	void DamageBoss(int damage);
	void RegisterEnemyHit(int damage);
	int GetRequiredChargeFrames() const;
	float GetDamageMultiplier() const;
	int GetAttackInterval(int baseInterval) const;
	float GetAttackSizeMultiplier() const;
	void UpdateBeamTransform(const KamataEngine::Vector3& aimDirection);
	void UpdateBeamTransformToTipDistance(
	    const KamataEngine::Vector3& beamOrigin,
	    float tipDistance);
	void BeginBeamExtension();
	void UpdateBeamExtensionTransform();
	void UpdateBeamRetractionTransform(BeamRetraction& beamRetraction) const;
	void ApplyBeamRetractionDamage(BeamRetraction& beamRetraction);
	void BeginBeamRetraction(const KamataEngine::WorldTransform& beamTransform);
	void StopChargeSounds();
	void StopBeamSounds();
	void SetBeamRetractionSegment(
	    BeamRetraction& beamRetraction, const KamataEngine::WorldTransform& beamTransform);

	KamataEngine::Model* bulletModel_ = nullptr;
	KamataEngine::Model* bombModel_ = nullptr;
	KamataEngine::Model* effectModel_ = nullptr;
	KamataEngine::Model* attackModel_ = nullptr;
	KamataEngine::Model* slashLeftModel_ = nullptr;
	KamataEngine::Model* slashRightModel_ = nullptr;
	KamataEngine::Model* beamModel_ = nullptr;
	KamataEngine::Model* predictionModel_ = nullptr;
	KamataEngine::ObjectColor predictionColor_;
	KamataEngine::ObjectColor beamColor_;
	uint32_t slashSeHandle_ = 0;
	uint32_t beamShotSeHandle_ = 0;
	uint32_t bulletShotSeHandle_ = 0;
	uint32_t bulletHitSeHandle_ = 0;
	uint32_t bombPlaceSeHandle_ = 0;
	uint32_t bombTickSeHandle_ = 0;
	uint32_t bombExplosionSeHandle_ = 0;
	uint32_t chargeStartSeHandle_ = 0;
	uint32_t chargeLoopSeHandle_ = 0;
	uint32_t chargeMaxSeHandle_ = 0;
	uint32_t dashStartSeHandle_ = 0;
	uint32_t dashReflectSeHandle_ = 0;
	uint32_t beamLoopSeHandle_ = 0;
	uint32_t chargeLoopVoiceHandle_ = 0;
	uint32_t chargeStartVoiceHandle_ = 0;
	uint32_t beamLoopVoiceHandle_ = 0;
	uint32_t beamShotVoiceHandle_ = 0;
	bool isChargeLoopPlaying_ = false;
	bool isChargeStartPlaying_ = false;
	bool isBeamLoopPlaying_ = false;
	bool isBeamShotPlaying_ = false;
	bool hasPlayedChargeMaxSe_ = false;
	SubSkillController subSkillController_;
	Boss* targetBoss_ = nullptr;
	MainSkillType mainSkill_ = MainSkillType::Bullet;
	std::vector<std::unique_ptr<Bullet>> bullets_;
	std::vector<std::unique_ptr<FlyingAttack>> flyingAttacks_;
	std::vector<std::unique_ptr<Bomb>> bombs_;
	std::vector<std::unique_ptr<ExplosionParticle>> explosionParticles_;
	std::vector<std::unique_ptr<FeedbackParticle>> feedbackParticles_;
	std::vector<std::unique_ptr<BeamRetraction>> beamRetractions_;
	std::vector<std::unique_ptr<ReflectedBeamSegment>> reflectedBeamSegments_;
	KamataEngine::WorldTransform slashTransform_;
	KamataEngine::WorldTransform slashVisualTransform_;
	KamataEngine::WorldTransform beamTransform_;
	KamataEngine::Vector3 slashOrigin_ = {0.0f, 0.0f, 0.0f};
	int bulletCooldownTimer_ = 0;
	int bulletCooldownDuration_ = 0;
	int bulletAttackLockTimer_ = 0;
	int slashTimer_ = 0;
	int slashCooldownTimer_ = 0;
	int slashCooldownDuration_ = 0;
	bool slashHasDealtDamage_ = false;
	int slashDamage_ = 20;
	KamataEngine::Vector3 slashSlideDirection_ = {1.0f, 0.0f, 0.0f};
	float slashSlideProgress_ = 0.0f;
	bool nextSlashSlidesRight_ = true;
	bool activeSlashSlidesRight_ = true;
	int chargeAttackTimer_ = 0;
	bool chargeAttackContactGuard_ = false;
	int chargeAttackCooldownTimer_ = 0;
	int chargeAttackCooldownDuration_ = 0;
	int deployCooldownTimer_ = 0;
	int deployCooldownDuration_ = 0;
	int deployAttackLockTimer_ = 0;
	int nextBombPredictionLayer_ = 0;
	KamataEngine::Vector3 chargeAttackDirection_ = {0.0f, 0.0f, 1.0f};
	KamataEngine::Vector3 chargeVelocity_ = {0.0f, 0.0f, 0.0f};
	bool chargeHasDealtDamage_ = false;
	int chargeAttackDamage_ = 30;
	int chargeReflectionCount_ = 0;
	bool isBeamActive_ = false;
	float beamEffectTime_ = 0.0f;
	KamataEngine::Vector3 beamOrigin_ = {0.0f, 0.0f, 0.0f};
	KamataEngine::Vector3 primaryBeamOrigin_ = {0.0f, 0.0f, 0.0f};
	KamataEngine::Vector3 beamDirection_ = {0.0f, 0.0f, 1.0f};
	int beamExtendTimer_ = 0;
	int beamExtendDurationFrames_ = 1;
	int beamDamageIntervalTimer_ = 0;
	int beamActiveTimer_ = 0;
	int beamCooldownTimer_ = 0;
	int beamCooldownDuration_ = 0;
	bool isBeamBlockedByEnemy_ = false;
	bool beamHasPersistentBossContact_ = false;
	// Bossから反射して離れている同一線分が、命中点で同じBossへ再命中するのを防ぐ。
	bool beamIgnoresBossOnCurrentSegment_ = false;
	bool beamContinuesWithoutHold_ = false;
	bool beamStopRequested_ = false;
	bool requiresBeamRelease_ = false;
	bool requiresAttackRelease_ = false;
	int beamReflectionCount_ = 0;
	int beamDamage_ = 3;
	float beamLength_ = 1000.0f;
	float beamMaximumLength_ = 1000.0f;
	float beamStartOffset_ = 1.5f;
	bool isChargingSubSkill_ = false;
	int chargeFrames_ = 0;
	int maxChargeHoldFrames_ = 0;
	KamataEngine::Vector3 chargeAimDirection_ = {0.0f, 0.0f, 1.0f};
	float pendingChargeDamageMultiplier_ = 1.0f;
	float pendingChargeRangeMultiplier_ = 1.0f;
	float pendingBerserkLifeSteal_ = 0.0f;
	int pendingSteelGrowth_ = 0;
	int pendingSteelBurstCount_ = 0;
	int pendingSuccessfulHitCount_ = 0;
	int pendingBossHitCount_ = 0;
	int pendingChargeGrowthEffectCount_ = 0;
	int steelHitCount_ = 0;
	int steelBurstHitCount_ = 0;
	int steelBurstBuffTimer_ = 0;
	int increaseHitCount_ = 0;
	int chargeHitCount_ = 0;
	int berserkHitCount_ = 0;
	int elapsedGameFrames_ = 0;
	float beamWidth_ = 2.0f;
	float chargeAttackHitRadius_ = 3.0f;
	KamataEngine::Vector3 ownerPosition_ = {};
	KamataEngine::Vector3 arenaCenter_ = {};
	float arenaRadius_ = 0.0f;

	inline static constexpr float kBulletSpeed = 2.5f;
	inline static constexpr float kBulletReflectionSpeedMultiplier = 1.25f;
	inline static constexpr float kBulletScale = 1.5f;
	// PlayerBullet.objの正面はゲーム側の+Z方向と逆なので180度補正する。
	inline static constexpr float kBulletModelForwardYawOffset = 3.14159265f;
	inline static constexpr float kBulletSpawnOffset = 2.0f;
	// 約0.33秒（60fps）。長押し時の弾の連射間隔。
	inline static constexpr int kBulletIntervalFrames = 20;
	inline static constexpr int kBulletAttackLockFrames = 2;
	inline static constexpr int kBulletDamage = 10;
	// 反翔1枠につき2回。2枠重複時は最大4回反射する。
	inline static constexpr int kMaxBulletReflectionCountPerStack = 2;
	inline static constexpr int kMaxBeamReflectionCount = 4;
	inline static constexpr float kBeamReflectionRandomAngleRadians = 0.65f;
	inline static constexpr int kSlashActiveFrames = 10;
	inline static constexpr int kSlashIntervalFrames = 24;
	inline static constexpr float kSlashForwardOffset = 4.0f;
	inline static constexpr float kSlashLungeDistance = 1.5f;
	inline static constexpr float kSlashSlideDistance = 4.0f;
	// Player追従とは別に、斬撃モデルを左右へ大きく横切らせる幅。
	inline static constexpr float kSlashVisualSweepHalfDistance = 3.0f;
	inline static constexpr float kSlashBossSeparationMargin = 1.1f;
	inline static constexpr int kSlashDamage = 20;
	inline static constexpr float kFlyingSlashSpeed = 2.2f;
	inline static constexpr int kFlyingSlashLifeFrames = 90;
	inline static constexpr float kFlyingSlashAngleStep = 0.16f;
	inline static constexpr int kDeployFlyingAttackCountPerStack = 3;
	inline static constexpr float kDeployFlyingAttackSpeed = 1.6f;
	inline static constexpr int kDeployFlyingAttackLifeFrames = 150;
	inline static constexpr float kFlyingAttackHomingInterpolation = 0.14f;
	inline static constexpr float kChargeAttackSpeed = 3.0f;
	inline static constexpr float kChargeAttackAcceleration = 0.35f;
	inline static constexpr float kChargeAttackEndDrag = 0.90f;
	// 終了前に十分な慣性を見せるため、0.75秒かけて減速する。
	inline static constexpr int kChargeAttackDecelerationFrames = 45;
	// Boss本体の通常接触ノックバック範囲（本体半径 + 1）より外へ出す。
	inline static constexpr float kChargeReflectionSeparation = 1.25f;
	// 反射時はモンスト風に、3秒間フィールド内を跳ね続ける。
	inline static constexpr int kChargeAttackFrames = 180;
	// 突進の連打を抑えるため、0.5秒から1.5秒へ延長する。
	inline static constexpr int kChargeAttackCooldownFrames = 90;
	inline static constexpr int kChargeAttackDamage = 30;
	inline static constexpr float kBombScale = 1.5f;
	// PlayerBomb.objは底面がほぼ原点にあるため、この分だけ持ち上げて接地する。
	inline static constexpr float kBombModelGroundOffset = 0.006456f;
	inline static constexpr float kBombForwardOffset = 3.0f;
	inline static constexpr int kBombFuseFrames = 90;
	inline static constexpr int kBombTickIntervalFrames = 30;
	inline static constexpr int kBombExplosionFrames = 20;
	inline static constexpr float kBombExplosionScale = 25.0f;
	inline static constexpr int kExplosionParticleCount = 24;
	inline static constexpr int kExplosionParticleLifeFrames = 30;
	inline static constexpr float kExplosionParticleGravity = 0.055f;
	// 約1.5秒（60fps）。範囲攻撃なので弾より長い再設置間隔にする。
	inline static constexpr int kBombIntervalFrames = 90;
	inline static constexpr int kBombAttackLockFrames = 4;
	inline static constexpr int kBombDamage = 40;
	inline static constexpr float kBombPredictionDisplayHeight = 0.04f;
	inline static constexpr float kBombPredictionLayerStep = 0.01f;
	inline static constexpr int kBombPredictionLayerCount = 8;
	inline static constexpr float kBeamWidth = 2.0f;
	inline static constexpr float kBeamHeight = 2.0f;
	// Playerの見た目は成長時に地面へ埋まらないよう上昇するが、攻撃まで上昇させない。
	// 初期サイズ時の中心高を、Mobと交差する戦闘用の基準高として固定する。
	inline static constexpr float kPlayerAttackBaseHeight = 2.0f;
	// 初期表示scale=2.0で判定半径3.1となる比率。見た目だけ大きくしても
	// 当たり判定が急増せず、スティール成長時には同じ割合で拡大する。
	inline static constexpr float kPlayerCollisionRadiusPerVisualScale = 1.55f;
	// 初期表示scale=2.0時の半径。巨大化で増えた半径分だけ近接攻撃を前へ伸ばす。
	inline static constexpr float kBasePlayerCollisionRadius = 3.1f;
	inline static constexpr float kPlayerForwardExtentPerVisualScale = 3.55f;
	inline static constexpr float kPlayerModelForwardYawOffset = 3.14159265f;
	// 実際の終点は壁・Bossの判定で決まるため、基礎射程は実質無制限にする。
	inline static constexpr float kBeamLength = 100000.0f;
	inline static constexpr float kBeamStartOffset = 1.5f;
	inline static constexpr float kBeamMuzzleMargin = 0.1f;
	// ビーム全長から伸長時間を決め、壁までの距離によらず速度を揃える。
	inline static constexpr float kBeamExtendWorldUnitsPerFrame = 6.0f;
	inline static constexpr int kBeamExtendMinimumFrames = 3;
	inline static constexpr int kBeamExtendMaximumFrames = 30;
	inline static constexpr int kBeamActiveFrames = 60;
	inline static constexpr int kBeamCooldownFrames = 100;
	// ビーム全長から収束時間を求める。反射数が多い場合も全体時間は上限内に収める。
	inline static constexpr float kBeamRetractFramesPerWorldUnit = 0.075f;
	inline static constexpr int kBeamRetractMinimumTotalFrames = 6;
	inline static constexpr int kBeamRetractMaximumTotalFrames = 30;
	inline static constexpr int kBeamRetractMinimumSegmentFrames = 2;
	inline static constexpr int kBeamDamage = 3;
	inline static constexpr int kBeamDamageIntervalFrames = 2;
	inline static constexpr int kChargeMaxFrames = 120;
	// チャージ装備中は命中するほど最大チャージまでの時間が短くなる。
	// 重複時はスタック数に応じて成長速度も倍になる。
	inline static constexpr int kChargeFramesReducedPerHit = 2;
	inline static constexpr int kChargeMinimumFrames = 30;
	inline static constexpr int kChargeAutoFireDelayFrames = 60;
	inline static constexpr float kChargeMaxDamageMultiplier = 3.0f;
	inline static constexpr float kChargeMaxRangeMultiplier = 2.0f;
	// チャージの射程倍率を、見た目にはっきり分かるビーム太さへ変換する強さ。
	inline static constexpr float kChargeBeamWidthBonusScale = 1.5f;
	// 60fps換算で30秒ごとに、チャージの最大増幅量を伸ばす。
	inline static constexpr int kChargeGrowthIntervalFrames = 60 * 30;
	// 30秒ごとに最大チャージダメージを+0.50倍成長させる。
	inline static constexpr float kChargeDamageGrowthPerInterval = 0.50f;
	inline static constexpr float kChargeRangeGrowthPerInterval = 0.10f;
	inline static constexpr float kBerserkDamageMultiplier = 1.5f;
	// 暴走を選択している間の基礎攻撃力ボーナス（1枠につき+5%）。
	inline static constexpr float kBerserkBaseDamageBonusPerStack = 0.05f;
	// 反翔を選択している間の基礎攻撃力ボーナス（1枠につき+10%）。
	inline static constexpr float kHanshoBaseDamageBonusPerStack = 0.15f;
	inline static constexpr float kBerserkAttackIntervalMultiplier = 0.65f;
	inline static constexpr int kBerserkLifeStealPerHit = 1;
	// 命中を重ねるたび、次回以降の吸収量を1枠あたり0.1ずつ増加する。
	inline static constexpr float kBerserkLifeStealGrowthPerHit = 0.10f;
	inline static constexpr int kIncreaseMaxHitCount = 10000;
	// 増大の威力成長を、1ヒットにつき3%へ強化する。
	inline static constexpr float kIncreaseDamagePerHit = 0.03f;
	inline static constexpr float kIncreaseSizePerHit = 0.03f;
	inline static constexpr float kIncreaseCooldownPerHit = 0.001f;
	inline static constexpr float kIncreaseBulletSpeedPenaltyPerHit = 0.0005f;
	inline static constexpr float kIncreaseMoveSpeedPenaltyPerHit = 0.0005f;
	inline static constexpr float kIncreaseMinimumSpeedMultiplier = 0.5f;
	inline static constexpr float kSteelMoveSpeedPenaltyPerHit = 0.005f;
	inline static constexpr float kSteelMinimumSpeedMultiplier = 0.4f;
	inline static constexpr int kSteelBurstRequiredHits = 10;
	inline static constexpr int kSteelBurstGrowthUnits = 15;
	inline static constexpr int kSteelBurstBuffFrames = 60 * 3;
	inline static constexpr float kSteelBurstDamageMultiplier = 1.20f;
	inline static constexpr float kSteelBurstMoveSpeedMultiplier = 1.15f;
};
