#include "GameTitle.h"

#include "KamataEngine.h"
#include "PostProcessCRT.h"

using namespace KamataEngine;

namespace {
constexpr Vector2 kButtonUiAOrigin = {180.0f, 65.0f};
constexpr Vector2 kButtonUiASize = {270.0f, 225.0f};
} // namespace

void GameTitle::Initialize() {
	isFinished_ = false;
	titleLogoSprite_.reset(Sprite::Create(
	    TextureManager::Load("UI/SelectTitleLogo.png"),
	    {115.0f, 55.0f}, {1.0f, 1.0f, 1.0f, 1.0f}));
	titleLogoSprite_->SetSize({1050.0f, 307.0f});
	SimpleButton::Colors colors;
	colors.normal = {0.72f, 0.72f, 0.72f, 0.82f};
	colors.hover = {1.20f, 1.20f, 1.20f, 1.0f};
	colors.pressed = {0.72f, 0.62f, 0.82f, 1.0f};
	startButton_.Initialize(
	    TextureManager::Load("ButtonUI.png"), {568.0f, 420.0f}, {144.0f, 120.0f}, colors);
	startButton_.SetTextureRect(kButtonUiAOrigin, kButtonUiASize);
	// ボタン自体がAボタンの画像なので、共通Aプロンプトは重ねない。
	startButton_.SetConfirmPromptVisible(false);
	startButton_.SetFocused(true);
}

void GameTitle::Update() {
#ifdef USE_IMGUI
	ImGui::Begin("Scene Status");
	ImGui::Text("TITLE SCENE");
	ImGui::Text("Click the blue button, press G, or press controller A to start");
	ImGui::End();
#endif
	startButton_.Update();

	if (startButton_.ConsumeClick() || Input::GetInstance()->TriggerKey(DIK_G)) {
		isFinished_ = true;
	}
}

void GameTitle::Draw() {
	Sprite::PreDraw();
	PostProcessCRT::RebindActiveSceneTarget();
	if (titleLogoSprite_ != nullptr) {
		titleLogoSprite_->Draw();
	}
	startButton_.Draw();
	Sprite::PostDraw();
}
