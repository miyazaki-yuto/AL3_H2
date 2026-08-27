#include "SimpleButton.h"

#include <algorithm>
#include <cmath>
#include <limits>

using namespace KamataEngine;

namespace {

constexpr Vector2 kButtonUiAOrigin = {180.0f, 65.0f};
constexpr Vector2 kButtonUiASize = {270.0f, 225.0f};
constexpr Vector2 kConfirmPromptSize = {44.0f, 38.0f};

uint32_t gButtonHoverSound = 0;
uint32_t gButtonDecisionSound = 0;
bool gButtonSoundsLoaded = false;

void EnsureButtonSoundsLoaded() {
	if (gButtonSoundsLoaded) return;
	Audio* audio = Audio::GetInstance();
	gButtonHoverSound = audio->LoadWave("Audio/Button_Hover_SE.wav");
	gButtonDecisionSound = audio->LoadWave("Audio/Button_Decision_SE.wav");
	gButtonSoundsLoaded = true;
}

void PlayButtonHoverSound() {
	EnsureButtonSoundsLoaded();
	Audio::GetInstance()->PlayWave(gButtonHoverSound, false, 0.55f);
}

void PlayButtonDecisionSound() {
	EnsureButtonSoundsLoaded();
	Audio::GetInstance()->PlayWave(gButtonDecisionSound, false, 0.72f);
}

bool IsControllerButtonTriggered(WORD button) {
	const Input* input = Input::GetInstance();
	if (input == nullptr) {
		return false;
	}
	XINPUT_STATE current{};
	XINPUT_STATE previous{};
	return input->GetJoystickState(0, current) && input->GetJoystickStatePrevious(0, previous) &&
	       (current.Gamepad.wButtons & button) != 0 && (previous.Gamepad.wButtons & button) == 0;
}

} // namespace

void SimpleButton::Initialize(
    uint32_t textureHandle, const Vector2& position, const Vector2& size, const Colors& colors) {
	position_ = position;
	baseSize_ = size;
	colors_ = colors;
	currentScale_ = 1.0f;
	pulsePhase_ = 0.0f;
	isHovered_ = false;
	isFocused_ = false;
	isPressed_ = false;
	wasClicked_ = false;
	isEnabled_ = true;
	EnsureButtonSoundsLoaded();
	sprite_.reset(Sprite::Create(textureHandle, position_, colors_.normal));
	confirmPromptSprite_.reset(Sprite::Create(
	    TextureManager::Load("ButtonUI.png"), position_,
	    {1.0f, 1.0f, 1.0f, 1.0f}));
	confirmPromptSprite_->SetTextureRect(kButtonUiAOrigin, kButtonUiASize);
	ApplyVisuals();
}

void SimpleButton::Update() {
	wasClicked_ = false;
	if (sprite_ == nullptr) {
		return;
	}

	const bool wasHovered = isHovered_;
	isHovered_ = isEnabled_ && IsMouseInside();
	if (isHovered_ && !wasHovered) {
		PlayButtonHoverSound();
	}
	const Input* input = Input::GetInstance();
	isPressed_ = isHovered_ && input != nullptr && input->IsPressMouse(0);
	wasClicked_ = isHovered_ && input != nullptr && input->IsTriggerMouse(0);
	if (isFocused_ && (IsControllerButtonTriggered(XINPUT_GAMEPAD_A) ||
	                   (input != nullptr && input->TriggerKey(DIK_RETURN)))) {
		wasClicked_ = true;
	}
	if (wasClicked_) {
		PlayButtonDecisionSound();
	}

	const bool isSelected = isEnabled_ && (isHovered_ || isFocused_);
	if (isSelected) {
		pulsePhase_ += kPulseSpeed;
	} else {
		// 次に選ばれた瞬間は必ず基準サイズから脈動を始める。
		pulsePhase_ = 0.0f;
	}
	const float pulseScale =
	    isSelected ? std::sin(pulsePhase_) * kPulseAmplitude : 0.0f;
	const float targetScale = !isEnabled_ ? 1.0f : isPressed_ ? kPressedScale :
	    isSelected ? kHoverScale + pulseScale : 1.0f;
	currentScale_ += (targetScale - currentScale_) * kAnimationSpeed;
	ApplyVisuals();
}

void SimpleButton::Draw() const {
	if (sprite_ != nullptr) {
		sprite_->Draw();
	}
	if (confirmPromptSprite_ != nullptr && showConfirmPrompt_ && isEnabled_ &&
	    isFocused_) {
		confirmPromptSprite_->Draw();
	}
}

bool SimpleButton::ConsumeClick() {
	const bool clicked = wasClicked_;
	wasClicked_ = false;
	return clicked;
}

void SimpleButton::SetFocused(bool focused) {
	if (focused && !isFocused_) {
		PlayButtonHoverSound();
	}
	isFocused_ = focused;
}

void SimpleButton::SetPosition(const Vector2& position) {
	position_ = position;
	ApplyVisuals();
}

void SimpleButton::SetSize(const Vector2& size) {
	baseSize_ = size;
	ApplyVisuals();
}

void SimpleButton::SetTextureRect(
    const Vector2& texturePosition, const Vector2& textureSize) {
	if (sprite_ != nullptr) {
		sprite_->SetTextureRect(texturePosition, textureSize);
	}
}

bool SimpleButton::IsMouseInside() const {
	const Input* input = Input::GetInstance();
	if (input == nullptr) {
		return false;
	}
	const Vector2& mousePosition = input->GetMousePosition();
	return mousePosition.x >= position_.x && mousePosition.x <= position_.x + baseSize_.x &&
	       mousePosition.y >= position_.y && mousePosition.y <= position_.y + baseSize_.y;
}

