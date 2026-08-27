#pragma once

#include "KamataEngine.h"

#include <cstdint>
#include <memory>
#include <vector>

// Sprite を使う軽量な2Dボタン。
// Update() の後に WasClicked() または ConsumeClick() でクリックを取得する。
class SimpleButton final {
public:
	struct Colors {
		KamataEngine::Vector4 normal = {1.0f, 1.0f, 1.0f, 1.0f};
		KamataEngine::Vector4 hover = {1.15f, 1.15f, 1.15f, 1.0f};
		KamataEngine::Vector4 pressed = {0.85f, 0.85f, 0.85f, 1.0f};
		KamataEngine::Vector4 disabled = {0.45f, 0.45f, 0.45f, 0.65f};
	};

	void Initialize(
	    uint32_t textureHandle,
	    const KamataEngine::Vector2& position,
	    const KamataEngine::Vector2& size,
	    const Colors& colors = {});
	void Update();
	void Draw() const;

	bool WasClicked() const { return wasClicked_; }
	bool ConsumeClick();
	bool IsHovered() const { return isHovered_; }
	void SetFocused(bool focused);
	bool IsFocused() const { return isFocused_; }
	void TriggerClick() { wasClicked_ = isEnabled_; }
	void SetEnabled(bool enabled) { isEnabled_ = enabled; }
	bool IsEnabled() const { return isEnabled_; }
	void SetPosition(const KamataEngine::Vector2& position);
	void SetSize(const KamataEngine::Vector2& size);
	void SetTextureRect(
	    const KamataEngine::Vector2& texturePosition,
	    const KamataEngine::Vector2& textureSize);
	void SetConfirmPromptVisible(bool visible) { showConfirmPrompt_ = visible; }
	void SetColors(const Colors& colors) { colors_ = colors; }
	const KamataEngine::Vector2& GetPosition() const { return position_; }
	const KamataEngine::Vector2& GetSize() const { return baseSize_; }

private:
	bool IsMouseInside() const;
	void ApplyVisuals();

	std::unique_ptr<KamataEngine::Sprite> sprite_;
	std::unique_ptr<KamataEngine::Sprite> confirmPromptSprite_;
	KamataEngine::Vector2 position_{};
	KamataEngine::Vector2 baseSize_{};
	Colors colors_{};
	float currentScale_ = 1.0f;
	float pulsePhase_ = 0.0f;
	bool isHovered_ = false;
	bool isFocused_ = false;
	bool isPressed_ = false;
	bool wasClicked_ = false;
	bool isEnabled_ = true;
	bool showConfirmPrompt_ = true;

	inline static constexpr float kHoverScale = 1.08f;
	inline static constexpr float kPressedScale = 0.94f;
	inline static constexpr float kAnimationSpeed = 0.25f;
	// 選択中のボタンを見失わないよう、約1.5秒周期でゆっくり脈動させる。
	inline static constexpr float kPulseAmplitude = 0.045f;
	inline static constexpr float kPulseSpeed = 0.07f;
};

// 多数のSimpleButtonをまとめて扱うためのコンテナ。
class SimpleButtonGroup final {
public:
	SimpleButton& Add(
	    uint32_t textureHandle,
	    const KamataEngine::Vector2& position,
	    const KamataEngine::Vector2& size,
	    const SimpleButton::Colors& colors = {});
	void Clear();
	void Update();
	void Draw() const;
	int ConsumeClickedIndex();
	void SetFocusedIndex(size_t index);
	int GetFocusedIndex() const { return focusedIndex_; }
	SimpleButton* Get(size_t index);
	const SimpleButton* Get(size_t index) const;
	size_t Size() const { return buttons_.size(); }

private:
	std::vector<std::unique_ptr<SimpleButton>> buttons_;
	int focusedIndex_ = -1;
};
