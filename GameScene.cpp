#include "GameScene.h"

#include "ColliderManager.h"
#include "PostProcessCRT.h"
#include "TransformUtility.h"

#include <algorithm>
#include <cmath>

using namespace KamataEngine;

namespace {

constexpr Vector2 kBossHpBarPosition = {340.0f, 30.0f};
constexpr Vector2 kBossHpBarSize = {600.0f, 26.0f};
constexpr Vector2 kBossHpBackgroundPosition = {334.0f, 24.0f};
constexpr Vector2 kBossHpBackgroundSize = {612.0f, 38.0f};
constexpr int kBossHpDamageTrailDelayFrames = 10;
constexpr float kBossHpDamageTrailFollowSpeed = 0.075f;
constexpr Vector2 kMobTimeLimitBarPosition = {340.0f, 65.0f};
constexpr Vector2 kMobTimeLimitBarSize = {600.0f, 6.0f};
constexpr Vector2 kMobTimeLimitBackgroundPosition = {337.0f, 62.0f};
constexpr Vector2 kMobTimeLimitBackgroundSize = {606.0f, 12.0f};
constexpr Vector2 kPlayerHpBarPosition = {40.0f, 660.0f};
constexpr Vector2 kPlayerHpBarSize = {360.0f, 22.0f};
constexpr Vector2 kPlayerHpBackgroundPosition = {34.0f, 654.0f};
constexpr Vector2 kPlayerHpBackgroundSize = {372.0f, 34.0f};
constexpr int kPlayerBaseMaxHp = 100;
// 最大HPが1増えるごとに、ゲージ容量を1.2pxずつ右へ伸ばす。
constexpr float kPlayerHpWidthPerMaxHp = 1.2f;
// 右下のスキルアイコンと重ならない範囲で表示幅を制限する。
constexpr float kPlayerHpBarMaximumWidth = 10000.0f;
constexpr Vector2 kChargeGaugeSize = {120.0f, 10.0f};
constexpr Vector2 kChargeGaugeBackgroundSize = {128.0f, 18.0f};
constexpr float kChargeGaugeVerticalOffset = 18.0f;
constexpr int kChargeGaugeElectricFrames = 18;
constexpr float kScreenWidth = 1280.0f;
constexpr float kScreenHeight = 720.0f;
constexpr Vector2 kMainSkillIconPosition = {1070.0f, 590.0f};
constexpr Vector2 kMainSkillIconSize = {100.0f, 100.0f};
constexpr Vector2 kMainSkillCooldownBackgroundPosition = {1068.0f, 696.0f};
constexpr Vector2 kMainSkillCooldownBackgroundSize = {104.0f, 10.0f};
constexpr Vector2 kMainSkillCooldownBarPosition = {1070.0f, 698.0f};
constexpr Vector2 kMainSkillCooldownBarSize = {100.0f, 6.0f};
constexpr std::array<Vector2, 2> kSubSkillIconPositions = {
	Vector2{990.0f, 630.0f}, Vector2{1190.0f, 630.0f}};
constexpr Vector2 kSubSkillIconSize = {64.0f, 64.0f};
constexpr Vector2 kSteelBurstGaugeBackgroundPosition = {987.0f, 699.0f};
constexpr Vector2 kSteelBurstGaugeBackgroundSize = {270.0f, 13.0f};
constexpr Vector2 kSteelBurstGaugeStartPosition = {991.0f, 702.0f};
constexpr Vector2 kSteelBurstGaugeSegmentSize = {23.0f, 7.0f};
constexpr float kSteelBurstGaugeSegmentStep = 26.0f;

const char* GetMainSkillIconPath(MainSkillType mainSkill) {
	switch (mainSkill) {
	case MainSkillType::Bullet:
		return "UI/SkillIcons/MainBulletSimple.png";
	case MainSkillType::Slash:
		return "UI/SkillIcons/MainSlashSimple.png";
	case MainSkillType::Deploy:
		return "UI/SkillIcons/MainDeploySimple.png";
	case MainSkillType::Charge:
		return "UI/SkillIcons/MainDashSimple.png";
	case MainSkillType::Beam:
		return "UI/SkillIcons/MainBeamSimple.png";
	default:
		return "UI/SkillIcons/MainBulletSimple.png";
	}
}

const char* GetSubSkillIconPath(SubSkillType subSkill) {
	switch (subSkill) {
	case SubSkillType::Hansho:
		return "UI/SkillIcons/SubHanshoSimple.png";
	case SubSkillType::Charge:
		return "UI/SkillIcons/SubChargeSimple.png";
	case SubSkillType::Berserk:
		return "UI/SkillIcons/SubBerserkSimple.png";
	case SubSkillType::Increase:
		return "UI/SkillIcons/SubIncreaseSimple.png";
	case SubSkillType::Steal:
		return "UI/SkillIcons/SubSteelSimple.png";
	case SubSkillType::None:
	default:
		return nullptr;
	}
}

bool IsPauseButtonTriggered() {
	const Input* input = Input::GetInstance();
	if (input == nullptr) return false;
	if (input->TriggerKey(DIK_ESCAPE)) return true;
	XINPUT_STATE current{};
	XINPUT_STATE previous{};
	return input->GetJoystickState(0, current) &&
	       input->GetJoystickStatePrevious(0, previous) &&
	       (current.Gamepad.wButtons & XINPUT_GAMEPAD_START) != 0 &&
	       (previous.Gamepad.wButtons & XINPUT_GAMEPAD_START) == 0;
}

} // namespace