void SimpleButton::ApplyVisuals() {
	if (sprite_ == nullptr) {
		return;
	}
	const Vector4& color = !isEnabled_ ? colors_.disabled : isPressed_ ? colors_.pressed :
	    ((isHovered_ || isFocused_) ? colors_.hover : colors_.normal);
	const Vector2 currentSize = {baseSize_.x * currentScale_, baseSize_.y * currentScale_};
	sprite_->SetPosition({
		position_.x - (currentSize.x - baseSize_.x) * 0.5f,
		position_.y - (currentSize.y - baseSize_.y) * 0.5f,
	});
	sprite_->SetSize(currentSize);
	sprite_->SetColor(color);
	if (confirmPromptSprite_ != nullptr) {
		const float rightSideX = position_.x + baseSize_.x + 8.0f;
		const float promptX = rightSideX + kConfirmPromptSize.x <= 1272.0f
		    ? rightSideX
		    : position_.x - kConfirmPromptSize.x - 8.0f;
		confirmPromptSprite_->SetPosition({
		    promptX,
		    position_.y + (baseSize_.y - kConfirmPromptSize.y) * 0.5f});
		confirmPromptSprite_->SetSize(kConfirmPromptSize);
		confirmPromptSprite_->SetColor(
		    isEnabled_ ? Vector4{1.0f, 1.0f, 1.0f, 0.96f}
		               : Vector4{0.45f, 0.45f, 0.45f, 0.55f});
	}
}

SimpleButton& SimpleButtonGroup::Add(
    uint32_t textureHandle,
    const Vector2& position,
    const Vector2& size,
    const SimpleButton::Colors& colors) {
	auto button = std::make_unique<SimpleButton>();
	button->Initialize(textureHandle, position, size, colors);
	buttons_.push_back(std::move(button));
	if (focusedIndex_ < 0) {
		SetFocusedIndex(0);
	}
	return *buttons_.back();
}

void SimpleButtonGroup::Clear() {
	buttons_.clear();
	focusedIndex_ = -1;
}

void SimpleButtonGroup::Update() {
	for (const auto& button : buttons_) {
		button->Update();
	}
	if (buttons_.empty()) {
		return;
	}

	const Input* input = Input::GetInstance();
	int directionX = 0;
	int directionY = 0;
	if (input != nullptr && input->TriggerKey(DIK_LEFT) || IsControllerButtonTriggered(XINPUT_GAMEPAD_DPAD_LEFT)) {
		directionX = -1;
	} else if (input != nullptr && input->TriggerKey(DIK_RIGHT) || IsControllerButtonTriggered(XINPUT_GAMEPAD_DPAD_RIGHT)) {
		directionX = 1;
	} else if (input != nullptr && input->TriggerKey(DIK_UP) || IsControllerButtonTriggered(XINPUT_GAMEPAD_DPAD_UP)) {
		directionY = -1;
	} else if (input != nullptr && input->TriggerKey(DIK_DOWN) || IsControllerButtonTriggered(XINPUT_GAMEPAD_DPAD_DOWN)) {
		directionY = 1;
	}
	if (directionX == 0 && directionY == 0 || focusedIndex_ < 0) {
		return;
	}

	const SimpleButton& focused = *buttons_[focusedIndex_];
	const Vector2 focusedCenter = {
		focused.GetPosition().x + focused.GetSize().x * 0.5f,
		focused.GetPosition().y + focused.GetSize().y * 0.5f,
	};
	float bestDistanceSquared = (std::numeric_limits<float>::max)();
	int nextIndex = focusedIndex_;
	for (size_t index = 0; index < buttons_.size(); ++index) {
		if (!buttons_[index]->IsEnabled() || static_cast<int>(index) == focusedIndex_) {
			continue;
		}
		const Vector2 candidateCenter = {
			buttons_[index]->GetPosition().x + buttons_[index]->GetSize().x * 0.5f,
			buttons_[index]->GetPosition().y + buttons_[index]->GetSize().y * 0.5f,
		};
		const float differenceX = candidateCenter.x - focusedCenter.x;
		const float differenceY = candidateCenter.y - focusedCenter.y;
		if ((directionX < 0 && differenceX >= 0.0f) || (directionX > 0 && differenceX <= 0.0f) ||
		    (directionY < 0 && differenceY >= 0.0f) || (directionY > 0 && differenceY <= 0.0f)) {
			continue;
		}
		const float distanceSquared = differenceX * differenceX + differenceY * differenceY;
		if (distanceSquared < bestDistanceSquared) {
			bestDistanceSquared = distanceSquared;
			nextIndex = static_cast<int>(index);
		}
	}
	if (nextIndex != focusedIndex_) {
		SetFocusedIndex(static_cast<size_t>(nextIndex));
	}
}

void SimpleButtonGroup::Draw() const {
	for (const auto& button : buttons_) {
		button->Draw();
	}
}

int SimpleButtonGroup::ConsumeClickedIndex() {
	for (size_t index = 0; index < buttons_.size(); ++index) {
		if (buttons_[index]->ConsumeClick()) {
			return static_cast<int>(index);
		}
	}
	return -1;
}

void SimpleButtonGroup::SetFocusedIndex(size_t index) {
	if (index >= buttons_.size() || !buttons_[index]->IsEnabled()) {
		return;
	}
	for (size_t buttonIndex = 0; buttonIndex < buttons_.size(); ++buttonIndex) {
		buttons_[buttonIndex]->SetFocused(buttonIndex == index);
	}
	focusedIndex_ = static_cast<int>(index);
}

SimpleButton* SimpleButtonGroup::Get(size_t index) {
	return index < buttons_.size() ? buttons_[index].get() : nullptr;
}

const SimpleButton* SimpleButtonGroup::Get(size_t index) const {
	return index < buttons_.size() ? buttons_[index].get() : nullptr;
}
