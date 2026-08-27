#include "GameOver.h"

#include "KamataEngine.h"
#include "PostProcessCRT.h"

using namespace KamataEngine;

namespace {

constexpr Vector2 kScreenSize = {1280.0f, 720.0f};
constexpr Vector2 kLogoPosition = {240.0f, 55.0f};
constexpr Vector2 kLogoSize = {800.0f, 269.0f};
constexpr Vector2 kButtonSize = {460.0f, 96.0f};
constexpr Vector2 kRetryPosition = {410.0f, 390.0f};
constexpr Vector2 kTitlePosition = {410.0f, 520.0f};

} // namespace

void GameOver::Initialize() {
	retryRequested_ = false;
	titleRequested_ = false;

	backgroundSprite_.reset(Sprite::Create(
	    TextureManager::Load("white1x1.png"), {0.0f, 0.0f}, {0.015f, 0.005f, 0.025f, 1.0f}));
	backgroundSprite_->SetSize(kScreenSize);
	logoSprite_.reset(Sprite::Create(
	    TextureManager::Load("UI/GameOverRogo.png"), kLogoPosition, {1.0f, 1.0f, 1.0f, 1.0f}));
	logoSprite_->SetSize(kLogoSize);

	SimpleButton::Colors colors;
	colors.normal = {0.68f, 0.68f, 0.68f, 0.82f};
	colors.hover = {1.0f, 1.0f, 1.0f, 1.0f};
	colors.pressed = {0.78f, 0.78f, 0.78f, 1.0f};
	buttons_.Clear();
	buttons_.Add(
	    TextureManager::Load("UI/GameButtomRetryOver.png"), kRetryPosition, kButtonSize, colors);
	buttons_.Add(
	    TextureManager::Load("UI/GameButtomBackTitleOver.png"), kTitlePosition, kButtonSize, colors);
	buttons_.SetFocusedIndex(0);
}

void GameOver::Update() {
#ifdef USE_IMGUI
	ImGui::Begin("Scene Status");
	ImGui::Text("GAME OVER");
	ImGui::Text("Up / Down: Select | A / Enter: Confirm");
	ImGui::Text("R: Retry | T: Back to Title");
	ImGui::End();
#endif
	buttons_.Update();

	const int clickedButton = buttons_.ConsumeClickedIndex();
	if (clickedButton == 0 || Input::GetInstance()->TriggerKey(DIK_R)) {
		retryRequested_ = true;
	} else if (clickedButton == 1 || Input::GetInstance()->TriggerKey(DIK_T)) {
		titleRequested_ = true;
	}
}

void GameOver::Draw() {
	// 全画面背景を透明UIレイヤーへ混ぜると、後続PNGの透明画素が
	// 背景のアルファだけを消して二重合成になるため、シーン側を直接クリアする。
	PostProcessCRT::ClearActiveSceneColor({0.015f, 0.005f, 0.025f, 1.0f});
	Sprite::PreDraw();
	PostProcessCRT::RebindActiveSceneTarget();
	if (logoSprite_) {
		logoSprite_->Draw();
	}
	buttons_.Draw();
	Sprite::PostDraw();
}
