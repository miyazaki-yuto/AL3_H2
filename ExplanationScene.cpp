#include "ExplanationScene.h"

#include "PostProcessCRT.h"

using namespace KamataEngine;

namespace {

constexpr Vector2 kButtonUiLeftOrigin = {825.0f, 780.0f};
constexpr Vector2 kButtonUiRightOrigin = {1080.0f, 780.0f};
constexpr Vector2 kButtonUiArrowSize = {170.0f, 165.0f};

bool IsControllerButtonTriggered(WORD button) {
	const Input* input = Input::GetInstance();
	if (input == nullptr) return false;
	XINPUT_STATE current{};
	XINPUT_STATE previous{};
	return input->GetJoystickState(0, current) &&
	       input->GetJoystickStatePrevious(0, previous) &&
	       (current.Gamepad.wButtons & button) != 0 &&
	       (previous.Gamepad.wButtons & button) == 0;
}

bool IsControllerLeftStickTriggered(int direction) {
	const Input* input = Input::GetInstance();
	if (input == nullptr) return false;
	XINPUT_STATE current{};
	XINPUT_STATE previous{};
	if (!input->GetJoystickState(0, current) ||
	    !input->GetJoystickStatePrevious(0, previous)) {
		return false;
	}
	constexpr SHORT kPageSwitchThreshold = 16000;
	const bool isCurrentDirection = direction < 0 ?
	    current.Gamepad.sThumbLX <= -kPageSwitchThreshold :
	    current.Gamepad.sThumbLX >= kPageSwitchThreshold;
	const bool wasPreviousDirection = direction < 0 ?
	    previous.Gamepad.sThumbLX <= -kPageSwitchThreshold :
	    previous.Gamepad.sThumbLX >= kPageSwitchThreshold;
	return isCurrentDirection && !wasPreviousDirection;
}

} // namespace

void ExplanationScene::Initialize() {
	isBackRequested_ = false;
	currentPage_ = 0;
	constexpr std::array<const char*, 3> kGuideTextures = {
	    "Tutorial/TutorialGuide-v3.png",
	    "Tutorial/MainSkillGuide.png",
	    "Tutorial/SubSkillGuide.png",
	};
	for (size_t index = 0; index < guideSprites_.size(); ++index) {
		guideSprites_[index].reset(Sprite::Create(
		    TextureManager::Load(kGuideTextures[index]),
		    {0.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}));
		guideSprites_[index]->SetSize({1280.0f, 720.0f});
	}

	const uint32_t buttonUiTexture = TextureManager::Load("ButtonUI.png");
	previousPagePromptSprite_.reset(Sprite::Create(
	    buttonUiTexture, {535.0f, 630.0f}, {1.0f, 1.0f, 1.0f, 0.92f}));
	previousPagePromptSprite_->SetSize({72.0f, 70.0f});
	previousPagePromptSprite_->SetTextureRect(kButtonUiLeftOrigin, kButtonUiArrowSize);
	nextPagePromptSprite_.reset(Sprite::Create(
	    buttonUiTexture, {673.0f, 630.0f}, {1.0f, 1.0f, 1.0f, 0.92f}));
	nextPagePromptSprite_->SetSize({72.0f, 70.0f});
	nextPagePromptSprite_->SetTextureRect(kButtonUiRightOrigin, kButtonUiArrowSize);
	const uint32_t whiteTexture = TextureManager::Load("white1x1.png");
	for (size_t index = 0; index < pageIndicatorSprites_.size(); ++index) {
		pageIndicatorSprites_[index].reset(Sprite::Create(
		    whiteTexture, {621.0f + 19.0f * static_cast<float>(index), 672.0f},
		    {0.38f, 0.22f, 0.55f, 0.88f}));
		pageIndicatorSprites_[index]->SetSize({10.0f, 10.0f});
	}

	SimpleButton::Colors colors;
	colors.normal = {0.72f, 0.72f, 0.72f, 0.82f};
	colors.hover = {1.18f, 1.18f, 1.18f, 1.0f};
	colors.pressed = {0.72f, 0.62f, 0.82f, 1.0f};
	backButton_.Initialize(
	    TextureManager::Load("UI/SelectBackButton.png"),
	    {28.0f, 574.0f}, {120.0f, 117.0f}, colors);
	backButton_.SetFocused(true);
}

void ExplanationScene::Update() {
	Input* const input = Input::GetInstance();
	if (IsControllerButtonTriggered(XINPUT_GAMEPAD_DPAD_LEFT) ||
	    IsControllerLeftStickTriggered(-1) ||
	    (input != nullptr && input->TriggerKey(DIK_LEFT))) {
		currentPage_ = (currentPage_ + guideSprites_.size() - 1) % guideSprites_.size();
	}
	if (IsControllerButtonTriggered(XINPUT_GAMEPAD_DPAD_RIGHT) ||
	    IsControllerLeftStickTriggered(1) ||
	    (input != nullptr && input->TriggerKey(DIK_RIGHT))) {
		currentPage_ = (currentPage_ + 1) % guideSprites_.size();
	}
	backButton_.Update();
	if (backButton_.ConsumeClick() || (input != nullptr && input->TriggerKey(DIK_ESCAPE))) {
		isBackRequested_ = true;
	}
}

void ExplanationScene::Draw() {
	Sprite::PreDraw();
	PostProcessCRT::RebindActiveSceneTarget();
	if (guideSprites_[currentPage_] != nullptr) {
		guideSprites_[currentPage_]->Draw();
	}
	if (previousPagePromptSprite_ != nullptr) previousPagePromptSprite_->Draw();
	if (nextPagePromptSprite_ != nullptr) nextPagePromptSprite_->Draw();
	for (size_t index = 0; index < pageIndicatorSprites_.size(); ++index) {
		if (pageIndicatorSprites_[index] == nullptr) continue;
		pageIndicatorSprites_[index]->SetColor(
		    index == currentPage_ ? Vector4{0.78f, 0.42f, 1.0f, 1.0f} :
		                            Vector4{0.28f, 0.18f, 0.38f, 0.88f});
		pageIndicatorSprites_[index]->Draw();
	}
	backButton_.Draw();
	Sprite::PostDraw();
}
