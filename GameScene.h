#pragma once
#include "KamataEngine.h"
#include "CameraController.h"
#include "Ground.h"
#include "Wall.h"
#include "Player.h"
#include "Boss.h"
#include "SkillTypes.h"
#include "SimpleButton.h"
#include <memory>
#include <array>
#include <vector>

class GameScene {
public:
	GameScene() = default;
	~GameScene() = default;

	void Initialize(
	    MainSkillType mainSkill = MainSkillType::Bullet,
	    std::array<SubSkillType, 2> subSkills = {SubSkillType::None, SubSkillType::None});
	void Update();
	void Draw();
	bool IsClearRequested() const { return isClearRequested_; }
	bool IsGameOverRequested() const { return isGameOverRequested_; }
	bool IsSelectRequested() const { return isSelectRequested_; }

private:
	void InitializeHpBars();
	void UpdateHpBars();
	void UpdateChargeGauge();
	void UpdateSkillCooldownGauge();
	void UpdateSteelBurstGauge();
	void SpawnPraiseParticles(int hitCount);
	void UpdatePraiseParticles();
	void InitializeSkillIcons(
	    MainSkillType mainSkill, const std::array<SubSkillType, 2>& subSkills);

	KamataEngine::WorldTransform worldTransform_;
	KamataEngine::Camera camera_;
	std::unique_ptr<KamataEngine::LightGroup> lightGroup_;
	CameraController cameraController_;
	bool isClearRequested_ = false;
	bool isGameOverRequested_ = false;
	bool isSelectRequested_ = false;
	bool isPaused_ = false;
	std::unique_ptr<KamataEngine::Sprite> menuButtonGuideSprite_;
	std::unique_ptr<KamataEngine::Sprite> pauseDimSprite_;
	SimpleButtonGroup pauseButtons_;
	int lightingAnimationFrame_ = 0;
	int previousPlayerHp_ = 0;
	bool wasChargeAtMaximum_ = false;

	// 見た目でも接触しないよう、ボス本体の当たり判定にわずかな余白を足す。
	inline static constexpr float kBossContactSeparationMargin = 1.0f;
	inline static constexpr float kDamageCameraShakeStrength = 1.15f;
	inline static constexpr int kDamageCameraShakeFrames = 18;
	inline static constexpr float kChargeMaxCameraShakeStrength = 0.58f;
	inline static constexpr int kChargeMaxCameraShakeFrames = 10;
	inline static constexpr float kBeamCameraShakeStrength = 0.13f;
	inline static constexpr float kGroundSpearCameraShakeStrength = 2.4f;
	inline static constexpr int kGroundSpearCameraShakeFrames = 8;

	// 地面のモデル
	std::unique_ptr<KamataEngine::Model> model_Ground_;
	std::unique_ptr<Ground> ground_;
	
	// 壁
	std::unique_ptr<KamataEngine::Model> model_Wall_;
	std::unique_ptr<Wall> wall_;

	// プレイヤー
	std::unique_ptr<KamataEngine::Model> model_Player_;
	std::unique_ptr<KamataEngine::Model> model_PlayerBullet_;
	std::unique_ptr<KamataEngine::Model> model_PlayerBomb_;
	std::unique_ptr<KamataEngine::Model> model_Effect_;
	std::unique_ptr<KamataEngine::Model> model_PlayerAttack_;
	std::unique_ptr<KamataEngine::Model> model_PlayerSlashLeft_;
	std::unique_ptr<KamataEngine::Model> model_PlayerSlashRight_;
	std::unique_ptr<KamataEngine::Model> model_PlayerBeam_;
	std::unique_ptr<Player> player_;
	struct PraiseParticle {
		KamataEngine::WorldTransform worldTransform;
		KamataEngine::ObjectColor color;
		KamataEngine::Vector4 baseColor = {1.0f, 1.0f, 1.0f, 1.0f};
		KamataEngine::Vector3 velocity = {};
		KamataEngine::Vector3 angularVelocity = {};
		float baseScale = 1.0f;
		int remainingFrames = 0;
		int totalFrames = 1;
	};
	std::vector<std::unique_ptr<PraiseParticle>> praiseParticles_;
	uint32_t praiseRandomState_ = 0x51A7E123u;
	std::unique_ptr<KamataEngine::Sprite> playerHpBackgroundSprite_;
	std::unique_ptr<KamataEngine::Sprite> playerHpBarSprite_;
	std::unique_ptr<KamataEngine::Sprite> chargeGaugeBackgroundSprite_;
	std::unique_ptr<KamataEngine::Sprite> chargeGaugeBarSprite_;
	std::array<std::unique_ptr<KamataEngine::Sprite>, 3> chargeGaugeElectricSprites_;
	int chargeGaugeElectricTimer_ = 0;

	// ボス
	std::unique_ptr<KamataEngine::Model> model_Boss_;
	std::unique_ptr<KamataEngine::Model> model_BossHead_;
	std::unique_ptr<KamataEngine::Model> model_BossBullet_;
	std::unique_ptr<KamataEngine::Model> model_Laser_;
	std::unique_ptr<KamataEngine::Model> model_PredictionCircle_;
	std::unique_ptr<KamataEngine::Model> model_ChargePredictionLine_;
	std::unique_ptr<KamataEngine::Model> model_GroundSpear_;
	std::unique_ptr<KamataEngine::Model> model_Mob_;
	std::unique_ptr<Boss> boss_;
	std::unique_ptr<KamataEngine::Sprite> bossHpBackgroundSprite_;
	std::unique_ptr<KamataEngine::Sprite> bossHpNextLayerSprite_;
	std::unique_ptr<KamataEngine::Sprite> bossHpDamageTrailSprite_;
	std::unique_ptr<KamataEngine::Sprite> bossHpBarSprite_;
	float bossHpPreviousRatio_ = 1.0f;
	float bossHpDamageTrailRatio_ = 1.0f;
	int bossHpDamageTrailDelayTimer_ = 0;
	int previousBossHpPhase_ = 1;
	std::unique_ptr<KamataEngine::Sprite> mobTimeLimitBackgroundSprite_;
	std::unique_ptr<KamataEngine::Sprite> mobTimeLimitBarSprite_;

	// 戦闘中に右下へ表示する、選択中のメイン1個＋サブ2個。
	std::unique_ptr<KamataEngine::Sprite> mainSkillIconSprite_;
	std::unique_ptr<KamataEngine::Sprite> mainSkillCooldownBackgroundSprite_;
	std::unique_ptr<KamataEngine::Sprite> mainSkillCooldownBarSprite_;
	std::array<std::unique_ptr<KamataEngine::Sprite>, 2> subSkillIconSprites_;
	std::unique_ptr<KamataEngine::Sprite> steelBurstGaugeBackgroundSprite_;
	std::array<std::unique_ptr<KamataEngine::Sprite>, 10> steelBurstGaugeSegments_;

#ifdef DEBUG
	bool isDebugCameraActive_ = false;
	std::unique_ptr<KamataEngine::DebugCamera> debugCamera_;
#endif
};
