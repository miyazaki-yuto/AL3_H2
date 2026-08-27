#pragma once

#include "KamataEngine.h"
#include "SimpleButton.h"

#include <array>
#include <memory>

class ExplanationScene final {
public:
	void Initialize();
	void Update();
	void Draw();

	bool IsBackRequested() const { return isBackRequested_; }

private:
	std::array<std::unique_ptr<KamataEngine::Sprite>, 3> guideSprites_;
	std::unique_ptr<KamataEngine::Sprite> previousPagePromptSprite_;
	std::unique_ptr<KamataEngine::Sprite> nextPagePromptSprite_;
	std::array<std::unique_ptr<KamataEngine::Sprite>, 3> pageIndicatorSprites_;
	SimpleButton backButton_;
	size_t currentPage_ = 0;
	bool isBackRequested_ = false;
};