void GameScene::Initialize(MainSkillType mainSkill, std::array<SubSkillType, 2> subSkills) {
	isClearRequested_ = false;
	isGameOverRequested_ = false;
	isSelectRequested_ = false;
	isPaused_ = false;
	lightingAnimationFrame_ = 0;
	praiseParticles_.clear();
	praiseRandomState_ = 0x51A7E123u;
	wasChargeAtMaximum_ = false;
	worldTransform_.Initialize();
	camera_.Initialize();
	pauseButtons_.Clear();
	// ButtonUI atlas のメニューボタンを、ポーズ操作の常設ガイドとして表示する。
	menuButtonGuideSprite_.reset(Sprite::Create(
	    TextureManager::Load("ButtonUI.png"), {1210.0f, 0.0f},
	    {1.0f, 1.0f, 1.0f, 0.92f}));
	menuButtonGuideSprite_->SetTextureRect({1150.0f, 535.0f}, {170.0f, 180.0f});
	menuButtonGuideSprite_->SetSize({64.0f, 68.0f});
	pauseDimSprite_.reset(Sprite::Create(
	    TextureManager::Load("white1x1.png"), {0.0f, 0.0f},
	    {0.0f, 0.0f, 0.0f, 0.72f}));
	pauseDimSprite_->SetSize({kScreenWidth, kScreenHeight});
	SimpleButton::Colors pauseButtonColors;
	pauseButtonColors.normal = {0.78f, 0.78f, 0.82f, 0.94f};
	pauseButtonColors.hover = {1.22f, 1.22f, 1.28f, 1.0f};
	pauseButtonColors.pressed = {0.70f, 0.66f, 0.78f, 1.0f};
	pauseButtons_.Add(
	    TextureManager::Load("UI/PauseBackButton.png"),
	    {446.0f, 272.0f}, {180.0f, 176.0f}, pauseButtonColors);
	pauseButtons_.Add(
	    TextureManager::Load("UI/PauseContinueButton.png"),
	    {666.0f, 272.0f}, {180.0f, 176.0f}, pauseButtonColors);
	pauseButtons_.SetFocusedIndex(1);

	// 再びGameシーンへ入った場合も、Groundが古いModelを参照しない順番で破棄する。
	ground_.reset();
	model_Ground_.reset();
	wall_.reset();
	model_Wall_.reset();
	player_.reset();
	model_PlayerBeam_.reset();
	model_PlayerAttack_.reset();
	model_PlayerSlashLeft_.reset();
	model_PlayerSlashRight_.reset();
	model_Effect_.reset();
	model_PlayerBomb_.reset();
	model_PlayerBullet_.reset();
	model_Player_.reset();
	boss_.reset();
	model_BossHead_.reset();
	model_Mob_.reset();
	model_GroundSpear_.reset();
	model_PredictionCircle_.reset();
	model_ChargePredictionLine_.reset();
	model_BossBullet_.reset();
	model_Laser_.reset();
	model_Boss_.reset();
	lightGroup_.reset(LightGroup::Create());
	if (lightGroup_) {
		// 平行光源は補助光に留め、SpotLightをフィールドの主照明にする。
		lightGroup_->SetAmbientColor({0.10f, 0.10f, 0.14f});
		lightGroup_->SetDirLightActive(0, true);
		lightGroup_->SetDirLightDir(0, {0.35f, 1.0f, 0.25f});
		lightGroup_->SetDirLightColor(0, {0.06f, 0.06f, 0.09f});
		lightGroup_->SetDirLightActive(1, false);
		lightGroup_->SetDirLightActive(2, false);

		// フィールド中央を強く照らし、外周へ向かって自然に暗くなる点光源。
		lightGroup_->SetPointLightActive(0, true);
		lightGroup_->SetPointLightPos(0, {0.0f, 70.0f, 0.0f});
		lightGroup_->SetPointLightColor(0, {0.72f, 0.82f, 1.0f});
		lightGroup_->SetPointLightIntensity(0, 2.0f);
		lightGroup_->SetPointLightRadius(0, 240.0f);
		lightGroup_->SetPointLightDecay(0, 2.0f);
		// 1番はBossの攻撃予兆へ追従する動的ライトとして使う。
		lightGroup_->SetPointLightActive(1, false);
		lightGroup_->SetPointLightColor(1, {1.0f, 0.16f, 0.04f});
		lightGroup_->SetPointLightIntensity(1, 2.2f);
		lightGroup_->SetPointLightRadius(1, 65.0f);
		lightGroup_->SetPointLightDecay(1, 1.4f);
		lightGroup_->SetPointLightActive(2, false);
		lightGroup_->SetPointLightColor(2, {0.05f, 0.45f, 1.0f});
		lightGroup_->SetPointLightIntensity(2, 1.8f);
		// Playerの弾・ビーム・斬撃・設置攻撃の周囲まで青い光を広く届ける。
		lightGroup_->SetPointLightRadius(2, 48.0f);
		lightGroup_->SetPointLightDecay(2, 1.35f);

		// フィールド中央の上空から、アリーナ全体を広角で照らす。
		// APIには光が進む向きを渡す。上空から地面へ向けるため-Y方向。
		lightGroup_->SetSpotLightActive(0, true);
		lightGroup_->SetSpotLightPos(0, {0.0f, 340.0f, 0.0f});
		lightGroup_->SetSpotLightDir(0, {0.0f, -1.0f, 0.0f});
		lightGroup_->SetSpotLightColor(0, {1.0f, 0.92f, 0.82f});
		lightGroup_->SetSpotLightIntensity(0, 0.65f);
		lightGroup_->SetSpotLightRadius(0, 650.0f);
		lightGroup_->SetSpotLightDecay(0, 1.0f);
		lightGroup_->SetSpotLightFactorAngle(0, {0.65f, 0.90f});
		// 1番はPlayerへ追従する青い個別スポットライト。
		lightGroup_->SetSpotLightActive(1, false);
		lightGroup_->SetSpotLightDir(1, {0.0f, -1.0f, 0.0f});
		lightGroup_->SetSpotLightColor(1, {0.18f, 0.58f, 1.0f});
		lightGroup_->SetSpotLightIntensity(1, 8.0f);
		lightGroup_->SetSpotLightRadius(1, 75.0f);
		lightGroup_->SetSpotLightDecay(1, 1.20f);
		lightGroup_->SetSpotLightFactorAngle(1, {0.12f, 0.38f});
		// 2番はBossへ追従する赤い個別スポットライト。
		lightGroup_->SetSpotLightActive(2, false);
		lightGroup_->SetSpotLightDir(2, {0.0f, -1.0f, 0.0f});
		lightGroup_->SetSpotLightColor(2, {1.0f, 0.16f, 0.08f});
		lightGroup_->SetSpotLightIntensity(2, 9.0f);
		lightGroup_->SetSpotLightRadius(2, 110.0f);
		lightGroup_->SetSpotLightDecay(2, 1.20f);
		lightGroup_->SetSpotLightFactorAngle(2, {0.14f, 0.46f});
		lightGroup_->Update();
	}
	InitializeHpBars();
	InitializeSkillIcons(mainSkill, subSkills);

	// 地面
	model_Ground_.reset(Model::CreateFromOBJ("Ground", false));
	if (model_Ground_) {
		ground_ = std::make_unique<Ground>();
		ground_->Initialize(model_Ground_.get(), &camera_);
	}

	// 壁
	model_Wall_.reset(Model::CreateFromOBJ("Wall", false));
	if (model_Wall_) {
		wall_ = std::make_unique<Wall>();
		wall_->Initialize(model_Wall_.get(), &camera_);
	}


	model_Player_.reset(Model::CreateFromOBJ("Player", false));
	model_PlayerBullet_.reset(Model::CreateFromOBJ("PlayerBullet", false));
	model_PlayerBomb_.reset(Model::CreateFromOBJ("PlayerBomb", false));
	model_Effect_.reset(Model::CreateFromOBJ("Effect", false));
	// 反翔の飛翔斬撃は引き続きcube、通常斬撃は左右専用モデルを使う。
	model_PlayerAttack_.reset(Model::CreateFromOBJ("cube", false));
	model_PlayerSlashLeft_.reset(Model::CreateFromOBJ("PlayerSlashLeft", false));
	model_PlayerSlashRight_.reset(Model::CreateFromOBJ("PlayerSlashRight", false));
	model_PlayerBeam_.reset(Model::CreateFromOBJ("PlayerBeam", false));
	model_PredictionCircle_.reset(Model::CreateFromOBJ("PredictionCircle", false));
	if (model_Player_ && model_PlayerBullet_ && model_PlayerBomb_ && model_Effect_ &&
	    model_PlayerAttack_ && model_PlayerSlashLeft_ && model_PlayerSlashRight_ &&
	    model_PlayerBeam_) {
		player_ = std::make_unique<Player>();
		player_->Initialize(
		    model_Player_.get(), model_PlayerBullet_.get(), model_PlayerBomb_.get(),
		    model_Effect_.get(), model_PlayerAttack_.get(),
		    // 作成モデルの見た目が攻撃方向と逆だったため、左右を入れ替えて渡す。
		    model_PlayerSlashRight_.get(), model_PlayerSlashLeft_.get(),
		    model_PlayerBeam_.get(),
		    model_PredictionCircle_.get(), &camera_);
		player_->SetMainSkill(mainSkill);
		player_->SetSubSkills(subSkills);
		if (wall_) {
			player_->SetArenaBounds(wall_->GetCenter(), wall_->GetInnerRadius());
		}
		cameraController_.Initialize(&camera_, &player_->GetWorldTransform());
		previousPlayerHp_ = player_->GetHP();
	}

	model_Boss_.reset(Model::CreateFromOBJ("BossBody", false));
	model_BossHead_.reset(Model::CreateFromOBJ("BossHead", false));
	// 周辺拡散弾と召喚Mob弾で、新しいBossBulletモデルを共有する。
	model_BossBullet_.reset(Model::CreateFromOBJ("BossBullet", false));
	model_Laser_.reset(Model::CreateFromOBJ("BossBeam", false));
	model_ChargePredictionLine_.reset(Model::CreateFromOBJ("BoxPredictionCircle", false));
	model_GroundSpear_.reset(Model::CreateFromOBJ("GroundSpears", false));
	// 召喚MobはBossの頭部モデルを小型化して使用する。
	model_Mob_.reset(Model::CreateFromOBJ("BossHead", false));
	if (
	    model_Boss_ && model_BossHead_ && model_BossBullet_ && model_Laser_ && model_PredictionCircle_ && model_ChargePredictionLine_ && model_GroundSpear_ &&
	    model_Mob_ && player_) {
		boss_ = std::make_unique<Boss>();
		boss_->Initialize(
		    model_Boss_.get(), model_BossHead_.get(),
		    model_BossBullet_.get(),
		    model_Laser_.get(),
		    model_PredictionCircle_.get(),
		    model_ChargePredictionLine_.get(),
		    model_GroundSpear_.get(),
		    model_Mob_.get(),
		    &camera_,
		    player_.get());
		player_->SetTargetBoss(boss_.get());
		if (wall_) {
			boss_->SetArenaBounds(wall_->GetCenter(), wall_->GetInnerRadius());
		}
		cameraController_.SetSecondaryTarget(&boss_->GetWorldTransform());
	}

	// 地面・壁・キャラクター・攻撃モデルで同じライティングを共有する。
	if (lightGroup_) {
		const auto applyLight = [this](const std::unique_ptr<Model>& model) {
			if (model) {
				model->SetLightGroup(lightGroup_.get());
			}
		};
		applyLight(model_Ground_);
		applyLight(model_Wall_);
		applyLight(model_Player_);
		applyLight(model_PlayerBullet_);
		applyLight(model_PlayerBomb_);
		applyLight(model_Effect_);
		applyLight(model_PlayerAttack_);
		applyLight(model_PlayerSlashLeft_);
		applyLight(model_PlayerSlashRight_);
		applyLight(model_PlayerBeam_);
		applyLight(model_Boss_);
		applyLight(model_BossHead_);
		applyLight(model_BossBullet_);
		applyLight(model_Laser_);
		applyLight(model_PredictionCircle_);
		applyLight(model_ChargePredictionLine_);
		applyLight(model_GroundSpear_);
		applyLight(model_Mob_);
	}

#ifdef DEBUG
	debugCamera_ = std::make_unique<DebugCamera>(1280, 720);
	PrimitiveDrawer::GetInstance()->SetCamera(&camera_);
	AxisIndicator::GetInstance()->SetVisible(true);
	AxisIndicator::GetInstance()->SetTargetCamera(&camera_);
#endif
}

