#pragma once

#include "SimpleButton.h"

class GameClear {
public:
	GameClear() = default;
	~GameClear() = default;

	void Initialize();
	void Update();
	void Draw();
	bool IsFinished() const { return retryRequested_ || titleRequested_; }
	bool IsRetryRequested() const { return retryRequested_; }
	bool IsTitleRequested() const { return titleRequested_; }

private:
	std::unique_ptr<KamataEngine::Sprite> backgroundSprite_;
	std::unique_ptr<KamataEngine::Sprite> logoSprite_;
	SimpleButtonGroup buttons_;
	bool retryRequested_ = false;
	bool titleRequested_ = false;
};
