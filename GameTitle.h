#pragma once

#include "SimpleButton.h"

#include <memory>

class GameTitle {
public:
	GameTitle() = default;
	~GameTitle() = default;

	void Initialize();
	void Update();
	void Draw();

	bool IsFinished() const { return isFinished_; }

private:
	std::unique_ptr<KamataEngine::Sprite> titleLogoSprite_;
	SimpleButton startButton_;
	bool isFinished_ = false;
};