void GameScene::Update() {
	if (IsPauseButtonTriggered()) {
		isPaused_ = !isPaused_;
		if (isPaused_) {
			// ポーズを開くたび、安全な「ゲーム続行」を初期選択にする。
			pauseButtons_.SetFocusedIndex(1);
		}
		// 開閉したフレームはゲーム本体を進めない。
		return;
	}
	if (isPaused_) {
		pauseButtons_.Update();
		const int clickedButton = pauseButtons_.ConsumeClickedIndex();
		if (clickedButton == 0) {
			isSelectRequested_ = true;
			if (player_) player_->StopAllAttackSounds();
			if (boss_) boss_->StopAllAttackSounds();
		} else if (clickedButton == 1) {
			isPaused_ = false;
		}
		return;
	}
	++lightingAnimationFrame_;
#ifdef USE_IMGUI
	ImGui::Begin("Scene Status");
	ImGui::Text("GAME SCENE");
	ImGui::Text("Press 1: Game Clear");
	ImGui::Text("Press 2: Game Over");
	ImGui::Text("Move Player: W A S D");
	ImGui::Text("Aim: Arrow Keys / Right Stick | Attack: Hold Space / RT");
	ImGui::Text("Camera Zoom: Mouse Wheel");
	if (player_) {
		ImGui::Text("Player HP: %d / %d", player_->GetHP(), player_->GetMaxHP());
		ImGui::Text("Charge: %.0f%%", player_->GetChargeProgress() * 100.0f);
		ImGui::Text("Charge Damage Multiplier: x%.2f", player_->GetChargeDamageMultiplier());
		ImGui::Text("Charge Range Multiplier: x%.2f", player_->GetChargeRangeMultiplier());
		ImGui::Text("Charge Max Damage: x%.2f", player_->GetChargeMaximumDamageMultiplier());
		ImGui::Text("Charge Max Range: x%.2f", player_->GetChargeMaximumRangeMultiplier());
	}
	if (boss_) {
		ImGui::Text("Boss HP: %d / %d  |  Phase: %d / 3",
		    boss_->GetHP(), boss_->GetMaxHP(), boss_->GetHpPhase());
	}
	ImGui::End();

	ImGui::SetNextWindowPos({900.0f, 40.0f}, ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize({340.0f, 230.0f}, ImGuiCond_FirstUseEver);
	ImGui::Begin("Boss DDA");
	if (boss_) {
		ImGui::Text("BOSS DYNAMIC DIFFICULTY (DDA)");
		ImGui::Text("HP Phase: %d / 3  |  Phase DDA bonus: +%d",
		    boss_->GetHpPhase(), boss_->GetHpPhase() - 1);
		const int ddaLevel = boss_->GetDDALevel();
		const char* levelNames[] = {"VERY EASY", "EASY", "NORMAL", "HARD", "VERY HARD"};
		const ImVec4 levelColors[] = {
		    ImVec4(0.30f, 0.75f, 1.00f, 1.00f), ImVec4(0.40f, 0.90f, 0.75f, 1.00f),
		    ImVec4(1.00f, 0.85f, 0.25f, 1.00f), ImVec4(1.00f, 0.55f, 0.20f, 1.00f),
		    ImVec4(1.00f, 0.25f, 0.20f, 1.00f)};
		ImGui::TextColored(levelColors[ddaLevel - 1], "LEVEL %d / 5  %s", ddaLevel, levelNames[ddaLevel - 1]);
		ImGui::ProgressBar(static_cast<float>(ddaLevel - 1) / 4.0f, ImVec2(-1.0f, 0.0f));
		ImGui::Text("Target level: %d  |  Evaluation: %.0f%%",
		    boss_->GetDDATargetLevel(), boss_->GetDDASampleProgress() * 100.0f);
		ImGui::Text("Player dealt: %d / 150", boss_->GetDDADamageThisSample());
		ImGui::Text("Player taken: %d / 60", boss_->GetDDADamageTakenThisSample());
		ImGui::Text("Attack range: x%.2f", boss_->GetDDAAttackRangeMultiplier());
		ImGui::Text("Wait: %d frames  (x%.3f)",
		    boss_->GetDDANextAttackIntervalFrames(), boss_->GetDDAAttackCooldownMultiplier());
		ImGui::Separator();
		ImGui::Text("Weapon bias: %s", boss_->GetDDAWeaponBiasName());
		ImGui::Text("Next attack: %s", boss_->GetDDANextAttackName());
		if (boss_->IsMobTimeLimitActive()) {
			ImGui::ProgressBar(
			    boss_->GetMobTimeLimitRatio(), ImVec2(-1.0f, 0.0f),
			    "Mob elimination time");
		} else {
			ImGui::TextUnformatted("Mob elimination timer: inactive");
		}
	} else {
		ImGui::Text("Waiting for Boss...");
	}
	ImGui::End();

	if (Input::GetInstance()->TriggerKey(DIK_1)) {
		isClearRequested_ = true;
	}
	if (Input::GetInstance()->TriggerKey(DIK_2)) {
		isGameOverRequested_ = true;
	}
#endif


	if (player_) {
		player_->Update();
		const int successfulHitCount = player_->ConsumeSuccessfulHitCount();
		if (successfulHitCount > 0) {
			PostProcessCRT::TriggerSuccessfulHitEffect(successfulHitCount);
		}
		const int bossHitCount = player_->ConsumeBossHitCount();
		if (bossHitCount > 0) {
			SpawnPraiseParticles(bossHitCount);
		}
		if (wall_) {
			player_->ConstrainInsideArena(wall_->GetCenter(), wall_->GetInnerRadius());
		}
		const int currentPlayerHp = player_->GetHP();
		if (currentPlayerHp < previousPlayerHp_) {
			PostProcessCRT::TriggerDamageEffect();
			cameraController_.StartShake(
			    kDamageCameraShakeStrength, kDamageCameraShakeFrames, 1.15f);
		}
		previousPlayerHp_ = currentPlayerHp;

		const bool chargeAtMaximum =
		    player_->IsChargingSubSkill() && player_->GetChargeProgress() >= 1.0f;
		if (chargeAtMaximum && !wasChargeAtMaximum_) {
			cameraController_.StartShake(
			    kChargeMaxCameraShakeStrength, kChargeMaxCameraShakeFrames, 1.35f);
		}
		wasChargeAtMaximum_ = chargeAtMaximum;

		if (player_->IsBeamActive()) {
			// 毎フレーム短く更新し、ビームが出ている間だけ弱く振動させる。
			cameraController_.StartShake(kBeamCameraShakeStrength, 2, 0.82f);
		}
		cameraController_.Update();
	}

	if (boss_) {
		boss_->Update();
		if (boss_->ConsumeGroundSpearEruptionEvent()) {
			// 地面を突き破る瞬間だけ、強く短く揺らして重量感を出す。
			cameraController_.StartShake(
			    kGroundSpearCameraShakeStrength,
			    kGroundSpearCameraShakeFrames, 1.65f);
		}
		if (wall_) {
			boss_->ConstrainInsideArena(wall_->GetCenter(), wall_->GetInnerRadius());
		}
		if (boss_->IsDeathAnimationFinished()) {
			isClearRequested_ = true;
		}
	}
	UpdatePraiseParticles();
	if (lightGroup_) {
		Vector3 predictionPosition{};
		float predictionRadius = 0.0f;
		const bool hasPrediction =
		    boss_ != nullptr && boss_->GetPredictionLightData(predictionPosition, predictionRadius);
		lightGroup_->SetPointLightActive(1, hasPrediction);
		if (hasPrediction) {
			// 球形PointLightの地面上の到達範囲が、予兆半径と一致するよう計算する。
			const float lightHeight = (std::max)(3.0f, predictionRadius * 0.35f);
			const float lightRadius =
			    std::sqrt(predictionRadius * predictionRadius + lightHeight * lightHeight) + 2.0f;
			predictionPosition.y += lightHeight;
			const float pulse = 2.2f +
			    std::sin(static_cast<float>(lightingAnimationFrame_) * 0.16f) * 0.35f;
			lightGroup_->SetPointLightPos(1, predictionPosition);
			lightGroup_->SetPointLightRadius(1, lightRadius);
			lightGroup_->SetPointLightIntensity(1, pulse);
		}
		Vector3 playerAttackLightPosition{};
		const bool hasPlayerAttack =
		    player_ != nullptr && player_->GetAttackLightPosition(playerAttackLightPosition);
		lightGroup_->SetPointLightActive(2, hasPlayerAttack);
		if (hasPlayerAttack) {
			// 地面にも青い光が届き、攻撃自体も発光して見える高さへ配置する。
			playerAttackLightPosition.y += 2.0f;
			const float bulletPulse = 1.8f +
			    std::sin(static_cast<float>(lightingAnimationFrame_) * 0.22f) * 0.20f;
			lightGroup_->SetPointLightPos(2, playerAttackLightPosition);
			lightGroup_->SetPointLightIntensity(2, bulletPulse);
		}
		const bool hasPlayerSpot = player_ != nullptr && !player_->IsDead();
		lightGroup_->SetSpotLightActive(1, hasPlayerSpot);
		if (hasPlayerSpot) {
			Vector3 playerSpotPosition = player_->GetWorldTransform().translation_;
			playerSpotPosition.y += 38.0f;
			lightGroup_->SetSpotLightPos(1, playerSpotPosition);
		}
		const bool hasBossSpot = boss_ != nullptr && !boss_->IsDead();
		lightGroup_->SetSpotLightActive(2, hasBossSpot);
		if (hasBossSpot) {
			Vector3 bossSpotPosition = boss_->GetWorldTransform().translation_;
			bossSpotPosition.y += 65.0f;
			lightGroup_->SetSpotLightPos(2, bossSpotPosition);
		}
		lightGroup_->Update();
	}
	if (player_ && boss_ && !boss_->IsDead() && !player_->IsChargeAttackActive() &&
	    ColliderManager::CheckCircleCircleXZ(
	                            {player_->GetWorldTransform().translation_, player_->GetCollisionRadius()},
	                            {boss_->GetWorldTransform().translation_,
	                             boss_->GetCollisionRadius() + kBossContactSeparationMargin})) {
		// 通常接触では吹き飛ばさず、Boss表面まで位置を戻してめり込みだけ解消する。
		// 突進中はPlayerAttackController側の停止・反射処理を優先する。
		player_->ResolveObstacleCollisionXZ(
		    boss_->GetWorldTransform().translation_,
		    boss_->GetCollisionRadius() + kBossContactSeparationMargin);
	}
	// Bossとの押し出しでも、壁の内側からは出さない。
	if (player_ && wall_) {
		player_->ConstrainInsideArena(wall_->GetCenter(), wall_->GetInnerRadius());
	}
	if (player_ && player_->IsDead()) {
		isGameOverRequested_ = true;
	}
	// クリア・ゲームオーバーが成立したフレームに直接停止する。
	// GameSceneやPlayerの破棄タイミングには依存させない。
	if (player_ && (isClearRequested_ || isGameOverRequested_)) {
		player_->StopAllAttackSounds();
	}
	if (boss_ && (isClearRequested_ || isGameOverRequested_)) {
		boss_->StopAllAttackSounds();
	}
	UpdateHpBars();

	if (ground_) {
		ground_->Update();
	}
	if (wall_) {
		wall_->Update();
	}

	worldTransform_.TransferMatrix();

#ifdef DEBUG
	if (Input::GetInstance()->TriggerKey(DIK_C)) {
		isDebugCameraActive_ = !isDebugCameraActive_;
	}

	if (isDebugCameraActive_) {
		debugCamera_->Update();
		camera_.matView = debugCamera_->GetCamera().matView;
		camera_.matProjection = debugCamera_->GetCamera().matProjection;
		camera_.TransferMatrix();
	}
	AxisIndicator::GetInstance()->Update();
#endif
	UpdateChargeGauge();
	UpdateSkillCooldownGauge();
	UpdateSteelBurstGauge();
}

void GameScene::Draw() {
	// 1枚の平面なので、カメラ位置にかかわらず確認できるよう両面描画にする。
	Model::PreDraw(Model::CullingMode::kNone);
	if (ground_) {
		ground_->Draw();
	}
	if (wall_) {
		wall_->Draw();
	}
	if (player_) {
		player_->Draw();
	}
	if (boss_) {
		boss_->Draw();
	}
	if (model_Effect_) {
		for (const auto& particle : praiseParticles_) {
			model_Effect_->Draw(
			    particle->worldTransform, camera_, &particle->color);
		}
	}
	Model::PostDraw();

	// 3Dシーンより後に描画して、常に画面最前面へ表示する。
	Sprite::PreDraw();
	PostProcessCRT::RebindActiveSceneTarget();
	if (bossHpBackgroundSprite_) {
		bossHpBackgroundSprite_->Draw();
	}
	if (bossHpNextLayerSprite_) {
		bossHpNextLayerSprite_->Draw();
	}
	if (bossHpDamageTrailSprite_) {
		bossHpDamageTrailSprite_->Draw();
	}
	if (bossHpBarSprite_) {
		bossHpBarSprite_->Draw();
	}
	if (mobTimeLimitBackgroundSprite_) {
		mobTimeLimitBackgroundSprite_->Draw();
	}
	if (mobTimeLimitBarSprite_) {
		mobTimeLimitBarSprite_->Draw();
	}
	if (playerHpBackgroundSprite_) {
		playerHpBackgroundSprite_->Draw();
	}
	if (playerHpBarSprite_) {
		playerHpBarSprite_->Draw();
	}
	if (chargeGaugeBackgroundSprite_) {
		chargeGaugeBackgroundSprite_->Draw();
	}
	if (chargeGaugeBarSprite_) {
		chargeGaugeBarSprite_->Draw();
	}
	for (const auto& electricSprite : chargeGaugeElectricSprites_) {
		if (electricSprite) electricSprite->Draw();
	}
	for (const auto& subSkillIcon : subSkillIconSprites_) {
		if (subSkillIcon) {
			subSkillIcon->Draw();
		}
	}
	if (steelBurstGaugeBackgroundSprite_) {
		steelBurstGaugeBackgroundSprite_->Draw();
	}
	for (const auto& segment : steelBurstGaugeSegments_) {
		if (segment) segment->Draw();
	}
	if (mainSkillIconSprite_) {
		mainSkillIconSprite_->Draw();
	}
	if (mainSkillCooldownBackgroundSprite_) {
		mainSkillCooldownBackgroundSprite_->Draw();
	}
	if (mainSkillCooldownBarSprite_) {
		mainSkillCooldownBarSprite_->Draw();
	}
	if (menuButtonGuideSprite_) {
		menuButtonGuideSprite_->Draw();
	}
	if (isPaused_) {
		if (pauseDimSprite_) pauseDimSprite_->Draw();
		pauseButtons_.Draw();
	}
	Sprite::PostDraw();
}

void GameScene::SpawnPraiseParticles(int hitCount) {
	if (!boss_ || !model_Effect_ || hitCount <= 0) return;
	constexpr int kMaximumParticles = 120;
	if (static_cast<int>(praiseParticles_.size()) >= kMaximumParticles) return;
	const int particleCount = (std::min)(18, 6 + (hitCount - 1) * 2);
	const Vector3 bossPosition = boss_->GetWorldTransform().translation_;
	const auto nextRandom01 = [this]() {
		praiseRandomState_ = praiseRandomState_ * 1664525u + 1013904223u;
		return static_cast<float>((praiseRandomState_ >> 8) & 0x00FFFFFFu) /
		       static_cast<float>(0x01000000u);
	};
	constexpr std::array<Vector4, 4> kPraiseColors = {
		Vector4{1.0f, 0.82f, 0.12f, 1.0f},
		Vector4{0.15f, 0.90f, 1.0f, 1.0f},
		Vector4{1.0f, 0.28f, 0.72f, 1.0f},
		Vector4{0.45f, 1.0f, 0.35f, 1.0f},
	};
	for (int index = 0;
	     index < particleCount &&
	     static_cast<int>(praiseParticles_.size()) < kMaximumParticles;
	     ++index) {
		const float angle = nextRandom01() * 6.28318531f;
		const float horizontalSpeed = 0.30f + nextRandom01() * 0.45f;
		// 命中報酬として遠目でも分かるよう、従来のおよそ2倍へ拡大する。
		const float scale = 0.65f + nextRandom01() * 0.55f;
		auto particle = std::make_unique<PraiseParticle>();
		particle->worldTransform.Initialize();
		particle->color.Initialize();
		particle->baseColor = kPraiseColors[index % kPraiseColors.size()];
		// alpha=5～6はObjPSで褒めエフェクトと寿命進行度を識別する。
		particle->color.SetColor({
		    particle->baseColor.x, particle->baseColor.y,
		    particle->baseColor.z, 5.0f});
		particle->worldTransform.scale_ = {scale, scale, scale};
		particle->worldTransform.translation_ = {
		    bossPosition.x + std::sin(angle) * 2.5f,
		    bossPosition.y + 6.0f + nextRandom01() * 4.0f,
		    bossPosition.z + std::cos(angle) * 2.5f,
		};
		particle->worldTransform.rotation_ = {
		    nextRandom01() * 6.28318531f,
		    nextRandom01() * 6.28318531f,
		    nextRandom01() * 6.28318531f,
		};
		particle->velocity = {
		    std::sin(angle) * horizontalSpeed,
		    0.75f + nextRandom01() * 0.85f,
		    std::cos(angle) * horizontalSpeed,
		};
		particle->angularVelocity = {
		    (nextRandom01() - 0.5f) * 0.34f,
		    (nextRandom01() - 0.5f) * 0.34f,
		    (nextRandom01() - 0.5f) * 0.34f,
		};
		particle->baseScale = scale;
		particle->remainingFrames = 36;
		particle->totalFrames = 36;
		UpdateWorldTransform(particle->worldTransform);
		praiseParticles_.push_back(std::move(particle));
	}
}

void GameScene::UpdatePraiseParticles() {
	for (const auto& particle : praiseParticles_) {
		particle->worldTransform.translation_.x += particle->velocity.x;
		particle->worldTransform.translation_.y += particle->velocity.y;
		particle->worldTransform.translation_.z += particle->velocity.z;
		particle->velocity.y -= 0.035f;
		particle->worldTransform.rotation_.x += particle->angularVelocity.x;
		particle->worldTransform.rotation_.y += particle->angularVelocity.y;
		particle->worldTransform.rotation_.z += particle->angularVelocity.z;
		--particle->remainingFrames;
		const float lifeRatio = std::clamp(
		    static_cast<float>(particle->remainingFrames) /
		        static_cast<float>(particle->totalFrames),
		    0.0f, 1.0f);
		const float lifeProgress = 1.0f - lifeRatio;
		particle->color.SetColor({
		    particle->baseColor.x, particle->baseColor.y,
		    particle->baseColor.z, 5.0f + lifeProgress});
		const float scale = particle->baseScale * (0.25f + lifeRatio * 0.75f);
		particle->worldTransform.scale_ = {scale, scale, scale};
		UpdateWorldTransform(particle->worldTransform);
	}
	std::erase_if(
	    praiseParticles_, [](const std::unique_ptr<PraiseParticle>& particle) {
		    return particle->remainingFrames <= 0;
	    });
}

void GameScene::InitializeHpBars() {
	const uint32_t whiteTexture = TextureManager::Load("white1x1.png");

	bossHpBackgroundSprite_.reset(Sprite::Create(
	    whiteTexture, kBossHpBackgroundPosition, {0.035f, 0.008f, 0.015f, 0.92f}));
	bossHpBackgroundSprite_->SetSize(kBossHpBackgroundSize);
	bossHpNextLayerSprite_.reset(Sprite::Create(
	    whiteTexture, kBossHpBarPosition, {1.0f, 0.42f, 0.04f, 1.0f}));
	bossHpNextLayerSprite_->SetSize(kBossHpBarSize);
	bossHpDamageTrailSprite_.reset(Sprite::Create(
	    whiteTexture, kBossHpBarPosition, {1.0f, 1.0f, 1.0f, 0.92f}));
	bossHpDamageTrailSprite_->SetSize(kBossHpBarSize);
	bossHpBarSprite_.reset(Sprite::Create(
	    whiteTexture, kBossHpBarPosition, {0.08f, 0.86f, 0.24f, 1.0f}));
	bossHpBarSprite_->SetSize(kBossHpBarSize);
	bossHpPreviousRatio_ = 1.0f;
	bossHpDamageTrailRatio_ = 1.0f;
	bossHpDamageTrailDelayTimer_ = 0;
	previousBossHpPhase_ = boss_ ? boss_->GetHpPhase() : 1;
	mobTimeLimitBackgroundSprite_.reset(Sprite::Create(
	    whiteTexture, kMobTimeLimitBackgroundPosition, {0.025f, 0.018f, 0.035f, 0.94f}));
	mobTimeLimitBackgroundSprite_->SetSize({0.0f, 0.0f});
	mobTimeLimitBarSprite_.reset(Sprite::Create(
	    whiteTexture, kMobTimeLimitBarPosition, {1.0f, 0.68f, 0.08f, 1.0f}));
	mobTimeLimitBarSprite_->SetSize({0.0f, 0.0f});

	playerHpBackgroundSprite_.reset(Sprite::Create(
	    whiteTexture, kPlayerHpBackgroundPosition, {0.008f, 0.025f, 0.035f, 0.92f}));
	playerHpBackgroundSprite_->SetSize(kPlayerHpBackgroundSize);
	playerHpBarSprite_.reset(Sprite::Create(
	    whiteTexture, kPlayerHpBarPosition, {0.10f, 0.90f, 0.72f, 1.0f}));
	playerHpBarSprite_->SetSize(kPlayerHpBarSize);

	chargeGaugeBackgroundSprite_.reset(Sprite::Create(
	    whiteTexture, {0.0f, 0.0f}, {0.015f, 0.025f, 0.045f, 0.90f}));
	chargeGaugeBackgroundSprite_->SetSize({0.0f, 0.0f});
	chargeGaugeBarSprite_.reset(Sprite::Create(
	    whiteTexture, {0.0f, 0.0f}, {0.10f, 0.72f, 1.0f, 1.0f}));
	chargeGaugeBarSprite_->SetSize({0.0f, 0.0f});
	for (auto& electricSprite : chargeGaugeElectricSprites_) {
		electricSprite.reset(Sprite::Create(
		    whiteTexture, {0.0f, 0.0f}, {0.85f, 0.96f, 1.0f, 0.0f}));
		electricSprite->SetSize({0.0f, 0.0f});
	}
	chargeGaugeElectricTimer_ = 0;

	UpdateHpBars();
}

void GameScene::UpdateChargeGauge() {
	if (player_ && player_->ConsumeChargeGrowthEffectCount() > 0) {
		chargeGaugeElectricTimer_ = kChargeGaugeElectricFrames;
	}
	if (!chargeGaugeBackgroundSprite_ || !chargeGaugeBarSprite_ || !player_ ||
	    !player_->IsChargingSubSkill()) {
		if (chargeGaugeBackgroundSprite_) {
			chargeGaugeBackgroundSprite_->SetSize({0.0f, 0.0f});
		}
		if (chargeGaugeBarSprite_) {
			chargeGaugeBarSprite_->SetSize({0.0f, 0.0f});
		}
		for (const auto& electricSprite : chargeGaugeElectricSprites_) {
			if (electricSprite) electricSprite->SetSize({0.0f, 0.0f});
		}
		return;
	}

	const Vector3& playerPosition = player_->GetWorldTransform().translation_;
	// モデルの下端にあたる地面座標を基準にして、画面上で少し下へ配置する。
	const Vector2 playerFootScreen = WorldToScreen(
	    {playerPosition.x, 0.0f, playerPosition.z}, camera_.matView,
	    camera_.matProjection, kScreenWidth, kScreenHeight);
	const Vector2 backgroundPosition = {
	    playerFootScreen.x - kChargeGaugeBackgroundSize.x * 0.5f,
	    playerFootScreen.y + kChargeGaugeVerticalOffset,
	};
	const Vector2 barPosition = {
	    playerFootScreen.x - kChargeGaugeSize.x * 0.5f,
	    backgroundPosition.y +
	        (kChargeGaugeBackgroundSize.y - kChargeGaugeSize.y) * 0.5f,
	};
	const float chargeProgress =
	    std::clamp(player_->GetChargeProgress(), 0.0f, 1.0f);
	chargeGaugeBackgroundSprite_->SetPosition(backgroundPosition);
	chargeGaugeBackgroundSprite_->SetSize(kChargeGaugeBackgroundSize);
	chargeGaugeBarSprite_->SetPosition(barPosition);
	chargeGaugeBarSprite_->SetSize({
	    kChargeGaugeSize.x * chargeProgress,
	    kChargeGaugeSize.y,
	});
	if (chargeGaugeElectricTimer_ > 0) {
		const float effectProgress = 1.0f -
		    static_cast<float>(chargeGaugeElectricTimer_) /
		        static_cast<float>(kChargeGaugeElectricFrames);
		for (size_t index = 0; index < chargeGaugeElectricSprites_.size(); ++index) {
			auto& electricSprite = chargeGaugeElectricSprites_[index];
			if (!electricSprite) continue;
			const float laneOffset = static_cast<float>(index) * 0.24f;
			const float travel = std::fmod(effectProgress + laneOffset, 1.0f);
			const float x = barPosition.x + travel * (kChargeGaugeSize.x - 42.0f);
			const float y = barPosition.y + 2.0f +
			    std::sin((travel + static_cast<float>(index)) * 12.0f) * 3.0f;
			electricSprite->SetPosition({x, y});
			electricSprite->SetSize({42.0f, 4.0f});
			electricSprite->SetColor({0.82f, 0.96f, 1.0f, 0.95f});
		}
		--chargeGaugeElectricTimer_;
	} else {
		for (const auto& electricSprite : chargeGaugeElectricSprites_) {
			if (electricSprite) electricSprite->SetSize({0.0f, 0.0f});
		}
	}
}

void GameScene::UpdateHpBars() {
	if (bossHpBarSprite_ && bossHpNextLayerSprite_ &&
	    bossHpDamageTrailSprite_ && boss_) {
		const float hpRatio =
		    std::clamp(boss_->GetCurrentHpLayerRatio(), 0.0f, 1.0f);
		auto getPhaseColor = [](int phase) {
			return phase == 1 ? Vector4{0.08f, 0.86f, 0.24f, 1.0f}
			     : phase == 2 ? Vector4{1.0f, 0.42f, 0.04f, 1.0f}
			                  : Vector4{0.95f, 0.06f, 0.12f, 1.0f};
		};
		const int currentPhase = boss_->GetHpPhase();
		if (currentPhase != previousBossHpPhase_) {
			// 新しい層は満タンから開始するので残像も同期して色切替の破綻を防ぐ。
			bossHpDamageTrailRatio_ = 1.0f;
			bossHpDamageTrailDelayTimer_ = 0;
			previousBossHpPhase_ = currentPhase;
		} else if (hpRatio < bossHpPreviousRatio_) {
			bossHpDamageTrailRatio_ =
			    (std::max)(bossHpDamageTrailRatio_, bossHpPreviousRatio_);
			bossHpDamageTrailDelayTimer_ = kBossHpDamageTrailDelayFrames;
		}
		if (hpRatio > bossHpDamageTrailRatio_) {
			// 回復時に白い残像が現在HPより短くならないよう即座に揃える。
			bossHpDamageTrailRatio_ = hpRatio;
		} else if (bossHpDamageTrailDelayTimer_ > 0) {
			--bossHpDamageTrailDelayTimer_;
		} else {
			bossHpDamageTrailRatio_ +=
			    (hpRatio - bossHpDamageTrailRatio_) * kBossHpDamageTrailFollowSpeed;
			if (std::abs(bossHpDamageTrailRatio_ - hpRatio) < 0.001f) {
				bossHpDamageTrailRatio_ = hpRatio;
			}
		}

		bossHpNextLayerSprite_->SetPosition(kBossHpBarPosition);
		bossHpNextLayerSprite_->SetSize({
		    currentPhase < 3 ? kBossHpBarSize.x : 0.0f, kBossHpBarSize.y});
		bossHpNextLayerSprite_->SetColor(getPhaseColor(currentPhase + 1));
		bossHpDamageTrailSprite_->SetPosition(kBossHpBarPosition);
		bossHpDamageTrailSprite_->SetSize({
		    kBossHpBarSize.x * bossHpDamageTrailRatio_, kBossHpBarSize.y});
		bossHpBarSprite_->SetPosition(kBossHpBarPosition);
		bossHpBarSprite_->SetSize({kBossHpBarSize.x * hpRatio, kBossHpBarSize.y});
		bossHpBarSprite_->SetColor(getPhaseColor(currentPhase));
		bossHpPreviousRatio_ = hpRatio;
	}
	if (mobTimeLimitBackgroundSprite_ && mobTimeLimitBarSprite_ && boss_) {
		if (boss_->IsMobTimeLimitActive()) {
			const float timeRatio =
			    std::clamp(boss_->GetMobTimeLimitRatio(), 0.0f, 1.0f);
			mobTimeLimitBackgroundSprite_->SetPosition(
			    kMobTimeLimitBackgroundPosition);
			mobTimeLimitBackgroundSprite_->SetSize(
			    kMobTimeLimitBackgroundSize);
			mobTimeLimitBarSprite_->SetPosition(kMobTimeLimitBarPosition);
			mobTimeLimitBarSprite_->SetSize({
			    kMobTimeLimitBarSize.x * timeRatio,
			    kMobTimeLimitBarSize.y,
			});
		} else {
			mobTimeLimitBackgroundSprite_->SetSize({0.0f, 0.0f});
			mobTimeLimitBarSprite_->SetSize({0.0f, 0.0f});
		}
	}

	if (playerHpBarSprite_ && player_) {
		const int maxHp = (std::max)(1, player_->GetMaxHP());
		const float hpRatio = std::clamp(
		    static_cast<float>(player_->GetHP()) / static_cast<float>(maxHp), 0.0f, 1.0f);
		const int increasedMaxHp = (std::max)(0, maxHp - kPlayerBaseMaxHp);
		const float capacityWidth = (std::min)(
		    kPlayerHpBarMaximumWidth,
		    kPlayerHpBarSize.x +
		        static_cast<float>(increasedMaxHp) * kPlayerHpWidthPerMaxHp);
		playerHpBarSprite_->SetPosition(kPlayerHpBarPosition);
		playerHpBarSprite_->SetSize({capacityWidth * hpRatio, kPlayerHpBarSize.y});
		if (playerHpBackgroundSprite_) {
			playerHpBackgroundSprite_->SetPosition(kPlayerHpBackgroundPosition);
			playerHpBackgroundSprite_->SetSize({
			    capacityWidth +
			        (kPlayerHpBackgroundSize.x - kPlayerHpBarSize.x),
			    kPlayerHpBackgroundSize.y,
			});
		}
	}
}

void GameScene::InitializeSkillIcons(
    MainSkillType mainSkill, const std::array<SubSkillType, 2>& subSkills) {
	mainSkillIconSprite_.reset(Sprite::Create(
	    TextureManager::Load(GetMainSkillIconPath(mainSkill)),
	    kMainSkillIconPosition, {1.0f, 1.0f, 1.0f, 1.0f}));
	mainSkillIconSprite_->SetSize(kMainSkillIconSize);
	const uint32_t whiteTexture = TextureManager::Load("white1x1.png");
	mainSkillCooldownBackgroundSprite_.reset(Sprite::Create(
	    whiteTexture, kMainSkillCooldownBackgroundPosition,
	    {0.06f, 0.05f, 0.015f, 0.92f}));
	mainSkillCooldownBackgroundSprite_->SetSize(kMainSkillCooldownBackgroundSize);
	mainSkillCooldownBarSprite_.reset(Sprite::Create(
	    whiteTexture, kMainSkillCooldownBarPosition,
	    {1.0f, 0.82f, 0.08f, 1.0f}));
	mainSkillCooldownBarSprite_->SetSize(kMainSkillCooldownBarSize);

	for (size_t index = 0; index < subSkillIconSprites_.size(); ++index) {
		subSkillIconSprites_[index].reset();
		const char* iconPath = GetSubSkillIconPath(subSkills[index]);
		if (iconPath == nullptr) {
			continue;
		}
		subSkillIconSprites_[index].reset(Sprite::Create(
		    TextureManager::Load(iconPath), kSubSkillIconPositions[index],
		    {0.90f, 0.90f, 0.90f, 0.95f}));
		subSkillIconSprites_[index]->SetSize(kSubSkillIconSize);
	}

	const bool isDoubleSteel =
	    subSkills[0] == SubSkillType::Steal &&
	    subSkills[1] == SubSkillType::Steal;
	steelBurstGaugeBackgroundSprite_.reset();
	for (auto& segment : steelBurstGaugeSegments_) segment.reset();
	if (isDoubleSteel) {
		steelBurstGaugeBackgroundSprite_.reset(Sprite::Create(
		    whiteTexture, kSteelBurstGaugeBackgroundPosition,
		    {0.04f, 0.025f, 0.005f, 0.94f}));
		steelBurstGaugeBackgroundSprite_->SetSize(kSteelBurstGaugeBackgroundSize);
		for (size_t index = 0; index < steelBurstGaugeSegments_.size(); ++index) {
			const Vector2 position = {
			    kSteelBurstGaugeStartPosition.x +
			        kSteelBurstGaugeSegmentStep * static_cast<float>(index),
			    kSteelBurstGaugeStartPosition.y,
			};
			steelBurstGaugeSegments_[index].reset(Sprite::Create(
			    whiteTexture, position, {0.20f, 0.13f, 0.025f, 0.92f}));
			steelBurstGaugeSegments_[index]->SetSize(kSteelBurstGaugeSegmentSize);
		}
	}
}

void GameScene::UpdateSkillCooldownGauge() {
	if (!mainSkillCooldownBarSprite_ || !player_) return;
	const float cooldownRatio =
	    std::clamp(player_->GetMainSkillCooldownRatio(), 0.0f, 1.0f);
	mainSkillCooldownBarSprite_->SetPosition(kMainSkillCooldownBarPosition);
	mainSkillCooldownBarSprite_->SetSize({
	    kMainSkillCooldownBarSize.x * cooldownRatio,
	    kMainSkillCooldownBarSize.y,
	});
}

void GameScene::UpdateSteelBurstGauge() {
	if (!steelBurstGaugeBackgroundSprite_ || !player_ ||
	    !player_->IsDoubleSteelActive()) {
		return;
	}
	const int filledSegments = std::clamp(
	    player_->GetSteelBurstStackCount(), 0,
	    static_cast<int>(steelBurstGaugeSegments_.size()));
	for (size_t index = 0; index < steelBurstGaugeSegments_.size(); ++index) {
		auto& segment = steelBurstGaugeSegments_[index];
		if (!segment) continue;
		const bool isFilled = static_cast<int>(index) < filledSegments;
		segment->SetColor(
		    isFilled ? Vector4{1.0f, 0.76f, 0.08f, 1.0f}
		             : Vector4{0.20f, 0.13f, 0.025f, 0.92f});
	}
}
